/*
 * bno08x.c
 * SHTP over I2C, and the few SH-2 messages Lab 6 and Assignment 4 need:
 * reset, Product ID, Set Feature / Get Feature, and input reports.
 *
 * How the BNO08x talks (BNO080 Datasheet Rev 1.3, section 1.4; SH-2 Reference
 * Manual Rev 1.2, sections 6.3 to 6.5):
 *   - Every packet starts with a 4-byte SHTP header: length (LSB, MSB; bit 15
 *     is a "continuation" flag; the length includes the header), channel,
 *     and a sequence number that counts per channel and per direction.
 *   - Each I2C read starts with a header. To read a packet, read the 4-byte
 *     header to learn its length, then read the whole packet (header again,
 *     then the payload). A packet too long for one read continues in the next
 *     read, behind a fresh header with the continuation bit set.
 *   - Channel 1 carries reset commands and "reset complete"; channel 2 carries
 *     SH-2 control reports (Product ID, Set Feature, Get Feature Response);
 *     channel 3 carries sensor input reports, several to a packet.
 *   - Sensor values are 16-bit signed fixed point: value = raw / 2^Q, where Q
 *     (the "Q point") depends on the report.
 *
 * Written for OCE360/560 from the two documents above. The I2C read pattern
 * (header first, then the whole packet) follows SparkFun's BNO080 Arduino
 * library (MIT) and Adafruit's BNO08x library (BSD); the report lengths match
 * CEVA's sh2 library (Apache 2.0). No code was copied from any of them.
 *
 * Author:  OCE360/560 instructors
 * License: MIT (see LICENSE in this folder)
 */
#include <math.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "bno08x.h"

// SHTP channels (datasheet 1.4.1)
#define CHANNEL_EXECUTABLE 1
#define CHANNEL_CONTROL 2
#define CHANNEL_REPORTS 3
#define CHANNEL_WAKE_REPORTS 4
#define CHANNEL_COUNT 6

#define EXECUTABLE_RESET 1   // host to IMU: "reset"; IMU to host: "reset complete"

// SH-2 report IDs that aren't sensor data (SH-2 Reference Manual 6.3, 6.5, 7.2)
#define REPORT_PRODUCT_ID_RESPONSE 0xF8
#define REPORT_PRODUCT_ID_REQUEST 0xF9
#define REPORT_GET_FEATURE_RESPONSE 0xFC
#define REPORT_SET_FEATURE 0xFD
#define REPORT_GET_FEATURE_REQUEST 0xFE
#define REPORT_FLUSH_COMPLETED 0xEF
#define REPORT_TIMESTAMP_REBASE 0xFA
#define REPORT_BASE_TIMESTAMP 0xFB

#define SHTP_HEADER_BYTES 4
#define SHTP_CONTINUATION_BIT 0x8000
#define SHTP_LENGTH_ALL_ONES 0x7FFF   // what a floating bus reads as: not a real packet
// The startup advertisement (about 280 bytes) is the biggest packet we see.
// Longer packets are read to the end but only this much is kept.
#define MAX_PAYLOAD_BYTES 300
#define SET_FEATURE_BYTES 17           // SH-2 Reference Manual Fig. 68
#define FEATURE_PERIOD_OFFSET 5        // report interval, bytes 5 to 8, little-endian

#define REPORT_HEADER_BYTES 4          // report ID, sequence, status, delay
#define STATUS_ACCURACY_MASK 0x03      // status bits 1:0; bits 7:2 are part of the delay
#define MAX_REPORT_FIELDS 5            // the rotation vector: i, j, k, real, accuracy
#define HIGHEST_REPORT_ID BNO08X_REPORT_GEOMAGNETIC_ROTATION_VECTOR
#define QUATERNION_Q_POINT 14
#define HEADING_ACCURACY_Q_POINT 12

#define MAX_PACKETS_PER_SERVICE 10
// One byte takes 90 us at 100 kHz. The rest is room for the IMU stretching the clock.
#define I2C_BASE_TIMEOUT_US 10000
#define I2C_PER_BYTE_TIMEOUT_US 200

#define RESET_PULSE_MS 10
#define SOFT_RESET_ATTEMPTS 5
#define SOFT_RESET_RETRY_MS 30         // the IMU may still be starting up and not answer yet
#define RESET_TIMEOUT_MS 1000
#define PRODUCT_ID_TIMEOUT_MS 500
#define PRODUCT_ID_ATTEMPTS 3
// The 0xFC with the granted period comes after the sensor starts: 110 ms for a 10 or 20 ms
// report, 190 ms for 50 ms, 343 ms for 100 ms (BNO085, 2026-10-19). So the wait grows with the period.
#define FEATURE_RESPONSE_BASE_MS 300
#define FEATURE_RESPONSE_PERIODS 4

/** How each sensor report we keep is laid out. q_point 0 means "not kept". */
typedef struct {
    uint8_t length;    // bytes, including the 4-byte report header
    uint8_t q_point;   // value = raw / 2^q_point
} report_format_t;

// SH-2 Reference Manual 6.5.9 to 6.5.20
static const report_format_t KEPT_REPORTS[HIGHEST_REPORT_ID + 1] = {
    [BNO08X_REPORT_ACCELEROMETER] = {10, 8},
    [BNO08X_REPORT_GYROSCOPE] = {10, 9},
    [BNO08X_REPORT_MAGNETIC_FIELD] = {10, 4},
    [BNO08X_REPORT_LINEAR_ACCELERATION] = {10, 8},
    [BNO08X_REPORT_ROTATION_VECTOR] = {14, QUATERNION_Q_POINT},
    [BNO08X_REPORT_GRAVITY] = {10, 8},
    [BNO08X_REPORT_GAME_ROTATION_VECTOR] = {12, QUATERNION_Q_POINT},
    [BNO08X_REPORT_GEOMAGNETIC_ROTATION_VECTOR] = {14, QUATERNION_Q_POINT},
};

/** The newest value of one report, still in fixed point. */
typedef struct {
    int16_t raw[MAX_REPORT_FIELDS];
    uint8_t status;
    uint64_t time_us;
    bool has_arrived;   // at least one report since init
    bool is_fresh;      // not yet read by a get function
} saved_report_t;

static bno08x_config_t imu;
static bool is_initialized = false;
static uint8_t next_sequence[CHANNEL_COUNT];

static saved_report_t newest[HIGHEST_REPORT_ID + 1];
static uint32_t requested_period_us[HIGHEST_REPORT_ID + 1];
static uint32_t granted_period_us[HIGHEST_REPORT_ID + 1];

// Static, not on the stack: they're big, and the Pico's stack is small.
static uint8_t transfer_buffer[SHTP_HEADER_BYTES + MAX_PAYLOAD_BYTES];
static uint8_t payload[MAX_PAYLOAD_BYTES];

// Set when the matching message arrives; the wait loops below watch them.
static bool reset_complete_arrived;
static bool product_id_arrived;
static int awaited_feature_id = -1;
static bool awaited_feature_arrived;

static size_t smaller(size_t a, size_t b) {
    return a < b ? a : b;
}

static int16_t read_int16(const uint8_t *bytes) {
    return (int16_t)(bytes[0] | (bytes[1] << 8));
}

static uint32_t read_uint32(const uint8_t *bytes) {
    return bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static void write_uint32(uint8_t *bytes, uint32_t value) {
    for (int n = 0; n < 4; n++) {
        bytes[n] = (value >> (8 * n)) & 0xFF;
    }
}

static uint32_t transfer_timeout_us(size_t length) {
    return I2C_BASE_TIMEOUT_US + I2C_PER_BYTE_TIMEOUT_US * length;
}

static bool is_kept_report(uint8_t report_id) {
    return report_id <= HIGHEST_REPORT_ID && KEPT_REPORTS[report_id].q_point != 0;
}

static bool is_quaternion_report(uint8_t report_id) {
    return report_id == BNO08X_REPORT_ROTATION_VECTOR || report_id == BNO08X_REPORT_GAME_ROTATION_VECTOR ||
           report_id == BNO08X_REPORT_GEOMAGNETIC_ROTATION_VECTOR;
}

static bool interrupt_says_nothing_waiting(void) {
    return imu.int_pin != BNO08X_PIN_NOT_WIRED && gpio_get(imu.int_pin);
}

/**
 * Length of any input report that can share a packet with ours, so we can step
 * over it (lengths from the SH-2 Reference Manual, as listed in CEVA's sh2.c).
 * Returns 0 for an ID we don't know: the rest of that packet can't be parsed.
 */
static size_t input_report_length(uint8_t report_id) {
    if (is_kept_report(report_id)) {
        return KEPT_REPORTS[report_id].length;
    }
    switch (report_id) {
    case REPORT_FLUSH_COMPLETED: return 2;
    case REPORT_TIMESTAMP_REBASE:
    case REPORT_BASE_TIMESTAMP:
    case 0x10: return 5;                                    // tap detector
    case 0x0C: case 0x0D: case 0x0E: case 0x12: case 0x13: // humidity, proximity, temperature,
    case 0x19: case 0x1A: case 0x1C: case 0x1F: case 0x20: // significant motion, stability and
    case 0x21: case 0x22: case 0x23: case 0x2B: return 6;  // other detectors
    case 0x0A: case 0x0B: case 0x18: case 0x1B: return 8;  // pressure, light, step, pickup
    case 0x11: case 0x29: case 0x2E: return 12;            // step counter, AR/VR game RV, wheel
    case 0x28: case 0x2A: return 14;                       // AR/VR RV, gyro-integrated RV
    case 0x07: case 0x0F: case 0x14: case 0x15:            // uncalibrated gyro and mag, raw
    case 0x16: case 0x1E: return 16;                       // sensors, activity classifier
    default: return 0;
    }
}

/** Keep one sensor report as the newest value for its ID. */
static void save_report(const uint8_t *report, uint64_t time_us) {
    uint8_t report_id = report[0];
    saved_report_t *saved = &newest[report_id];
    int field_count = (KEPT_REPORTS[report_id].length - REPORT_HEADER_BYTES) / 2;
    for (int field = 0; field < field_count; field++) {
        saved->raw[field] = read_int16(&report[REPORT_HEADER_BYTES + 2 * field]);
    }
    saved->status = report[2] & STATUS_ACCURACY_MASK;
    saved->time_us = time_us;
    saved->has_arrived = true;
    saved->is_fresh = true;
}

/**
 * Walk a channel 3 (or 4) payload: one or more reports back to back, usually
 * a base timestamp (0xFB) first. Returns the number of sensor reports kept.
 */
static int parse_input_reports(const uint8_t *data, size_t length, uint64_t time_us) {
    int reports_kept = 0;
    size_t cursor = 0;
    while (cursor < length) {
        size_t report_length = input_report_length(data[cursor]);
        if (report_length == 0 || cursor + report_length > length) {
            break;   // unknown or cut-off report: we can't find the next one, so drop the rest
        }
        if (is_kept_report(data[cursor])) {
            save_report(&data[cursor], time_us);
            reports_kept++;
        }
        cursor += report_length;
    }
    return reports_kept;
}

static int send_packet(uint8_t channel, const uint8_t *data, size_t length) {
    uint8_t packet[SHTP_HEADER_BYTES + SET_FEATURE_BYTES];
    if (length > SET_FEATURE_BYTES) {
        return PICO_ERROR_INVALID_ARG;
    }
    size_t packet_length = SHTP_HEADER_BYTES + length;
    packet[0] = packet_length & 0xFF;
    packet[1] = packet_length >> 8;
    packet[2] = channel;
    packet[3] = next_sequence[channel]++;
    memcpy(&packet[SHTP_HEADER_BYTES], data, length);
    int written = i2c_write_timeout_us(imu.i2c, imu.address, packet, packet_length, false,
                                       transfer_timeout_us(packet_length));
    if (written < 0) {
        return written;
    }
    return written == (int)packet_length ? PICO_OK : PICO_ERROR_GENERIC;
}

static int send_set_feature(uint8_t report_id, uint32_t period_us) {
    uint8_t command[SET_FEATURE_BYTES] = {0};   // flags, sensitivity, batch, config all 0
    command[0] = REPORT_SET_FEATURE;
    command[1] = report_id;
    write_uint32(&command[FEATURE_PERIOD_OFFSET], period_us);
    return send_packet(CHANNEL_CONTROL, command, sizeof command);
}

/** An unexpected reset turns every report off. Turn back on the ones we asked for. */
static void resend_enabled_reports(void) {
    for (int report_id = 0; report_id <= HIGHEST_REPORT_ID; report_id++) {
        if (requested_period_us[report_id] > 0) {
            send_set_feature(report_id, requested_period_us[report_id]);
        }
    }
}

static void handle_control_report(const uint8_t *data, size_t length) {
    if (length == 0) {
        return;
    }
    if (data[0] == REPORT_PRODUCT_ID_RESPONSE) {
        product_id_arrived = true;
    } else if (data[0] == REPORT_GET_FEATURE_RESPONSE && length >= SET_FEATURE_BYTES) {
        uint8_t report_id = data[1];
        uint32_t period_us = read_uint32(&data[FEATURE_PERIOD_OFFSET]);
        if (report_id <= HIGHEST_REPORT_ID) {
            granted_period_us[report_id] = period_us;
        }
        // The first 0xFC after a Set Feature can say period 0 (sensor not started yet),
        // and the real period follows in a second 0xFC (seen on a BNO085, 2026-10-19).
        if (report_id == awaited_feature_id && period_us > 0) {
            awaited_feature_arrived = true;
        }
    }
}

/** Act on one packet. Returns the number of sensor reports kept from it. */
static int handle_packet(uint8_t channel, const uint8_t *data, size_t length, uint64_t time_us) {
    switch (channel) {
    case CHANNEL_EXECUTABLE:
        if (length >= 1 && data[0] == EXECUTABLE_RESET) {
            reset_complete_arrived = true;
            if (is_initialized) {
                resend_enabled_reports();
            }
        }
        return 0;
    case CHANNEL_CONTROL:
        handle_control_report(data, length);
        return 0;
    case CHANNEL_REPORTS:
    case CHANNEL_WAKE_REPORTS:
        return parse_input_reports(data, length, time_us);
    default:
        return 0;   // channel 0 (the advertisement) and 5 (gyro-integrated RV): not used
    }
}

/**
 * Read one packet into payload[].
 * Returns 1 if a packet was read, 0 if nothing was waiting, or a negative
 * PICO_ERROR_ code if a transfer failed.
 */
static int read_packet(uint8_t *channel, size_t *payload_length) {
    uint8_t header[SHTP_HEADER_BYTES];
    int result = i2c_read_timeout_us(imu.i2c, imu.address, header, sizeof header, false,
                                     transfer_timeout_us(sizeof header));
    if (result < 0) {
        return result;
    }
    uint16_t packet_length = (header[0] | (header[1] << 8)) & ~SHTP_CONTINUATION_BIT;
    if (packet_length == SHTP_LENGTH_ALL_ONES) {
        return PICO_ERROR_IO;
    }
    if (packet_length <= SHTP_HEADER_BYTES) {
        return 0;
    }
    *channel = header[2];

    // Each read repeats the header, so ask for 4 more bytes than the payload we want.
    size_t bytes_left = packet_length - SHTP_HEADER_BYTES;
    size_t bytes_kept = 0;
    while (bytes_left > 0) {
        size_t chunk = smaller(bytes_left, MAX_PAYLOAD_BYTES);
        size_t transfer_length = SHTP_HEADER_BYTES + chunk;
        result = i2c_read_timeout_us(imu.i2c, imu.address, transfer_buffer, transfer_length, false,
                                     transfer_timeout_us(transfer_length));
        if (result < 0) {
            return result;
        }
        size_t keep = smaller(chunk, MAX_PAYLOAD_BYTES - bytes_kept);
        memcpy(&payload[bytes_kept], &transfer_buffer[SHTP_HEADER_BYTES], keep);
        bytes_kept += keep;
        bytes_left -= chunk;
    }
    *payload_length = bytes_kept;
    return 1;
}

/**
 * Read and act on one packet.
 *   reports_kept: incremented by the sensor reports kept (may be NULL)
 * Returns 1, 0, or a negative error, as read_packet() does.
 */
static int read_and_handle_packet(int *reports_kept) {
    uint8_t channel;
    size_t length;
    int result = read_packet(&channel, &length);
    if (result <= 0) {
        return result;
    }
    int kept = handle_packet(channel, payload, length, time_us_64());
    if (reports_kept != NULL) {
        *reports_kept += kept;
    }
    return 1;
}

/** Keep reading packets until *done becomes true. Returns false on timeout. */
static bool read_packets_until(const bool *done, uint32_t timeout_ms) {
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    while (!*done) {
        if (time_reached(deadline)) {
            return false;
        }
        // I2C errors are expected here while the IMU restarts, so just try again.
        int result = interrupt_says_nothing_waiting() ? 0 : read_and_handle_packet(NULL);
        if (result <= 0) {
            sleep_ms(1);
        }
    }
    return true;
}

static void set_up_optional_pins(void) {
    if (imu.int_pin != BNO08X_PIN_NOT_WIRED) {
        gpio_init(imu.int_pin);
        gpio_set_dir(imu.int_pin, GPIO_IN);
        gpio_pull_up(imu.int_pin);
    }
    if (imu.reset_pin != BNO08X_PIN_NOT_WIRED) {
        gpio_init(imu.reset_pin);
        gpio_put(imu.reset_pin, 1);   // set high before it becomes an output, so no reset glitch
        gpio_set_dir(imu.reset_pin, GPIO_OUT);
    }
}

/** Restart the IMU with RST if it's wired, otherwise with a reset command. */
static bool reset_imu(void) {
    if (imu.reset_pin != BNO08X_PIN_NOT_WIRED) {
        gpio_put(imu.reset_pin, 0);
        sleep_ms(RESET_PULSE_MS);
        gpio_put(imu.reset_pin, 1);
        return true;
    }
    const uint8_t reset_command = EXECUTABLE_RESET;
    for (int attempt = 0; attempt < SOFT_RESET_ATTEMPTS; attempt++) {
        if (send_packet(CHANNEL_EXECUTABLE, &reset_command, 1) == PICO_OK) {
            return true;
        }
        sleep_ms(SOFT_RESET_RETRY_MS);
    }
    return false;
}

static void forget_everything(void) {
    is_initialized = false;
    memset(next_sequence, 0, sizeof next_sequence);
    memset(newest, 0, sizeof newest);
    memset(requested_period_us, 0, sizeof requested_period_us);
    memset(granted_period_us, 0, sizeof granted_period_us);
    reset_complete_arrived = false;
    product_id_arrived = false;
    awaited_feature_id = -1;
}

bool bno08x_init(const bno08x_config_t *config) {
    imu = *config;
    forget_everything();
    set_up_optional_pins();
    if (!reset_imu()) {
        return false;
    }
    // Drains the advertisement and startup messages. Not fatal if "reset complete"
    // is missed: the Product ID answer below is the real test.
    read_packets_until(&reset_complete_arrived, RESET_TIMEOUT_MS);

    // Retried, because a "reset complete" left over from power-up can arrive before
    // ours, and a request sent while the IMU is still restarting is lost.
    const uint8_t request[] = {REPORT_PRODUCT_ID_REQUEST, 0};
    for (int attempt = 0; attempt < PRODUCT_ID_ATTEMPTS && !product_id_arrived; attempt++) {
        if (send_packet(CHANNEL_CONTROL, request, sizeof request) == PICO_OK) {
            read_packets_until(&product_id_arrived, PRODUCT_ID_TIMEOUT_MS);
        } else {
            sleep_ms(PRODUCT_ID_TIMEOUT_MS);
        }
    }
    is_initialized = product_id_arrived;
    return is_initialized;
}

bool bno08x_enable_report(uint8_t report_id, uint32_t period_us) {
    if (!is_initialized || !is_kept_report(report_id)) {
        return false;
    }
    requested_period_us[report_id] = period_us;
    granted_period_us[report_id] = 0;
    if (send_set_feature(report_id, period_us) != PICO_OK) {
        return false;
    }
    // Ask what was granted: the answer is our confirmation (SH-2 Reference Manual 6.5.3, 6.5.5).
    awaited_feature_id = report_id;
    awaited_feature_arrived = false;
    const uint8_t request[] = {REPORT_GET_FEATURE_REQUEST, report_id};
    bool answered = send_packet(CHANNEL_CONTROL, request, sizeof request) == PICO_OK &&
                    read_packets_until(&awaited_feature_arrived,
                                       FEATURE_RESPONSE_BASE_MS + FEATURE_RESPONSE_PERIODS * period_us / 1000);
    awaited_feature_id = -1;
    return answered && granted_period_us[report_id] > 0;
}

uint32_t bno08x_report_period_us(uint8_t report_id) {
    return report_id <= HIGHEST_REPORT_ID ? granted_period_us[report_id] : 0;
}

int bno08x_service(void) {
    if (!is_initialized) {
        return PICO_ERROR_NOT_PERMITTED;
    }
    int reports_kept = 0;
    for (int packet = 0; packet < MAX_PACKETS_PER_SERVICE; packet++) {
        if (interrupt_says_nothing_waiting()) {
            break;
        }
        int result = read_and_handle_packet(&reports_kept);
        if (result < 0) {
            return result;
        }
        if (result == 0) {
            break;
        }
    }
    return reports_kept;
}

/** Copy out the newest report's status and time; clear and return its "fresh" flag. */
static bool take_report(saved_report_t *saved, uint8_t *status, uint64_t *time_us) {
    *status = saved->status;
    *time_us = saved->time_us;
    bool was_fresh = saved->is_fresh;
    saved->is_fresh = false;
    return was_fresh;
}

bool bno08x_get_quaternion(uint8_t report_id, bno08x_quaternion_t *quaternion) {
    if (!is_quaternion_report(report_id) || !newest[report_id].has_arrived) {
        return false;
    }
    const int16_t *raw = newest[report_id].raw;
    const float scale = 1.0f / (1 << QUATERNION_Q_POINT);
    quaternion->i = raw[0] * scale;
    quaternion->j = raw[1] * scale;
    quaternion->k = raw[2] * scale;
    quaternion->real = raw[3] * scale;
    quaternion->accuracy_rad = report_id == BNO08X_REPORT_GAME_ROTATION_VECTOR
                                   ? NAN
                                   : raw[4] * (1.0f / (1 << HEADING_ACCURACY_Q_POINT));
    return take_report(&newest[report_id], &quaternion->status, &quaternion->time_us);
}

bool bno08x_get_vector(uint8_t report_id, bno08x_vector_t *vector) {
    if (!is_kept_report(report_id) || is_quaternion_report(report_id) || !newest[report_id].has_arrived) {
        return false;
    }
    const int16_t *raw = newest[report_id].raw;
    const float scale = 1.0f / (1 << KEPT_REPORTS[report_id].q_point);
    vector->x = raw[0] * scale;
    vector->y = raw[1] * scale;
    vector->z = raw[2] * scale;
    return take_report(&newest[report_id], &vector->status, &vector->time_us);
}
