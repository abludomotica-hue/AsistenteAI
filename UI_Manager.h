#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Semáforo Mutex para protección concurrente inter-núcleos (Core 0 vs Core 1)
extern SemaphoreHandle_t xGuiSemaphore;

// Estados conversacionales visuales del Asistente IA HMI
enum UIState {
    UI_STATE_IDLE = 0,      // Reposo / Reloj / Esperando orden
    UI_STATE_LISTENING,     // Escuchando / Captura I2S Activa
    UI_STATE_THINKING,      // Pensando / Procesando en NVIDIA NIM Cloud
    UI_STATE_SPEAKING       // Hablando / Reproduciendo stream de voz Magpie TTS
};

// Funciones públicas del módulo HMI
void setupUIManager();
void ui_set_state(UIState state, const char* infoText = NULL);
void ui_update_wifi_status(bool connected, int rssi = 0);

#endif // UI_MANAGER_H
