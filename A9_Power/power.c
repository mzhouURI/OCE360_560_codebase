/*
 * power.c
 * Battery voltage from a resistor divider on the ADC, and current and bus
 * voltage from an INA219 (Adafruit 904 breakout) on I2C (Lab 9).
 *
 * Wiring:
 *   Divider: battery + -> 33k -> GP28 (ADC2) -> 10k -> GND
 *            (check the middle point reads under 3.3 V BEFORE it touches GP28)
 *   INA219:  VCC -> 3V3(OUT), GND -> GND, SDA -> GP2, SCL -> GP3
 *            (i2c1, shared with the BNO080 at 0x4A; address 0x40 with A0, A1 open)
 *            battery + -> Vin+, Vin- -> everything the battery powers (the 7805 input)
 *
 * The INA219's calibration and current registers are not used: this file reads
 * the shunt voltage and applies Ohm's law itself (TI INA219 datasheet, SBOS448G).
 *
 * Author:  OCE360/560 instructors
 * License: MIT (see LICENSE)
 */
#include "hardware/adc.h"
#include "power.h"

#define DIVIDER_PIN 28
#define DIVIDER_ADC_INPUT 2
#define DIVIDER_TOP_OHMS 33000.0f
#define DIVIDER_BOTTOM_OHMS 10000.0f
#define ADC_REFERENCE_VOLTS 3.3f
#define ADC_FULL_SCALE_COUNTS 4096.0f

#define INA219_ADDRESS 0x40
#define INA219_REG_CONFIG 0x00
#define INA219_REG_SHUNT_VOLTAGE 0x01
#define INA219_REG_BUS_VOLTAGE 0x02

// Configuration register fields (datasheet section 8.6.2.1).
#define INA219_BUS_RANGE_32V (1u << 13)
#define INA219_SHUNT_RANGE_320MV (3u << 11)         // gain /8: +-320 mV, +-3.2 A on 0.1 ohm
#define INA219_BUS_AVERAGE_128 (0xFu << 7)          // 128 samples, 68.10 ms (Table 5)
#define INA219_SHUNT_AVERAGE_128 (0xFu << 3)
#define INA219_MODE_CONTINUOUS 0x7u                 // shunt and bus, continuous
// = 0x3FFF. Averaging smooths the PWM current pulses of a motor.
#define INA219_CONFIG (INA219_BUS_RANGE_32V | INA219_SHUNT_RANGE_320MV | INA219_BUS_AVERAGE_128 | \
                       INA219_SHUNT_AVERAGE_128 | INA219_MODE_CONTINUOUS)

#define INA219_SHUNT_VOLTS_PER_COUNT 10e-6f
#define INA219_BUS_VOLTS_PER_COUNT 0.004f
#define INA219_BUS_VOLTAGE_SHIFT 3                  // bus voltage sits in bits 15 to 3
#define SHUNT_OHMS 0.1f                             // on the Adafruit 904 board
#define MA_PER_AMP 1000.0f

// Long enough for the BNO080 to finish stretching the clock, short enough not to hang the logger.
#define I2C_TIMEOUT_US 10000

static i2c_inst_t *ina219_bus;

/** Write one 16-bit INA219 register, most significant byte first. */
static bool ina219_write(uint8_t reg, uint16_t value) {
    uint8_t bytes[3] = {reg, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF)};
    return i2c_write_timeout_us(ina219_bus, INA219_ADDRESS, bytes, 3, false, I2C_TIMEOUT_US) == 3;
}

/** Read one 16-bit INA219 register: send the register number, then read two bytes. */
static bool ina219_read(uint8_t reg, uint16_t *value) {
    uint8_t bytes[2];
    if (i2c_write_timeout_us(ina219_bus, INA219_ADDRESS, &reg, 1, true, I2C_TIMEOUT_US) != 1) {
        return false;
    }
    if (i2c_read_timeout_us(ina219_bus, INA219_ADDRESS, bytes, 2, false, I2C_TIMEOUT_US) != 2) {
        return false;
    }
    *value = (uint16_t)((bytes[0] << 8) | bytes[1]);
    return true;
}

bool power_init(i2c_inst_t *i2c) {
    ina219_bus = i2c;
    adc_init();   // resets the ADC; harmless after sensors_init() because every read selects its input
    adc_gpio_init(DIVIDER_PIN);
    return ina219_write(INA219_REG_CONFIG, INA219_CONFIG);
}

float power_read_divider_volts(void) {
    adc_select_input(DIVIDER_ADC_INPUT);
    float pin_volts = adc_read() * (ADC_REFERENCE_VOLTS / ADC_FULL_SCALE_COUNTS);
    return pin_volts * (DIVIDER_TOP_OHMS + DIVIDER_BOTTOM_OHMS) / DIVIDER_BOTTOM_OHMS;
}

bool power_read_ina219(float *bus_volts, float *current_ma) {
    uint16_t shunt_raw, bus_raw;
    if (!ina219_read(INA219_REG_SHUNT_VOLTAGE, &shunt_raw) ||
        !ina219_read(INA219_REG_BUS_VOLTAGE, &bus_raw)) {
        return false;
    }
    // The shunt register is signed (two's complement): current can flow backwards.
    float shunt_volts = (int16_t)shunt_raw * INA219_SHUNT_VOLTS_PER_COUNT;
    *current_ma = shunt_volts / SHUNT_OHMS * MA_PER_AMP;
    *bus_volts = (bus_raw >> INA219_BUS_VOLTAGE_SHIFT) * INA219_BUS_VOLTS_PER_COUNT;
    return true;
}
