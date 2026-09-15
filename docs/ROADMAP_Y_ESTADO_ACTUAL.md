# 🚀 Estado Actual del Proyecto y Roadmap Ejecutivo

**Fecha de Actualización:** Agosto 2026  
**Madurez del Sistema:** **FASE 3 (ESP-IDF v5.3.2 LTS + LVGL 9 + Streaming Bidireccional) EN VALIDACIÓN ACTIVA**  

---

## 📊 1. Matriz de Estado por Subsistemas

| Subsistema | Componentes | Estado | Detalles de Implementación |
|---|---|---|---|
| **Hardware & Energía** | ESP32-P4, C6, ES8311, NS4168 | **100% Operativo** ✅ | Calibrado a 15dBm WiFi y DAC 0x80 (Anti-Brownout BOD superado). HPF y PGA +24dB activos. |
| **Pantalla & Touch** | MIPI DSI ST7701S, GT911, LVGL 9 | **100% Operativo** ✅ | Panel 480x800 a 60 FPS en Core 1 con Double Buffer en PSRAM. Botón táctil e interfaz en Obsidian Dark Mode. |
| **Transporte & WiFi** | Coprocesador C6, SDIO, `esp_wifi_remote` | **100% Operativo** ✅ | Enlace SDIO a 40MHz. Inyección de MAC física por eFuse (`esp_efuse_mac_get_default`). |
| **Captura de Audio (I2S RX)** | Driver I2S v5, Chunks 8KB | **100% Operativo** ✅ | I2S Standard en Core 0 (MCLK 13, BCLK 12, WS 10, DIN 48). Ráfaga de 4 segundos a 16kHz Mono. |
| **AI Gateway Server** | Debian 13 VM Proxmox (Waitress 8 hilos) | **100% Operativo** ✅ | Endpoints `/stt`, `/llm`, `/tts` y `/v1/conversation_stream` con `request.get_data()`. |
| **Servicios NVIDIA NIM** | Parakeet ASR, Nemotron LLM, Magpie TTS | **100% Operativo** ✅ | Pipeline de IA configurado en nube privada de NVIDIA (< 2.2s TTFA). |
| **Reproducción TTS (I2S TX)** | `network_stream.cpp` → `audio_manager` | **100% Operativo** ✅ | Transmisión DMA directa hacia ES8311 DAC y altavoz NS4168 (GPIO 11). |
| **Domótica (Home Assistant)** | REST API Gateway + Auto-Dispatch | **100% Operativo** ✅ | Control en tiempo real de luces (`light.ampolleta_oficina`, `light.living_floodlight`) y switches en <80ms. |
| **Wake Word Offline** | ESP-SR / WakeNet | **Fase 4.2 (Próxima)** ⚪ | Activación por voz sin tocar la pantalla ("Oye Asistente"). |
| **Servicio de Música y Streaming** | Media Player / MP3 Streaming | **Fase 4.3 (Próxima)** ⚪ | Reproducción de música y radio por streaming. |
| **Actualización OTA** | Doble partición A/B | **Fase 4.4 (Próxima)** ⚪ | Actualización remota de firmware sin cables. |

---

## 🗺️ 2. Fases de Desarrollo

### ✅ FASE 1: Validación Física & Eléctrica (Completada)
- Identificación de pinout y arranque del codec ES8311.
- Corrección de saturación DC Bias en el micrófono analógico.
- Superación de reinicios por Brownout Detector (BOD).

### ✅ FASE 2: Migración a ESP-IDF Nativo v5.3.2 LTS + FreeRTOS (Completada)
- Eliminación de dependencias de Arduino IDE.
- Implementación de BSP oficial para MIPI DSI ST7701S.
- Integración de LVGL 9 con Double Buffering en PSRAM sobre Core 1.
- Gestión de coprocesador C6 sobre SDIO con dirección MAC válida.

### ✅ FASE 3: Pipeline de Streaming Bidireccional E2E (100% CERTIFICADA)
- `[x]` Endpoint de streaming chunked en Debian Gateway (`/v1/conversation_stream`).
- `[x]` Captura I2S por DMA en ESP32-P4 en ráfagas de 8KB por paquete en modo Estéreo con downmix.
- `[x]` Delimitación estricta de chunks RFC 7230 con cabeceras hexadecimales y terminador `0\r\n\r\n`.
- `[x]` Transcripción ultrarrápida con Parakeet ASR (740ms) y razonamiento con Nemotron-3 LLM (1.5s).
- `[x]` Síntesis de voz Magpie TTS y reproducción limpia y continua por el altavoz de la placa.

### 🟡 FASE 4: Domótica, Wake Word & Servicios Avanzados (EN PROGRESO EXITOSO)
- `[x]` **Fase 4.1 Domótica Home Assistant (100% OPERATIVA):** Control en lenguaje natural de luces e interruptores en tiempo real con confirmación vocal simultánea.
- `[ ]` **Fase 4.2 Wake Word Offline:** Integración de WakeNet / ESP-SR para activación por voz ("Oye Asistente").
- `[ ]` **Fase 4.3 Reproductor Multimedia:** Streaming de audio continuo y música.
- `[ ]` **Fase 4.4 Actualizaciones OTA:** Particionado A/B con rollback automático.
