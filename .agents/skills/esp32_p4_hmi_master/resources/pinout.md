# JC4880P443C (ESP32-P4) Hardware Pinout Reference

Always consult this table before assigning GPIOs in the code.

## 1. Display (ST7701S RGB Interface)
*   Managed internally by the ESP32-P4 LCD peripheral. Do not repurpose these pins for general I/O unless the display is disabled.

## 2. Touch Panel (Capacitive)
*   **I2C SDA:** GPIO 45
*   **I2C SCL:** GPIO 46
*   **INT:** GPIO 47
*   **RST:** GPIO 48

## 3. Audio (I2S)
*   **BCLK (Bit Clock):** GPIO 21
*   **WS (Word Select / LRCLK):** GPIO 22
*   **DOUT (Data Out to Speaker):** GPIO 23
*   **DIN (Data In from Mic):** GPIO 24

## 4. Communication Interfaces
*   **RS485 TX:** GPIO 17
*   **RS485 RX:** GPIO 18
*   **RS485 RE/DE (Direction Control):** GPIO 16
*   **UART0 TX (Debug):** GPIO 43
*   **UART0 RX (Debug):** GPIO 44
*   **General UART TX:** GPIO 39
*   **General UART RX:** GPIO 40

## 5. SD Card (SDIO)
*   **CMD:** GPIO 35
*   **CLK:** GPIO 36
*   **D0:** GPIO 37
*   **D1:** GPIO 38
*   **D2:** GPIO 33
*   **D3:** GPIO 34

## 6. Camera (DVP)
*   Standard DVP pins are mapped to GPIO 0-15. Refer to specific camera init code.

*Note: The ESP32-C6 acts as a Wi-Fi/BT coprocessor and communicates with the ESP32-P4 over a high-speed SDIO/SPI bus internally. Do not interfere with this bus.*
