#include <stdio.h>
#include "pico/stdlib.h"

#include "hardware/gpio.h"


#define BLUE_LED_PIN 16
#define YELLOW_BUTTON 17
// #define LED_DELAY_MS 250
#define LED_DELAY_MS 10


void gpio_callback(uint gpio, uint32_t events)
{
    bool level = gpio_get(gpio);

    if (level)
    {
        printf("GPIO %d is HIGH\n", gpio);
        gpio_put(BLUE_LED_PIN, true);
    }
    else
    {
        printf("GPIO %d is LOW\n", gpio);
        gpio_put(BLUE_LED_PIN, false);
    }
    // You can also determine what caused the interrupt
    // if (events & GPIO_IRQ_EDGE_RISE)
    //     printf("Rising edge\n");

    // if (events & GPIO_IRQ_EDGE_FALL)
    //     printf("Falling edge\n");
}

// Initialize the GPIO for the LED
void pico_led_init(void) {

    // A device like Pico that uses a GPIO for the LED will define PICO_DEFAULT_LED_PIN
    // so we can use normal GPIO functionality to turn the led on and off
    gpio_init(BLUE_LED_PIN);
    gpio_set_dir(BLUE_LED_PIN, GPIO_OUT);

    gpio_init(YELLOW_BUTTON);
    gpio_set_dir(YELLOW_BUTTON, GPIO_IN);
    gpio_set_irq_enabled_with_callback(
        YELLOW_BUTTON,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        true,
    &gpio_callback);

}


int main()
{
    stdio_init_all();
    pico_led_init();
    gpio_put(BLUE_LED_PIN, false);
    while (true) {
        // gpio_put(BLUE_LED_PIN, true);
        sleep_ms(LED_DELAY_MS);
        // gpio_put(BLUE_LED_PIN, false);
        // sleep_ms(LED_DELAY_MS);
    }
}
