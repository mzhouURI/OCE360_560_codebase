/*
 * sd_logger.h
 * Appends lines of text to a file on the SD card. Every line is synced to the
 * card as soon as it's written, so a power cut loses at most the line being
 * written. Wiring is in sd_logger.c.
 *
 * Author:  OCE360/560 instructors
 * License: MIT (see LICENSE)
 */
#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <stdbool.h>

/**
 * Mount the card and open a file for appending.
 *   path:   file name on the card, e.g. "log.csv"
 *   header: written first, only if the file is new (empty)
 * Returns false if the card or file can't be opened; the FatFs error code
 * is printed to serial.
 */
bool sd_logger_open(const char *path, const char *header);

/**
 * Append one line (include the "\r\n") and sync it to the card.
 * Returns false if any part of the line didn't reach the card.
 */
bool sd_logger_write(const char *line);

/** Close the file and unmount the card. The card is safe to remove afterwards. */
void sd_logger_close(void);

#endif
