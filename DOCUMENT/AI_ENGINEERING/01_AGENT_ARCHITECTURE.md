# 01. AGENT KNOWLEDGE & CONTEXT ARCHITECTURE
## Progressive Disclosure, Tiered Sources & Memory Model

---

## 1. The Core Principle: Minimum Necessary Context

Los modelos de IA degradan su rendimiento y cometen alucinaciones cuando se les alimenta con miles de líneas de contexto permanente innecesario.
El **AI Engineering Operating System** implementa una arquitectura desacoplada y basada en **Progressive Disclosure** (Divulgación Progresiva).

```
┌─────────────────────────────────────────────────────────────┐
│ TIER 0: PERMANENT CONTEXT (Minimal & Invariant)             │
│ AGENTS.md (Identidad, Reglas Críticas, Invariantes, DoD)    │
└──────────────────────────────┬──────────────────────────────┘
                               │ Trigger Condicional
┌──────────────────────────────▼──────────────────────────────┐
│ TIER 1: SPECIALIZED SKILLS (On-Demand)                      │
│ .agents/skills/<skill>/SKILL.md (Workflows, Rules, Checks)  │
└──────────────────────────────┬──────────────────────────────┘
                               │ Acceso Profundo Solo si Aplica
┌──────────────────────────────▼──────────────────────────────┐
│ TIER 2: TECHNICAL REFERENCES & TARGETED MEMORY              │
│ .agents/memory/, DOCUMENT/ADR/, DOCUMENT/POSTMORTEMS/       │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Directory Taxonomy & Separation of Concerns

Cada artefacto de conocimiento tiene un hogar único y exclusivo:

| Directorio / Archivo | Capa de Conocimiento | Propósito y Contenido Exclusivo |
|---|---|---|
| `AGENTS.md` | **Persistent Rules** | Reglas maestras, identidad CAEO, invariantes no negociables y Definition of Done. |
| `.agents/rules/` | **Modular Guidelines** | Guías de estilo, reglas de hardware y políticas de Git. |
| `.agents/skills/` | **Specialized Skills** | Paquetes modulares con instrucciones ejecutables para dominios concretos. |
| `.agents/workflows/` | **Standard Operating Procedures** | Flujos de paso a paso para depuración, flasheo, despliegue y benchmarking. |
| `.agents/memory/` | **Project Memory & Lessons** | Catálogo de fallos resueltos, notas de hardware y lecciones aprendidas. |
| `DOCUMENT/` | **Technical Documentation** | Especificaciones formales del sistema para humanos e IAs. |
| `DOCUMENT/ADR/` | **Architectural Decisions** | Registros formales de decisiones estructurales inmutables. |
| `DOCUMENT/POSTMORTEMS/` | **Failure Analysis** | Análisis post-mortem detallados con cronología y RCA. |

---

## 3. Tiered Source Authority

Al resolver dudas técnicas o conflictos entre código, documentación y suposiciones, los agentes deben acatar estrictamente la siguiente jerarquía:

```text
Tier 1: Official Vendor / Silicon Datasheet / Current Verified Hardware Execution
Tier 2: Official Espressif ESP-IDF & LVGL Documentation / Official NVIDIA NIM Docs
Tier 3: Verified Project ADRs & Documented Post-Mortems
Tier 4: High-Quality Community Implementations (e.g. OSS Reference Boards)
Tier 5: Model Memory / Heuristic Guessing (PROHIBIDO para decisiones críticas)
```

**Regla de Oro:** Nunca utilizar Tier 5 para contradecir Tier 1 o Tier 2 sin evidencia experimental directa.
