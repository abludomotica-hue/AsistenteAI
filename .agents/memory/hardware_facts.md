# Hardware Facts & Pinout Reference (Guition JC4880P443C)

- **Host SoC:** ESP32-P4 Dual Core RISC-V @ 360/400MHz with 32MB PSRAM.
- **WiFi/BT SoC:** ESP32-C6 connected over 40MHz 4-bit SDIO (`CLK:18, CMD:19, D0:14, D1:15, D2:16, D3:17, RST:54`).
- **Audio Codec:** ES8311 (I2C address `0x18` on `SDA:7, SCL:8`).
- **Audio I2S:** `MCLK:13, BCLK:12, WS:10, DOUT:9, DIN:48`.
- **Power Amplifier:** NS4168 enabled via `GPIO 11` (HIGH = ON).
- **Display:** 4.43" 480x800 MIPI DSI 2-lanes ST7701S (`RST:5, Backlight:23`).
- **Touch:** Goodix GT911 (`SDA:7, SCL:8, INT:21, RST:22`).
- **Boot Button:** `GPIO 35` (Active Low).
