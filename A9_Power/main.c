/*
 * main.c
 * Lab 9 test program for the power module: sets up I2C1, lists the devices
 * that answer on it, then prints the battery voltage from the divider and the
 * INA219's bus voltage and current every 500 ms as
 * "divider_V, bus_V, current_mA".
 *
 * Modules:
 *   power.c/.h  battery divider on GP28, INA219 on I2C1 (GP2/GP3)
 *
 * Author:  OCE360/560 instructors
 * License: MIT (see LICENSE)
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "power.h"

#define SENSOR_I2C i2c1
#define SENSOR_SDA_PIN 2
#define SENSOR_SCL_PIN 3
// 100 kHz, the same as Lab 6: the BNO080 on this bus stretches the clock
#define SENSOR_I2C_HZ (100 * 1000)
#define SCAN_TIMEOUT_US 10000

#define PRINT_PERIOD_MS 500
#define USB_CONNECT_WAIT_MS 2000

/** Print the address of every device that answers on the bus (0x40 INA219, 0x4A BNO080). */
static void scan_bus(void) {
    printf("# I2C scan:");
    for (int address = 0x08; address < 0x78; address++) {   // 0x00-0x07, 0x78-0x7F are reserved
        uint8_t ignored;
        if (i2c_read_timeout_us(SENSOR_I2C, address, &ignored, 1, false, SCAN_TIMEOUT_US) >= 0) {
            printf(" 0x%02X", address);
        }
    }
    printf("\r\n");
}

int main() {
    stdio_init_all();
    sleep_ms(USB_CONNECT_WAIT_MS);   // so the scan reaches the Serial Monitor

    // The program sets up the bus, not the modules: the IMU and the INA219 share it.
    i2c_init(SENSOR_I2C, SENSOR_I2C_HZ);
    gpio_set_function(SENSOR_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SENSOR_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SENSOR_SDA_PIN);
    gpio_pull_up(SENSOR_SCL_PIN);
    scan_bus();

    if (!power_init(SENSOR_I2C)) {
        printf("# INA219 not found: only divider_V is real\r\n");
    }
    printf("divider_V, bus_V, current_mA\r\n");

    while (true) {
        float bus_volts = 0.0f, current_ma = 0.0f;
        if (!power_read_ina219(&bus_volts, &current_ma)) {
            printf("# INA219 read failed\r\n");
        }
        printf("%.2f, %.3f, %.1f\r\n", power_read_divider_volts(), bus_volts, current_ma);
        sleep_ms(PRINT_PERIOD_MS);
    }
}
