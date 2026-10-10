# Servo and DC motor modules (OCE360 Lab 8)

Two modules for the Pico 2, and a test program that drives them from the Serial Monitor:

- `servo`: a hobby servo (Adafruit 169) on 50 Hz PWM, clamped to the endpoints you measure.
- `motor`: the geared 12 V DC motor through a two-input H-bridge (20 kHz PWM), and its
  Hall quadrature encoder counted with GPIO interrupts (2,160 counts per output-shaft turn).

In Lab 8 you copy `servo.c/.h` and `motor.c/.h` into your own `A8_Motor` project, and use
`main.c` as your first `A8_Motor.c`.

```
Commands: p <pulse_us>, a <angle_deg>, d <duty_pct>, c (counts)
d 50
duty 50%  counts <encoder count>  rpm <output-shaft speed>
```

## Hardware

| Part | Pico 2 pin |
|---|---|
| Servo signal (orange) | GP6 (PWM 3A) |
| Servo red (+5 V), brown (GND) | 5 V rail from the kit 7805 or a bench supply, **not 3V3**; GND shared with the Pico |
| H-bridge IN1, IN2 | GP8 (PWM 4A), GP9 (PWM 4B) |
| Encoder A (yellow), B (white) | GP10, GP11 |
| Encoder Vcc (blue), GND (green) | 3V3(OUT), GND (see Known issues) |
| H-bridge GND, motor supply minus | GND, shared with the Pico |

The motor runs from its own supply through the H-bridge, never from the Pico.

## Build and run

1. Open the folder in VS Code with the Raspberry Pi Pico extension and click **Compile**.
2. Hold BOOTSEL, plug in the Pico, and copy `build/A8_Motor.uf2` to it.
3. Open the Serial Monitor and type a command, for example `p 1500` or `d 30`.

## Files

| File | What it does |
|---|---|
| `main.c` | test program: servo and motor commands from the Serial Monitor |
| `servo.c/.h` | `servo_init`, `servo_set_pulse_us`, `servo_set_angle` |
| `motor.c/.h` | `motor_init`, `motor_set_duty`, `motor_read_counts`, `motor_counts_to_rpm`, `motor_measure_rpm` |

## License

MIT, see [LICENSE](LICENSE).

## Known issues

- Not yet run on hardware. The servo endpoints in `servo.c` (1000 and 2000 us) are a safe
  default: measure your own servo and change `MIN_PULSE_US` and `MAX_PULSE_US`.
- The motor's seller lists the encoder supply as 5 V. Check that it works at 3.3 V before
  you connect it. At 5 V its outputs swing to 5 V, which only the RP2350 A4 stepping is
  rated to accept.
- Which direction counts up depends on the wiring: if positive duty gives negative counts,
  swap the encoder A and B wires.
