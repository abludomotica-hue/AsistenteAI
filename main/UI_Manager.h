#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Estados conversacionales visuales del Asistente IA HMI
enum UIState {
    UI_STATE_IDLE = 0,      // Reposo / Reloj / Esperando orden
    UI_STATE_LISTENING,     // Escuchando / Captura I2S Activa
    UI_STATE_THINKING,      // Pensando / Procesando en NVIDIA NIM Cloud
    UI_STATE_SPEAKING       // Hablando / Reproduciendo stream de voz Magpie TTS
};

// Comandos de audio entre UI y pipeline de fondo
enum AudioCommand {
    CMD_START_PIPELINE = 0,
    CMD_START_OTA
};

#include "driver/i2c_master.h"

// Funciones públicas del módulo HMI
// IMPORTANTE: Estas funciones usan internamente lvgl_port_lock()/unlock()
// que es el mutex nativo del puerto LVGL. NO crear mutex adicionales.
void setupUIManager(i2c_master_bus_handle_t shared_i2c_bus = NULL);
void ui_set_state(UIState state, const char* infoText = NULL);
void ui_update_wifi_status(bool connected, int rssi = 0);

#endif // UI_MANAGER_H
