

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"

#define PHOTO_CELL_PIN 26
#define PHOTO_CELL_ADC 0

#define TEMP_CELL_PIN 27
#define TEMP_CELL_ADC 1


int main() {
    const float conversion_factor = 3.3f / (1 << 12);


    stdio_init_all();
    printf("ADC Example, measuring GPIO26\n");

    adc_init();

    // Make sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(PHOTO_CELL_PIN);
    adc_gpio_init(TEMP_CELL_PIN);

    

    while (1) {
        adc_select_input(PHOTO_CELL_ADC);
        // 12-bit conversion, assume max value == ADC_VREF == 3.3 V
        uint16_t result = adc_read();
        printf("Photo value: 0x%03x, voltage: %f V\n", result, result * conversion_factor);

        adc_select_input(TEMP_CELL_ADC);
        // 12-bit conversion, assume max value == ADC_VREF == 3.3 V
        result = adc_read();
        printf("Temp value: 0x%03x, voltage: %f V\n", result, result * conversion_factor);
        sleep_ms(500);
    }
}