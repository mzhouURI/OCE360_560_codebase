// Lab 3 (A3_ADC): photocell and LM19 temperature sensor
// on the ADC. Prints "time_s, lux, temp_C\r\n" once per second, in a format the
// VS Code serial plotter reads (a header line naming the series, then numbers only).
//
// Wiring (Lab-3_ADC deck):
//   Photocell: 3V3 OUT -> photocell (R1) -> GP26 -> 7.5k (R2) -> GND
//   LM19 (flat face toward you, leads down: +V, Vout, GND):
//     +V -> 3V3 OUT, Vout -> GP27, GND -> GND
//   Stretch LED: GP16 -> 100 ohm -> LED -> GND
#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

#define PHOTO_CELL_PIN 26
#define PHOTO_CELL_ADC 0
#define TEMP_PIN 27
#define TEMP_ADC 1
#define LED_PIN 16

#define VCC 3.3f
#define R2_OHMS 7500.0f     // fixed resistor in the photocell divider

// Photocell (Adafruit 161 = Luna PDV-P8001). The datasheet's "sensitivity 0.6" is
// the slope of log10(R) vs log10(lux), so log10(R) = -0.6*log10(lux) + const.
// Solved for lux: log10(lux) = A*log10(R) + B with A = -1/0.6.
// R10 is the resistance at 10 lux; the datasheet only gives 3k to 11k, so we use
// the geometric middle. That spread alone is roughly a factor of 3 in lux.
#define PHOTO_GAMMA 0.6f
#define PHOTO_R10_OHMS 5700.0f

// Stretch thresholds for switching the LED
#define LUX_DARK 20.0f
#define TEMP_HOT_C 30.0f

// 1 = also print raw counts, volts, and ohms for checking against a multimeter.
// Leave at 0 for the plotter: it only understands comma-separated numbers.
#define DEBUG_PRINT 0

// Photocell voltage -> photocell resistance (Ohm's law on the divider)
float photocell_ohms(float v1) {
    if (v1 < 0.001f) v1 = 0.001f;   // avoid divide by zero in the dark
    return VCC * R2_OHMS / v1 - R2_OHMS;
}

// Photocell resistance -> lux
float photocell_lux(float r_ohms) {
    const float A = -1.0f / PHOTO_GAMMA;
    const float B = log10f(PHOTO_R10_OHMS) / PHOTO_GAMMA + 1.0f;
    return powf(10.0f, A * log10f(r_ohms) + B);
}

// LM19 output voltage -> temperature, datasheet parabolic transfer function (eq. 3)
float lm19_celsius(float vo) {
    return -1481.96f + sqrtf(2.1962e6f + (1.8639f - vo) / 3.88e-6f);
}

int main() {
    const float conversion_factor = VCC / (1 << 12);   // 12-bit ADC, volts per count

    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    adc_init();
    adc_gpio_init(PHOTO_CELL_PIN);   // high impedance, no pulls, digital input off
    adc_gpio_init(TEMP_PIN);

    sleep_ms(2000);                     // give USB time to connect so the header isn't lost
    printf("time_s, lux, temp_C\r\n");  // names the series for the plotter

    while (true) {
        float t_s = to_ms_since_boot(get_absolute_time()) / 1000.0f;

        adc_select_input(PHOTO_CELL_ADC);
        uint16_t photo_raw = adc_read();
        float photo_v = photo_raw * conversion_factor;
        float photo_r = photocell_ohms(photo_v);
        float lux = photocell_lux(photo_r);

        adc_select_input(TEMP_ADC);
        uint16_t temp_raw = adc_read();
        float temp_v = temp_raw * conversion_factor;
        float temp_c = lm19_celsius(temp_v);

        // Required output line
        printf("%.3f, %.1f, %.1f\r\n", t_s, lux, temp_c);
#if DEBUG_PRINT
        // Debug line for checking the math against a multimeter
        printf("# photo 0x%03X %.3f V %.0f ohm | temp 0x%03X %.3f V\r\n",
               photo_raw, photo_v, photo_r, temp_raw, temp_v);
#endif

        // Stretch: LED on when it's dark or hot
        gpio_put(LED_PIN, lux < LUX_DARK || temp_c > TEMP_HOT_C);

        sleep_ms(1000);
    }
}
