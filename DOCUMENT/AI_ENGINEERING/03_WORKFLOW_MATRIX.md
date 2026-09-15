# 03. WORKFLOW MATRIX & STANDARD OPERATING PROCEDURES (SOP)
## Repeatable Engineering Execution Pipelines

---

## 1. Core Development Workflows

### 🛠️ Workflow A: Feature Implementation (Spec-Driven)
```
1. Requirement Analysis → 2. Consult Architecture & Invariants → 3. Draft Implementation Plan
   → 4. Verify No Regressions → 5. Implement in Firmware/Gateway → 6. Build (`idf.py build`)
   → 7. Flash & Monitored Test → 8. Update Docs/Bitácora → 9. Declare Done
```

### 🔬 Workflow B: Systematic Hardware & Firmware Debugging
```
1. Capture Full Serial & Server Logs (No Guessing)
2. Isolate Layer (Layer 1 Hardware vs Layer 2 Firmware vs Layer 3 Network vs Layer 4 Cloud)
3. Formulate Competing Hypotheses (e.g. SDIO congestion vs HTTP formatting vs Waitress timeout)
4. Design Disqualifying Test (e.g. Run Python mock client vs Increase buffer size)
5. Execute Test & Measure Result
6. Identify Root Cause (RCA)
7. Apply Minimal Target Fix
8. Run Regression Suite
9. Document in Post-Mortem & Memory Catalog
```

### 🚀 Workflow C: Flashing & Hardware Verification
```
1. Check Serial Port Availability (`Get-PortNames` / COM3)
2. Verify No External Serial Monitors (PuTTY / VS Code Monitor must be CLOSED)
3. Execute `idf.py -p COM3 flash monitor`
4. Confirm Boot Sequence:
   - PSRAM 32MB detected @ 200MHz
   - ST7701S initialized & Backlight ON
   - ES8311 I2C initialized & DAC volume set to 0x80
   - ESP-Hosted SDIO communication active
   - WiFi connected & Valid STA IP assigned
5. Perform Functional Trigger (Touch Button / Voice)
6. Inspect Server Journalctl (`journalctl -u riva-bridge -f`)
```
