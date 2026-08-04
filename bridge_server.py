"""
Bridge Server v2 - NVIDIA Riva / Magpie TTS Multilingual
=========================================================
Servidor puente Flask que conecta el ESP32-P4 con NVIDIA NIM vía gRPC.

Endpoints:
  POST /stt  - Recibe audio WAV, devuelve texto (Parakeet ASR)
  POST /tts  - Recibe JSON {"input": "texto"}, devuelve audio PCM (Magpie TTS)
  GET  /health - Estado del servidor y servicios

Despliegue: /home/ablutech/riva-bridge/ en Debian VM (192.168.1.58)
Configuración: Variables de entorno en .env (cargadas por systemd)

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
import grpc
import riva.client
import re
from flask import Flask, request, Response, jsonify, stream_with_context

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
        "version": "2.0 (Magpie TTS)",
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
        "api_key": "set" if NVIDIA_API_KEY != "FALTA_API_KEY" else "NOT_SET"
    }
    return jsonify(status)


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

    try:
        response = asr_service.offline_recognize(audio_bytes, config)
        transcript = ""
        if len(response.results) > 0 and len(response.results[0].alternatives) > 0:
            transcript = response.results[0].alternatives[0].transcript
        log.info(f"📝 STT Resultado: \"{transcript}\"")
        return transcript
    except grpc.RpcError as e:
        log.error(f"❌ Error gRPC STT: code={e.code()}, details={e.details()}")
        return f"Error gRPC: {e.details()}", 500
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
                    import time
                    time.sleep(1)
                except Exception as e:
                    log.error(f"❌ Error TTS inesperado en chunk {i+1}: {e}")
                    log.error(traceback.format_exc())
                    return # Termina el stream

    # Al retornar un generador en Response, Flask usa automáticamente Transfer-Encoding: chunked
    return Response(stream_with_context(generate_audio()), mimetype="audio/pcm")


# ==========================================
# ARRANQUE
# ==========================================
if __name__ == '__main__':
    log.info("=" * 60)
    log.info("  NVIDIA Riva Bridge Server v2.0")
    log.info("  (Parakeet ASR + Magpie TTS Multilingual)")
    log.info("=" * 60)

    # Validar configuración
    if NVIDIA_API_KEY == "FALTA_API_KEY":
        log.warning("⚠️  NVIDIA_API_KEY no configurada!")
        log.warning("   Edita /home/ablutech/riva-bridge/.env")

    init_stt()
    init_tts()

    log.info("")
    log.info("Servidor escuchando en http://0.0.0.0:5000")
    log.info("Endpoints:")
    log.info("  POST /stt    - Transcripción de audio")
    log.info("  POST /tts    - Síntesis de voz")
    log.info("  GET  /health - Estado del servidor")
    log.info("")

    app.run(host='0.0.0.0', port=5000)
