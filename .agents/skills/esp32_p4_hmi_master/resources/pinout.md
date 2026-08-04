# JC4880P443C (ESP32-P4) Hardware Pinout Reference

Always consult this table before assigning GPIOs in the code.

Fuentes de verificación (etiquetas al final de cada línea):
- [FW] Firmware de este proyecto, probado y funcionando (audio/I2C/PA).
- [XZ] xiaozhi-esp32, board `guition-jc4880p443` (proyecto OSS funcional para esta placa).
- [VD] Demos Arduino del fabricante (`JC4880P443C_I_W/1-Demo/arduino_examples`).
- [SC] Esquemáticos del fabricante (`JC4880P443C_I_W/5-Schematic`).

## 1. Audio (Codec ES8311, addr I2C 0x18)
*   **I2S MCLK:** GPIO 13  [FW][XZ][SC]
*   **I2S BCLK:** GPIO 12  [FW][XZ][SC]
*   **I2S WS / LRCK:** GPIO 10  [FW][XZ][SC]
*   **I2S DOUT (ESP32 → codec, playback):** GPIO 9  [FW][XZ][SC]
*   **I2S DIN (codec → ESP32, mic):** GPIO 48  [FW][XZ][SC]
*   **PA Enable (amplificador):** GPIO 11  [FW][XZ][SC]
*   **I2C del codec:** SDA GPIO 7 / SCL GPIO 8 — bus COMPARTIDO con el touch  [FW][VD][SC]

## 2. Display (ST7701S, 480×800, MIPI DSI 2 lanes)
*   Interfaz MIPI DSI gestionada por el periférico LCD del P4 (pares DSI_A_*). No usar esos pines para otra cosa.  [SC]
*   **LCD RST:** GPIO 5  [XZ]
*   **Backlight (LCD_PWM):** GPIO 23  [XZ][SC]

## 3. Touch Panel (GT911, capacitivo)
*   **I2C:** compartido con el codec: SDA GPIO 7 / SCL GPIO 8  [VD][SC]
*   **INT:** GPIO 21  [XZ]
*   **RST:** GPIO 22  [XZ]
*   Nota: el GT911 funciona sin INT/RST (demos del fabricante los dejan en -1)  [VD].
      El esquemático sugiere nets INT/RST hacia GPIO27/GPIO28; si 21/22 fallara en
      una revisión de placa, verificar contra `3_ESP32-P4.png` antes de cambiar.

## 4. Botón y LED
*   **BOOT button:** GPIO 35  [XZ][SC]
*   **LED integrado:** GPIO 26  [XZ] — OJO: el esquemático también rutea GPIO26 como
      TX1 del RS485  [SC]; no usar ambos a la vez.

## 5. Tarjeta TF (SDMMC)
*   **CLK:** GPIO 43 · **CMD:** GPIO 44  [VD][SC]
*   **D0/D1/D2/D3:** GPIO 39 / 40 / 41 / 42  [VD][SC]
*   **Power enable (MOSFET A03401):** GPIO 45  [SC]
*   FAT32, ≤ 32 GB según el fabricante.

## 6. RS485 (transceptor MAX485)
*   **TX:** GPIO 26  [SC]
*   **RX / RE-DE:** nets RX1/EN en `5_485.png`; verificar en la revisión de placa
      antes de usar (los valores antiguos 17/18/16 de este documento eran INCORRECTOS:
      corresponden al bus del ESP32-C6).  [SC]

## 7. UART
*   **UART0 debug:** TX GPIO 37 / RX GPIO 38  [SC]
*   Consola de desarrollo real: USB CDC (HWCDC) por el puerto Type-C.
*   **UART del coprocesador C6:** pines 16/17/18 del módulo (C6_U0RXD/C6_U0TXD/C6_IO9). NO tocar.  [SC]

## 8. Cámara (OV02C10, 2 MP)
*   Interfaz **MIPI CSI-2** (pares CSI_A_*), NO DVP paralelo.  [SC]

## 9. Header de expansión (26 pines)
*   Expone GPIO 40–49, bus ES_I2C (SDA/SCL del codec), señales C6 y CHIP_PU.  [SC]
*   Útil para prototipado; evita consumir estos GPIOs en el diseño final.

## 10. WiFi / Bluetooth
*   El ESP32-C6 actúa como coprocesador Wi-Fi/BT vía SDIO/SPI interno.
      No interferir con su bus. En Arduino, `WiFi.begin()` funciona transparente.
