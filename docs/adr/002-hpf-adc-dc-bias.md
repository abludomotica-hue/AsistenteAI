# ADR-002: Habilitación de Filtro Paso Alto (HPF) en el ADC del ES8311

**Fecha:** Agosto 2026
**Estado:** Aceptado

## Contexto
El micrófono integrado en la placa (conectado al ES8311 vía I2S) presentaba un severo DC Offset. El búfer de audio en reposo arrojaba valores crudos desplazados de cero, causando que el algoritmo de Voice Activity Detection (VAD) de ESP-SR asumiera erróneamente actividad vocal continua, bloqueando el estado del sistema.

## Decisión
Se activó a nivel de registro I2C el filtro High-Pass (HPF) interno del ES8311.
Específicamente, se escribieron los registros `0x1B` y `0x1C` del códec durante la fase de inicialización `setupAudio()` para cortar las frecuencias en el rango del DC Bias.

## Consecuencias
- **Positivas:** La onda de captura I2S ahora oscila limpiamente alrededor del cero. El VAD y el WakeNet `Oye Asistente` alcanzan precisiones altísimas, reduciendo los falsos positivos a prácticamente cero.
- **Negativas:** Obliga a mantener control explícito sobre la configuración del chip es8311 por I2C en la secuencia de boot (alargar ~5ms el arranque), en lugar de delegar puramente en el framework.
