# 🚀 Edge AI Voice Assistant (ESP32-P4 + Debian Proxmox Gateway + NVIDIA NIM)

![ESP32-P4](https://img.shields.io/badge/Hardware-ESP32--P4%20(JC4880P443C)-orange?style=for-the-badge&logo=espressif)
![NVIDIA NIM](https://img.shields.io/badge/AI_Cloud-NVIDIA_NIM%20%7C%20Parakeet%20%7C%20Nemotron--3-76B900?style=for-the-badge&logo=nvidia)
![ESP-IDF](https://img.shields.io/badge/Firmware-ESP--IDF%20v5.3.2%20LTS-red?style=for-the-badge)
![FreeRTOS](https://img.shields.io/badge/OS-FreeRTOS%20Dual--Core-008000?style=for-the-badge)
![LVGL](https://img.shields.io/badge/UI-LVGL%209%20MIPI%20DSI-blue?style=for-the-badge)

Asistente de voz inteligente de grado industrial con pantalla táctil, diseñado sobre el microcontrolador de alto rendimiento **ESP32-P4 (Dual-Core RISC-V @ 360/400MHz)** con conectividad inalámbrica gestionada por co-procesador **ESP32-C6 (SDIO)**. El sistema delega el procesamiento pesado de Inteligencia Artificial (ASR, LLM, TTS) a los servicios de **NVIDIA NIM** en la nube, interconectados mediante un Gateway industrial seguro desplegado en una máquina virtual **Debian sobre Proxmox**.

---

## 📚 Documentación Canónica del Proyecto

Para mantener el principio de *Single Source of Truth* y máxima claridad técnica, consulta la documentación oficial en la carpeta `docs/`:

| Documento | Descripción |
|---|---|
| 🏛️ **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** | Arquitectura canónica en 4 capas, asignación de núcleos FreeRTOS y flujo de memoria. |
| 📘 **[docs/BITACORA_EVOLUCION_SISTEMA.md](docs/BITACORA_EVOLUCION_SISTEMA.md)** | Bitácora maestra, post-mortems de ingeniería, análisis de causa raíz (RCA) y ADRs. |
| 🔌 **[docs/HARDWARE_PINOUT_JC4880P443C.md](docs/HARDWARE_PINOUT_JC4880P443C.md)** | Mapeo oficial de pines (I2S, I2C, MIPI DSI, GT911, SDIO, Amplificador PA). |
| 🚀 **[docs/ROADMAP_Y_ESTADO_ACTUAL.md](docs/ROADMAP_Y_ESTADO_ACTUAL.md)** | Estado de madurez del proyecto, matriz de subsistemas y tareas de las Fases 3 y 4. |

---

## 🏛️ Arquitectura del Sistema (Las 4 Capas de Ingeniería)

```mermaid
flowchart TD
    subgraph Capa1_Hardware ["Capa 1: Hardware (Placa JC4880P443C)"]
        MIC[Micrófono Analógico] -->|PGA +24dB / HPF| ES[Codec ES8311 I2C:0x18]
        ES -->|DAC Volume 0x80| AMP[Amplificador PA GPIO 11] --> SPK[Altavoz 8Ω]
        TOUCH[Touch GT911] -->|Eventos| LCD[MIPI DSI ST7701S 480x800]
    end

    subgraph Capa2_Firmware ["Capa 2: Firmware ESP32-P4 (ESP-IDF v5.3.2 + FreeRTOS)"]
        C0[Core 0: Audio Pipeline I2S RX/TX + HTTP Stream]
        PSRAM[(Buffers PSRAM 8KB Chunks)]
        C0 <--> PSRAM
        C1[Core 1: LVGL 9 UI HMI @ 60 FPS]
        C1 -->|audioCommandQueue| C0
    end

    subgraph Capa3_Gateway ["Capa 3: AI Gateway (Debian VM en Proxmox)"]
        W[Waitress WSGI Industrial - 8 Hilos / Puerto 5000]
        STR_E[Endpoint POST /v1/conversation_stream]
        STT_E[Endpoint POST /stt]
        LLM_E[Endpoint POST /llm]
        TTS_E[Endpoint POST /tts]
        W --- STR_E & STT_E & LLM_E & TTS_E
    end

    subgraph Capa4_Cloud ["Capa 4: Servicios IA (NVIDIA NIM Cloud)"]
        ASR[Parakeet 0.6B RNNT ASR]
        LLM[Nemotron-3 30B LLM]
        TTS[Magpie Multilingual TTS Diego]
    end

    ES <-->|I2S 16kHz 16-bit| C0
    C0 <-->|WiFi 15dBm / HTTP Chunked| W
    STR_E --> ASR --> LLM --> TTS --> STR_E
```

---

## ⚡ Guía de Instalación y Operación

### 1. Configuración de Credenciales
> [!IMPORTANT]
> **Nunca subas secretos al repositorio.** Todos los archivos con claves están protegidos en `.gitignore`.

1. **Firmware:** Copia `main/config.h.example` a `main/config.h` y ajusta tu SSID de WiFi y URL del Gateway:
   ```c
   #define WIFI_SSID "TuRedWiFi"
   #define WIFI_PASS "TuPassword"
   #define BRIDGE_BASE_URL "http://192.168.1.58:5000"
   ```
2. **Gateway:** En la VM Debian, copia `bridge.env.example` a `bridge.env` y coloca tu `NVIDIA_API_KEY`.

### 2. Compilación y Flasheo del Firmware (ESP-IDF v5.3.2 LTS)
En tu terminal de Windows / PowerShell con el entorno ESP-IDF cargado:
```powershell
# Activar entorno ESP-IDF
. C:\Espressif\frameworks\esp-idf-v5.3.2\export.ps1

# Compilar proyecto
idf.py build

# Flashear y monitorear puerto serie (asegúrate de cerrar PuTTY primero)
idf.py -p COM3 flash monitor
```

### 3. Servicio Gateway (Debian VM en Proxmox)
En el servidor Debian (`192.168.1.58`):
```bash
# Iniciar / reiniciar servicio del puente
sudo systemctl restart riva-bridge

# Monitorear logs en vivo
journalctl -u riva-bridge -f
```

---

## 📜 Licencia y Autoría
Diseñado y orquestado bajo estándares estrictos del **Chief AI Engineering Officer (CAEO)**.  
Consulta el archivo `LICENSE.txt` para términos de uso.
