"""
Bridge Server v2.2 - NVIDIA Riva / Magpie TTS Multilingual / LLM Proxy
=======================================================================
Servidor puente Flask que conecta el ESP32-P4 con NVIDIA NIM vía gRPC/REST.

Endpoints:
  POST /stt  - Recibe audio WAV, devuelve texto (Parakeet ASR)
  POST /tts  - Recibe JSON {"input": "texto"}, devuelve audio PCM (Magpie TTS)
  POST /llm  - Recibe JSON {"input": "texto"}, devuelve respuesta en texto
               plano (Nemotron). El Bridge mantiene el historial de
               conversación y el system prompt; el ESP32 no guarda estado.
               Opcional: {"reset": true} para reiniciar la conversación.
  GET  /health - Estado del servidor y servicios (sin autenticación)

Seguridad (H4): si BRIDGE_AUTH_TOKEN está definido en .env, todos los
endpoints salvo /health exigen el header `X-Bridge-Token`.

Despliegue: /home/ablutech/riva-bridge/ en Debian VM (192.168.1.58)
Configuración: Variables de entorno en .env (cargadas por systemd)
Servido con waitress (WSGI de producción) en lugar del dev server de Flask.

Cambios v2.2 (04-Ago-2026):
  - waitress como servidor WSGI (8 hilos) en vez de Flask dev server (H4)
  - Retry de cold-start en /stt (3 intentos, como /tts) (H4)
  - Autenticación opcional por token compartido (X-Bridge-Token) (H4)
Cambios v2.1 (03-Ago-2026):
  - Endpoint /llm: proxy REST hacia NVIDIA Nemotron (tarea H2). El firmware
    ya no habla directo con NVIDIA ni contiene API keys.
  - Historial de conversación y system prompt gestionados en el Bridge.
Cambios v2 (28-Jul-2026):
  - Migración de FastPitch (deprecado) a Magpie TTS Multilingual
  - Voz: Magpie-Multilingual.ES-US.Diego (español)
  - Endpoint /health para diagnóstico remoto
  - Manejo de errores mejorado con logging
  - Listado de voces disponibles al arrancar
"""

import os
import logging
import traceback
import time
import threading
import hmac
import grpc
import riva.client
import re
import requests
from flask import Flask, request, Response, jsonify, stream_with_context, send_from_directory
from waitress import serve

# ==========================================
# LOGGING
# ==========================================
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S'
)
log = logging.getLogger("bridge")

app = Flask(__name__)

# ==========================================
# CONFIGURACIÓN (desde variables de entorno)
# ==========================================
NVIDIA_API_KEY   = os.getenv("NVIDIA_API_KEY", "FALTA_API_KEY")
STT_FUNCTION_ID  = os.getenv("STT_FUNCTION_ID", "FALTA_STT_ID")
TTS_FUNCTION_ID  = os.getenv("TTS_FUNCTION_ID", "FALTA_TTS_ID")

# Voces disponibles en Magpie TTS Multilingual para español:
#   Magpie-Multilingual.ES-US.Diego    (masculina)
#   Magpie-Multilingual.ES-US.Camila   (femenina)
# Para otros idiomas: EN-US.Aria, EN-US.James, DE-DE.Florian, FR-FR.Vivienne, etc.
TTS_VOICE   = os.getenv("TTS_VOICE", "Magpie-Multilingual.ES-US.Diego")
TTS_LANG    = os.getenv("TTS_LANG", "es-US")
TTS_SAMPLE_RATE = int(os.getenv("TTS_SAMPLE_RATE", "16000"))

GRPC_URI = "grpc.nvcf.nvidia.com:443"

# ==========================================
# CONFIGURACIÓN LLM (Nemotron vía NVIDIA NIM REST)
# ==========================================
LLM_API_URL     = "https://integrate.api.nvidia.com/v1/chat/completions"
LLM_MODEL       = os.getenv("LLM_MODEL", "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning")
LLM_MAX_TOKENS  = int(os.getenv("LLM_MAX_TOKENS", "1024"))
LLM_TEMPERATURE = float(os.getenv("LLM_TEMPERATURE", "0.6"))
# Máximo de mensajes de historial a conservar (user + assistant combinados)
LLM_MAX_HISTORY = int(os.getenv("LLM_MAX_HISTORY", "10"))
# El LLM puede usar una API key propia (en este proyecto el LLM usa una key
# distinta a la de STT/TTS). Si LLM_API_KEY no está definida, se usa NVIDIA_API_KEY.
LLM_API_KEY     = os.getenv("LLM_API_KEY", "") or NVIDIA_API_KEY

# Home Assistant Config (F3.3)
HA_URL = os.getenv("HA_URL", "")
HA_TOKEN = os.getenv("HA_TOKEN", "")

# Generar Prompt dinámico basado en si HA está configurado
if HA_URL and HA_TOKEN:
    LLM_SYSTEM_PROMPT = os.getenv(
        "LLM_SYSTEM_PROMPT",
        "Eres un asistente de voz doméstico inteligente desarrollado por Ablutech. Responde siempre en español, "
        "de forma breve y concisa (máximo 1 o 2 frases), sin formato markdown ni emojis, ya que tu respuesta será sintetizada por voz. "
        "Tienes control domótico de la casa a través de Home Assistant. Si el usuario te pide controlar una luz, interruptor o dispositivo, "
        "debes confirmar la acción de forma natural y terminar tu respuesta con el comando especial: [HA_CMD: domain.service: entity_id]. "
        "Dispositivos disponibles en el hogar: "
        "- Ampolleta o luz de la oficina: light.ampolleta_oficina "
        "- Foco o luz del living: light.living_floodlight "
        "- Luz de pared: light.fancy_wall_light "
        "- Interruptor o medidor general: switch.medidor_interruptor "
        "Ejemplo para encender la luz de la oficina: 'Entendido, encendiendo la luz de la oficina. [HA_CMD: light.turn_on: light.ampolleta_oficina]'. "
        "Ejemplo para apagar la luz del living: 'Listo, apagando el foco del living. [HA_CMD: light.turn_off: light.living_floodlight]'."
    )
else:
    LLM_SYSTEM_PROMPT = os.getenv(
        "LLM_SYSTEM_PROMPT",
        "Eres un asistente de voz doméstico inteligente. Responde siempre en español, "
        "de forma breve y natural (máximo 2 o 3 frases), ya que tu respuesta será "
        "reproducida por voz. No uses emojis ni formato markdown."
    )

# ==========================================
# SEGURIDAD HOME ASSISTANT (A4 Hardening)
# ==========================================
ALLOWED_HA_SERVICES = {
    "light": {"turn_on", "turn_off", "toggle"},
    "switch": {"turn_on", "turn_off", "toggle"},
    "climate": {"set_temperature", "set_hvac_mode", "turn_on", "turn_off"},
    "cover": {"open_cover", "close_cover", "stop_cover"},
}

def execute_ha_command_if_present(text: str) -> str:
    """Interpreta y ejecuta [HA_CMD: domain.service: entity_id] validando contra la Whitelist."""
    if not (HA_URL and HA_TOKEN):
        return text

    match = re.search(r'\[HA_CMD:\s*([a-zA-Z0-9_]+)\.([a-zA-Z0-9_]+):\s*([a-zA-Z0-9_.]+)\]', text)
    if match:
        domain = match.group(1)
        service = match.group(2)
        entity_id = match.group(3)
        
        if domain in ALLOWED_HA_SERVICES and service in ALLOWED_HA_SERVICES[domain]:
            log.info(f"🏠 [HA] Ejecutando comando autorizado: {domain}.{service} sobre {entity_id}")
            ha_endpoint = f"{HA_URL.rstrip('/')}/api/services/{domain}/{service}"
            ha_headers = {
                "Authorization": f"Bearer {HA_TOKEN}",
                "Content-Type": "application/json"
            }
            try:
                ha_resp = requests.post(ha_endpoint, headers=ha_headers, json={"entity_id": entity_id}, timeout=3)
                if ha_resp.status_code == 200:
                    log.info(f"✅ HA Command exitoso.")
                else:
                    log.error(f"❌ HA Command falló con HTTP {ha_resp.status_code}: {ha_resp.text}")
            except Exception as e:
                log.error(f"❌ Error contactando HA: {e}")
        else:
            log.warning(f"⚠️ [HA Security] Bloqueado comando NO autorizado por Whitelist: {domain}.{service} sobre {entity_id}")

        # Limpiar el texto para que el TTS no lo pronuncie
        text = re.sub(r'\[HA_CMD:.*?\]', '', text).strip()
        if not text:
            text = "Comando ejecutado."

    return text

def safe_header_str(text: str) -> str:
    """Sanitiza texto a codificación compatible con WSGI Latin-1, preservando acentos y caracteres españoles."""
    if not text:
        return ""
    cleaned = text.replace('\r', ' ').replace('\n', ' ')
    return cleaned.encode('latin-1', 'replace').decode('latin-1')

# ==========================================
# AUTENTICACIÓN (H4)
# Token compartido opcional. Si está vacío, la auth queda deshabilitada
# (compatibilidad con firmwares antiguos). El firmware lo envía en el
# header X-Bridge-Token cuando BRIDGE_AUTH_TOKEN está definido en config.h.
# ==========================================
BRIDGE_AUTH_TOKEN = os.getenv("BRIDGE_AUTH_TOKEN", "")


@app.before_request
def check_auth_token():
    """Exige X-Bridge-Token en todos los endpoints salvo /health."""
    if not BRIDGE_AUTH_TOKEN:
        return None
    if request.path == "/health":
        return None
    provided = request.headers.get("X-Bridge-Token", "")
    if not hmac.compare_digest(provided.encode("utf-8"), BRIDGE_AUTH_TOKEN.encode("utf-8")):
        log.warning(f"🔒 Auth rechazada desde {request.remote_addr} en {request.path}")
        return jsonify({"error": "unauthorized"}), 401
    return None

# ==========================================
# INICIALIZACIÓN DE CLIENTES gRPC
# ==========================================
asr_service = None
tts_service = None

def init_stt():
    """Inicializa el cliente STT (Parakeet ASR)."""
    global asr_service
    if STT_FUNCTION_ID == "FALTA_STT_ID":
        log.warning("⚠️  STT_FUNCTION_ID no configurado. Endpoint /stt deshabilitado.")
        log.warning("   Configura STT_FUNCTION_ID en el archivo .env")
        return
    try:
        auth = riva.client.Auth(
            use_ssl=True,
            uri=GRPC_URI,
            metadata_args=[
                ["function-id", STT_FUNCTION_ID],
                ["authorization", f"Bearer {NVIDIA_API_KEY}"]
            ]
        )
        asr_service = riva.client.ASRService(auth)
        log.info(f"✅ Cliente STT inicializado (Function ID: {STT_FUNCTION_ID[:8]}...)")
    except Exception as e:
        log.error(f"❌ Error iniciando cliente STT: {e}")
        log.error(traceback.format_exc())

def init_tts():
    """Inicializa el cliente TTS (Magpie TTS Multilingual)."""
    global tts_service
    if TTS_FUNCTION_ID == "FALTA_TTS_ID":
        log.warning("⚠️  TTS_FUNCTION_ID no configurado. Endpoint /tts deshabilitado.")
        log.warning("   Configura TTS_FUNCTION_ID en el archivo .env")
        return
    try:
        auth = riva.client.Auth(
            use_ssl=True,
            uri=GRPC_URI,
            metadata_args=[
                ["function-id", TTS_FUNCTION_ID],
                ["authorization", f"Bearer {NVIDIA_API_KEY}"]
            ]
        )
        tts_service = riva.client.SpeechSynthesisService(auth)
        log.info(f"✅ Cliente TTS inicializado (Magpie Multilingual)")
        log.info(f"   Function ID: {TTS_FUNCTION_ID[:8]}...")
        log.info(f"   Voz: {TTS_VOICE}")
        log.info(f"   Idioma: {TTS_LANG}")
        log.info(f"   Sample Rate: {TTS_SAMPLE_RATE} Hz")
    except Exception as e:
        log.error(f"❌ Error iniciando cliente TTS: {e}")
        log.error(traceback.format_exc())

# ==========================================
# ENDPOINTS
# ==========================================

@app.route('/health', methods=['GET'])
def health():
    """Endpoint de diagnóstico para verificar el estado del servidor."""
    status = {
        "server": "ok",
        "version": "2.2 (Magpie TTS + LLM proxy + waitress + auth)",
        "stt": {
            "status": "ok" if asr_service else "error",
            "model": "parakeet-1.1b-rnnt-multilingual-asr",
            "function_id": STT_FUNCTION_ID[:8] + "..." if STT_FUNCTION_ID != "FALTA_STT_ID" else "NOT_SET"
        },
        "tts": {
            "status": "ok" if tts_service else "error",
            "model": "magpie-tts-multilingual",
            "function_id": TTS_FUNCTION_ID[:8] + "..." if TTS_FUNCTION_ID != "FALTA_TTS_ID" else "NOT_SET",
            "voice": TTS_VOICE,
            "language": TTS_LANG,
            "sample_rate": TTS_SAMPLE_RATE
        },
        "llm": {
            "status": "ok" if LLM_API_KEY and LLM_API_KEY != "FALTA_API_KEY" else "error",
            "model": LLM_MODEL,
            "history_messages": len(llm_history),
            "max_history": LLM_MAX_HISTORY
        },
        "home_assistant": {
            "status": "configured" if HA_URL and HA_TOKEN else "not_configured",
            "url": HA_URL
        },
        "api_key": "set" if NVIDIA_API_KEY != "FALTA_API_KEY" else "NOT_SET",
        "auth": "enabled" if BRIDGE_AUTH_TOKEN else "disabled"
    }
    return jsonify(status)


@app.route('/firmware/<path:filename>', methods=['GET'])
def download_firmware(filename):
    """Endpoint para descargas OTA (Fase 4.2). Sirve binarios desde /home/ablutech/riva-bridge/firmware/"""
    firmware_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "firmware")
    if not os.path.exists(firmware_dir):
        os.makedirs(firmware_dir)
    return send_from_directory(firmware_dir, filename)



@app.route('/stt', methods=['POST'])
def stt():
    """Endpoint STT: Recibe audio WAV del ESP32, devuelve texto plano."""
    if not asr_service:
        log.error("STT no disponible - cliente no inicializado")
        return "Error: STT no disponible", 503

    if 'file' not in request.files:
        return "Error: No se envió el archivo de audio", 400

    file = request.files['file']
    audio_bytes = file.read()
    log.info(f"📥 STT: Recibido audio del ESP32: {len(audio_bytes)} bytes")

    config = riva.client.RecognitionConfig(
        encoding=riva.client.AudioEncoding.LINEAR_PCM,
        sample_rate_hertz=16000,
        audio_channel_count=1,
        language_code="es-US",
        max_alternatives=1,
        enable_automatic_punctuation=True
    )

    # Retry de cold-start (H4): los workers de NVIDIA NIM pueden tardar en
    # arrancar; mismo patrón de 3 intentos que /tts.
    for attempt in range(3):
        try:
            response = asr_service.offline_recognize(audio_bytes, config)
            transcript = ""
            if len(response.results) > 0 and len(response.results[0].alternatives) > 0:
                transcript = response.results[0].alternatives[0].transcript
            log.info(f"📝 STT Resultado: \"{transcript}\"")
            return transcript
        except grpc.RpcError as e:
            log.warning(f"⚠️  STT intento {attempt+1}/3 falló: code={e.code()}, details={e.details()}")
            if attempt == 2:
                log.error(f"❌ Error gRPC STT tras 3 intentos: code={e.code()}, details={e.details()}")
                return f"Error gRPC: {e.details()}", 500
            time.sleep(1)
        except Exception as e:
            log.error(f"❌ Error STT inesperado: {e}")
            log.error(traceback.format_exc())
            return "Error interno", 500


@app.route('/tts', methods=['POST'])
def tts():
    """Endpoint TTS: Recibe JSON {"input": "texto"}, devuelve audio PCM en streaming chunked."""
    if not tts_service:
        log.error("TTS no disponible - cliente no inicializado")
        log.error("  Verifica que TTS_FUNCTION_ID esté configurado en .env")
        return "Error: TTS no disponible. Verifica TTS_FUNCTION_ID en .env", 503

    data = request.json
    if not data or 'input' not in data:
        return "Error: Falta el campo 'input' en el JSON", 400

    text = data.get('input', '')
    if not text.strip():
        return "Error: Texto vacío", 400

    log.info(f"🗣️  TTS: Sintetizando \"{text[:80]}{'...' if len(text) > 80 else ''}\"")

    def generate_audio():
        yield b"" # Forzar el envío inmediato de los HTTP Headers (200 OK) para evitar el timeout del ESP32
        
        # Limpiar saltos de línea extra y dividir en oraciones cortas (por puntuación)
        clean_text = text.replace('\n', ' ').strip()
        # Eliminar emojis y caracteres raros que puedan colgar a Magpie TTS
        clean_text = re.sub(r'[^\w\s.,?!¿¡ñÑáéíóúÁÉÍÓÚüÜ:;-]', '', clean_text)
        
        chunks = re.split(r'(?<=[.?!;])\s+', clean_text)
        
        for i, chunk in enumerate(chunks):
            if not chunk.strip():
                continue
            log.info(f"🔊 TTS Generando chunk {i+1}/{len(chunks)}: '{chunk[:40]}...'")
            
            for attempt in range(3):
                try:
                    resp = tts_service.synthesize(
                        text=chunk,
                        language_code=TTS_LANG,
                        voice_name=TTS_VOICE,
                        sample_rate_hz=TTS_SAMPLE_RATE,
                        encoding=riva.client.AudioEncoding.LINEAR_PCM
                    )
                    
                    # Como pedimos LINEAR_PCM, audio_data es PCM puro, sin cabeceras WAV! Cero estática.
                    yield resp.audio
                    break # Éxito, salir del loop de intentos
                except grpc.RpcError as e:
                    log.warning(f"⚠️  Intento {attempt+1} falló en chunk {i+1}. Error gRPC TTS: code={e.code()}, details={e.details()}")
                    if attempt == 2:
                        # Dar información útil para debugging en el último intento
                        if "UNAUTHENTICATED" in str(e.code()):
                            log.error("  → Verifica NVIDIA_API_KEY y TTS_FUNCTION_ID en .env")
                        elif "NOT_FOUND" in str(e.code()):
                            log.error(f"  → La voz '{TTS_VOICE}' puede no estar disponible")
                        elif "DEADLINE_EXCEEDED" in str(e.code()):
                            log.error("  → NVIDIA Server timeout (cold start/overload)")
                        log.error(f"❌ Abortando stream de audio por error gRPC repetido.")
                        return # Termina el stream abruptamente
                    time.sleep(1)
                except Exception as e:
                    log.error(f"❌ Error TTS inesperado en chunk {i+1}: {e}")
                    log.error(traceback.format_exc())
                    return # Termina el stream

    # Al retornar un generador en Response, Flask usa automáticamente Transfer-Encoding: chunked
    return Response(stream_with_context(generate_audio()), mimetype="audio/pcm")


# ==========================================
# LLM (Nemotron vía NVIDIA NIM REST) - Tarea H2
# ==========================================
# Historial de conversación en memoria (sesión única del dispositivo).
# Flask sirve requests en hilos → toda mutación se protege con el lock.
llm_history = []  # [{"role": "user"/"assistant", "content": str}, ...]
llm_lock = threading.Lock()


@app.route('/llm', methods=['POST'])
def llm():
    """Endpoint LLM: recibe {"input": "texto"}, devuelve texto plano.

    El Bridge mantiene el historial y el system prompt; el ESP32 queda sin
    estado y sin API keys. Opcional: {"reset": true} reinicia la conversación.
    """
    data = request.json
    if not data or 'input' not in data:
        return "Error: Falta el campo 'input' en el JSON", 400

    text = data.get('input', '').strip()
    if not text:
        return "Error: Texto vacío", 400

    if data.get('reset'):
        with llm_lock:
            llm_history.clear()
        log.info("🔄 LLM: Historial de conversación reiniciado")

    with llm_lock:
        messages = [{"role": "system", "content": LLM_SYSTEM_PROMPT}]
        messages.extend(llm_history)
        messages.append({"role": "user", "content": text})

    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {LLM_API_KEY}",
    }
    payload = {
        "model": LLM_MODEL,
        "max_tokens": LLM_MAX_TOKENS,
        "temperature": LLM_TEMPERATURE,
        "messages": messages,
    }

    log.info(f"🧠 LLM: \"{text[:80]}{'...' if len(text) > 80 else ''}\" (historial: {len(llm_history)} msgs)")

    response_text = ""
    last_error = ""
    for attempt in range(2):
        try:
            resp = requests.post(LLM_API_URL, headers=headers, json=payload, timeout=(5, 45))
            if resp.status_code == 200:
                doc = resp.json()
                response_text = doc["choices"][0]["message"]["content"] or ""
                break
            last_error = f"HTTP {resp.status_code}: {resp.text[:200]}"
            log.warning(f"⚠️  LLM intento {attempt+1} falló: {last_error}")
        except Exception as e:
            last_error = str(e)
            log.warning(f"⚠️  LLM intento {attempt+1} falló: {e}")
        time.sleep(1)

    if not response_text:
        log.error(f"❌ LLM sin respuesta. Último error: {last_error}")
        return "Error: LLM no disponible", 502

    # --- Home Assistant Interception (F3.3 + Whitelist A4) ---
    response_text = execute_ha_command_if_present(response_text)

    # Solo se consolida el historial si la llamada fue exitosa
    with llm_lock:
        llm_history.append({"role": "user", "content": text})
        llm_history.append({"role": "assistant", "content": response_text})
        while len(llm_history) > LLM_MAX_HISTORY:
            llm_history.pop(0)

    log.info(f"💬 LLM respondió: \"{response_text[:80]}{'...' if len(response_text) > 80 else ''}\"")
    return response_text


# ============================================================
# ENDPOINT UNIFICADO STREAMING: /v1/conversation_stream (Fase 4)
# ============================================================
@app.route('/v1/conversation_stream', methods=['POST'])
def conversation_stream():
    """
    Endpoint orquestado de ultra baja latencia.
    Recibe audio WAV del ESP32 -> Transcribe (ASR) -> Razona (LLM) -> Sintetiza (TTS)
    Retorna el stream PCM de audio directamente con cabeceras de texto.
    """
    if not asr_service or not tts_service:
        log.error("Servicios STT o TTS no disponibles")
        return "Error: Servicios de voz no disponibles", 503

    # El ESP32 envía el audio en bruto (chunked POST) como application/octet-stream
    audio_bytes = request.get_data()
    
    if not audio_bytes or len(audio_bytes) < 100:
        return "Error: Payload de audio vacío", 400

    t0 = time.time()
    log.info(f"🚀 [Orquestador] Recibido stream de voz ({len(audio_bytes)} bytes)...")

    # 1. Transcribir con Parakeet ASR
    config = riva.client.RecognitionConfig(
        encoding=riva.client.AudioEncoding.LINEAR_PCM,
        sample_rate_hertz=16000,
        audio_channel_count=1,
        language_code="es-US",
        max_alternatives=1,
        enable_automatic_punctuation=True
    )
    transcript = ""
    for attempt in range(2):
        try:
            response = asr_service.offline_recognize(audio_bytes, config)
            if len(response.results) > 0 and len(response.results[0].alternatives) > 0:
                transcript = response.results[0].alternatives[0].transcript
            break
        except Exception as e:
            log.warning(f"⚠️ [Orquestador] ASR intento {attempt+1} falló: {e}")
            time.sleep(0.5)

    if not transcript.strip():
        log.warning("⚠️  Audio sin voz inteligible.")
        return "Silencio", 204

    t_asr = time.time() - t0
    log.info(f"📝 [Orquestador ASR {t_asr*1000:.0f}ms]: \"{transcript}\"")

    # 2. Consultar LLM (Nemotron)
    with llm_lock:
        messages = [{"role": "system", "content": LLM_SYSTEM_PROMPT}]
        messages.extend(llm_history)
        messages.append({"role": "user", "content": transcript})

    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {LLM_API_KEY}",
    }
    payload = {
        "model": LLM_MODEL,
        "max_tokens": LLM_MAX_TOKENS,
        "temperature": LLM_TEMPERATURE,
        "messages": messages,
    }

    t_llm_start = time.time()
    llm_resp_text = ""
    max_retries = 3
    for attempt in range(max_retries):
        try:
            resp = requests.post(LLM_API_URL, headers=headers, json=payload, timeout=(5, 30))
            if resp.status_code == 200:
                doc = resp.json()
                llm_resp_text = doc["choices"][0]["message"]["content"] or ""
                break
            elif resp.status_code in (429, 502, 503, 504):
                log.warning(f"⚠️ [Orquestador] LLM intento {attempt+1}/{max_retries} HTTP {resp.status_code}, reintentando en 1s...")
                time.sleep(1)
            else:
                log.error(f"❌ [Orquestador] Error LLM API HTTP {resp.status_code}: {resp.text}")
                break
        except Exception as e:
            log.warning(f"⚠️ [Orquestador] Error contactando LLM intento {attempt+1}: {e}")
            time.sleep(1)

    if not llm_resp_text.strip():
        llm_resp_text = "Disculpa, tuve un problema temporal al consultar a la inteligencia artificial."

    # Control Home Assistant si aplica (con Whitelist A4)
    llm_resp_text = execute_ha_command_if_present(llm_resp_text)

    # Guardar en memoria
    with llm_lock:
        llm_history.append({"role": "user", "content": transcript})
        llm_history.append({"role": "assistant", "content": llm_resp_text})
        while len(llm_history) > LLM_MAX_HISTORY:
            llm_history.pop(0)

    t_llm = time.time() - t_llm_start
    log.info(f"💬 [Orquestador LLM {t_llm*1000:.0f}ms]: \"{llm_resp_text[:60]}...\"")

    # 3. Sintetizar con Magpie TTS en streaming
    clean_text = llm_resp_text.replace('\n', ' ').strip()
    clean_text = re.sub(r'[^\w\s.,?!¿¡ñÑáéíóúÁÉÍÓÚüÜ:;-]', '', clean_text)
    chunks = [c for c in re.split(r'(?<=[.?!;])\s+', clean_text) if c.strip()]

    def generate_unified_pcm():
        yield b"" # Headers inmediatos
        for chunk in chunks:
            try:
                tts_resp = tts_service.synthesize(
                    text=chunk,
                    language_code=TTS_LANG,
                    voice_name=TTS_VOICE,
                    sample_rate_hz=TTS_SAMPLE_RATE,
                    encoding=riva.client.AudioEncoding.LINEAR_PCM
                )
                yield tts_resp.audio
            except Exception as e:
                log.error(f"❌ Error en TTS chunk: {e}")
                break

    response = Response(stream_with_context(generate_unified_pcm()), mimetype="audio/pcm")
    response.headers["X-Transcript"] = safe_header_str(transcript)
    response.headers["X-Response-Text"] = safe_header_str(llm_resp_text)
    return response


# ==========================================
# ARRANQUE
# ==========================================
if __name__ == '__main__':
    log.info("=" * 60)
    log.info("  NVIDIA Riva Bridge Server v2.2")
    log.info("  (Parakeet ASR + Magpie TTS + LLM Proxy)")
    log.info("=" * 60)

    # Validar configuración
    if NVIDIA_API_KEY == "FALTA_API_KEY":
        log.warning("⚠️  NVIDIA_API_KEY no configurada!")
        log.warning("   Edita /home/ablutech/riva-bridge/.env")

    init_stt()
    init_tts()

    if not LLM_API_KEY or LLM_API_KEY == "FALTA_API_KEY":
        log.warning("⚠️  LLM_API_KEY no configurada. Endpoint /llm devolverá 502.")
    else:
        log.info(f"✅ LLM configurado: {LLM_MODEL}")
        log.info(f"   Historial máximo: {LLM_MAX_HISTORY} mensajes")

    if BRIDGE_AUTH_TOKEN:
        log.info("🔒 Autenticación por token ACTIVA (header X-Bridge-Token)")
    else:
        log.warning("⚠️  BRIDGE_AUTH_TOKEN no configurado: endpoints SIN autenticación.")
        log.warning("   Defínelo en /home/ablutech/riva-bridge/.env (y en config.h del firmware).")

    log.info("")
    log.info("Servidor escuchando en http://0.0.0.0:5000 (waitress, 8 hilos)")
    log.info("Endpoints:")
    log.info("  POST /stt    - Transcripción de audio")
    log.info("  POST /tts    - Síntesis de voz")
    log.info("  POST /llm    - Conversación (Nemotron, con historial)")
    log.info("  GET  /health - Estado del servidor")
    log.info("")

    # waitress (H4): servidor WSGI de producción; el dev server de Flask no
    # es apto para uso continuo ni para streaming concurrente.
    serve(app, host='0.0.0.0', port=5000, threads=8)
