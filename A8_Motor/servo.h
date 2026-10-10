/*
 * servo.h
 * A hobby servo (Adafruit 169, TowerPro SG92R) on one PWM pin at 50 Hz.
 * Wiring and the measured endpoints are in servo.c.
 *
 * Author:  OCE360/560 instructors
 * License: MIT (see LICENSE)
 */
#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>

/** Set up the servo pin for 50 Hz PWM and send the center pulse (1500 us). */
void servo_init(void);

/**
 * Send a pulse of pulse_us microseconds every 20 ms.
 * The pulse is clamped to the endpoints measured on your servo (MIN_PULSE_US,
 * MAX_PULSE_US in servo.c), so the servo never pushes against its end stop.
 * Returns the pulse width actually sent, in us.
 */
uint16_t servo_set_pulse_us(uint16_t pulse_us);

/**
 * Move to angle_deg, from -90 (MIN_PULSE_US) to +90 (MAX_PULSE_US) degrees,
 * on a straight line between the measured endpoints. Angles outside that
 * range are clamped. Returns the pulse width sent, in us.
 */
uint16_t servo_set_angle(float angle_deg);

#endif
