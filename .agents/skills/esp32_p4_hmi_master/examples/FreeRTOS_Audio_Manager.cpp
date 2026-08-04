/*
 * FreeRTOS_Audio_Manager.cpp
 * Reference architecture for non-blocking I2S Audio on ESP32-P4
 */

#include <Arduino.h>
#include <ESP_I2S.h>
#include <esp_heap_caps.h>

I2SClass i2s;
TaskHandle_t audioTaskHandle = NULL;
QueueHandle_t audioCommandQueue;

enum AudioCommand {
    CMD_IDLE,
    CMD_RECORD,
    CMD_PLAY
};

void audioTask(void *pvParameters) {
    AudioCommand cmd;
    
    // Allocate large audio buffer in PSRAM, NEVER on stack!
    const size_t BUFFER_SIZE = 128000; 
    uint8_t* audioBuffer = (uint8_t*)heap_caps_malloc(BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    
    if (audioBuffer == NULL) {
        Serial.println("Error: No PSRAM available for Audio Buffer");
        vTaskDelete(NULL);
    }

    while (1) {
        // Wait for a command indefinitely
        if (xQueueReceive(audioCommandQueue, &cmd, portMAX_DELAY) == pdTRUE) {
            
            if (cmd == CMD_RECORD) {
                Serial.println("Audio Task: Recording started...");
                // Note: Always use esp_timer_get_time() or millis() to prevent tight blocking loops
                unsigned long startTime = millis();
                size_t bytesReadTotal = 0;
                
                while (millis() - startTime < 4000 && bytesReadTotal < BUFFER_SIZE) {
                    size_t bytesRead = i2s.read(audioBuffer + bytesReadTotal, BUFFER_SIZE - bytesReadTotal);
                    bytesReadTotal += bytesRead;
                    // Yield to FreeRTOS watchdog
                    vTaskDelay(pdMS_TO_TICKS(10)); 
                }
                
                Serial.println("Audio Task: Recording finished. Processing HTTP upload...");
                // Perform heavy HTTP POST here on Core 0 so Core 1 (UI) doesn't freeze
                
            } else if (cmd == CMD_PLAY) {
                Serial.println("Audio Task: Playing...");
                // Play logic here
            }
        }
    }
}

void setupAudioManager() {
    // 1. Create the command queue
    audioCommandQueue = xQueueCreate(10, sizeof(AudioCommand));
    
    // 2. Initialize I2S Pins (Refer to pinout.md)
    i2s.setPins(21, 22, 23, 24); // BCLK, WS, DOUT, DIN
    i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_BOTH);
    
    // 3. Pin the heavy audio task to Core 0 (PRO_CPU)
    xTaskCreatePinnedToCore(
        audioTask,          // Task function
        "AudioTask",        // Task name
        16384,              // Stack size (bytes)
        NULL,               // Parameters
        1,                  // Priority (1 is slightly above idle)
        &audioTaskHandle,   // Task handle
        0                   // Core 0 (0 = PRO, 1 = APP)
    );
}

// How to trigger from main loop or buttons:
// AudioCommand cmd = CMD_RECORD;
// xQueueSend(audioCommandQueue, &cmd, portMAX_DELAY);
