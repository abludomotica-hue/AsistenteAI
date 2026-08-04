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

- [ ] **H1.** Corregir `.agents/skills/esp32_p4_hmi_master/resources/pinout.md` con el pinout
      verificado (fuente: board `guition-jc4880p443` de xiaozhi-esp32 + firmware funcionando):
      I2S MCLK/BCLK/LRCK/DOUT/DIN = 13/12/10/9/48, I2C SDA/SCL = 7/8 (bus compartido codec+touch),
      PA = 11, Display MIPI-DSI RST=5 BL=23, Touch GT911 RST=22 INT=21, Boot=35, LED=26.
- [ ] **H2.** Migrar `/llm` al Bridge (DECIDIDO: el ESP32 no debe hablar directo con NVIDIA):
      - Nuevo endpoint `/llm` en `bridge_server.py` (proxy REST hacia Nemotron).
      - Historial de conversación + system prompt gestionados en el Bridge.
      - Firmware: cambiar `getLLMResponse()` a `http://192.168.1.58:5000/llm` y eliminar
        `API_KEY_LLM` del dispositivo.
- [ ] **H3.** Robustez firmware: timeout en `setupWiFi()` (hoy bloquea infinito), reconexión
      WiFi automática por eventos, reintentos con backoff en STT/TTS/LLM, eliminar el
      `while (!Serial)` bloqueante del arranque (AsistenteAI.ino:86).
- [ ] **H4.** Robustez bridge: servir con waitress/gunicorn (no Flask dev server), retry de
      cold-start en `/stt` (hoy solo `/tts` lo tiene), timeouts en llamadas gRPC,
      autenticación simple por token en los endpoints.
- [ ] **H5.** `requirements.txt` del bridge + documentación de despliegue reproducible.

## 🟡 MEDIUM

- [ ] **M1.** Buffers de audio estáticos en PSRAM (una sola asignación en el arranque de
      `audioTask`) en lugar de malloc/free por interacción (~1.9 MB/ciclo → fragmentación).
- [ ] **M2.** VAD: timeout inicial si nadie habla (ej. 5 s; hoy graba los 15 s completos) y
      medición real de ms por chunk en vez de asumir 100 ms fijos.
- [ ] **M3.** Debounce del disparador '1' y drenado de la queue tras barge-in (queue prof. 5
      permite encolar pipelines consecutivos).
- [ ] **M4.** Instrumentar latencia por etapa (grabar/subir/STT/LLM/primer-chunk-TTS) para
      atacar el objetivo < 3 s (hoy estimado 5–15 s).
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
