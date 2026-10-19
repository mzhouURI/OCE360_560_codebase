# Battery voltage and current (OCE360 Lab 9)

One module for the Pico 2, and a test program that prints what it measures:

- `power`: the battery voltage from a 33 kΩ / 10 kΩ divider on GP28 (ADC2), and the
  current and bus voltage from an INA219 current sensor (Adafruit 904) on I2C1.

In Lab 9 you copy `power.c` and `power.h` into your own project, and use `main.c` as a
first test. The program lists the I2C devices it finds, then prints three columns every
500 ms:

```
# I2C scan: 0x40 0x4A
divider_V, bus_V, current_mA
<battery V>, <INA219 Vin- V>, <mA>
```

## Hardware

| Part | Pico 2 pin |
|---|---|
| Divider middle point (battery + → 33 kΩ → **here** → 10 kΩ → GND) | GP28 (ADC2, pin 34) |
| INA219 VCC, GND | 3V3(OUT), GND |
| INA219 SDA, SCL | GP2, GP3 (I2C1, shared with the BNO080 IMU at 0x4A) |
| INA219 Vin+ | battery + |
| INA219 Vin− | the load: the 7805 input |
| Battery − | GND, shared with the Pico |

**Before the divider touches GP28**, measure its middle point with a multimeter: it must
read under 3.3 V. With 33 kΩ on top and 10 kΩ on the bottom, 14.2 V of battery gives 3.3 V.
Swapped resistors put about 9 V on the pin and destroy it.

The battery never connects to VSYS, 3V3, VBUS or a GP pin directly. In Lab 9 the Pico runs
on USB until the untethered exercise.

## Build and run

1. Open the folder in VS Code with the Raspberry Pi Pico extension and click **Compile**.
2. Hold BOOTSEL, plug in the Pico, and copy `build/A9_Power.uf2` to it.
3. Open the Serial Monitor within 2 s to see the I2C scan.

To add the module to your own project: copy `power.c` and `power.h`, add `power.c` to
`add_executable`, and `hardware_i2c hardware_adc` to `target_link_libraries`. Set up the
I2C bus in `main` before `power_init(i2c1)`, as this `main.c` does.

## Files

| File | What it does |
|---|---|
| `main.c` | test program: I2C scan, then divider and INA219 readings every 500 ms |
| `power.c/.h` | `power_init`, `power_read_divider_volts`, `power_read_ina219` |

## License

MIT, see [LICENSE](LICENSE).

## Known issues

- Not yet run on hardware.
- The INA219 averages 128 samples (68 ms) for each reading, so a reading describes the
  last 68 ms, not one instant. Short spikes (an SD card write) are averaged in.
- The divider draws about 0.28 mA from the battery all the time, even with the Pico off.
- Two INA219 boards at address 0x40 on one bus corrupt each other's reads: only one at a time.
