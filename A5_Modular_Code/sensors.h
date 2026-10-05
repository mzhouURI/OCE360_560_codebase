/*
 * sensors.h
 * The Lab 3 sensors, read in physical units: a photocell divider (lux)
 * and an LM19 temperature sensor (degrees C). Wiring is in sensors.c.
 *
 * Author:  OCE360/560 instructors
 * License: MIT (see LICENSE)
 */
#ifndef SENSORS_H
#define SENSORS_H

/** Set up the ADC and both sensor pins. Call once before reading. */
void sensors_init(void);

/** Read the photocell once. Returns illuminance in lux (good to about a factor of 3). */
float sensors_read_lux(void);

/** Read the LM19 once. Returns temperature in degrees C. */
float sensors_read_celsius(void);

#endif
