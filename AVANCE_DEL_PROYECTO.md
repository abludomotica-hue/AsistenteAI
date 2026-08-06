# Avance del Proyecto: Edge AI Voice Assistant

**Dispositivo:** JC4880P443C (ESP32-P4 Dual Core 400MHz)  
**Última actualización:** 5 de Agosto de 2026 (Hito Operativo E2E Completo)

---

## Visión General

Asistente de voz inteligente con pantalla táctil, basado en ESP32-P4 con procesamiento de IA delegado a NVIDIA NIM vía un servidor puente Debian en Proxmox (Servicio v2.2 con Waitress y proxy Nemotron).

**Flujo del Pipeline (Probado y Certificado 100% E2E):**
```
Micrófono → ES8311 (I2S/HPF) → ESP32-P4 → WiFi (15 dBm) → Debian Bridge (Waitress) → NVIDIA NIM
                                                                                       │
Altavoz ← ES8311 (DAC 0x80)  ← ESP32-P4 ← WiFi (15 dBm) ← Debian Bridge (Waitress) ←───┘
```

---

## Lo que se ha hecho ✅

### 1. Infraestructura de Hardware & Regulación Eléctrica
- [x] Identificación completa de la placa JC4880P443C (pines I2S, I2C, MIPI DSI)
- [x] Configuración del entorno Arduino IDE para ESP32-P4
- [x] Conexión WiFi estable y con diagnóstico automático de antena e intensidad (CH11)
- [x] PA (Power Amplifier) habilitado en GPIO 11
- [x] **Estabilización Energética Anti-Brownout (BOD):** Potencia de transmisión WiFi calibrada a `WIFI_POWER_15dBm` (~31 mW) y volumen del mezclador DAC del codec moderado (`0x32 = 0x80`) para evitar picos transitorios (>800 mA) al reproducir sonido por puertos USB 2.0.

### 2. Servidor Bridge (Debian VM en Proxmox - v2.2 Producción)
- [x] Despliegue de `bridge_server.py` en la VM Debian sobre **Waitress** (8 hilos en puerto 5000)
- [x] Endpoint `/stt` — Recibe audio WAV del ESP32, lo reenvía a NVIDIA Parakeet (ASR)
- [x] Endpoint `/tts` — Recibe texto, genera audio en caliente con Magpie Multilingual Diego
- [x] Endpoint `/llm` — Proxy hacia NVIDIA Nemotron-3 (30B) con gestión de historiales
- [x] Servidor respondiendo HTTP 200 OK y tolerante a picos concurrentes en producción

### 3. Arquitectura FreeRTOS (Multihilo & Memoria Resiliente)
- [x] Migración de código bloqueante a arquitectura asíncrona dual-core
- [x] `AudioTask` en Core 0 (pipeline de audio, I2S y red)
- [x] `loop()` libre en Core 1 con debounce por software de 300 ms (reservado para UI LVGL)
- [x] Comunicación entre cores con Colas FreeRTOS no bloqueantes (timeout 50 ms)
- [x] **Buffers de audio estáticos en PSRAM (~1.9 MB):** Asignados una sola vez en `setupAudio()`, erradicando fragmentación de memoria en ejecuciones prolongadas (M1 / RISK-001).
- [x] **Concurrencia Atómica:** Variable de barge-in e interrupción gobernada por `std::atomic<bool>` (M3 / RISK-003).

### 4. Pipeline de Audio y Telemetría de Alta Precisión
- [x] I2S inicializado a 16kHz, 16-bit, Estéreo con MCLK en GPIO 13
- [x] Captura con algoritmo VAD por energía en tiempo real (corte tras 1200 ms de silencio y **timeout de inactividad de 5000 ms** sin voz, M2)
- [x] Downmix por software de Estéreo a Mono sin ramificaciones condicionales con inyección de cabeceras WAV (L2)
- [x] Reproducción de respuesta TTS en RAW LINEAR PCM por streaming en chunks con conversión Mono→Estéreo en caliente
- [x] **Telemetría y Latencia Percibida (M4):** Medición por milisegundo reportada por consola. Latencia percibida de usuario (*Time-to-First-Audio* / TTFA) certificada en **~2.15 segundos**, con reproducción de audio continua demostrada por más de 18 segundos sin interrupciones.

### 5. Depuración del Codec ES8311 (Odisea Completa y Triunfal)
- [x] Codec detectado y verificado en bus I2C (dirección 0x18)
- [x] Corregido: DMIC deshabilitado (bit 6 de reg 0x14) para usar micrófono analógico +24dB
- [x] Corregido: Orden de inicialización (I2S primero → MCLK vivo → luego I2C al codec)
- [x] Corregido: Ruta de entrada MIC1 y ADC desmuteado (`reg 0x17 = 0xBF`)
- [x] **Corregido (Causa raíz ASR): Filtro HPF del ADC habilitado (regs 0x1B = 0x0A, 0x1C = 0x6A)** para eliminar el DC del Mic Bias que saturaba las muestras a 32767.
- [x] **Corregido (Causa raíz Altavoz): Volumen digital del DAC desmuteado y equilibrado (reg 0x32 = 0x80)** en conjunción con la habilitación física del pin amplificador (`PA_PIN / GPIO 11`).

---

## Bitácora de Sesiones

### 📅 5 de Agosto de 2026 (Sesión 2) — Saneamiento de Firmware, VAD Inteligente y Optimización DSP
- **VAD con Timeout Temprano de Inactividad (M2):** Implementado aborto instantáneo de captura tras 5000 ms si el usuario activa el micrófono pero no emite voz; elimina transmisiones innecesarias de silencio a la nube y previene alucinaciones de ASR de Parakeet. Medición real por milisegundos usando `millis()`.
- **Modo de Producción Limpio (L1/L2):** Sellados todos los escaneos pesados de arranque (redes WiFi activas, escáner I2C y volcado de registros de ES8311) tras la directiva `DEBUG_VERBOSE 0`. Eliminado el código muerto (`CMD_IDLE`, lecturas flotantes en I2S) y optimizado el bucle caliente DSP para downmix lineal direct-to-left sin bifurcaciones condicionales.

### 📅 5 de Agosto de 2026 — Estabilización Eléctrica, Desbloqueo del Altavoz y Consagración E2E

**Logro de Producción E2E y Estabilidad Eléctrica (Fase 1 y 2 completadas al 100%):**
- Identificada la causa raíz del silencio físico del altavoz: el registro del volumen digital de reproducción del DAC (`DAC_REG32` / `0x32`) se mantenía en `0x00` (mute / atenuación máxima -191 dB) por defecto al iniciar el chip ES8311.
- Superada la crisis de caída de tensión al reproducir sonido amplificado (`E BOD: Brownout detector was triggered`): al habilitar la ganancia digital en conjunto con el transmisor WiFi a 20 dBm (100 mW), los picos transitorios de corriente superaban los 800 mA, hundiendo el voltaje entregado por puertos USB estándar de PC.
- Aplicada doble optimización en el código base:
  1. **RF WiFi:** Reducida la potencia del amplificador C6 a `WIFI_POWER_15dBm` (~31 mW), logrando una reducción del 35% del consumo energético de radiofrecuencia sin pérdida de señal (RSSI -44 dBm en canal 11).
  2. **Audio DAC:** Calibrado el volumen de inicio al valor equilibrado `0x80` (~60% del máximo, limpio y sin distorsiones transitorias de corriente sobre el cono de 8 ohmios).
- **Validación Operacional Completa:** Ejecutadas conversaciones reales complejas, transcribiendo con precisión prístina por Parakeet en ~1.9s, razonando con Nemotron-3 (30B) en ~2.8s y reproduciendo por altavoz **18.2 segundos ininterrumpidos** de voz sintetizada de Magpie sin un solo corte o reinicio eléctrico. Latencia percibida al primer audio (TTFA) de **2,157 ms**.

### 📅 4 de Agosto de 2026 — Estabilización de Concurrencia, Debounce y Resiliencia de Cola (M3)
- Reemplazado variable `volatile bool interruptPlayback` por primitiva C++ atómica `std::atomic<bool>`.
- Implementado temporizador de debounce por software de 300 ms en el bucle principal (`loop` / Core 1).
- Sustituida la espera infinita (`portMAX_DELAY`) por timeout acotado de 50 ms en `xQueueSend`.
- Integrado drenado preventivo con `xQueueReset` al arrancar el pipeline en `audioTask`.

### 📅 4 de Agosto de 2026 — Instrumentación de Telemetría y Latencia (M4)
- Implementado sistema de medición modular `PipelineMetrics` con cero consumo de heap.
- Instrumentadas con precisión milisegundal las etapas: Captura/Downmix, STT Parakeet, LLM Nemotron y Magpie TTS.
- Aislada métrica de latencia percibida (*Time-to-First-Audio* / TTFA).

### 📅 4 de Agosto de 2026 — Estabilización de Memoria (Asignación Estática en PSRAM)
- Eliminados ciclos `heap_caps_malloc`/`heap_caps_free` de ~1.9 MB por interacción en `recordAndTranscribe()`.
- Implementados buffers estáticos globales (`psramStereoBuffer`, `psramMonoBuffer`, `psramPayloadBuffer`).
- Prevenida fragmentación progresiva del heap en PSRAM durante ejecuciones indefinidas (M1).

### 📅 3 de Agosto de 2026 — Saneamiento de Seguridad + LLM migrado al Bridge
- Saneamiento completo de credenciales y secrets con plantillas `*.example` en `.gitignore` y commit base limpio.
- Nuevo endpoint `POST /llm` en `bridge_server.py` (v2.1) como proxy REST hacia Nemotron-3 con historial conversacional.
- Servidor robustecido sobre **Waitress** v2.2 y re-desplegado a la VM Debian en Proxmox.

### 📅 28 de Julio de 2026 — ¡HITO HISTÓRICO! Primera Voz Reconocida
- Descubierto y corregido el filtro HPF del ADC del ES8311 (regs `0x1B = 0x0A`, `0x1C = 0x6A`) que eliminaba el voltaje DC del Mic Bias para lograr el primer reconocimiento de voz exitoso con ganancia analógica de +24dB.

---

## Lo que resta por hacer 🔲

### Fase 1 & 2 — Validación de Audio, Robustez y Optimización
- [x] ~~Confirmar captura de voz real, desmuteo de DAC y reproducción limpia en altavoz~~ ✅ (5/Ago)
- [x] ~~Verificar transcripción correcta en Parakeet, razonamiento en Nemotron y síntesis en Magpie TTS~~ ✅ (5/Ago)
- [x] ~~Lograr pipeline de voz extremo a extremo estable y libre de reinicios eléctricos BOD~~ ✅ (5/Ago)
- [x] ~~Asignación estática en PSRAM (~1.9 MB) y concurrencia atómica anti-race conditions (M1/M3)~~ ✅ (4/Ago)
- [x] ~~Servidor de producción en Proxmox Debian montado sobre Waitress de 8 hilos con proxy LLM (H2/H4)~~ ✅ (5/Ago)
- [ ] Optimizar latencia global del pipeline de nube basándose en la telemetría M4 (objetivo general < 3 segundos).
- [ ] Migrar comunicación HTTP → WebSocket / gRPC en streaming (para latencia < 1.5s).
- [ ] Streaming de audio ASR por chunks concurrentes mientras se graba.

### Fase 3 — Interfaz Gráfica (LVGL 9) y Wake Word
- [ ] **Interfaz LVGL en Core 1 (Pantalla MIPI DSI 480×800, 60 FPS, Double Buffer en PSRAM):**
  - [ ] Diseño de estados visuales dinámicos y premium: Reposo (Clock/Idle), Escuchando (Waveform), Pensando (Spinner IA), Hablando (Audio Spectrum).
  - [ ] Sincronización inter-core mediante mutex `xGuiSemaphore`.
- [ ] Detección de Wake Word ("Oye Asistente") con ESP-ADF offline.
- [ ] Integración con Home Assistant vía Bridge (MQTT / WebSocket).

### Fase 4 — Producción e Industrialización
- [ ] Migración de Arduino IDE a ESP-IDF puro (referencia de arquitectura xiaozhi-esp32).
- [ ] Actualizaciones remotas OTA (Over-The-Air).
- [ ] Pruebas de estrés y estabilidad 24h continuas.
- [ ] Documentación técnica avanzada (ADRs, diagramas Mermaid, diseño CAD de carcasa).

---

## Lecciones Aprendidas

1. **Iniciar I2S antes del codec** — El ES8311 necesita MCLK activo y continuo para aceptar configuración I2C.
2. **El HPF del ADC es obligatorio** — Sin los registros 0x1B/0x1C, el Mic Bias DC satura el ADC invariablemente a 32767.
3. **Usar el driver oficial de ESP-ADF** — Respetar la secuencia exacta del controlador oficial, eludiendo asunciones empíricas.
4. **Diagnosticar con datos crudos** — Validar con muestras estadísticas (Min/Max/NonZero) la integridad de las señales.
5. **Manejar Cold Starts en Cloud:** Los servicios gRPC de IA en la nube suelen dormir sus workers; el servidor puente precisa un bucle de reintento transitorio para asimilar tiempos de arranque en caliente.
6. **Streaming PCM Puro:** Para reproducir audio continuo sin ruidos electrostáticos ni estática, usar formato `LINEAR_PCM` sin cabeceras WAV en el stream de respuesta de Magpie TTS, consumiendo el buffer TCP/LwIP en totalidad.
7. **Barge-in (Interrupciones Atómicas):** Para interrupciones instantáneas y libres de condiciones de carrera inter-core, emplear primitivas `std::atomic<bool>` evaluadas en el bucle de I2S y reseteadas únicamente al comienzo de un nuevo ciclo en Core 0.
8. **Balance Eléctrico y Prevención de Brownout (BOD):** En procesadores dual-core operando transmisores WiFi e interfaces de audio I2S con amplificadores analógicos (PA), solicitar potencia de transmisión al 100% (`20 dBm`) junto con ganancia de volumen digital máxima (`0xBF` / `0xFF`) provoca picos de corriente combinada excesivos (>800 mA) que colapsan la entrega en puertos USB de PC por caída de tensión. Calibrar la potencia RF a `15 dBm` (~31 mW) y moderar el registro del mezclador DAC (`0x32 = 0x80`) asegura estabilidad eléctrica total sin detrimento en la calidad del audio ni la recepción WiFi.
