# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

ESP32 firmware (Arduino framework, PlatformIO) that reads a smart electricity meter's P1 port (DSMR/IEC 62056-21 style telegrams with OBIS codes) and republishes each line over MQTT, plus a small web server for status and WiFi provisioning. Code comments and user-facing strings are mostly in Hungarian.

## Commands

PlatformIO single environment: `esp32doit-devkit-v1`. The platform is pinned to `espressif32@7.0.1` (Arduino-ESP32 2.0.17); Arduino-ESP32 3.x changes the `esp_task_wdt_init` API, so don't bump it without porting the watchdog setup.

- Build: `pio run`
- Upload firmware: `pio run -t upload` — **uploads over OTA by default** (`upload_protocol = espota`, `upload_port = esppowermeter`). For a USB/serial flash, comment out those two lines in `platformio.ini`.
- Upload SPIFFS contents of `data/`: `pio run -t uploadfs`
- Serial monitor: `pio device monitor` (115200 baud)

There are no unit tests or linters configured (`test/` only holds the PlatformIO template README).

## Architecture

Everything lives in `src/main.cpp`; `src/main.h` holds configuration constants, global state, and the OBIS-code → Hungarian description table (`txtP1Rows`, a JSON string parsed with cJSON on demand). `main.h` *defines* globals (not just declarations), so it must only be included from one translation unit.

`loop()` is a cooperative, non-blocking scheduler: web server, serial P1 reading, MQTT loop, OTA, WiFi health check, and `timer()` (LED blink + watchdog feed + AP-mode timeout). A task watchdog (12 s, panic on timeout) is enabled in `setup()`, so any new blocking code must call `esp_task_wdt_reset()` (as `connectToRouter` and the OTA progress callback do).

**P1 data path** (`onSerialData`): meter connected to `Serial2` RX on GPIO16, 115200 8N1, **inverted** signal. Lines are read one at a time:
- Line starting with `/` = telegram header → resets the CRC16 and the ring buffer.
- Line starting with `!` = trailer → computed CRC16 (poly 0xA001, `src/crc16.h`) is compared to the transmitted one and the line is replaced by `CRC (… OK)` / `CRC (CRC error)`.
- Every line is stored in a 50-entry ring buffer (served as HTML at `/dump`). Nothing is published while the telegram is arriving: at the trailer `publishTelegram()` sends the whole telegram to MQTT if the CRC is OK, or only the `CRC (CRC error)` line if not.

**MQTT** (`sendMQTTMessage`): broker/credentials are hardcoded in `main.h`. Topic is `PowerMeter/<OBIS code>` with the payload being the value inside the parentheses minus the `*unit` suffix; lines without parentheses go to `PowerMeter/Dump`. Nothing is published unless WiFi is connected; the broker connection is (re)established lazily on publish, at most every 5 s, with a 5 s socket timeout to stay under the watchdog. Buffer size is 1024; oversized messages are skipped with a log line.

**WiFi state machine**: credentials are stored in SPIFFS `/wifi.txt` (ssid and password on separate lines). On boot: credentials present → STA mode, else → AP mode (`ESP32-AP`). In AP mode `/` serves a form posting to `/connect`, which saves credentials, replies, and sets `wifiConnectRequested`; `loop()` then does the actual connect so the HTTP response is sent before the AP goes down. Only `/favicon.ico` is served statically from SPIFFS — never expose `/wifi.txt`. `apActiveMillis != 0` means AP mode is active: LED blinks fast (200 ms) and the device restarts after 10 minutes. `connectToRouter()` is non-blocking: it only starts the connection and sets `wifiConnecting`; `checkWiFiConnection()` (every `loop()`) completes it, or falls back to AP mode after 30 s. Once connected, it checks every 10 s and calls `connectToRouter()` on loss.

**OTA**: ArduinoOTA with hostname `esppowermeter`, no password.
