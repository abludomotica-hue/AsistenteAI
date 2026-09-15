# 06. FAILURE MODE & EFFECTS CATALOG (FMEA)
## Historical Incident Database & Preventative Rules

---

## 1. Catalog of Documented Failure Modes

### 🚨 FM-001: ES8311 ADC Saturation (Mic DC Bias)
- **Symptom:** Audio sample values locked at +32767; transcription returns gibberish or empty text.
- **Root Cause:** Condenser mic bias voltage injected into ADC without high-pass filtering; ES8311 internal HPF disabled by default.
- **Detection:** Inspect raw audio buffer; all int16 values near max positive limit.
- **Corrective Action:** Enable ES8311 ADC HPF registers `0x1B = 0x0A` and `0x1C = 0x6A`; set analog PGA to +24dB (`0x17 = 0xBF`).
- **Preventative Rule:** Always verify HPF register initialization before opening I2S RX streams.

---

### 🚨 FM-002: System Brownout Reset (BOD Triggered)
- **Symptom:** ESP32 resets abruptly (`Brownout detector was triggered`) during loud audio playback.
- **Root Cause:** Simultaneous 20 dBm (100 mW) WiFi transmission peaks and unattenuated DAC output drive current draw > 850 mA, collapsing 3.3V rail on standard PC USB ports.
- **Detection:** ESP-IDF boot log shows `rst:0x3 (RTC_SW_SYS_RST)` with BOD warning.
- **Corrective Action:** Throttle WiFi RF power to `15 dBm` (~31 mW) and limit DAC volume register to `0x32 = 0x80` (~60%).
- **Preventative Rule:** Never increase WiFi TX power above 15 dBm or DAC volume above 0x90 without external dedicated 5V 2A power supply.

---

### 🚨 FM-003: Null MAC Address on Hosted WiFi Interface
- **Symptom:** ESP32-C6 fails to associate with WiFi router; AP rejects connection.
- **Root Cause:** `esp-hosted` / `esp_wifi_remote` initialized virtual STA interface with `00:00:00:00:00:00` MAC address.
- **Detection:** Serial log shows MAC `00:00:00:00:00:00` during `esp_wifi_set_config`.
- **Corrective Action:** Fetch hardware eFuse MAC at boot, flip STA local bit (`mac[5] ^= 0x02`), and invoke `esp_wifi_set_mac(WIFI_IF_STA, mac)`.
- **Preventative Rule:** Mandatory MAC validation before calling `esp_wifi_start()`.

---

### 🚨 FM-004: MIPI DSI Driver Panic on `swap_xy`
- **Symptom:** Firmware crashes during LCD setup with `esp_lcd_panel_swap_xy: not supported by this panel`.
- **Root Cause:** ST7701S controller does not support hardware register coordinate swapping in 2-lane DSI video mode.
- **Detection:** ESP-IDF error log during `bsp_display_start()`.
- **Corrective Action:** Perform all screen rotation and coordinate transformations in software via LVGL 9 display rotation flags.
- **Preventative Rule:** Never call hardware swap_xy on ST7701S MIPI DSI panels.

---

### 🚨 FM-005: Chunked HTTP Connection Reset by `Content-Length: -1`
- **Symptom:** Gateway server closes connection immediately with TCP RST (`Connection reset by peer`).
- **Root Cause:** Passing `-1` to `esp_http_client_set_post_field` casts signed `-1` to `(size_t)4294967295`, emitting `Content-Length: 4294967295`.
- **Detection:** Gateway WSGI server rejects request for exceeding max body size.
- **Corrective Action:** Do not call `esp_http_client_set_post_field`; enable chunked transfer by passing `-1` directly as `write_len` in `esp_http_client_open(client, -1)`.
- **Preventative Rule:** Never use `esp_http_client_set_post_field` with negative lengths.

---

### 🚨 FM-006: Gateway Payload Contract Mismatch
- **Symptom:** Gateway returns HTTP 400 or crashes trying to read `request.files['audio']`.
- **Root Cause:** Firmware streams raw PCM bytes (`application/octet-stream`), but backend expected multipart form data.
- **Detection:** Flask logs show `KeyError: 'audio'`.
- **Corrective Action:** Use `request.get_data()` in `bridge_server.py` to ingest raw binary streams directly.
- **Preventative Rule:** Ensure API contracts between firmware and gateway are strictly typed and documented.

---

### 🚨 FM-007: SDIO Packet Flooding on Small Audio Chunks
- **Symptom:** Audio stream drops after ~800ms with `poll_write select error 104, errno = Connection reset by peer`.
- **Root Cause:** 1024-byte audio buffer generates 31.25 TCP packets/sec, overwhelming `esp-hosted` SDIO queue buffers.
- **Detection:** `esp_hosted` RPC timeout warnings and sudden socket drop during streaming.
- **Corrective Action:** Increase audio buffer size to `8192 bytes` (256ms audio = 4 pkts/sec); implement `network_stream_abort()` to clean up sockets.
- **Preventative Rule:** Audio chunk size over SDIO WiFi must never be smaller than 4096 bytes (ideal: 8192 bytes).

---

### 🚨 FM-008: Windows COM Port Lockout
- **Symptom:** `idf.py flash` fails with `PermissionError(13, 'Acceso denegado', 'COM3')`.
- **Root Cause:** PuTTY, serial monitors or background tasks holding exclusive Windows handle on COM3.
- **Detection:** Esptool fatal error on serial port open.
- **Corrective Action:** Close PuTTY / serial monitors before running flashing commands.
- **Preventative Rule:** Always release serial handles prior to invoking build/flash scripts.

---

### 🚨 FM-009: Missing RFC 7230 Chunk Boundaries on `esp_http_client_write`
- **Symptom:** Client times out waiting for response headers (`Connection timed out before data was ready!`) or receives HTTP 400 Bad Request.
- **Root Cause:** In ESP-IDF v5, `esp_http_client_write` writes raw bytes directly to transport without adding HTTP chunk headers/trailers. When opened in chunked mode (`write_len = -1`), the server waits indefinitely for `<hex_size>\r\n` and terminating `0\r\n\r\n`.
- **Detection:** Gateway server never finishes reading `request.get_data()`; serial log shows 10-second timeout in `esp_http_client_fetch_headers`.
- **Corrective Action:** Format each chunk with `<HEX_SIZE>\r\n<DATA>\r\n` and terminate the stream with `"0\r\n\r\n"`.
- **Preventative Rule:** Never send raw data in chunked HTTP mode without explicit RFC 7230 chunk framing.
