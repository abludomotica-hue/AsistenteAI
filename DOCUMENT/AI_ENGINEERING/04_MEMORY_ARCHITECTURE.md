# 04. MEMORY & CONTINUOUS LEARNING ARCHITECTURE
## Long-Term Retention, Incident Learning Loop & Knowledge Base

---

## 1. The Continuous Learning Loop

El sistema AsistenteAI aprende de cada interacción y fallo operacional para garantizar que ningún error se repita dos veces:

```
┌─────────────────────────────────────────────────────────────┐
│ 1. INCIDENT DETECTED                                        │
│ (e.g. Connection reset by peer after 800ms)                 │
└──────────────────────────────┬──────────────────────────────┘
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. ROOT CAUSE ISOLATED (RCA)                                │
│ (1024B audio buffers = 31 TCP pkts/sec overflowing SDIO)    │
└──────────────────────────────┬──────────────────────────────┘
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. TARGETED REMEDIATION APPLIED                             │
│ (Buffer expanded to 8192B = 4 pkts/sec + socket abort API)  │
└──────────────────────────────┬──────────────────────────────┘
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ 4. SYSTEM MEMORY CATALOGED                                  │
│ - Recorded in Failure Mode Catalog (FM-007)                 │
│ - Recorded in BITACORA_EVOLUCION_SISTEMA.md                 │
│ - Invariant created in ENGINEERING_INVARIANTS.md            │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Structure of `.agents/memory/`

Los archivos en `.agents/memory/` proporcionan resúmenes compactos que los agentes pueden consultar bajo demanda:

- **`.agents/memory/hardware_facts.md`:** Resumen conciso de tensiones, corrientes, frecuencias de bus (SDIO 40MHz, I2S 16kHz, MCLK 4.096MHz), direcciones I2C (0x18) y pines críticos.
- **`.agents/memory/failure_modes_summary.md`:** Tabla rápida de los 8 modos de fallo conocidos y su solución inmediata.
- **`.agents/memory/network_contracts.md`:** Esquema exacto de cabeceras, métodos y payloads HTTP de la pasarela Debian.
