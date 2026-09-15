# ADR-001: Forzar HTTP/1.0 en la Reproducción de TTS

**Fecha:** Agosto 2026
**Estado:** Aceptado

## Contexto
Durante las pruebas de streaming del TTS de NVIDIA Magpie a través de Flask hacia el ESP32, se detectaron corrupciones de audio severas (clicks, ruidos estáticos). 
Flask por defecto utiliza *Chunked Transfer Encoding* en HTTP/1.1 para streams. El decodificador de audio nativo del ESP-ADF y la librería de Arduino HTTPClient fallan al extraer limpiamente las cabeceras hexadecimales del chunked HTTP de la carga PCM.

## Decisión
Se decidió forzar la respuesta de la petición HTTP a **HTTP/1.0** para el endpoint de TTS en la integración. HTTP/1.0 no soporta transfer-encoding chunked de forma estricta, lo que obliga al servidor puente y al ESP32 a tratar el stream como un simple canal de bytes continuos sin metadatos intercalados, inyectando el flujo crudo directo al I2S.

## Consecuencias
- **Positivas:** Desaparecen por completo las ráfagas estáticas en el altavoz; el PCM lineal (16000Hz, 16-bit, Mono) suena cristalino.
- **Negativas:** HTTP/1.0 impide la reutilización eficiente de la conexión (Keep-Alive), obligando al firmware a realizar el handshake TCP cada vez que el TTS va a reproducirse, sumando ~50ms de latencia de red en cada interacción.
