/*
 * main.c
 * Lab 8 test program: drive the servo and the DC motor by typing commands in
 * the Serial Monitor, and read the encoder.
 *
 *   p 1500   servo pulse width in us (clamped to MIN_PULSE_US..MAX_PULSE_US)
 *   a 45     servo angle in degrees, -90 to +90
 *   d 30     motor duty in percent, -100 to 100; prints counts and RPM after it settles
 *   c        print the encoder count (for turning the shaft by hand)
 *
 * Modules:
 *   servo.c/.h  servo on GP6
 *   motor.c/.h  H-bridge on GP8/GP9, encoder on GP10/GP11
 *
 * Author:  OCE360/560 instructors
 * License: MIT (see LICENSE)
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "servo.h"
#include "motor.h"

#define SETTLE_MS 1000                  // let the motor reach its new speed before measuring
#define RPM_WINDOW_MS 500
#define PERCENT 100.0f

static void print_help(void) {
    printf("Commands: p <pulse_us>, a <angle_deg>, d <duty_pct>, c (counts)\r\n");
}

// After a typing mistake, throw away the rest of the line so the next command starts clean.
static void skip_rest_of_line(void) {
    int c;
    do {
        c = getchar();
    } while (c != '\n' && c != '\r' && c != EOF);
}

// Returns false for a command it doesn't know.
static bool run_command(char command, float value) {
    switch (command) {
    case 'p': {
        uint16_t pulse_us = value < 0.0f ? 0 : value > UINT16_MAX ? UINT16_MAX : (uint16_t)value;
        printf("pulse %u us\r\n", servo_set_pulse_us(pulse_us));
        return true;
    }
    case 'a':
        printf("angle %.1f deg, pulse %u us\r\n", value, servo_set_angle(value));
        return true;
    case 'd': {
        motor_set_duty(value / PERCENT);
        sleep_ms(SETTLE_MS);
        float rpm = motor_measure_rpm(RPM_WINDOW_MS);
        printf("duty %.0f%%  counts %ld  rpm %.1f\r\n", value, (long)motor_read_counts(), rpm);
        return true;
    }
    case 'c':
        printf("counts %ld\r\n", (long)motor_read_counts());
        return true;
    default:
        return false;
    }
}

int main() {
    stdio_init_all();
    servo_init();                       // centered: 1500 us
    motor_init();                       // stopped
    print_help();

    while (true) {
        char command;
        float value = 0.0f;
        if (scanf(" %c", &command) != 1) continue;

        bool needs_value = command == 'p' || command == 'a' || command == 'd';
        if (needs_value && scanf("%f", &value) != 1) {
            printf("? %c needs a number\r\n", command);
            skip_rest_of_line();
            continue;
        }
        if (!run_command(command, value)) {
            print_help();
            skip_rest_of_line();
        }
    }
}
