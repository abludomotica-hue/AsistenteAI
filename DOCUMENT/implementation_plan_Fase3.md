# Phase 3: Integración Real de Audio y APIs de NVIDIA

Hemos logrado configurar el hardware base. Ahora debemos reemplazar los "mocks" (textos simulados) por flujos de bytes reales hacia y desde las APIs de NVIDIA.

## User Review Required

> [!WARNING]
> **Consumo de Memoria (RAM/PSRAM):** El ESP32 no tiene suficiente RAM interna para grabar audios largos antes de enviarlos. Deberemos grabar el audio en fragmentos cortos (streaming) o habilitar y usar explícitamente la **PSRAM** (RAM Externa) de la placa para guardar un archivo `.wav` temporal en memoria. Afortunadamente, activaste PSRAM al compilar, así que la usaremos.
> 
> **Decodificación MP3 vs PCM:** Para evitar instalar pesadas librerías de decodificación MP3 (que sobrecargarían el chip), le pediremos a la API de TTS de NVIDIA que nos devuelva el audio en formato **PCM (Raw Audio)**. Así, el ESP32 simplemente tomará los bytes que llegan de internet y los inyectará directamente al altavoz por I2S en tiempo real.

## Open Questions

> [!IMPORTANT]
> 1. **Librería JSON:** Necesitaremos parsear las respuestas del modelo de lenguaje. ¿Estás de acuerdo con instalar la librería `ArduinoJson` desde el Gestor de Librerías de tu Arduino IDE?
> 2. **Duración de escucha:** Por ahora, ¿grabamos un fragmento de audio fijo (por ejemplo, 4 o 5 segundos exactos) tras pulsar el botón "1", en lugar de detectar silencios automáticamente (VAD)? (El VAD es complejo y puede fallar, un tiempo fijo es más confiable para esta primera versión real).

## Proposed Changes

### AsistenteAI.ino

#### [MODIFY] AsistenteAI.ino
*   **Librerías:** Descomentar e incluir `#include <ArduinoJson.h>`.
*   **recordAndTranscribe():** 
    *   Asignar un buffer en PSRAM (`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`).
    *   Leer el micrófono mediante `i2s.read()` durante 5 segundos y guardar los datos PCM.
    *   Construir manualmente los headers de un request HTTP `multipart/form-data` para enviar el bloque PCM como un archivo de audio falso a la API `NVIDIA Parakeet`.
    *   Extraer el texto de la respuesta JSON.
*   **getLLMResponse():**
    *   Usar `JsonDocument` de ArduinoJson para parsear la respuesta HTTP y extraer de forma segura el nodo `choices[0].message.content`.
*   **synthesizeAndPlay():**
    *   Modificar el payload JSON enviado al modelo TTS para incluir `"response_format": "pcm"`.
    *   Leer el stream HTTP por chunks (pedazos) a medida que llega de internet.
    *   Por cada pedazo de bytes recibido, enviarlo directamente al altavoz usando `i2s.write()`.

## Verification Plan

### Manual Verification
1.  Te pediré que instales `ArduinoJson`.
2.  Subiremos el código.
3.  Pulsarás `1` y hablarás directamente al micrófono de la placa.
4.  Confirmaremos que la transcripción en el Monitor Serie coincida con lo que dijiste.
5.  Escucharemos por el altavoz la voz de NVIDIA generada en tiempo real.
