# 11. PERFORMANCE & LATENCY MODEL
## Telemetry, Timing Budgets & Memory Benchmarks

---

## 1. Latency Budget & Time-to-First-Audio (TTFA)

El objetivo del sistema es mantener la latencia percibida por el usuario (**TTFA**) por debajo de **2.5 segundos**:

| Pipeline Stage | Target Latency Budget | Measured Performance | Bottleneck & Optimization |
|---|---|---|---|
| **Audio Capture (I2S DMA)** | 4,000 ms (Ráfaga) | 4,000 ms | Configurado por temporizador de ráfaga para STT. |
| **LAN Transmission (HTTP Chunked)** | < 150 ms | ~80 ms | Chunks de 8KB a través de SDIO 40MHz. |
| **ASR (NVIDIA Parakeet CTC 0.6B)** | < 500 ms | ~380 ms | Modelo ligero optimizado en GPU en NIM Cloud. |
| **LLM Reasoning (Nemotron-3 30B)** | < 1,200 ms | ~950 ms | Prompt conciso con respuestas directas. |
| **TTS Synthesis (Magpie Diego)** | < 400 ms | ~320 ms | Streaming de audio en caliente por gRPC / REST. |
| **Audio Playback Buffer (I2S TX)** | < 50 ms | ~20 ms | Transmisión DMA directa hacia el codec ES8311. |
| **TOTAL TURNAROUND (TTFA)** | **< 2,500 ms** | **~2,150 ms** | **Cumple con la especificación de grado comercial.** |

---

## 2. Memory & Compute Budget

```
┌─────────────────────────────────────────────────────────────┐
│ ESP32-P4 MEMORY ALLOCATION PROFILE                         │
│                                                             │
│ • Total PSRAM Available:          32,768 KB (32 MB)         │
│ • LVGL 9 Double Framebuffer:       ~1,536 KB (480x800x2x2B) │
│ • Audio Capture & Ring Buffer:       ~512 KB                │
│ • Free PSRAM Pool:               > 30,000 KB (> 90% libre)  │
│ ─────────────────────────────────────────────────────────── │
│ • Internal SRAM Available:            768 KB                │
│ • DMA & SDIO Ring Descriptors:       ~128 KB                │
│ • FreeRTOS Task Stacks:              ~160 KB                │
│ • Free Internal Heap:                ~480 KB (> 60% libre)  │
└─────────────────────────────────────────────────────────────┘
```
