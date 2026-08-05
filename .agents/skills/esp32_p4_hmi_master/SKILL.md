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
*   **Hardware & FreeRTOS**
    - Arquitectura: Maximizar el uso de los núcleos del ESP32-P4. Aislar las tareas de red (WiFi/MQTT) de las tareas de procesamiento en tiempo real (Audio I2S/DSP).
    - Gestión de Memoria: Control estricto del Heap y PSRAM. Evitar la fragmentación en la asignación de buffers de audio.
    - Concurrencia: Utilizar Mutex y Semáforos para proteger variables compartidas. Evitar deadlocks y starvation asignando prioridades correctas a las tareas críticas (Watchdog siempre activo).
    
*   **LVGL & UI (User Experience)**
    - Animación y Transiciones: Implementar transiciones suaves (Fade, Slide, Scale) para cambios de pantalla. Evitar el parpadeo (Flickering) usando double buffering y delays precisos.
    - Feedback Háptico y Auditivo: Integrar el driver I2S para emitir sonidos de confirmación (Beep) o vibraciones (vibration_driver) al interactuar con botones táctiles.
    - Gestión de Energía: Implementar Sleep/Wake modes eficientes. Apagar la pantalla o reducir el backlight cuando no esté en uso.
    - Localización: Preparar la UI para multilingüismo (i18n). Usar tablas de strings que puedan ser actualizadas por OTA o configuradas en tiempo real.
    - Accesibilidad: Incluir modo de alto contraste y tamaño de fuente ajustable para cumplir normativas de accesibilidad industrial (ISO 9241-303).

*   **Redes y Conectividad (AI Services Integration)**
    - Manejo de Errores de Red: Implementar protocolos de reconexión automática (Retry Logic) con backoff exponencial para WiFi y MQTT.
    - Buffer de Mensajes: Utilizar colas (Queues) para almacenar mensajes MQTT cuando la red no está disponible, asegurando la persistencia de datos.
    - Seguridad: Configuración segura de TLS/SSL para la API de Google/AWS. Implementar Certificate Pinning y gestión segura de claves (KeyStore).
    - Latencia y Performance: Optimizar las peticiones HTTP/S. Usar Keep-Alive y pooling de conexiones. Comprimir datos (Gzip) cuando sea posible para ahorrar ancho de banda.

*   **Audio & Voz (Voice AI)**
    - Procesamiento en Tiempo Real: Implementar el pipeline de audio usando streams continuos en lugar de archivos (No Write-to-SD-Card-First). Esto reduce la latencia drásticamente.
    - Wake Word Detection: Utilizar el co-procesador o librerías ligeras (como ESP-Skainet) para detectar palabras de activación sin saturar el CPU principal.
    - Ecualización (EQ) y Volumen: Implementar filtros IIR y controladores de volumen digital para compensar la acústica de la caja y mejorar la inteligibilidad del habla (Speech Intelligibility).
    - Formatos de Audio: Soporte para codecs eficientes (Opus, AAC) para streaming y formato sin comprimir (PCM/WAV) para grabaciones locales.

* **Diseño de Interfaz LVGL**
- Rendimiento: Mantener 60 FPS
utilizando Double Buffering y DMA para la transferencia de píxeles al LCD.
- Estados Visuales: Mapear claramente los estados (Idle, Listening, Thinking, Speaking, Offline).
- Nomenclatura Estricta UI: Para todas las pantallas, menús y opciones del sistema, utiliza invariablemente la palabra "configuración". Está estrictamente prohibido utilizar el término "ajustes" en cualquier elemento de la interfaz gráfica o en el código que la renderiza.