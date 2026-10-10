/*
 * servo.c
 * Drives the micro servo with 50 Hz PWM from the Pico 2's PWM slice 3.
 *
 * Wiring (Lab 8):
 *   Servo orange (signal) -> GP6 (PWM 3A)
 *   Servo red (+5 V)      -> 5 V rail (kit 7805 or a bench supply), NOT 3V3
 *   Servo brown (GND)     -> GND, shared with the Pico
 *
 * Author:  OCE360/560 instructors
 * License: MIT (see LICENSE)
 */
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "servo.h"

#define SERVO_PIN 6

// 150 MHz / 150 = 1 MHz, so one PWM count is 1 us and the level IS the pulse width in us.
#define PWM_CLOCK_DIVIDER 150.0f
#define SERVO_PERIOD_US 20000               // 50 Hz
#define CENTER_PULSE_US 1500

// Replace these with the endpoints you measured on your servo (Lab 8, Step 5).
#define MIN_PULSE_US 1000                   // -90 degrees
#define MAX_PULSE_US 2000                   // +90 degrees

#define MIN_ANGLE_DEG -90.0f
#define MAX_ANGLE_DEG 90.0f

static uint slice;
static uint channel;

void servo_init(void) {
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    slice = pwm_gpio_to_slice_num(SERVO_PIN);
    channel = pwm_gpio_to_channel(SERVO_PIN);

    pwm_set_clkdiv(slice, PWM_CLOCK_DIVIDER);
    pwm_set_wrap(slice, SERVO_PERIOD_US - 1);   // the counter runs 0..wrap: wrap + 1 counts per period
    pwm_set_chan_level(slice, channel, CENTER_PULSE_US);
    pwm_set_enabled(slice, true);
}

uint16_t servo_set_pulse_us(uint16_t pulse_us) {
    // Past the endpoints the servo pushes against its stop and draws stall current.
    if (pulse_us < MIN_PULSE_US) pulse_us = MIN_PULSE_US;
    if (pulse_us > MAX_PULSE_US) pulse_us = MAX_PULSE_US;
    pwm_set_chan_level(slice, channel, pulse_us);
    return pulse_us;
}

uint16_t servo_set_angle(float angle_deg) {
    if (angle_deg < MIN_ANGLE_DEG) angle_deg = MIN_ANGLE_DEG;
    if (angle_deg > MAX_ANGLE_DEG) angle_deg = MAX_ANGLE_DEG;

    float fraction = (angle_deg - MIN_ANGLE_DEG) / (MAX_ANGLE_DEG - MIN_ANGLE_DEG);   // 0 to 1
    float pulse_us = MIN_PULSE_US + fraction * (MAX_PULSE_US - MIN_PULSE_US);
    return servo_set_pulse_us((uint16_t)(pulse_us + 0.5f));   // + 0.5: round, don't truncate
}
