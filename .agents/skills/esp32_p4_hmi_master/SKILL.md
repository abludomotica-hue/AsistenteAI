---
name: esp32_p4_hmi_master
description: Best practices and architecture guidelines for programming the ESP32-P4 JC4880P443C HMI device with LVGL, Audio, and FreeRTOS.
---

# ESP32-P4 (JC4880P443C) Development Guidelines

This skill provides the architectural foundation for writing non-blocking, high-performance code on the ESP32-P4 JC4880P443C HMI device. When asked to implement new features for this board, strictly adhere to these rules.

## 1. Core Architecture (FreeRTOS)
The ESP32-P4 has a powerful dual-core RISC-V processor. To prevent UI freezes and audio stuttering, you MUST use FreeRTOS tasks to divide workloads:

*   **Core 1 (App Core):** Dedicated exclusively to the LVGL Graphical User Interface (UI). The LVGL timer task `lv_timer_handler()` must run here in an infinite loop with a short delay (`vTaskDelay(pdMS_TO_TICKS(5))`).
*   **Core 0 (Pro Core):** Dedicated to heavy background tasks: Wi-Fi/Networking, Audio processing (I2S STT/TTS), and Industrial protocols (RS485/Modbus).

## 2. Memory Management (PSRAM)
The device has 32MB of high-speed PSRAM. 
*   **NEVER** allocate large audio buffers (e.g., > 10KB) or image buffers on the internal stack or standard heap.
*   **ALWAYS** use `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)` for large arrays, audio payloads, and HTTP multipart buffers.

## 3. Concurrency and Thread Safety
*   When a background task on Core 0 needs to update a UI element (e.g., changing a label text to show a transcription), it **MUST NOT** call LVGL functions directly. 
*   **LVGL is NOT thread-safe by default.** You must wrap any LVGL function call originating from outside the LVGL task with a Mutex (e.g., `xSemaphoreTake(xGuiSemaphore, portMAX_DELAY); ... xSemaphoreGive(xGuiSemaphore);`).

## 4. Hardware Interfaces
Consult the `resources/pinout.md` file in this skill directory for the exact GPIO mappings before configuring any peripheral (I2C, I2S, RS485, Camera).

## 5. Development Environment & ESP-IDF Native Support
This device is targeted for industrial "Alexa-like" smart displays. While rapid prototyping can be done in Arduino IDE, the final architecture MUST support **Native ESP-IDF**:
*   **Component-based Architecture:** In ESP-IDF, separate LVGL, Audio, and Network logic into distinct CMake components.
*   **Audio Pipeline (ESP-ADF):** For advanced voice assistant features (like Wake-Word detection or Echo Cancellation), rely on the native Espressif Audio Development Framework (ESP-ADF) APIs (`audio_pipeline`, `i2s_stream`) instead of basic Arduino I2S wrappers.
*   **FreeRTOS Natives:** Always use native FreeRTOS APIs (`xTaskCreatePinnedToCore`, `xQueue`, `xSemaphore`) which are compatible across both Arduino-ESP32 and pure ESP-IDF environments.
