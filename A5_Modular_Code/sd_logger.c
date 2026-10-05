/*
 * sd_logger.c
 * SD card logging over SPI0, using elehobica/pico_fatfs (ChaN's FatFs R0.15)
 * from lib/pico_fatfs.
 *
 * Wiring: breakout CLK -> GP18 (SPI0 SCK), DI -> GP19 (SPI0 TX),
 * DO -> GP20 (SPI0 RX), CS -> GP21, 3V -> 3V3(OUT), GND -> GND.
 *
 * Author:  OCE360/560 instructors
 * License: MIT (see LICENSE)
 */
#include <stdio.h>
#include <string.h>
#include "ff.h"
#include "tf_card.h"
#include "sd_logger.h"

static pico_fatfs_spi_config_t sd_config = {
    spi0,
    CLK_SLOW_DEFAULT,   // 100 kHz while the card starts up
    12500 * KHZ,        // then 12.5 MHz: the 37.5 MHz default is too fast for jumper wires
    20,                 // DO  (SPI0 RX)
    21,                 // CS
    18,                 // CLK (SPI0 SCK)
    19,                 // DI  (SPI0 TX)
    true                // internal pull-ups on DO and DI
};

// FatFs keeps pointers to these between calls, so they can't live on the stack.
static FATFS filesystem;
static FIL log_file;

static bool write_text(const char *text) {
    UINT length = strlen(text);
    UINT bytes_written;
    return f_write(&log_file, text, length, &bytes_written) == FR_OK && bytes_written == length;
}

bool sd_logger_open(const char *path, const char *header) {
    pico_fatfs_set_config(&sd_config);   // must come before f_mount

    FRESULT result = f_mount(&filesystem, "", 1);
    if (result != FR_OK) {
        printf("f_mount error %d (FRESULT codes are listed in ff.h)\r\n", result);
        return false;
    }
    result = f_open(&log_file, path, FA_OPEN_APPEND | FA_WRITE);
    if (result != FR_OK) {
        printf("f_open error %d (FRESULT codes are listed in ff.h)\r\n", result);
        return false;
    }
    bool file_is_new = f_size(&log_file) == 0;
    if (file_is_new) {
        return write_text(header) && f_sync(&log_file) == FR_OK;
    }
    return true;
}

bool sd_logger_write(const char *line) {
    return write_text(line) && f_sync(&log_file) == FR_OK;
}

void sd_logger_close(void) {
    f_close(&log_file);
    f_unmount("");
}
