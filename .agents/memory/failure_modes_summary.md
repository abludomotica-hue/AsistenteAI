# Failure Modes Summary Matrix

| ID | Failure Mode | Root Cause | Solution |
|---|---|---|---|
| **FM-001** | Mic Saturation (+32767) | DC bias from mic without HPF | Enable ADC HPF (`0x1B=0x0A, 0x1C=0x6A`), PGA +24dB (`0x17=0xBF`). |
| **FM-002** | Brownout (BOD) Reset | 20dBm WiFi + high DAC volume > 850mA | WiFi TX at 15dBm, DAC volume at `0x80`. |
| **FM-003** | Null MAC Address | Virtual STA had 00:00:00:00:00:00 | Invert STA bit on eFuse MAC before `esp_wifi_start()`. |
| **FM-004** | ST7701S `swap_xy` Panic | Hardware register unsupported | Rotate display via software in LVGL 9. |
| **FM-005** | `Content-Length: -1` Reset | `-1` cast to uint32_t `4294967295` | Use `esp_http_client_open(client, -1)` for chunked. |
| **FM-006** | Payload Contract Mismatch | Server expected multipart, ESP sends octet-stream | Ingest raw binary with `request.get_data()`. |
| **FM-007** | SDIO Packet Flooding | 1024B buffer = 31 pkts/sec overflowed SDIO | Increase audio buffer to 8192B (4 pkts/sec). |
| **FM-008** | Serial Port Lockout | PuTTY holding handle on COM3 | Close terminal before executing `idf.py flash`. |
