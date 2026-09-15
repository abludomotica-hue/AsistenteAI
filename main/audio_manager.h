#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa el bus I2S, el AFE (Cancelador de eco/Ruido) y el WakeNet
esp_err_t audio_manager_init(void);

// Permite reproducir audio (TTS)
esp_err_t audio_manager_play_chunk(const uint8_t *data, size_t length);

// Reproduce tono/chime de confirmación de activación acústica
esp_err_t audio_manager_play_chime(void);

// Inicia el procesamiento activo manualmente (ej. botón táctil)
void audio_manager_trigger_interaction(void);

#ifdef __cplusplus
}
#endif
