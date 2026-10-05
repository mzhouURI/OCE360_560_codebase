# SD card logger (OCE360 Lab 5)

Reads a photocell and an LM19 temperature sensor on a Pico 2 once a second,
prints each reading over USB, and appends it to `log.csv` on a microSD card.

```
time_s, lux, temp_C
2.001, 312.4, 24.6
3.001, 305.9, 24.7
```

## Hardware

| Part | Pico 2 pin |
|---|---|
| Photocell divider (photocell to 3V3, 7.5 kΩ to GND) | GP26 (ADC0) |
| LM19 Vout | GP27 (ADC1) |
| SD breakout CLK, DI, DO, CS | GP18, GP19, GP20, GP21 (SPI0) |
| SD breakout 3V, GND | 3V3(OUT), GND |
| Status LED + 100 Ω | GP16 |

## Build and run

1. Open the folder in VS Code with the Raspberry Pi Pico extension and click **Compile**.
2. Hold BOOTSEL, plug in the Pico, and copy `build/A4_SD_Logging.uf2` to it.
3. Open the Serial Monitor within 2 s to see any SD card errors.

## Files

| File | What it does |
|---|---|
| `A4_SD_Logging.c` | main loop: timing, formatting, status LED |
| `sensors.c/.h` | photocell and LM19 readings in lux and °C |
| `sd_logger.c/.h` | mount the card, append a line, sync it |

## Third-party code

`lib/pico_fatfs` is [elehobica/pico_fatfs](https://github.com/elehobica/pico_fatfs), tag
`fatfs-R0.15-1.0.3` (ChaN's FatFs R0.15 plus an SD card SPI driver), copied in unchanged.
Its own license files are in that folder.

## License

MIT, see [LICENSE](LICENSE). The library in `lib/` keeps its own license.

## Known issues

- Lux is only good to about a factor of 3: the photocell datasheet gives a wide resistance range.
- `time_s` is time since boot, not time of day. Write down when you started.
