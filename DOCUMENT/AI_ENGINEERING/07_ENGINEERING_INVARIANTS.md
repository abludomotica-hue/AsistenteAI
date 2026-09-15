# 07. ENGINEERING INVARIANTS & HARD RULES
## Non-Negotiable Architectural Constraints

---

## 1. System Invariants

Los siguientes invariantes de ingeniería son **leyes del sistema**. Ningún agente o desarrollador puede romperlos sin un ADR formal, justificación explícita y pruebas rigurosas:

### 🎙️ Audio Pipeline Invariants
1. **Sampling & Format:** Audio stream must be strictly **16 kHz, 16-bit, Mono PCM**.
2. **Buffer Allocation:** Audio capture and streaming buffers must be allocated in **PSRAM** (`MALLOC_CAP_SPIRAM`), never on internal task stack.
3. **Minimum Chunk Size:** Streaming audio chunk size over SDIO must be $\ge$ **4096 bytes** (Standard: **8192 bytes**) to prevent SDIO queue saturation.
4. **Codec Initialization Order:** MCLK clock must be running on GPIO 13 before issuing I2C configuration commands to the ES8311 chip.
5. **Amplifier Control:** Power amplifier NS4168 is controlled exclusively via **GPIO 11** (HIGH = Enabled).

---

### 🧵 FreeRTOS & Concurrency Invariants
6. **Core 1 Ownership:** Core 1 is reserved **exclusively** for the LVGL 9 HMI and touch polling.
7. **Core 0 Ownership:** Core 0 owns all background operations: I2S DMA, DSP, SDIO transport, WiFi remote, HTTP client, and state machine.
8. **Thread Safety:** LVGL functions must **never** be invoked directly from Core 0. Inter-core communication must use FreeRTOS queues (`audioCommandQueue`) or atomic variables.

---

### ⚡ Power & RF Invariants
9. **WiFi Output Power:** WiFi TX power must remain calibrated at **15 dBm** (~31 mW) to prevent brownout resets during USB 2.0 power operation.
10. **DAC Volume Limit:** ES8311 DAC digital volume register (`0x32`) must not exceed **0x85** under bus-powered operation.

---

### 🖥️ Display & UI Invariants
11. **Software Rotation:** ST7701S MIPI DSI panel orientation is handled in software by LVGL 9; hardware register `swap_xy` calls are forbidden.
12. **UI Nomenclature:** In all UI screens, menus, and labels, the word **"configuración"** must be used invariably. The term *"ajustes"* is strictly prohibited.

---

### 🌐 Network & Gateway Invariants
13. **Chunked HTTP:** Streaming audio upload must use native chunked encoding with `esp_http_client_open(client, -1)`. Passing negative lengths to `esp_http_client_set_post_field` is forbidden.
14. **Payload Contract:** The `/v1/conversation_stream` endpoint ingests raw binary octet-streams via `request.get_data()`.
