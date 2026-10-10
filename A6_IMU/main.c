/*
 * main.c
 * Smallest working use of the bno08x driver: set up I2C1, start the BNO080,
 * turn on the rotation vector at 50 Hz, and print each quaternion over USB.
 *
 * Wiring (Adafruit 4754 BNO085 breakout):
 *   VIN -> 3V3(OUT), GND -> GND, SDA -> GP2 (I2C1 SDA), SCL -> GP3 (I2C1 SCL),
 *   INT -> GP12 and RST -> GP13 (both optional; set them to
 *   BNO08X_PIN_NOT_WIRED if you leave them off). Leave P0, P1, DI unconnected.
 *
 * Author:  OCE360/560 instructors
 * License: MIT (see LICENSE)
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "bno08x.h"

#define IMU_I2C i2c1
#define IMU_SDA_PIN 2
#define IMU_SCL_PIN 3
#define IMU_INT_PIN 12
#define IMU_RESET_PIN 13
// 100 kHz, not 400: the BNO080 stretches the clock, and slower is safer
#define IMU_I2C_HZ (100 * 1000)
#define REPORT_PERIOD_US (20 * 1000)   // 50 Hz
#define USB_CONNECT_WAIT_MS 2000

static void halt(void) {
    while (true) sleep_ms(1000);
}

int main() {
    stdio_init_all();
    sleep_ms(USB_CONNECT_WAIT_MS);   // so the first messages reach the Serial Monitor

    // The program sets up the bus, not the driver: other I2C parts can share it.
    i2c_init(IMU_I2C, IMU_I2C_HZ);
    gpio_set_function(IMU_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(IMU_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(IMU_SDA_PIN);
    gpio_pull_up(IMU_SCL_PIN);

    bno08x_config_t imu_config = {
        .i2c = IMU_I2C,
        .address = BNO08X_ADDRESS_DEFAULT,
        .int_pin = IMU_INT_PIN,
        .reset_pin = IMU_RESET_PIN,
    };
    if (!bno08x_init(&imu_config)) {
        printf("# IMU init failed: no answer at 0x%02X\r\n", BNO08X_ADDRESS_DEFAULT);
        halt();
    }
    if (!bno08x_enable_report(BNO08X_REPORT_ROTATION_VECTOR, REPORT_PERIOD_US)) {
        printf("# IMU did not accept the rotation vector request\r\n");
        halt();
    }
    printf("# rotation vector every %lu us\r\n",
           (unsigned long)bno08x_report_period_us(BNO08X_REPORT_ROTATION_VECTOR));
    printf("real, i, j, k, status\r\n");

    while (true) {
        int reports = bno08x_service();
        if (reports < 0) {
            printf("# I2C error %d\r\n", reports);
        }
        bno08x_quaternion_t quaternion;
        if (bno08x_get_quaternion(BNO08X_REPORT_ROTATION_VECTOR, &quaternion)) {
            printf("%.4f, %.4f, %.4f, %.4f, %u\r\n", quaternion.real, quaternion.i, quaternion.j,
                   quaternion.k, quaternion.status);
        }
    }
}
