# Firmware

Arduino sketches for the Waveshare ESP32-S3-Touch-LCD-1.28. Each sketch lives in a folder with the same name as its `.ino` file, which the Arduino IDE requires.

| Sketch | What it does |
| --- | --- |
| `gym_logger/` | Main sketch: mic screen plus tap-to-log screen. Use this one. |
| `mic_screen/` | Earlier step: just the mic screen and animation. |
| `rep_counter/` | Reads the QMI8658 motion sensor and tries to count reps. Prints to Serial Monitor. Not working yet. |

## Setup

- Board: `ESP32S3 Dev Module`, USB CDC On Boot: Enabled, Serial Monitor at 115200.
- Library: **GFX Library for Arduino** (moononournation).

## Pins (from the Waveshare wiki)

| Part | Pins |
| --- | --- |
| LCD | DC 8, CS 9, SCLK 10, MOSI 11, MISO 12, RST 14, backlight 2 |
| Touch (CST816S) | SDA 6, SCL 7, RST 13 |
| Motion sensor (QMI8658) | SDA 6, SCL 7 (same bus as touch) |

GPIO 4 and 5 are also wired to MOSFET control lines on this board, so avoid using them for other things.

## Known gaps

- The board has no microphone.
- Logged sets are not saved when the board resets.
- Rep counting counted 0 on first tests.
