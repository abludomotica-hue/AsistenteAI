# 🏛️ Arquitectura de Sistema: Edge AI Voice Assistant

**Framework:** ESP-IDF v5.3.2 LTS Nativo + FreeRTOS + LVGL 9  
**Hardware:** ESP32-P4 (Host) + ESP32-C6 (WiFi SDIO) + ES8311 + ST7701S  
**AI Gateway:** Debian 13 VM (Proxmox `192.168.1.58:5000`) + Waitress WSGI  
**Cloud AI:** NVIDIA NIM (Parakeet ASR + Nemotron LLM + Magpie TTS)  

---

## 1. Topología del Sistema (4 Capas de Aislamiento)

```mermaid
flowchart TD
    subgraph CAPA_1["Capa 1: Hardware Edge (JC4880P443C)"]
        MIC["Micrófono Analógico (+24dB)"] -->|Señal Analógica| CODEC["Codec ES8311 (I2C 0x18)"]
        CODEC -->|I2S DIN GPIO 48| P4_I2S["ESP32-P4 I2S RX (16kHz Mono)"]
        P4_I2S -->|DMA Stream| PSRAM_BUF["PSRAM Ring Buffer (8KB Chunks)"]
        
        TOUCH["Touch GT911 (I2C 0x18)"] -->|Eventos Táctiles| LVGL["UI LVGL 9 (Core 1)"]
        LVGL -->|AudioCommand Queue| CTRL_TASK["audio_control_task (Core 0)"]
    end

    subgraph CAPA_2["Capa 2: Firmware & Transporte"]
        PSRAM_BUF -->|Chunked POST| NET_STREAM["network_stream (esp_http_client)"]
        NET_STREAM -->|SDIO Bus| C6_WIFI["ESP32-C6 (esp_wifi_remote)"]
        C6_WIFI -->|WiFi 2.4GHz 15dBm| ROUTER["Router LAN (192.168.1.x)"]
    end

    subgraph CAPA_3["Capa 3: AI Gateway (Debian VM 192.168.1.58:5000)"]
        ROUTER -->|HTTP/1.1 Chunked Stream| WAITRESS["Waitress WSGI Server (8 Hilos)"]
        WAITRESS -->|Flask Route /v1/conversation_stream| GATEWAY["bridge_server.py"]
    end

    subgraph CAPA_4["Capa 4: Servicios NVIDIA NIM"]
        GATEWAY -->|PCM Raw Audio| STT["NVIDIA Parakeet CTC 0.6B (ASR)"]
        STT -->|Texto Transcrito| LLM["NVIDIA Nemotron-3 30B (LLM)"]
        LLM -->|Texto Respuesta| TTS["NVIDIA Magpie Multilingual (TTS)"]
        TTS -->|PCM Streaming WAV/Raw| GATEWAY
    end

    GATEWAY -->|HTTP Response Audio Stream| NET_STREAM
    NET_STREAM -->|I2S DOUT GPIO 9| CODEC
    CODEC -->|GPIO 11 PA ON| AMP["Amplificador NS4168"]
    AMP -->|Audio Analógico| SPK["Altavoz 8Ω 2W"]
```

---

## 2. Asignación Concurrente de Núcleos (FreeRTOS)

| Núcleo | Tareas Asignadas | Responsabilidades | Prioridad |
|---|---|---|---|
| **Core 1 (App Core)** | `lvgl_task` | Motor gráfico LVGL 9, eventos táctiles GT911, animaciones a 60 FPS con Double Buffer en PSRAM. | 4 |
| **Core 0 (Pro Core)** | `afe_fetch` (I2S RX), `audio_ctrl`, `esp_wifi_remote`, `network_stream` | Captura DMA de audio, streaming HTTP/1.1 chunked, comunicación SDIO con el coprocesador C6, recepción de audio TTS y reproducción I2S TX. | 5 (Alta) |

---

## 3. Filosofía de Memoria y Rendimiento
1. **Buffers en PSRAM:** Los buffers de captura de audio (`8192 bytes` por chunk) y renderizado gráfico se alojan en **PSRAM externa (32MB)** con `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`.
2. **Protección de SRAM Interno:** La memoria interna SRAM se reserva para DMA y descriptores de alta velocidad, evitando la fragmentación del heap.
3. **Comunicación Thread-Safe:** La UI y el subsistema de audio se comunican exclusivamente mediante colas de FreeRTOS (`audioCommandQueue`), sin llamadas directas no sincronizadas entre núcleos.
