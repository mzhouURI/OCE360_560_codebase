/*
 * bno08x.h
 * A small polled driver for the BNO080/BNO085/BNO086 IMU over I2C on the
 * Raspberry Pi Pico SDK. The chip does its own sensor fusion and sends
 * "reports" (SHTP packets carrying SH-2 input reports); this driver turns on
 * the reports you ask for and keeps the newest value of each.
 *
 * The caller owns the I2C bus: set it up (i2c_init, gpio_set_function,
 * pull-ups) before bno08x_init(). The driver never changes the bus speed or
 * pins, so other devices can share it. 100 kHz is recommended: the BNO08x
 * stretches the clock.
 *
 * Every I2C transfer has a timeout, so a missing or stuck IMU makes these
 * functions return an error instead of freezing the Pico.
 *
 * Only one BNO08x per program (the driver keeps its state in file-level
 * variables).
 *
 * Sources: BNO080 Datasheet Rev 1.3 (Hillcrest Labs, 1000-3927) and the SH-2
 * Reference Manual Rev 1.2 (Hillcrest Labs, 1000-3625). See README.md.
 *
 * Author:  OCE360/560 instructors
 * License: MIT (see LICENSE in this folder)
 */
#ifndef BNO08X_H
#define BNO08X_H

#include <stdbool.h>
#include <stdint.h>
#include "hardware/i2c.h"

#define BNO08X_ADDRESS_DEFAULT 0x4A     // Adafruit 4754: DI (SA0) pin low
#define BNO08X_ADDRESS_ALTERNATE 0x4B   // SA0 high (SparkFun's default)

// Report IDs (SH-2 Reference Manual, section 6.5). Units are what the get functions return.
#define BNO08X_REPORT_ACCELEROMETER        0x01  // m/s^2, includes gravity
#define BNO08X_REPORT_GYROSCOPE            0x02  // rad/s, calibrated
#define BNO08X_REPORT_MAGNETIC_FIELD       0x03  // microtesla, calibrated
#define BNO08X_REPORT_LINEAR_ACCELERATION  0x04  // m/s^2, gravity removed
#define BNO08X_REPORT_ROTATION_VECTOR      0x05  // quaternion: accel + gyro + mag
#define BNO08X_REPORT_GRAVITY              0x06  // m/s^2, gravity only
#define BNO08X_REPORT_GAME_ROTATION_VECTOR 0x08  // quaternion: accel + gyro, no mag
#define BNO08X_REPORT_GEOMAGNETIC_ROTATION_VECTOR 0x09  // quaternion: accel + mag, no gyro

#define BNO08X_PIN_NOT_WIRED (-1)

/** Where the IMU is. The bus must already be set up by the caller. */
typedef struct {
    i2c_inst_t *i2c;   // i2c0 or i2c1
    uint8_t address;   // BNO08X_ADDRESS_DEFAULT unless SA0 is high
    int int_pin;       // H_INTN (the breakout's INT), active low, or BNO08X_PIN_NOT_WIRED
    int reset_pin;     // NRST (the breakout's RST), active low, or BNO08X_PIN_NOT_WIRED
} bno08x_config_t;

/** One orientation report. */
typedef struct {
    float real, i, j, k;   // unit quaternion
    float accuracy_rad;    // heading accuracy estimate; NAN for the game rotation vector
    uint8_t status;        // 0 unreliable, 1 low, 2 medium, 3 high accuracy
    uint64_t time_us;      // Pico time (since boot) when the report was read
} bno08x_quaternion_t;

/** One three-axis report (accelerometer, gyroscope, magnetometer, ...). */
typedef struct {
    float x, y, z;         // in the report's units, on the chip's X, Y, Z axes
    uint8_t status;        // 0 unreliable, 1 low, 2 medium, 3 high accuracy
    uint64_t time_us;      // Pico time (since boot) when the report was read
} bno08x_vector_t;

/**
 * Reset the IMU and check that it answers.
 * Pulses RST if it's wired, otherwise sends a reset command over I2C, then
 * waits up to about 2.5 s for the chip to restart and answer a Product ID
 * request. All reports start off; turn them on with bno08x_enable_report().
 *   config: bus, address, and optional INT and RST pins (copied; may be a local)
 * Returns true if the IMU answered, false if it didn't (wiring, address, power).
 */
bool bno08x_init(const bno08x_config_t *config);

/**
 * Ask the IMU to send one report at a fixed period, and check that it agreed.
 * The chip may pick a nearby period: bno08x_report_period_us() says which.
 * The request is remembered and sent again if the IMU resets on its own.
 *   report_id: one of the BNO08X_REPORT_ values
 *   period_us: time between reports in microseconds (20000 = 50 Hz)
 * Returns false for an unsupported report ID, an I2C error, or no answer
 * within 300 ms.
 */
bool bno08x_enable_report(uint8_t report_id, uint32_t period_us);

/**
 * The report period the IMU actually granted, in microseconds.
 * Returns 0 if the report isn't enabled.
 */
uint32_t bno08x_report_period_us(uint8_t report_id);

/**
 * Read every packet the IMU has waiting (up to 10) and save the newest value
 * of each report. Call it often: every pass through your main loop.
 * If INT is wired and high, returns right away with no I2C traffic.
 * Returns the number of sensor reports decoded (0 if nothing was waiting),
 * or a negative PICO_ERROR_ code if an I2C transfer failed or timed out.
 */
int bno08x_service(void);

/**
 * Get the newest quaternion report.
 *   report_id:  BNO08X_REPORT_ROTATION_VECTOR, _GAME_ROTATION_VECTOR, or
 *               _GEOMAGNETIC_ROTATION_VECTOR
 *   quaternion: filled with the newest value (left unchanged if none ever arrived)
 * Returns true only if a new report arrived since the last call for this ID.
 */
bool bno08x_get_quaternion(uint8_t report_id, bno08x_quaternion_t *quaternion);

/**
 * Get the newest three-axis report.
 *   report_id: BNO08X_REPORT_ACCELEROMETER, _GYROSCOPE, _MAGNETIC_FIELD,
 *              _LINEAR_ACCELERATION, or _GRAVITY
 *   vector:    filled with the newest value (left unchanged if none ever arrived)
 * Returns true only if a new report arrived since the last call for this ID.
 */
bool bno08x_get_vector(uint8_t report_id, bno08x_vector_t *vector);

#endif
