# BNO080 IMU example (OCE360 Lab 6)

The smallest working use of the `bno08x` driver: sets up I2C1, starts the
BNO080 on the Adafruit BNO085 breakout, turns on its rotation vector at 50 Hz,
and prints each quaternion over USB.

```
# rotation vector every 20000 us
real, i, j, k, status
0.9998, 0.0021, -0.0113, 0.0154, 3
```

(Example format; the numbers are illustrative.)

**For Lab 6, you only need `lib/bno08x/`.** Copy that folder into your own
project's `lib/` and commit it, the same way as `lib/pico_fatfs` in Lab 4.

## Hardware

| Breakout (Adafruit 4754) | Pico 2 pin |
|---|---|
| VIN | 3V3(OUT) |
| GND | GND |
| SDA | GP2 (I2C1 SDA) |
| SCL | GP3 (I2C1 SCL) |
| INT (optional) | GP12 |
| RST (optional) | GP13 |

Leave P0, P1, DI, and BT unconnected: their defaults select I2C at address 0x4A.
If you don't wire INT or RST, set that pin to `BNO08X_PIN_NOT_WIRED` in `main.c`.

## Build and run

1. Open the folder in VS Code with the Raspberry Pi Pico extension and click **Compile**.
2. Hold BOOTSEL, plug in the Pico, and copy `build/A6_IMU.uf2` to it.
3. Open the Serial Monitor within 2 s. Move the board in a figure-8 for a few
   seconds and watch `status` climb to 3.

## Files

| File | What it does |
|---|---|
| `main.c` | I2C setup, IMU start, print loop |
| `lib/bno08x/` | the BNO080/BNO085 driver: see its [README](lib/bno08x/README.md) |

## Third-party code

None. `lib/bno08x` was written for this course from the BNO080 datasheet and
the SH-2 Reference Manual; its README lists the sources.

## License

MIT, see [LICENSE](LICENSE).

## Known issues

- Not yet run on hardware (2026-10-10).
- The I2C bus runs at 100 kHz on purpose: the BNO080 stretches the clock.
