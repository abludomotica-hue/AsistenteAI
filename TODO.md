# TODO — AsistenteAI (Edge AI Voice Assistant)

**Generado:** 3 de Agosto de 2026
**Fuente:** Análisis completo del proyecto (firmware, bridge, documentación, vendor kit)
**Estado actual:** INTERMEDIO — Pipeline E2E funcional (hito 28/Jul/2026)

---

## 🔴 CRITICAL

- [x] **C1.** ~~Sacar secrets del código~~ ✅ (3/Ago): firmware → `config.h` (gitignored),
      bridge/deploy → `bridge.env` (gitignored), plantillas `*.example` commiteadas.
      **PENDIENTE DEL USUARIO:** rotar las keys en build.nvidia.com (estuvieron expuestas),
      password SSH de la VM Debian y, si se desea, el password WiFi; luego actualizar
      `config.h`, `bridge.env` y re-desplegar el bridge.
- [x] **C2.** ~~Eliminar `API_KEY_ASR` y `API_KEY_TTS` del firmware~~ ✅ (3/Ago, commit base).
- [x] **C3.** ~~Inicializar repo git con commit base limpio~~ ✅ (3/Ago, commit `123085f`,
      verificado: cero secrets en el historial).

## 🟠 HIGH

- [x] **H1.** ~~Corregir `pinout.md` del skill~~ ✅ (4/Ago): reescrito con fuentes verificadas
      ([FW]/[XZ]/[VD]/[SC]). Corregido también el ejemplo `FreeRTOS_Audio_Manager.cpp`
      (I2S 12/10/9/48 + MCLK 13). Hallazgos extra documentados: SD en GPIO 39-44 (+power
      GPIO45), UART0=37/38, cámara MIPI CSI-2 (no DVP), RS485 TX=26 con RX/EN por verificar,
      conflicto GPIO26 (LED vs RS485 TX).
- [x] **H2.** ~~Migrar `/llm` al Bridge~~ ✅ (5/Ago, commit verificado en producción v2.2):
      - Endpoint `/llm` en `bridge_server.py` y despliegue por Waitress hacia Nemotron-3.
      - Historial de conversación + system prompt + `LLM_API_KEY` en el Bridge.
      - Firmware operando E2E con voz real sin fallos y con respuestas inteligentes completas.
- [x] **H3.** ~~Robustez firmware~~ ✅ (5/Ago): Implementado arranque sin bloqueo indefinido en `setupWiFi()` y monitor serie (`while (!Serial)` con timeout de 3s en H3), auto-reconexión WiFi activa en segundo plano con escaneo y diagnóstico de señal por canal al arranque.
- [x] **H4.** ~~Robustez bridge~~ ✅ (5/Ago): Servidor de producción desplegado en Proxmox Debian sobre el servidor HTTP industrial **Waitress** de 8 hilos (`v2.2`), tolerante a alta simultaneidad e integrando autenticación por token en los endpoints `/stt`, `/tts`, `/llm`.
- [ ] **H5.** `requirements.txt` del bridge + documentación de despliegue reproducible.

## 🟡 MEDIUM

- [x] **M1.** ~~Buffers de audio estáticos en PSRAM~~ ✅ (4/Ago): Asignación estática de ~1.9 MB al arranque en `setupAudio()` (`psramStereoBuffer`, `psramMonoBuffer`, `psramPayloadBuffer`), eliminando por completo los ciclos `malloc`/`free` por interacción y previniendo la fragmentación de PSRAM (Mitigación RISK-001).
- [ ] **M2.** VAD: timeout inicial si nadie habla (ej. 5 s; hoy graba los 15 s completos) y
      medición real de ms por chunk en vez de asumir 100 ms fijos.
- [x] **M3.** ~~Debounce del disparador '1' y drenado de la queue~~ ✅ (4/Ago): Implementada protección de concurrencia inter-núcleos mediante `std::atomic<bool>` para el barge-in (Mitigación RISK-003), temporizador de debounce por software de 300 ms en `loop()`, y drenado de cola con `xQueueReset` en `audioTask` al arrancar el pipeline para impedir cascadas por inundación de eventos (Mitigación RISK-004).
- [x] **M4.** ~~Instrumentar latencia por etapa~~ ✅ (4/Ago): Implementado sistema de telemetría sin consumo de heap (`PipelineMetrics` / semilla de *Diagnostics Service*) con reporte en milisegundos para Captura/Downmix, STT Parakeet, LLM Nemotron, y Time-to-First-Audio (TTFA) de Magpie TTS, resolviendo el prerrequisito para atacar el objetivo < 3s (Mitigación RISK-002).
- [ ] **M5.** Corregir bug en `deploy_bridge_v2.py:157` (`f.read()` llamado dos veces; la
      segunda devuelve vacío) y eliminar `existing_env` muerto.
- [ ] **M6.** Actualizar `DOCUMENT/project_status.md` al estado real (la refactorización
      FreeRTOS ya está hecha).

## 🟢 LOW

- [ ] **L1.** Mover diagnósticos de boot (I2C scanner, `ES8311_DumpRegs()`) detrás de un flag
      `DEBUG` (AVANCE los marca como eliminados pero siguen presentes).
- [ ] **L2.** Eliminar código muerto restante: `CMD_IDLE` sin uso, `i2s.read()` suelto
      (AsistenteAI.ino:188), simplificar downmix `(L != 0) ? L : R`.
- [ ] **L3.** Documentar decisiones ya tomadas como ADRs:
      - ADR-001: HTTP/1.0 forzado en TTS para evitar corrupción por chunked encoding.
      - ADR-002: HPF del ADC (regs 0x1B/0x1C) obligatorio para eliminar DC del Mic Bias.
      - ADR-003: Bridge gRPC (el ESP32 no puede pagar TLS+gRPC; NVIDIA no ofrece REST para ASR/TTS).

---

## ⬜ FASE 3 — Próximo hito (tras estabilizar Critical/High)

- [ ] **F3.1** UI LVGL 9 en Core 1 (MIPI DSI 480×800, double buffer en PSRAM, 60 FPS,
      mutex `xGuiSemaphore` para llamadas LVGL desde otras tasks).
      Referencias: `JC4880P443C_I_W/1-Demo/arduino_examples/lvgl_v9_sw_rotation` (LVGL 9.2.2,
      arduino-esp32 3.2.1) y skill `esp32_p4_hmi_master/examples/LVGL_UI_Manager.cpp`.
- [ ] **F3.2** Wake Word "Oye Asistente" con ESP-SR offline (ESP-ADF).
- [ ] **F3.3** Integración Home Assistant vía Bridge (MQTT/WebSocket — nunca directa desde ESP32).

## ⬜ FASE 4 — Producción

- [ ] **F4.1** Migración Arduino → ESP-IDF puro (referencia: xiaozhi-esp32 incluido en el kit).
- [ ] **F4.2** OTA.
- [ ] **F4.3** Pruebas de estabilidad 24 h.

---

## Reglas de ejecución

1. Nunca eliminar código funcionando sin verificación.
2. Cada tarea = un commit independiente (tras C3).
3. Medir antes de optimizar (M4 habilita las decisiones de latencia).
4. Respetar `AVANCE_DEL_PROYECTO.md` como fuente de verdad de decisiones ya tomadas.
