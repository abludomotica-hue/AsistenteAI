# 🔬 AUDITORÍA FORENSE — Proyecto AsistenteAI

**Fecha de auditoría:** 22 de agosto de 2026
**Alcance:** `C:\Users\Arondon\Documents\PERSONAL\Proyectos\AsistenteAI`
**Sistema auditado:** Edge AI Voice Assistant (ESP32-P4 JC4880P443C + Gateway Flask/Debian en Proxmox + NVIDIA NIM)
**Metodología:** Inspección de árbol completo (incl. ocultos/ignorados), análisis del historial git (15 commits, `rev-list --all`), verificación del remoto GitHub, revisión estática de firmware (C/C++), bridge (Python) y scripts de despliegue.

---

## 1. RESUMEN EJECUTIVO

| Severidad | Cantidad | Categoría |
|---|---|---|
| 🔴 CRÍTICO | 6 | Credenciales reales en texto plano en disco |
| 🟠 ALTO | 8 | Superficie de ataque de red, OTA, despliegue |
| 🟡 MEDIO | 6 | Dependencias vulnerables, bugs de código |
| ⚪ BAJO | 5 | Higiene de repositorio, documentación inconsistente |

**Conclusión principal:** el historial git está **limpio** de secretos (verificado commit a commit), pero existen **credenciales reales en texto plano** en archivos del árbol de trabajo que **NO están cubiertos por `.gitignore`** y que serían publicados en el repositorio **público** de GitHub con un simple `git add .`. Adicionalmente, el sistema en ejecución opera **sin autenticación** entre firmware y bridge, con OTA sobre HTTP sin firma y con un vector de inyección de prompt hacia Home Assistant.

---

## 2. HALLAZGOS CRÍTICOS — CREDENCIALES EXPUESTAS

### C1 — `docs/API_nvidia.txt` (4 API keys NVIDIA + credenciales WiFi) 🔴

**Archivo:** `docs/API_nvidia.txt:1-14`

Contiene en texto plano:
- Credenciales WiFi: SSID `Familia Rondon` / password `[REDACTADO]` (8 caracteres, débil)
- 4 API keys NVIDIA reales:
  - `nvapi-Iexd…` (nemotron-3-nano-omni-30b)
  - `nvapi--6CP…` (parakeet-1.1b-rnnt-multilingual-asr)
  - `nvapi-o0Xg…` (nemotron-voicechat)
  - `nvapi-Wh1I…` (riva-translate-4b-instruct-v1_1)

**Agravante forense:** el `.gitignore` protege `DOCUMENT/API_nvidia.txt` (ruta antigua), pero el archivo fue **movido a `docs/`** y la nueva ruta **NO está ignorada**. Verificado con `git check-ignore docs/API_nvidia.txt` → **no ignorado**. Un `git add .` lo publicaría en el repo público.

### C2 — `docs/HA.md` (token Home Assistant de 10 años) 🔴

**Archivo:** `docs/HA.md:6`

Token JWT de larga duración de Home Assistant (`192.168.1.34`), emitido con expiración en **2036** (`exp: 2102294686`). Solo está protegido por la línea `docs/HA.md` añadida al `.gitignore` en el working tree **sin commit** (modificación pendiente). Si se descarta ese cambio del `.gitignore`, el archivo queda expuesto.

### C3 — Token HA hardcodeado en scripts de scratch 🔴

**Archivos:**
- `scratch/test_ha.py:3` — token JWT completo hardcodeado
- `scratch/update_env.py:6` — mismo token hardcodeado

La carpeta `scratch/` **no está en `.gitignore`** (verificado con `git check-ignore` → no ignorado). Ambos archivos son untracked y se subirían con `git add .`.

### C4 — `bridge.env` (2 API keys NVIDIA reales) 🔴

**Archivo:** `bridge.env:7,23`

Contiene `NVIDIA_API_KEY` y `LLM_API_KEY` reales. Está correctamente ignorado por `.gitignore:5`. Sin embargo, el propio archivo documenta en sus TODO (líneas 3-4 y 22) que **ambas keys "estuvieron expuestas en texto plano" y nunca fueron rotadas** (tarea C1 pendiente desde el 3-ago-2026). Deben considerarse comprometidas.

### C5 — `main/config.h` (password WiFi) 🔴

**Archivo:** `main/config.h:11`

Password WiFi `[REDACTADO]` en texto plano. Correctamente ignorado por `.gitignore:4` (patrón `config.h`). Mismo password que C1 → una sola rotación cubre ambos.

### C6 — `scratch/keygen.py` (llave SSH sin passphrase) 🔴

**Archivo:** `scratch/keygen.py:6`

Genera llave RSA-2048 en `C:\Users\Arondon\.ssh\id_rsa` con `-N ""` (sin passphrase) de forma automatizada. Patrón inseguro: una llave sin passphrase copiada a cualquier host otorga acceso total.

---

## 3. VERIFICACIÓN FORENSE DEL HISTORIAL GIT

### 3.1 Metodología
- `git rev-list --all` → 15 commits (commit inicial `123085f` del 3-ago-2026 a `9b568c7` del 6-ago-2026).
- `git grep` sobre **todos** los commits buscando: las 4 keys NVIDIA reales, `Pepito95`, el JWT de Home Assistant.
- `git log --all -p -S` para cada key individual.
- Revisión de reflog completo y `git stash list`.

### 3.2 Resultados
| Artefacto | ¿Presente en historial? |
|---|---|
| Keys NVIDIA reales (`nvapi-…` completas) | ❌ NO (0 coincidencias en 15 commits) |
| Password WiFi `Pepito95` | ❌ NO |
| Token JWT Home Assistant | ❌ NO |
| Placeholders (`nvapi-XXXX`, `TU_RED_WIFI`) | ✅ Sí, solo en plantillas `.example` |

El commit inicial `123085f` ("Commit base: proyecto saneado, secrets fuera de git") fue un `init` limpio: el saneamiento ocurrió **antes** del primer commit. No hay stashes, no hay ramas huérfanas, no hay refs colgantes con secretos.

### 3.3 Conclusión forense
La exposición de credenciales es **extra-git**: archivos en texto plano en el árbol de trabajo, coincidente con el TODO C1 auto-documentado en `bridge.env`. El riesgo actual no es el historial, sino el **próximo commit** (ver C1/C3) y el acceso local al disco.

---

## 4. ESTADO DEL REMOTO Y DIVERGENCIA

- **Remoto:** `https://github.com/abludomotica-hue/AsistenteAI.git` — repositorio **PÚBLICO** (verificado en vivo), licencia GPL-3.0, 0 forks.
- **Remoto en:** `cfe40f0` (7 commits). **Local en:** `9b568c7` (15 commits).
- **6 commits locales sin push** (toda la Fase HMI: LVGL 9, touch GT911, fixes MIPI DSI).
- **Migración completa a ESP-IDF sin commitear:** todo el working tree nuevo (`main/`, `docs/`, `CMakeLists.txt`, `sdkconfig`, `partitions.csv`, `managed_components/`, `dependencies.lock`) es **untracked**, y los archivos Arduino antiguos (`AsistenteAI.ino`, `src/`, `UI_Manager.cpp` raíz, etc.) figuran como **borrados sin commit**.
- **Riesgo:** la totalidad del trabajo de las Fases 2-4 existe **solo en este disco**. Hay 2 copias manuales en la carpeta padre (`AsistenteAI - copia`, `AsistenteAI - copia (2)`, del 3-ago-2026, anteriores a la migración ESP-IDF).
- El README público describe la arquitectura **Arduino antigua** (obsoleto vs. realidad local ESP-IDF v5.3.2).

---

## 5. SUPERFICIE DE ATAQUE DE RED Y DESPLIEGUE

### A1 — Bridge sin autenticación 🟠
`bridge.env:36` → `BRIDGE_AUTH_TOKEN=` (vacío). Con token vacío, `bridge_server.py:133` deshabilita la autenticación. Todos los endpoints quedan abiertos en `0.0.0.0:5000` para cualquier dispositivo de la LAN: `/stt`, `/tts`, `/llm` (consumo de cuota NVIDIA), `/v1/conversation_stream` y `/firmware/<path>` (distribución de binarios OTA, `bridge_server.py:236-242`).

### A2 — OTA inseguro 🟠
`main/OTA_Manager.cpp:14,27-32`:
- Descarga por **HTTP plano** (`http://192.168.1.58:5000/firmware/firmware_v2.bin`).
- Sin verificación de certificado (comentario: "aprobado en F4.2") y **sin firma/verificación de imagen**.
- Un atacante en la LAN (ARP spoofing o simplemente otro dispositivo) puede servir firmware malicioso que el ESP32 flasheará y ejecutará.

### A3 — Bug: el firmware nunca envía el token de autenticación 🟠
`main/network_stream.cpp:36` solo fija `Content-Type`; **no existe código que envíe el header `X-Bridge-Token`** aunque `config.h` defina `BRIDGE_AUTH_TOKEN`. Igual omisión en `OTA_Manager.cpp`. Consecuencia: si se activa el token en el bridge (remediación de A1), **el pipeline de voz y el OTA se rompen con 401**. El mecanismo H4 está implementado a medias (solo lado servidor).

### A4 — Inyección de prompt → ejecución domótica 🟠
`bridge_server.py:429-449` y `:551-560`: cualquier respuesta del LLM que contenga `[HA_CMD: domain.service: entity_id]` se ejecuta contra la API de Home Assistant **sin whitelist** de dominios, servicios ni entidades. El system prompt lista entidades (incl. `switch.medidor_interruptor`, el interruptor general), pero nada impide que el LLM —vía voz manipulada o prompt injection— emita p. ej. `homeassistant.restart` u otros servicios. El regex solo valida formato `[a-zA-Z0-9_.]`, no semántica.

### A5 — Despliegue SSH débil 🟠
`deploy_bridge_v2.py`:
- Línea 138: `paramiko.AutoAddPolicy()` → no verifica host key (vulnerable a MITM en primer despliegue).
- Líneas 204-217: password sudo inyectado con `echo '{PASSWORD}' | sudo -S` → visible en `/proc/*/cmdline` y en logs del shell remoto.
- Autenticación por password en vez de llave SSH.

### A6 — WiFi: umbral de red abierta 🟠
`main/main.cpp:93`: `wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN` → el dispositivo aceptará conectar a una red abierta con el SSID configurado (evil twin sin password). Combinado con C5 (password débil y filtrado), el perfil de red del dispositivo es comprometible.

### A7 — Bridge expone información de configuración 🟡
`GET /health` (sin auth por diseño) revela modelo LLM, estado de keys, URL de Home Assistant y tamaño del historial (`bridge_server.py:201-233`). Aceptable en LAN de confianza, pero es reconocimiento gratuito para un atacante.

### A8 — Logs con datos de voz 🟡
`bridge_server.py:277,307,404,463,513`: transcripciones completas del usuario y respuestas del LLM se escriben a `journalctl` sin política de retención ni enmascaramiento. Dato sensible (conversaciones domésticas) persistido indefinidamente.

---

## 6. DEPENDENCIAS VULNERABLES

**Archivo:** `requirements.txt` (desplegado en la VM Debian vía `deploy_bridge_v2.py`)

| Paquete | Versión | Vulnerabilidades conocidas |
|---|---|---|
| `requests` | 2.31.0 | CVE-2024-35195 (verificación de certs persistida en Session), CVE-2024-47081 (Netrc leak). Corregido en ≥2.32.x |
| `waitress` | 3.0.0 | CVE-2024-49766, CVE-2024-49767 (resource exhaustion / multipart). Corregido en ≥3.0.1/3.0.2 |
| `grpcio` | 1.62.1 | Múltiples CVEs posteriores (HTTP/2). Actualizar a release vigente |
| `flask` | 3.0.3 | Vigente en su serie; revisar al actualizar |

El bridge corre como servicio systemd `Restart=always` expuesto en la LAN: dependencias antiguas + sin auth (A1) = riesgo compuesto.

---

## 7. BUGS Y DEFECTOS DE CÓDIGO

### B1 — Posible doble chunked encoding 🟡
`main/network_stream.cpp:39` abre con `esp_http_client_open(http_client, -1)` (lo que activa chunked transfer en el propio cliente HTTP) y además escribe **manualmente** el framing RFC 7230 (`:51-68`, terminador `:86-87`). Si el cliente ya frama, los headers hexadecimales manuales viajan como payload. El endpoint del bridge usa `request.get_data()` (bytes crudos), por lo que el ASR podría estar recibiendo bytes de framing. **Verificar con captura de red**; podría explicar transcripciones degradadas. El FM-005 de `.agents/memory/failure_modes_summary.md` sugiere que `-1` era la solución, no la combinación de ambas.

### B2 — malloc/free en caliente durante reproducción TTS 🟡
`main/audio_manager.cpp:168-182`: cada chunk de audio recibido reserva y libera memoria PSRAM. Contradice la afirmación del README/roadmap de "cero malloc/free en caliente" y genera riesgo de fragmentación en operación 24/7.

### B3 — Estado compartido sin primitivas 🟡
`main/audio_manager.cpp:24-26`: `is_recording` es `volatile bool` y `record_elapsed_ms` es `int` simple, ambos escritos/leídos desde `afe_fetch_task` y `audio_manager_trigger_interaction` (contextos de tarea distintos). `volatile` no garantiza atomicidad en RISC-V dual-core; debería ser `std::atomic` (patrón ya usado correctamente para `interruptPlayback` en `main.cpp:42`).

### B4 — Headers HTTP con texto no-latin-1 🟡
`bridge_server.py:597-598`: `X-Transcript` / `X-Response-Text` contienen la transcripción cruda. Caracteres fuera de latin-1 (emojis del ASR, símbolos) pueden provocar excepción al serializar headers en waitress y abortar la respuesta completa. Sanitizar a ASCII o URL-encode.

### B5 — `/v1/conversation_stream` sin retry de cold-start 🟡
`bridge_server.py:500-506`: el ASR del orquestador no tiene el retry de 3 intentos que sí tienen `/stt` (`:271`) y `/tts` (`:324`). Los cold-starts de NVIDIA NIM fallarán la conversación completa.

### B6 — Historial LLM global de sesión única 🟡
`bridge_server.py:364`: un solo `llm_history` para todo el servicio. Aceptable con 1 dispositivo, pero si dos clientes usan el bridge (p. ej. tests `scratch/test_chunked.py` + dispositivo real), las conversaciones se contaminan. Sin mecanismo de sesión por cliente.

---

## 8. HIGIENE DEL REPOSITORIO

| Problema | Detalle |
|---|---|
| `sdkconfig` (93 KB) untracked | No ignorado; se subirá con `git add .`. Convención ESP-IDF: ignorar y versionar solo `sdkconfig.defaults` |
| `managed_components/` untracked | Miles de archivos de terceros (vendor lock ya existe en `dependencies.lock`); debe ignorarse |
| `.cache/` (clangd) | Índices de editor; no ignorado |
| `scratch/` | Scripts de prueba con secretos (C3, C6); no ignorado |
| `__pycache__/` | `.pyc` del bridge presentes en disco (sí ignorado por `.gitignore:33-34`, OK) |
| `.gitignore` modificado sin commit | La protección de `docs/HA.md` existe solo en el working tree |
| `P4-JC4880P443-V1.8.bin` (13.7 MB) | Ignorado ✅, pero ocupa disco; documentado como material de consulta |
| `JC4880P443C_I_W/` (~1 GB kit fabricante) | Ignorado ✅ |
| Documentación divergente | README público = arquitectura Arduino; `docs/` local = ESP-IDF. ROADMAP local marca OTA como "Fase 4.4 próxima" pero `OTA_Manager.cpp` ya existe |
| Carpetas copia en el padre | `AsistenteAI - copia` y `AsistenteAI - copia (2)` (3-ago-2026, pre-migración). Fuera de alcance de esta auditoría por decisión del cliente |

---

## 9. PLAN DE REMEDIACIÓN PRIORIZADO

### FASE 1 — Contención inmediata (repo)
1. Corregir `.gitignore`: añadir `docs/API_nvidia.txt`, `scratch/`, `sdkconfig`, `managed_components/`, `.cache/`.
2. Commitear el `.gitignore` (incluye la línea `docs/HA.md` actualmente sin commit).
3. Sanear `scratch/test_ha.py` y `scratch/update_env.py`: leer el token desde variable de entorno.
4. Reemplazar `docs/API_nvidia.txt` por una versión sin valores (solo nombres de servicios) o moverlo fuera del árbol del proyecto.

### FASE 2 — Rotación de credenciales (externa, no automatizable desde el repo)
1. Rotar las **4 keys NVIDIA** en build.nvidia.com (ya documentado como pendiente en `bridge.env` TODO C1).
2. Revocar el token HA de larga duración (expira 2036) y emitir uno nuevo con alcance mínimo.
3. Cambiar el password WiFi (débil y filtrado en C1/C5).
4. Actualizar `bridge.env`, `main/config.h` y el `.env` de la VM Debian con los nuevos valores.

### FASE 3 — Hardening del sistema
1. Generar `BRIDGE_AUTH_TOKEN` (`openssl rand -hex 32`) y **completar el lado cliente**: enviar `X-Bridge-Token` en `network_stream.cpp` y `OTA_Manager.cpp` (bug A3).
2. Whitelist de dominios/servicios/entidades para `[HA_CMD]` en `bridge_server.py` (mitiga A4).
3. OTA: exigir token en `/firmware/*` y validar firma/hash de la imagen antes de `esp_restart()` (mitiga A2).
4. Actualizar `requirements.txt`: `requests>=2.32.4`, `waitress>=3.0.2`, `grpcio` vigente.
5. Despliegue: llave SSH en vez de password; eliminar `echo password | sudo -S`; verificar host key.
6. Subir `threshold.authmode` a `WIFI_AUTH_WPA2_PSK` en `main.cpp:93`.
7. Enmascarar transcripciones en logs y/o fijar política de retención de `journalctl`.

### FASE 4 — Orden del repositorio
1. Commitear la migración ESP-IDF completa (working tree actual) en commits atómicos.
2. Push de los 6 commits HMI pendientes + migración.
3. Actualizar README público a la arquitectura ESP-IDF real.
4. Decidir destino de las carpetas "copia" (auditarlas o eliminarlas).

---

## 10. EVIDENCIA Y COMANDOS DE VERIFICACIÓN

| Verificación | Comando | Resultado |
|---|---|---|
| Secrets en historial | `git grep -E 'nvapi-[A-Za-z0-9_-]{30,}|<wifi-pass>|eyJhbGci…' $(git rev-list --all)` | 0 coincidencias |
| `docs/API_nvidia.txt` ignorado | `git check-ignore -v docs/API_nvidia.txt` | **No ignorado** |
| `scratch/*` ignorado | `git check-ignore scratch/test_ha.py` | **No ignorado** |
| `bridge.env` / `main/config.h` ignorados | `git check-ignore -v` | Ignorados ✅ (líneas 5 y 4) |
| Estado remoto | `git ls-remote origin` | `cfe40f0` (6 commits detrás de local) |
| Visibilidad del repo | Fetch de la página GitHub | **Público** |
| Stashes / ramas ocultas | `git stash list`, `git branch -a` | Vacío / solo `master` |

---

*Informe generado por auditoría forense automatizada. Los valores de credenciales citados están redondeados parcialmente en este documento; las rutas y líneas exactas permiten localizar cada hallazgo.*
