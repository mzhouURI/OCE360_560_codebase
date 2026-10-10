/*
 * motor.c
 * Speed and direction of the geared DC motor with 20 kHz PWM on both inputs of
 * a two-input H-bridge, and its quadrature encoder read with GPIO interrupts.
 *
 * Wiring (Lab 8, motor station):
 *   Pico GP8 (PWM 4A) -> H-bridge IN1      Pico GP9 (PWM 4B) -> H-bridge IN2
 *   H-bridge OUT1/OUT2 -> motor red/black   H-bridge motor supply -> station bench supply
 *   Encoder yellow (A) -> GP10   Encoder white (B) -> GP11
 *   Encoder blue (Vcc) -> 3V3(OUT)   Encoder green (GND) -> GND
 *   Pico GND, H-bridge GND and the bench supply minus are all connected.
 *
 * Author:  OCE360/560 instructors
 * License: MIT (see LICENSE)
 */
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "motor.h"

#define IN1_PIN 8                       // PWM 4A
#define IN2_PIN 9                       // PWM 4B, same slice as IN1
#define ENCODER_A_PIN 10
#define ENCODER_B_PIN 11

// 150 MHz / (1 x 7500 counts) = 20 kHz: above hearing, and the motor's inductance smooths it.
#define PWM_CLOCK_DIVIDER 1.0f
#define PWM_COUNTS_PER_PERIOD 7500

// 12 pulses per motor turn on each channel, 4 edges per pulse, 45:1 gearbox: 2,160.
#define ENCODER_PULSES_PER_MOTOR_REV 12
#define EDGES_PER_PULSE 4
#define GEAR_RATIO 45
#define COUNTS_PER_OUTPUT_REV (ENCODER_PULSES_PER_MOTOR_REV * EDGES_PER_PULSE * GEAR_RATIO)

#define MS_PER_MINUTE 60000.0f
#define BOTH_EDGES (GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL)

static uint slice;

// Written by the interrupt, read by the main loop: volatile so the compiler re-reads it.
static volatile int32_t encoder_counts = 0;
static uint8_t last_encoder_state = 0;

// Change in count for each (old state, new state) pair, where state = A * 2 + B.
// Forward (A leads B) is 00 -> 10 -> 11 -> 01 -> 00. A pair where both channels
// changed means an edge was missed: it counts 0.
static const int8_t COUNT_STEP[16] = {
//  new: 00  01  10  11
         0, -1, +1,  0,    // old 00
        +1,  0,  0, -1,    // old 01
        -1,  0,  0, +1,    // old 10
         0, +1, -1,  0     // old 11
};

static uint8_t read_encoder_state(void) {
    return (gpio_get(ENCODER_A_PIN) << 1) | gpio_get(ENCODER_B_PIN);
}

// Runs on every edge of A or B. Keep it short: no printf, no sleep.
static void encoder_irq_handler(void) {
    // A raw handler clears its own events (Lab 2's callback did that for you).
    // Clear first, then read: an edge after the read raises a new interrupt instead of being lost.
    gpio_acknowledge_irq(ENCODER_A_PIN, BOTH_EDGES);
    gpio_acknowledge_irq(ENCODER_B_PIN, BOTH_EDGES);

    // Also runs when another pin's interrupt fires; then the state hasn't changed and this adds 0.
    uint8_t state = read_encoder_state();
    encoder_counts += COUNT_STEP[(last_encoder_state << 2) | state];
    last_encoder_state = state;
}

static void encoder_init(void) {
    gpio_init(ENCODER_A_PIN);
    gpio_init(ENCODER_B_PIN);
    gpio_set_dir(ENCODER_A_PIN, GPIO_IN);
    gpio_set_dir(ENCODER_B_PIN, GPIO_IN);
    gpio_pull_up(ENCODER_A_PIN);        // harmless if the encoder drives its outputs; needed if they are open collector
    gpio_pull_up(ENCODER_B_PIN);
    last_encoder_state = read_encoder_state();

    // A handler for these two pins only, not the one-per-core callback from Lab 2.
    gpio_add_raw_irq_handler_masked((1u << ENCODER_A_PIN) | (1u << ENCODER_B_PIN), encoder_irq_handler);
    gpio_set_irq_enabled(ENCODER_A_PIN, BOTH_EDGES, true);
    gpio_set_irq_enabled(ENCODER_B_PIN, BOTH_EDGES, true);
    irq_set_enabled(IO_IRQ_BANK0, true);
}

void motor_init(void) {
    gpio_set_function(IN1_PIN, GPIO_FUNC_PWM);
    gpio_set_function(IN2_PIN, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(IN1_PIN);

    pwm_set_clkdiv(slice, PWM_CLOCK_DIVIDER);
    pwm_set_wrap(slice, PWM_COUNTS_PER_PERIOD - 1);
    pwm_set_both_levels(slice, 0, 0);   // both inputs low: stopped (coast)
    pwm_set_enabled(slice, true);

    encoder_init();
}

void motor_set_duty(float duty) {
    if (duty > 1.0f) duty = 1.0f;
    if (duty < -1.0f) duty = -1.0f;

    // At 100% the level is 7500, one more than wrap: the output stays high all period.
    float magnitude = duty < 0.0f ? -duty : duty;
    uint16_t level = (uint16_t)(magnitude * PWM_COUNTS_PER_PERIOD + 0.5f);
    if (duty >= 0.0f) {
        pwm_set_both_levels(slice, level, 0);   // forward: PWM on IN1, IN2 low
    } else {
        pwm_set_both_levels(slice, 0, level);   // reverse: IN1 low, PWM on IN2
    }
}

int32_t motor_read_counts(void) {
    return encoder_counts;   // one 32-bit read: the interrupt can't split it
}

float motor_counts_to_rpm(int32_t counts, uint32_t window_ms) {
    if (window_ms == 0) return 0.0f;
    float revolutions = (float)counts / COUNTS_PER_OUTPUT_REV;
    float minutes = window_ms / MS_PER_MINUTE;
    return revolutions / minutes;
}

float motor_measure_rpm(uint32_t window_ms) {
    int32_t start_counts = motor_read_counts();
    sleep_ms(window_ms);
    return motor_counts_to_rpm(motor_read_counts() - start_counts, window_ms);
}
