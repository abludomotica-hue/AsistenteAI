#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa el cliente HTTP o WebSocket para el streaming
esp_err_t network_stream_init(void);

// Inicia la conexión con el servidor
esp_err_t network_stream_start(void);

// Envía un chunk de audio PCM al backend
esp_err_t network_stream_send_chunk(const int16_t *buffer, size_t len_bytes);

void network_stream_abort(void);

// Finaliza el stream de envío y espera la respuesta TTS
esp_err_t network_stream_finish_and_receive(void);

#ifdef __cplusplus
}
#endif
