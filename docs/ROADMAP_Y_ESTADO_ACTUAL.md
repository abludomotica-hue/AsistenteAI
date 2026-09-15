# 🚀 Estado Actual del Proyecto y Roadmap Ejecutivo

**Fecha de Actualización:** Septiembre 2026  
**Madurez del Sistema:** **FASE 4 (Wake Word Offline + Domótica HA + Hardening OTA A/B) 100% OPERATIVA**  

---

## 📊 1. Matriz de Estado por Subsistemas

| Subsistema | Componentes | Estado | Detalles de Implementación |
|---|---|---|---|
| **Hardware & Energía** | ESP32-P4, C6, ES8311, NS4168 | **100% Operativo** ✅ | Calibrado a 15dBm WiFi y DAC 0x80 (Anti-Brownout BOD superado). HPF y PGA +24dB activos. |
| **Pantalla & Touch** | MIPI DSI ST7701S, GT911, LVGL 9 | **100% Operativo** ✅ | Panel 480x800 a 60 FPS en Core 1 con Double Buffer en PSRAM. Nomenclatura estricta "configuración". |
| **Transporte & WiFi** | Coprocesador C6, SDIO, `esp_wifi_remote` | **100% Operativo** ✅ | Enlace SDIO a 40MHz. Inyección de MAC física por eFuse (`esp_efuse_mac_get_default`). WPA2-PSK reforzado. |
| **Captura de Audio (I2S RX)** | Driver I2S v5, Chunks 8KB | **100% Operativo** ✅ | I2S Standard en Core 0 (MCLK 13, BCLK 12, WS 10, DIN 48). Downmix estéreo a mono limpio. |
| **Feedback Acústico (I2S TX)** | Generador Senoidal Dual (880/1320 Hz) | **100% Operativo** ✅ | Chime armónico de 110ms con envolvente anti-clic ejecutado en buffer estático PSRAM al despertar. |
| **AI Gateway Server** | Debian 13 VM Proxmox (Waitress 8 hilos) | **100% Operativo** ✅ | Endpoints seguros `/stt`, `/llm`, `/tts` y `/v1/conversation_stream` con token `X-Bridge-Token` y retry exponencial 503/429. |
| **Servicios NVIDIA NIM** | Parakeet ASR, Nemotron LLM, Magpie TTS | **100% Operativo** ✅ | Pipeline de IA en nube privada NVIDIA (< 2.2s TTFA) con reintentos automáticos y sanitización de cabeceras Latin-1. |
| **Reproducción TTS (I2S TX)** | `network_stream.cpp` → `audio_manager` | **100% Operativo** ✅ | Buffer estático PSRAM de 16KB (`s_tx_stereo_buffer`), cero fragmentación de memoria dinámica. |
| **Domótica (Home Assistant)** | REST API Gateway + Auto-Dispatch | **100% Operativo** ✅ | Whitelist estricta `ALLOWED_HA_SERVICES` (`light`, `switch`, `climate`, `cover`) y dispatch seguro en <80ms. |
| **Wake Word Offline** | ESP-SR v2.0 / WakeNet 9 (`Hi, ESP`) | **100% Operativo** ✅ | Ingesta continua en AFE (`afe_fetch_task`) sobre Core 0 en reposo. Activación por voz sin latencia. |
| **Actualización OTA (A/B)** | Doble partición A/B + Rollback Seguro | **100% Operativo** ✅ | `esp_https_ota` iterativo con reporte de % a LVGL, verificación de imagen y `esp_ota_mark_app_valid_cancel_rollback()`. |
| **Servicio de Música y Streaming** | Media Player / MP3 Streaming | **Fase 4.3 (En Diseño)** ⚪ | Reproducción de radio por streaming y archivos de audio en segundo plano. |

---

## 🗺️ 2. Fases de Desarrollo

### ✅ FASE 1: Validación Física & Eléctrica (Completada)
- Identificación de pinout y arranque del codec ES8311.
- Corrección de saturación DC Bias en el micrófono analógico (+24dB y filtro HPF).
- Superación de reinicios por Brownout Detector (BOD) mediante calibración a 15 dBm y DAC 0x80.

### ✅ FASE 2: Migración a ESP-IDF Nativo v5.3.2 LTS + FreeRTOS (Completada)
- Eliminación de dependencias de Arduino IDE.
- Implementación de BSP oficial para MIPI DSI ST7701S.
- Integración de LVGL 9 con Double Buffering en PSRAM sobre Core 1 (60 FPS estables).
- Gestión de coprocesador C6 sobre SDIO con dirección MAC válida por hardware.

### ✅ FASE 3: Pipeline de Streaming Bidireccional E2E (Completada & Certificada)
- `[x]` Endpoint de streaming chunked en Debian Gateway (`/v1/conversation_stream`).
- `[x]` Captura I2S por DMA en ESP32-P4 en ráfagas de 8KB por paquete en modo Estéreo con downmix.
- `[x]` Delimitación estricta de chunks RFC 7230 con cabeceras hexadecimales y terminador `0\r\n\r\n`.
- `[x]` Transcripción ultrarrápida con Parakeet ASR (740ms) y razonamiento con Nemotron-3 LLM (1.5s).
- `[x]` Síntesis de voz Magpie TTS y reproducción limpia y continua por el altavoz de la placa.

### 🚀 FASE 4: Domótica, Wake Word, OTA & Servicios Avanzados (ESTADO ACTUAL)
- `[x]` **Fase 4.1 Domótica Home Assistant:** Control en lenguaje natural de luces e interruptores con whitelist estricta y confirmación vocal simultánea.
- `[x]` **Fase 4.2 Wake Word Offline:** Integración de WakeNet 9 / ESP-SR en Core 0 con ingesta continua en reposo y feedback acústico instantáneo (Chime senoidal suave).
- `[x]` **Fase 4.4 Hardening OTA A/B:** Particionado A/B con rollback automático (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`), reporte porcentual visual en LVGL y validación de firmware al arranque.
- `[ ]` **Fase 4.3 Reproductor Multimedia:** Streaming continuo de audio / MP3 hacia el DAC ES8311 con buffer circular en PSRAM.
