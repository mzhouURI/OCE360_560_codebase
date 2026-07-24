#include <stdio.h>
#include "pico/stdlib.h"

//define global variables
#define BLUE_LED_PIN 16
#define LED_DELAY_MS 250

// Initialize the GPIO for the LED
void pico_led_init(void) {
    // Initialize Pin
    gpio_init(BLUE_LED_PIN);

    //set pin function
    gpio_set_dir(BLUE_LED_PIN, GPIO_OUT);
}

int main()
{
    stdio_init_all();
    pico_led_init();
    
    while (true) {
        gpio_put(BLUE_LED_PIN, true);
        sleep_ms(LED_DELAY_MS);
        gpio_put(BLUE_LED_PIN, false);
        sleep_ms(LED_DELAY_MS);
    }
}
