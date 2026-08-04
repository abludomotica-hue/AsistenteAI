/*
 * LVGL_UI_Manager.cpp
 * Reference architecture for thread-safe LVGL UI on ESP32-P4
 */

#include <Arduino.h>
#include <lvgl.h>

TaskHandle_t uiTaskHandle = NULL;
SemaphoreHandle_t xGuiSemaphore = NULL;

lv_obj_t* statusLabel;

void uiTask(void *pvParameters) {
    // 1. Initialize LVGL and Display Drivers here
    // lv_init();
    // display_init();
    
    // Create UI elements
    statusLabel = lv_label_create(lv_scr_act());
    lv_label_set_text(statusLabel, "Waiting...");
    lv_obj_align(statusLabel, LV_ALIGN_CENTER, 0, 0);

    while (1) {
        // Take the semaphore before calling any LVGL functions!
        if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
            lv_timer_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
        
        // Let the GUI task sleep to free up the CPU
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void setupUIManager() {
    // 1. Create a Mutex to protect LVGL from concurrent core access
    xGuiSemaphore = xSemaphoreCreateMutex();
    
    if (xGuiSemaphore != NULL) {
        // 2. Pin the UI task to Core 1 (APP_CPU)
        // Core 1 is ideal for UI while Core 0 handles Wi-Fi/Audio
        xTaskCreatePinnedToCore(
            uiTask,           // Task function
            "UITask",         // Task name
            32768,            // Stack size (UI requires large stack)
            NULL,             // Parameters
            2,                // Priority (2 is higher than idle and audio)
            &uiTaskHandle,    // Task handle
            1                 // Core 1 (1 = APP)
        );
    }
}

// How to update the UI from ANOTHER task safely (e.g. from the Audio Task):
void updateStatusFromAudioTask(const char* newStatus) {
    // ALWAYS take the semaphore if you are not inside the uiTask!
    if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
        lv_label_set_text(statusLabel, newStatus);
        xSemaphoreGive(xGuiSemaphore);
    }
}
