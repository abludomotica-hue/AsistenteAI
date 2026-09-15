# 02. SKILL MATRIX & DOMAIN SPECIALIZATION
## Specialized Agent Capabilities Catalog

---

## 1. Skill Taxonomy & Activation Triggers

Para evitar la sobrecarga de contexto, los skills sólo se activan cuando una condición explícita lo demanda:

| Skill Identifier | Domain | Activation Trigger / Context | Key Capabilities |
|---|---|---|---|
| `esp32_p4_hmi_master` | Hardware & BSP | JC4880P443C pinout, MIPI DSI, GT911, Audio ES8311, PA GPIO 11 | Mapeo de pines, configuración BSP oficial, clocks y buses I2C/I2S. |
| `audio-engineering` | Digital Audio DSP | ES8311 I2C registers, I2S standard, DMA buffers, HPF, VAD, PCM | Calibración de ganancias, buffers I2S, filtros pasa-altos, muestreo 16kHz. |
| `lvgl-engineering` | Graphical UI | Pantalla ST7701S, Core 1 UI, Double Buffer PSRAM, 60 FPS, eventos | Sincronización LVGL 9, diseño de pantallas, transiciones fluidas. |
| `network-sdio-engineering` | Transport Layer | ESP32-C6, SDIO, `esp_wifi_remote`, MTU, chunked HTTP, sockets | Gestión de transacciones SDIO, control de tamaño de chunks, reconexión WiFi. |
| `gateway-engineering` | Backend & WSGI | Debian VM, Proxmox, Waitress, Flask, streaming binario, tokens | Configuración WSGI multi-hilo, proxies REST/gRPC, contratos de API. |
| `nvidia-nim-engineering` | Cloud AI Services | Parakeet ASR, Nemotron-3 LLM, Magpie TTS, latencia, streaming | Optimización de prompts, TTFA tuning, gRPC/REST clients, gestión de voz. |
| `systematic-debugging` | Problem Solving | Errores de hardware, panics, resets, timeouts, degradación | Diagnóstico por hipótesis, aislamiento de capas, validación experimental. |
| `adr-management` | Architecture | Decisiones estructurales, cambios de protocolo, nuevos contratos | Redacción formal de ADRs (Contexto, Decisión, Consecuencias). |

---

## 2. Skill Design Standard

Cada skill debe seguir estrictamente este formato en su archivo `SKILL.md`:

```markdown
---
name: <skill-name>
description: <Acción concisa y condición de activación inequívoca>
---

# <Skill Title>

## 1. Activation Criteria
Cuándo cargar esta skill y cuándo NO cargarla.

## 2. Non-Negotiable Engineering Rules
Invariantes y restricciones duras del dominio.

## 3. Workflow / Action Guide
Pasos sistemáticos para ejecutar tareas de este dominio.

## 4. Verification & Validation Checklist
Pruebas obligatorias antes de declarar la tarea terminada.

## 5. References
Rutas a documentos técnicos profundos en `DOCUMENT/` o `references/`.
```
