# Integración de Puente Python (NVIDIA gRPC) en Proxmox

Este plan detalla la construcción de un servidor intermediario (bridge) en tu máquina virtual de Debian 13 para conectar el ESP32 con los servidores de alto rendimiento gRPC de NVIDIA.

## User Review Required

> [!IMPORTANT]
> **Dirección IP de Debian:** Necesitaremos que me indiques cuál es la IP local de tu máquina virtual Debian 13 (ej. `192.168.1.X`) para que el ESP32 sepa a dónde enviar el audio.
> 
> **ID de Función TTS:** En tu captura vimos que el modelo de STT (Parakeet) usa un `function-id` específico (REDACTADO, ver `bridge.env`). Para la voz (TTS), necesito que vayas nuevamente a [build.nvidia.com](https://build.nvidia.com/), busques el modelo TTS (probablemente se llame *FastPitch* o *Riva TTS*), le des a "Try API" -> Python, y me copies el `function-id` de ese también.

## Proposed Changes

### 1. Servidor Puente (Python en Debian 13)
Crearemos un script de Python ligero usando `Flask` y `nvidia-riva-client`. 
- **Endpoint `/stt`**: Recibirá el archivo WAV del ESP32, usará gRPC para hablar con NVIDIA Parakeet, y devolverá el texto.
- **Endpoint `/tts`**: Recibirá el texto a hablar, usará gRPC para hablar con NVIDIA TTS, y devolverá el audio crudo (PCM) para el altavoz.

#### [NEW] `bridge_server.py`
Código Python que se ejecutará en tu Debian.
#### [NEW] `requirements.txt`
Dependencias necesarias (`flask`, `nvidia-riva-client`).

### 2. Modificación del Código del ESP32
Redirigiremos las peticiones del ESP32 para que apunten a tu servidor local de Proxmox en lugar de apuntar a internet directamente para el audio. El LLM (Nemotron) seguirá conectándose directamente a NVIDIA ya que ese sí soporta REST.

#### [MODIFY] `AsistenteAI.ino`
- Cambiar la URL de STT a `http://<IP_DEBIAN>:5000/stt`.
- Cambiar la URL de TTS a `http://<IP_DEBIAN>:5000/tts`.
- Simplificar el envío HTTP ya que ahora hablaremos con nuestro propio servidor.

## Verification Plan

### Manual Verification
1. Copiaremos y ejecutaremos el servidor Python en tu VM Debian usando la consola de Proxmox o SSH.
2. Subiremos el nuevo código al ESP32.
3. Hablaremos al micrófono y verificaremos que los logs del servidor Python en Debian muestren la recepción del audio y la comunicación exitosa con NVIDIA gRPC.
