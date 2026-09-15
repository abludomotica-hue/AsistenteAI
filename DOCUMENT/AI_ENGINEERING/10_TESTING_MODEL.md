# 10. TESTING & VERIFICATION MODEL
## Hardware-in-the-Loop, Regression & Unit Testing Strategy

---

## 1. Testing Pyramid for Embedded Edge AI

```
             ┌─────────────────────────────┐
             │       E2E Hardware Test     │ (Voice capture → NIM → Speaker)
             ├─────────────────────────────┤
             │    Hardware-in-the-Loop     │ (Serial telemetry, Power BOD, Touch)
             ├─────────────────────────────┤
             │    Gateway Integration      │ (Mock HTTP clients, Waitress tests)
             ├─────────────────────────────┤
             │   DSP / Unit Validation     │ (WAV headers, I2S chunk math, VAD)
             └─────────────────────────────┘
```

---

## 2. Test Execution Catalog

### 🧪 Test 1: Python Gateway Mock Test (`scratch/test_chunked.py`)
- **Objective:** Validar que el servidor Waitress en Debian acepte streaming chunked sin abortar la conexión.
- **Method:** Enviar 4 chunks de 1024 bytes con delays de 1 segundo vía script Python.
- **Expected Outcome:** HTTP 200 / 204 response sin caídas de socket.

### 🧪 Test 2: Serial Boot Verification (HIL)
- **Objective:** Validar la inicialización secuencial del hardware.
- **Pass Criteria:**
  1. `esp_psram: Found 32MB PSRAM device`
  2. `ESP32_P4_EV: Display initialized`
  3. `ES8311: Init completo y equilibrado`
  4. `sdio_wrapper: SDIO master: Slot 1, Data-Lines: 4-bit Freq(KHz)[40000 KHz]`
  5. `MAIN: ¡WiFi Conectado con éxito!`

### 🧪 Test 3: Audio Stream & Buffer Stress Test
- **Objective:** Validar que el streaming I2S de 4 segundos no sufra desbordamientos de cola SDIO.
- **Pass Criteria:** Cero errores `Connection reset by peer` o `poll_write select error` en el log serial.
