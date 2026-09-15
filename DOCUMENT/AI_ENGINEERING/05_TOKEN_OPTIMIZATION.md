# 05. TOKEN OPTIMIZATION & CONTEXT MANAGEMENT
## Maximum Reasoning Power at Minimum Token Footprint

---

## 1. The Token Budget Strategy

El contexto de los LLMs debe tratarse como memoria RAM en un microcontrolador: **un recurso finito que debe optimizarse**.

```
┌─────────────────────────────────────────────────────────────┐
│ TARGET TOKEN PROFILE (Per Conversation Turn)                │
│                                                             │
│ • System & Permanent Rules (AGENTS.md):   ~1,200 tokens     │
│ • Active Specialized Skill (if triggered): ~1,500 tokens    │
│ • Targeted File Inspection:                ~2,000 tokens    │
│ • User Prompt & Metadata:                  ~1,000 tokens    │
│ • Reasoning & Generation Buffer:           ~3,000 tokens    │
│ ─────────────────────────────────────────────────────────── │
│ TOTAL WORKING CONTEXT:                     < 8,700 tokens   │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Anti-Patterns to Avoid

| Anti-Pattern | Why It Fails | Operating System Solution |
|---|---|---|
| **Injecting all project code in prompt** | Overflows context, creates hallucinations, misses subtle bugs. | Read only the specific target file with `view_file` or slice notation. |
| **Loading all skills simultaneously** | Wastes tokens and causes confusion between domain rules. | Progressive disclosure: load only the skill triggered by the task. |
| **Keeping raw terminal dumps in prompt** | Eats tens of thousands of tokens on repetitive logs. | Truncate and extract only relevant error signatures and line numbers. |
| **Verbose narrative status files** | Dilutes critical engineering facts in conversational filler. | Use strict Markdown tables, ADRs and failure mode identifiers. |

---

## 3. Token-Saving Guidelines for Subagents and Tools
1. **Never read entire large repositories at once.** Use `grep_search` to find exact symbol definitions.
2. **Never duplicate hardware tables.** Reference `docs/HARDWARE_PINOUT_JC4880P443C.md` as the single canonical source.
3. **Keep response headers and plans concise.** Follow the 9-point CAEO structure without repeating code that was not modified.
