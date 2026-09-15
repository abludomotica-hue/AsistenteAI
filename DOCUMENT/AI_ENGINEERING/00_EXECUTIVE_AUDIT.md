# 00. EXECUTIVE AUDIT & SYSTEM STATE
## AsistenteAI Engineering Operating System Audit

**Audit Date:** Agosto 2026  
**Auditor:** Principal AI Engineering Architect / CAEO  
**Hardware Target:** Guition JC4880P443C (ESP32-P4 + ESP32-C6 + ES8311 + ST7701S + GT911 + NS4168)  
**Host Firmware:** ESP-IDF v5.3.2 LTS + FreeRTOS Dual-Core + LVGL 9  
**Gateway Infrastructure:** Debian 13 VM (Proxmox @ `192.168.1.58:5000`) + Waitress WSGI (8 Threads)  
**Cloud AI Services:** NVIDIA NIM (Parakeet CTC 0.6B ASR + Nemotron-3 30B LLM + Magpie Multilingual TTS)  

---

## 1. Executive Summary

El proyecto **AsistenteAI** ha completado una transición arquitectónica crítica: la migración total desde prototipos en Arduino IDE hacia **ESP-IDF v5.3.2 LTS nativo**.
Durante esta evolución, se superaron 8 modos de fallo severos a nivel de hardware, registros I2C, colas SDIO y protocolos HTTP.

Actualmente, el sistema cuenta con:
- **Capa 1 (Hardware):** 100% caracterizado, pines validados, desmuteo de registros DAC y activación de preamplificador/HPF en el codec ES8311, y estabilización de potencia RF (15 dBm) para prevenir cortes por Brownout (BOD).
- **Capa 2 (Firmware ESP-IDF):** Motor gráfico LVGL 9 desacoplado en **Core 1** a 60 FPS con Double Buffer en PSRAM. Pipeline de audio I2S RX/TX y cliente HTTP Chunked desacoplado en **Core 0** bajo colas thread-safe FreeRTOS.
- **Capa 3 (AI Gateway):** Servidor WSGI Waitress en producción con endpoints optimizados para streaming de audio binario crudo (`application/octet-stream`) vía `request.get_data()`.
- **Capa 4 (NVIDIA NIM):** Integración verificada con Parakeet ASR, Nemotron-3 LLM y Magpie TTS en la nube.

---

## 2. Architectural Layer Audit

```
┌─────────────────────────────────────────────────────────────┐
│ LAYER 4: NVIDIA NIM CLOUD                                   │
│ Parakeet CTC 0.6B (ASR) │ Nemotron-3 (LLM) │ Magpie (TTS)   │
└──────────────────────────────▲──────────────────────────────┘
                               │ gRPC / REST (WAN)
┌──────────────────────────────▼──────────────────────────────┐
│ LAYER 3: AI GATEWAY (Debian Proxmox VM 192.168.1.58:5000)   │
│ Waitress WSGI (8 Hilos) │ Flask Routing │ Token Auth        │
└──────────────────────────────▲──────────────────────────────┘
                               │ HTTP/1.1 Chunked (LAN WiFi)
┌──────────────────────────────▼──────────────────────────────┐
│ LAYER 2: FIRMWARE ESP32-P4 (ESP-IDF v5.3.2 LTS)             │
│ Core 1: LVGL 9 HMI (480x800) │ Core 0: I2S/Audio/SDIO/Net   │
└──────────────────────────────▲──────────────────────────────┘
                               │ I2S / I2C / MIPI DSI / SDIO
┌──────────────────────────────▼──────────────────────────────┐
│ LAYER 1: HARDWARE (Guition JC4880P443C)                     │
│ ESP32-P4 │ ESP32-C6 (WiFi) │ ES8311 │ ST7701S │ GT911 │ PA │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. Subsystem Health Matrix

| Subsystem | Health | Risk Level | Bottlenecks & Critical Notes |
|---|---|---|---|
| **Audio Capture (I2S RX)** | 🟢 Healthy | Low | 16kHz / 16-bit Mono. Chunks de 8192 bytes (256ms) en PSRAM. Registros HPF activos. |
| **Audio Output (I2S TX)** | 🟡 In-Progress | Medium | Conexión de stream de respuesta TTS hacia I2S TX y control de desmuteo PA GPIO 11. |
| **Display (MIPI DSI)** | 🟢 Healthy | Low | ST7701S configurado vía BSP oficial. Rotación delegada a software LVGL 9 (evita `swap_xy`). |
| **Touch (GT911)** | 🟢 Healthy | Low | I2C en bus compartido (SDA 7, SCL 8). Eventos táctiles manejados fluidamente. |
| **Wireless Transport** | 🟢 Healthy | Medium | ESP32-C6 sobre SDIO a 40MHz. Inyección de MAC física por eFuse obligatoria. |
| **Gateway Proxy** | 🟢 Healthy | Low | Waitress tolerante a streaming chunked. Manejo de excepciones y aborto limpio de sockets. |
| **Cloud AI Services** | 🟢 Healthy | Low | Latencia de red a NVIDIA NIM dentro de parámetros esperados (<2.2s TTFA). |

---

## 4. Key Failure Modes Identified & Remediated

1. **FM-001:** Saturación de ADC en ES8311 por offset DC $\to$ Solucionado con HPF (`0x1B=0x0A`, `0x1C=0x6A`).
2. **FM-002:** Reinicios por Brownout (BOD) $\to$ Resuelto moderando WiFi a 15 dBm y DAC a `0x80`.
3. **FM-003:** Dirección MAC nula en STA $\to$ Resuelto inyectando MAC desde eFuse con bit local.
4. **FM-004:** Error `swap_xy` en panel ST7701S $\to$ Resuelto mediante rotación por software en LVGL 9.
5. **FM-005:** Rechazo HTTP por `Content-Length: -1` $\to$ Resuelto usando chunked puro en `esp_http_client_open`.
6. **FM-006:** Incompatibilidad de Payload en Gateway $\to$ Resuelto con `request.get_data()` para raw PCM.
7. **FM-007:** Saturación SDIO por paquetes diminutos (1024B) $\to$ Resuelto con buffers de 8192B (4 pkts/s).
8. **FM-008:** Bloqueo de puerto serie COM3 en Windows $\to$ Resuelto cerrando monitores antes de flashear.

---

## 5. Decision & Governance Mandate

A partir de este hito, **ningún agente de IA modificará el sistema sin apegarse al AI Engineering Operating System**, manteniendo un contexto permanente mínimo y activando skills bajo demanda con evidencia verificable.
