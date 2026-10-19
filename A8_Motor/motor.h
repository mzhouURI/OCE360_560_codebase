/*
 * motor.h
 * The shared geared DC motor (12 V, 45:1, Hall quadrature encoder) through a
 * two-input H-bridge (DRV8833, TB6612FNG or L298N style). Wiring is in motor.c.
 *
 * Sign convention: positive duty drives IN1, and forward counts are positive
 * (encoder A leading B). If positive duty gives negative counts on your
 * station, swap the encoder A and B wires (or the pin numbers in motor.c).
 *
 * Author:  OCE360/560 instructors
 * License: MIT (see LICENSE)
 */
#ifndef MOTOR_H
#define MOTOR_H

#include <stdint.h>

/**
 * Set up both H-bridge inputs for 20 kHz PWM with the motor stopped, and start
 * counting encoder edges. Call it once. It uses its own interrupt handler for
 * the encoder pins, so a button callback set with
 * gpio_set_irq_enabled_with_callback still works.
 */
void motor_init(void);

/**
 * Set speed and direction. duty runs from -1.0 (full reverse) through 0 (stop)
 * to +1.0 (full forward); values outside that range are clamped.
 * Go through 0 before changing direction: reversing a spinning motor draws
 * more current than a start.
 */
void motor_set_duty(float duty);

/** Encoder count since motor_init: 4 counts per encoder pulse, positive = forward. */
int32_t motor_read_counts(void);

/**
 * Convert a change in counts over window_ms milliseconds into output-shaft
 * revolutions per minute (2,160 counts per output turn). Negative in reverse.
 * Use it with two motor_read_counts() calls when the program can't block.
 */
float motor_counts_to_rpm(int32_t counts, uint32_t window_ms);

/**
 * Measure output-shaft speed by counting encoder edges for window_ms.
 * Blocks for window_ms. Returns revolutions per minute, negative in reverse.
 */
float motor_measure_rpm(uint32_t window_ms);

#endif
