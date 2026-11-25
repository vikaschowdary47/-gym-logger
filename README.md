# Gym Logger

A small wearable that logs gym sets (exercise, weight, reps) from a round touch screen on your arm or wrist, and sends them to your phone.

Built on the Waveshare ESP32-S3-Touch-LCD-1.28.

## Structure

| Folder | What it is | Status |
| --- | --- | --- |
| `firmware/` | Arduino sketches for the board | Mic screen and tap-to-log screen work |
| `web/` | Phone web page that receives and shows logged sets | Not started |
| `app/` | Expo (React Native) app, if the web page isn't enough | Not started |
| `docs/` | Notes, data format, research | In progress |

## Current state

- Tap-to-log screen: pick exercise, weight and reps, tap LOG SET. Each set prints one line to the Serial Monitor.
- Mic screen: tap the mic for a listening animation. It is UI only; the board has no microphone.
- Rep counting from the motion sensor: parked. It did not count reps yet.
- Not built: Bluetooth link to the phone, saving sets across resets, real voice.

## Run the firmware

1. Install the Arduino IDE and the ESP32 board package.
2. Library Manager: install **GFX Library for Arduino** (moononournation).
3. Board: `ESP32S3 Dev Module`, USB CDC On Boot: Enabled.
4. Open `firmware/gym_logger/gym_logger.ino` and upload.

Pins and board details are in `firmware/README.md`.

## Roadmap

1. Bluetooth Low Energy link from the board to the phone (see `docs/protocol.md`).
2. Web page to show sets on the phone.
3. Save sets on the board so a reset or lost connection doesn't lose them.
4. Dumbbell exercise list for an apartment gym (weights up to 50 lb).
5. Voice logging using the phone's microphone.
6. Rep counting from the motion sensor.
