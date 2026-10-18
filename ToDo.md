# ToDo

## PR 1 – Real bugs

- [x] **1. WiFi password readable over HTTP** – `server.serveStatic("/", SPIFFS, "/")` serves `/wifi.txt` to anyone on the LAN. Serve only `/favicon.ico`, or move credentials to Preferences (NVS).
- [x] **2. `connectToRouter()` false failure** – if the connection succeeds on the last retry, `countDown` is already 0 and `countDown < 1` falls back to AP mode. Check `WiFi.status() != WL_CONNECTED` after the loop instead.
- [x] **3. `handleConnect()` stale credentials / no response** – the global `ssid`/`password` are not updated, so a later reconnect uses empty or old values; no HTTP response is sent, so the browser hangs. Copy into the globals, send a reply page, then connect.
- [x] **4. CRC compare drops leading zeros** – `String(crc16Value, HEX)` turns `0x0ABC` into `"abc"`, so ~1/16 of valid telegrams report a CRC error. Format with `snprintf("%04X")` before comparing.
- [x] **5. `readWiFiCredentials()` edge cases** – an empty line writes to `ssid[-1]`; a 32-char SSID or 64-char password is truncated (no room for the terminator); an empty file still returns success. Use `trim()`, size arrays 33/65, fail on an empty SSID.
- [x] **6. MQTT reconnect storm / watchdog reset** – reconnects on every line (~40 per telegram), even when WiFi is down; a broker that accepts TCP but never answers blocks up to 15 s (> 12 s watchdog). Skip when WiFi is down and retry at most every ~5 s.
- [x] **7. MQTT buffer not actually increased** – `setBufferSize(256)` equals the library default and the length check ignores the topic, so long lines (`0-0:96.13.0`, `0-0:98.1.0`) are silently dropped. Raise to ~1024 and check topic + payload against `mqttClient.getBufferSize()`.
- [x] **14. WiFi password printed to serial** – remove the `Serial.print(password)` in `connectToRouter()`.

## PR 2 – Hardening / hygiene

- [ ] **10. `getP1Text()` re-parses the JSON table** for every line of every `/dump` request. Parse once and cache, or use a static lookup table.
- [ ] **11. `/dump` output not HTML-escaped** – meter text messages go into the page unescaped.
- [ ] **12. SPIFFS failure aborts `setup()`** – no web server or OTA, so no remote recovery. Continue without saved credentials (go to AP mode).
- [ ] **13. Hardcoded secrets** – MQTT login, AP password `almaalma`, OTA with no password. Move to a gitignored `secrets.h` (commit `secrets.example.h`) and set an OTA password.
- [ ] **15. `mqttCallback` reads past the payload** (not zero-terminated). It isn't registered; fix it or delete it with the commented-out subscribe code.
- [ ] **16. Unpinned platform** – `platform = espressif32`; Arduino-ESP32 3.x changed `esp_task_wdt_init`, so an update would break the build. Pin the currently installed version.

## PR 3 – Optional design changes

- [ ] **8. Publish only after CRC check** – lines are published as they arrive, so corrupted telegrams publish bad values. Buffer the telegram and publish only on CRC OK (CRC result to `PowerMeter/Dump`).
- [ ] **9. Non-blocking WiFi reconnect** – reconnecting blocks `loop()` for up to 30 s, while the 3500-byte receive buffer holds only about one telegram.
