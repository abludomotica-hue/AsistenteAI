# 🔌 Especificación de Hardware y Pinout Canónico: Guition JC4880P443C

**Placa:** JC4880P443C_I_W (Guition / JCZN)  
**Procesador Principal:** ESP32-P4 (Dual-Core RISC-V @ 360/400MHz, 32MB PSRAM, 16MB Flash)  
**Coprocesador WiFi/BLE:** ESP32-C6 (enlace SDIO interno de alta velocidad, 40MHz 4-bit)  
**Codec de Audio:** Everest Semi ES8311 (I2C 0x18 + I2S)  
**Pantalla:** 4.43" IPS 480×800 MIPI DSI 2-lanes (Controlador ST7701S)  
**Panel Táctil:** Goodix GT911 Capacitivo (I2C 0x18 / 0x5D)  

---

## 1. Mapeo de Pines y Periféricos

### 🎙️ Audio Digital (Codec ES8311 & Amplificador)
| Señal | Pin ESP32-P4 | Descripción | Notas de Calibración / Registros |
|---|---|---|---|
| **I2S MCLK** | **GPIO 13** | Master Clock del Codec (I2S) | 256 * Fs = 4.096 MHz (a 16kHz) |
| **I2S BCLK** | **GPIO 12** | Bit Clock | Sincronía de bits |
| **I2S WS / LRCK** | **GPIO 10** | Word Select / Frame Sync | 16 kHz |
| **I2S DOUT** | **GPIO 9** | Datos TX (ESP32 → DAC Codec) | Reproducción hacia altavoz |
| **I2S DIN** | **GPIO 48** | Datos RX (Micrófono → ESP32) | Captura de voz a 16kHz 16-bit Mono |
| **PA Enable** | **GPIO 11** | Habilitación de Amplificador NS4168 | Nivel Alto (1) = Encendido |
| **Codec I2C SDA** | **GPIO 7** | Bus I2C de Control (Dirección 0x18) | Compartido con Touch GT911 |
| **Codec I2C SCL** | **GPIO 8** | Bus I2C Clock | Compartido con Touch GT911 |

> **Registros Clave ES8311:**
> - `0x1B = 0x0A` & `0x1C = 0x6A`: Filtro HPF del ADC activado (elimina DC Bias del micrófono).
> - `0x17 = 0xBF`: Ganancia analógica preamplificador a +24dB.
> - `0x32 = 0x80`: Volumen digital del DAC calibrado (evita Brownout BOD).

---

### 🖥️ Pantalla (MIPI DSI ST7701S 480×800)
| Señal | Bus / Pin | Descripción |
|---|---|---|
| **DSI Data/Clock** | **Pares DSI_A_*** | Interfaz MIPI DSI 2-lanes nativa del ESP32-P4 |
| **LCD Reset** | **GPIO 5** | Reset del panel LCD |
| **Backlight PWM** | **GPIO 23** | Control de brillo de retroiluminación |

---

### 👆 Panel Táctil Capacitivo (Goodix GT911)
| Señal | Pin ESP32-P4 | Descripción |
|---|---|---|
| **Touch I2C SDA** | **GPIO 7** | Compartido con Codec ES8311 |
| **Touch I2C SCL** | **GPIO 8** | Compartido con Codec ES8311 |
| **Touch INT** | **GPIO 21** | Interrupción táctil |
| **Touch RST** | **GPIO 22** | Reset del controlador táctil |

---

### 📡 Coprocesador WiFi/BT (ESP32-C6 vía SDIO)
| Señal SDIO | Pin ESP32-P4 | Función |
|---|---|---|
| **SDIO CLK** | **GPIO 18** | Reloj SDIO 40 MHz |
| **SDIO CMD** | **GPIO 19** | Comando SDIO |
| **SDIO D0** | **GPIO 14** | Línea de datos 0 |
| **SDIO D1** | **GPIO 15** | Línea de datos 1 |
| **SDIO D2** | **GPIO 16** | Línea de datos 2 |
| **SDIO D3** | **GPIO 17** | Línea de datos 3 |
| **C6 Reset** | **GPIO 54** | Reset por hardware del coprocesador C6 |

---

### 🔌 Otras Interfaces Disponibles
- **Botón BOOT:** GPIO 35 (Pull-up interno, nivel bajo al presionar).
- **LED Integrado:** GPIO 26.
- **MicroSD (TF Card Slot):** CLK (GPIO 43), CMD (GPIO 44), D0-D3 (GPIO 39-42), Power EN (GPIO 45).
- **RS485:** TX (GPIO 26) con transceptor MAX485.
- **Cámara:** Conector MIPI CSI-2 (Soporta OV02C10 2MP).
- **Alimentación:** 5V USB Type-C o conector de batería LiPo 3.7V con circuito de carga integrado.
