# 08. OPEN SOURCE & REPOSITORY EVALUATION MATRIX
## External Tooling & Methodologies Assessment

---

## 1. Evaluation of Evaluated Ecosystem Frameworks

Evaluación exhaustiva de metodologías y repositorios externos frente a las necesidades embebidas de **AsistenteAI**:

| Framework / Repository | Nature | Evaluation Result | Rationale & Practical Adoption |
|---|---|---|---|
| **`espressif/esp-idf` (v5.3 LTS)** | Official Firmware Core | **ADOPT (Core)** | Entorno de compilación nativo. Acceso directo a periféricos del P4 (MIPI DSI, DMA, I2S, PSRAM). |
| **`espressif/esp-hosted-mcu`** | Coprocessor Transport | **ADOPT (Core)** | Necesario para comunicar el ESP32-P4 con el ESP32-C6 vía bus SDIO a 40 MHz. |
| **`lvgl/lvgl` (v9.x)** | Graphics Engine | **ADOPT (Core)** | Motor de interfaz gráfica para el panel MIPI DSI 480x800 a 60 FPS con Double Buffer en PSRAM. |
| **`github/spec-kit`** | Spec-Driven Dev | **ADAPT** | Adoptamos la filosofía de redactar especificaciones e invariantes antes del código, simplificado para embedded. |
| **`obra/superpowers`** | Agentic Workflows | **ADAPT** | Adoptamos el enfoque de TDD, revisión sistemática y pruebas de habilidades en agentes de IA. |
| **`agentskills/agentskills`** | Skill Standards | **ADOPT** | Estructura modular `SKILL.md` con metadata YAML, activación condicional y referencias externas. |
| **`google/osv-scanner`** | Security Audit | **ADAPT** | Escaneo de dependencias en Python (`bridge_server.py`) y componentes de ESP-IDF para evitar CVEs. |
| **`agentsmd/agents.md`** | Agent Constitution | **ADOPT** | Archivo maestro de reglas persistentes de mínima sobrecarga para gobernar el comportamiento del CAEO. |
