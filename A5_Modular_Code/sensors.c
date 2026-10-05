/*
 * sensors.c
 * Reads the photocell and the LM19 on the Pico 2's 12-bit ADC and converts
 * each reading to physical units (Lab 3).
 *
 * Wiring (Lab-3_ADC deck):
 *   Photocell: 3V3(OUT) -> photocell -> GP26 -> 7.5k -> GND
 *   LM19 (flat face toward you, leads down: +V, Vout, GND):
 *     +V -> 3V3(OUT), Vout -> GP27, GND -> GND
 *
 * Author:  OCE360/560 instructors
 * License: MIT (see LICENSE)
 */
#include <math.h>
#include "hardware/adc.h"
#include "sensors.h"

#define PHOTOCELL_PIN 26
#define PHOTOCELL_ADC_INPUT 0
#define LM19_PIN 27
#define LM19_ADC_INPUT 1

#define SUPPLY_VOLTS 3.3f
#define ADC_FULL_SCALE_COUNTS 4096.0f
#define DIVIDER_OHMS 7500.0f

// Photocell (Adafruit 161 = Luna PDV-P8001). Its datasheet "sensitivity" of 0.6 is the
// slope of log10(R) against log10(lux). The resistance at 10 lux is only given as 3k to 11k;
// we use the geometric middle, which alone leaves lux uncertain by about a factor of 3.
#define PHOTOCELL_SLOPE 0.6f
#define PHOTOCELL_OHMS_AT_10_LUX 5700.0f

static float read_volts(unsigned int adc_input) {
    adc_select_input(adc_input);
    return adc_read() * (SUPPLY_VOLTS / ADC_FULL_SCALE_COUNTS);
}

/** Photocell resistance from the divider voltage (Ohm's law). */
static float photocell_ohms(float divider_volts) {
    if (divider_volts < 0.001f) divider_volts = 0.001f;   // fully dark: avoid dividing by zero
    return SUPPLY_VOLTS * DIVIDER_OHMS / divider_volts - DIVIDER_OHMS;
}

/** Illuminance from photocell resistance: log10(lux) = slope * log10(R) + offset. */
static float photocell_lux(float resistance_ohms) {
    const float slope = -1.0f / PHOTOCELL_SLOPE;
    const float offset = log10f(PHOTOCELL_OHMS_AT_10_LUX) / PHOTOCELL_SLOPE + 1.0f;
    return powf(10.0f, slope * log10f(resistance_ohms) + offset);
}

/** LM19 temperature from its output voltage: the datasheet's parabolic equation (eq. 3). */
static float lm19_celsius(float output_volts) {
    return -1481.96f + sqrtf(2.1962e6f + (1.8639f - output_volts) / 3.88e-6f);
}

void sensors_init(void) {
    adc_init();
    adc_gpio_init(PHOTOCELL_PIN);
    adc_gpio_init(LM19_PIN);
}

float sensors_read_lux(void) {
    return photocell_lux(photocell_ohms(read_volts(PHOTOCELL_ADC_INPUT)));
}

float sensors_read_celsius(void) {
    return lm19_celsius(read_volts(LM19_ADC_INPUT));
}
