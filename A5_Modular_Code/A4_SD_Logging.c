/*
 * A4_SD_Logging.c
 * Environmental logger: reads light and temperature once per sample period,
 * prints each reading over USB serial, and appends it to log.csv on the SD card
 * as "time_s, lux, temp_C". A status LED lights when it's dark or hot.
 *
 * Modules:
 *   sensors.c/.h    photocell and LM19 readings in lux and degrees C
 *   sd_logger.c/.h  SD card mount, append, sync
 *
 * Status LED: GP16 -> 100 ohm -> LED -> GND.
 *
 * Author:  OCE360/560 instructors
 * License: MIT (see LICENSE)
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "sensors.h"
#include "sd_logger.h"

#define STATUS_LED_PIN 16
#define SAMPLE_PERIOD_MS 1000
#define USB_CONNECT_WAIT_MS 2000

#define DARK_LUX 20.0f
#define HOT_CELSIUS 30.0f

#define LOG_FILE_NAME "log.csv"
#define LOG_HEADER "time_s, lux, temp_C\r\n"

static void halt(void) {
    while (true) sleep_ms(1000);
}

int main() {
    stdio_init_all();
    gpio_init(STATUS_LED_PIN);
    gpio_set_dir(STATUS_LED_PIN, GPIO_OUT);
    sensors_init();

    sleep_ms(USB_CONNECT_WAIT_MS);   // so SD card errors reach the Serial Monitor

    if (!sd_logger_open(LOG_FILE_NAME, LOG_HEADER)) {
        halt();
    }

    absolute_time_t next_sample_time = get_absolute_time();
    while (true) {
        // double: a float can't resolve milliseconds once the logger has run for hours
        double time_s = to_ms_since_boot(get_absolute_time()) / 1000.0;
        float lux = sensors_read_lux();
        float temp_c = sensors_read_celsius();

        char line[64];
        snprintf(line, sizeof line, "%.3f, %.1f, %.1f\r\n", time_s, lux, temp_c);
        printf("%s", line);
        if (!sd_logger_write(line)) {
            printf("# SD write failed\r\n");
        }

        gpio_put(STATUS_LED_PIN, lux < DARK_LUX || temp_c > HOT_CELSIUS);

        // sleep_until, not sleep_ms: a slow SD write doesn't push every later sample back
        next_sample_time = delayed_by_ms(next_sample_time, SAMPLE_PERIOD_MS);
        sleep_until(next_sample_time);
    }
}
