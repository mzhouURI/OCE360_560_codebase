/*
 * power.h
 * Battery and supply monitoring: battery voltage from a resistor divider on
 * the ADC, and current and voltage from an INA219 current sensor on I2C.
 * Wiring is in power.c.
 *
 * Author:  OCE360/560 instructors
 * License: MIT (see LICENSE)
 */
#ifndef POWER_H
#define POWER_H

#include <stdbool.h>
#include "hardware/i2c.h"

/**
 * Set up the divider's ADC pin and configure the INA219 on an I2C bus that is
 * already running. main sets up the bus (pins, speed) because the IMU shares it.
 *   i2c: the bus the INA219 is on, e.g. i2c1
 * Returns false if the INA219 doesn't answer at 0x40. The divider works either way.
 */
bool power_init(i2c_inst_t *i2c);

/** Battery voltage from the divider on GP28, in volts. */
float power_read_divider_volts(void);

/**
 * Read the INA219 once. Each value is the average of its last 128 samples (68 ms).
 *   bus_volts:  voltage on the load side of the shunt (Vin-) to GND, in volts
 *   current_ma: current from Vin+ to Vin-, in mA (negative if it flows backwards)
 * Returns false if the I2C read fails; the outputs are then unchanged.
 */
bool power_read_ina219(float *bus_volts, float *current_ma);

#endif
