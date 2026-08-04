# Avance del Proyecto: Edge AI Voice Assistant

**Dispositivo:** JC4880P443C (ESP32-P4 Dual Core 400MHz)  
**Última actualización:** 28 de Julio de 2026

---

## Visión General

Asistente de voz inteligente con pantalla táctil, basado en ESP32-P4 con procesamiento de IA delegado a NVIDIA NIM vía un servidor puente Debian en Proxmox.

**Flujo del Pipeline:**
```
Micrófono → ES8311 (I2S) → ESP32-P4 → WiFi → Debian Bridge → NVIDIA NIM
                                                                    │
Altavoz ← ES8311 (DAC) ← ESP32-P4 ← WiFi ← Debian Bridge ←────────┘
```

---

## Lo que se ha hecho ✅

### 1. Infraestructura de Hardware
- [x] Identificación completa de la placa JC4880P443C (pines I2S, I2C, MIPI DSI)
- [x] Configuración del entorno Arduino IDE para ESP32-P4
- [x] Conexión WiFi estable a red local
- [x] PA (Power Amplifier) habilitado en GPIO 11

### 2. Servidor Bridge (Debian VM en Proxmox)
- [x] Despliegue de `bridge_server.py` en la VM Debian (IP: 192.168.1.58:5000)
- [x] Endpoint `/stt` — Recibe audio WAV del ESP32, lo reenvía a NVIDIA Parakeet (ASR)
- [x] Endpoint `/tts` — Recibe texto, genera audio con NVIDIA FastPitch
- [x] Endpoint `/llm` — Proxy hacia NVIDIA Nemotron-3 (30B)
- [x] Servidor respondiendo HTTP 200 OK correctamente

### 3. Arquitectura FreeRTOS (Multihilo)
- [x] Migración de código bloqueante a arquitectura asíncrona
- [x] `AudioTask` en Core 0 (pipeline de audio completo)
- [x] `loop()` libre en Core 1 (reservado para futura UI LVGL)
- [x] Comunicación entre cores con Colas FreeRTOS (`xQueueSend`/`xQueueReceive`)
- [x] Buffers de audio asignados en PSRAM (`heap_caps_malloc`)

### 4. Pipeline de Audio (I2S)
- [x] I2S inicializado a 16kHz, 16-bit, Estéreo con MCLK en GPIO 13
- [x] Captura de 4 segundos de audio estéreo directo a PSRAM
- [x] Downmix por software de Estéreo a Mono
- [x] Generación de cabecera WAV y envío HTTP multipart al bridge
- [x] Reproducción de respuesta TTS (conversión Mono→Estéreo en tiempo real)

### 5. Depuración del Codec ES8311 (Odisea Completa)
- [x] Codec detectado en bus I2C (dirección 0x18)
- [x] Corregido: DMIC deshabilitado (bit 6 de reg 0x14) para usar micrófono analógico
- [x] Corregido: Orden de inicialización (I2S primero → MCLK vivo → luego I2C al codec)
- [x] Corregido: Ruta de entrada MIC1 seleccionada (reg 0x0A)
- [x] Corregido: ADC desmuteado (reg 0x17 = 0xBF)
- [x] Corregido: Divisores de reloj BCLK/LRCK configurados (regs 0x06, 0x07, 0x08)
- [x] **Corregido (causa raíz): Filtro HPF del ADC habilitado (regs 0x1B = 0x0A, 0x1C = 0x6A)**
  - Sin HPF, el voltaje DC del Mic Bias saturaba el ADC → todas las muestras = 32767
  - Con HPF, el DC se filtra y el ADC lee señal real
- [x] Función `ES8311_DumpRegs()` añadida para leer y verificar todos los registros
- [x] Función `es8311_read_reg()` implementada para diagnóstico I2C
- [x] Diagnóstico de buffer crudo añadido (imprime primeros 20 samples + estadísticas Min/Max/NonZero)

---

## Bitácora de Sesiones

### 📅 28 de Julio de 2026 — ¡HITO HISTÓRICO! Primera Voz Reconocida

**Duración:** ~4 horas de depuración intensiva del codec ES8311.

**Problema inicial:** El pipeline se ejecutaba completo (Grabar → Enviar → Recibir), pero el servidor STT siempre devolvía texto vacío ("SILENCIO DETECTADO").

**Proceso de depuración:**
1. Se añadió diagnóstico de buffer crudo para inspeccionar las muestras I2S.
2. Se descubrió que **todas las 128,000 muestras eran exactamente 32767** (0x7FFF) — el ADC estaba completamente saturado.
3. Se localizó el driver oficial de ESP-ADF en el disco local (`Arduino/libraries/audiokit/src/audio_driver/es8311/es8311.c`).
4. Se comparó registro por registro con nuestro código y se encontraron **3 registros faltantes críticos:**
   - `0x13 = 0x10` — Configuración del sistema
   - `0x1B = 0x0A` — **Filtro HPF Stage 1** (eliminación de DC)
   - `0x1C = 0x6A` — **Filtro HPF Stage 2** (eliminación de DC)
5. Se confirmó con ganancia 0dB que el HPF eliminaba la saturación (Min=-4, Max=4).
6. Se restauró la ganancia a +24dB y se logró captura de audio real.

**Resultado final (03:46 AM):**
```
>>> STATS Mono: Min=-3466, Max=3279, NonZero=62908/64000
[Core 0] Usuario dijo: Hola, cómo¿ estás?
[Core 0] Asistente responde: ¡Hola! Estoy bien, gracias. ¿Y tú? ¿En qué puedo ayudarte hoy?
```

**Estado del pipeline al cierre de sesión:**

| Etapa | Estado | Detalle |
|-------|--------|---------|
| 🎤 Captura (I2S + ES8311) | ✅ Funciona | Min=-3466, Max=3279 |
| 🗣️ STT (Parakeet) | ✅ Funciona | Transcribió "Hola, cómo estás" |
| 🧠 LLM (Nemotron) | ✅ Funciona | Respondió coherentemente |
| 🔊 TTS (Magpie Multilingual) | ✅ Funciona | Responde y reproduce audio tras bypass de Cold Start |

---

## Lo que resta por hacer 🔲

### Fase 1 — Validación de Audio
- [x] ~~Confirmar captura de voz real con ganancia +24dB y HPF activo~~ ✅ (28/Jul)
- [x] ~~Verificar que NVIDIA Parakeet transcriba el audio correctamente~~ ✅ (28/Jul)
- [x] ~~Corregir error TTS HTTP 500 en el bridge server (migrado a Magpie)~~ ✅ (28/Jul)
- [x] ~~Lograr el primer pipeline completo de extremo a extremo~~ ✅ (28/Jul)
- [x] Eliminar código de diagnóstico (buffer dump) una vez validado

### Fase 2 — Robustez y Optimización
- [x] Detección de actividad de voz (VAD) básica por amplitud para cortar silencios largos.
- [x] Streaming de audio TTS (Chunked → RAW PCM) para reproducir respuestas largas sin esperas masivas.
- [x] Soporte de "Barge-in": Interrupción del asistente mientras habla presionando el botón '1'.
- [ ] Optimizar latencia global del pipeline (objetivo: < 3 segundos)
- [ ] Manejo de errores robusto (reconexión WiFi, timeouts, reintentos)
- [ ] Migrar comunicación HTTP → WebSocket (menor latencia)
- [ ] Streaming de audio ASR (enviar micrófono mientras se graba)

### Fase 3 — Interfaz Gráfica y Wake Word
- [ ] Interfaz LVGL en Core 1 (pantalla MIPI DSI 480×800)
  - [ ] Diseño de estados visuales: Idle, Escuchando, Pensando, Hablando
  - [ ] Animaciones y retroalimentación visual
  - [ ] Double buffering para 60 FPS
- [ ] Detección de Wake Word ("Oye Asistente") con ESP-ADF
- [ ] Integración con Home Assistant (MQTT / WebSocket)

### Fase 4 — Producción
- [ ] Migración de Arduino IDE a ESP-IDF puro
- [ ] Actualizaciones OTA (Over-The-Air)
- [ ] Pruebas de estabilidad 24h continuas
- [ ] Documentación técnica (ADRs, diagramas, README)
- [ ] Diseño de carcasa

---

## Archivos Principales del Proyecto

| Archivo | Descripción |
|---------|-------------|
| `AsistenteAI.ino` | Sketch principal: FreeRTOS, pipeline STT→LLM→TTS, diagnóstico de audio |
| `ES8311_Init.h` | Driver del codec ES8311: inicialización I2C, volcado de registros, HPF |
| `bridge_server.py` | Servidor puente en Debian VM (endpoints /stt, /tts, /llm) |

## Configuración de Pines (JC4880P443C)

| Pin | GPIO | Función |
|-----|------|---------|
| I2S MCLK | 13 | Master Clock al ES8311 |
| I2S BCLK | 12 | Bit Clock |
| I2S LRCK | 10 | Word Select (Left/Right) |
| I2S DOUT | 9 | Datos ESP32 → Codec (playback) |
| I2S DIN | 48 | Datos Codec → ESP32 (captura) |
| I2C SDA | 7 | Datos I2C al ES8311 |
| I2C SCL | 8 | Reloj I2C |
| PA Enable | 11 | Power Amplifier |

---

## Lecciones Aprendidas

1. **Iniciar I2S antes del codec** — El ES8311 necesita MCLK activo para aceptar configuración I2C.
2. **El HPF del ADC es obligatorio** — Sin regs 0x1B/0x1C, el Mic Bias DC satura el ADC a 32767.
3. **Usar el driver oficial como referencia** — Copiar la secuencia de `es8311.c` de ESP-ADF, no inventar valores.
4. **Diagnosticar con datos crudos** — Imprimir samples del buffer antes de asumir que el servidor falla.
5. **Manejar Cold Starts en Cloud:** Los servicios gRPC de IA en la nube (como NVIDIA NIM) suelen dormir los workers. Se requiere un bucle de reintento (`retry loop`) en el bridge server para tolerar tiempos de arranque.
6. **Streaming PCM Puro:** Para reproducir audio continuo sin estática, usar formato `LINEAR_PCM` puro sin cabeceras WAV y consumir el buffer TCP/LwIP por completo (`stream->available() > 0`) aún si el socket se cierra.
7. **Barge-in (Interrupciones):** Para lograr interrupciones instantáneas, la variable de bandera debe evaluarse dentro del bucle de reproducción de I2S y resetearse únicamente al inicio del pipeline.
