#include "OTA_Manager.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "UI_Manager.h" // Para actualizar la UI durante el OTA
#include "config.h"

static const char *TAG = "OTA_MANAGER";

// URL por defecto del bridge en Debian VM
#define OTA_URL "http://192.168.1.58:5000/firmware/firmware_v2.bin"

static esp_err_t _ota_http_client_init_cb(esp_http_client_handle_t client) {
#ifdef BRIDGE_AUTH_TOKEN
    if (strlen(BRIDGE_AUTH_TOKEN) > 0) {
        esp_http_client_set_header(client, "X-Bridge-Token", BRIDGE_AUTH_TOKEN);
    }
#endif
    return ESP_OK;
}

static void ota_task(void *pvParameter) {
    ESP_LOGI(TAG, "Iniciando descarga OTA desde: %s", OTA_URL);
    
    // Notificar a la UI
    ui_set_state(UI_STATE_THINKING, "Actualizando firmware (OTA)...");

    esp_http_client_config_t config = {};
    config.url = OTA_URL;
    config.timeout_ms = 10000;
    config.keep_alive_enable = true;

    // Configuración OTA: Ignoramos la validación del certificado por estar en LAN (aprobado en F4.2)
    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &config;
    ota_config.http_client_init_cb = _ota_http_client_init_cb;

    esp_err_t ret = esp_https_ota(&ota_config);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA descargado exitosamente. Reiniciando...");
        ui_set_state(UI_STATE_IDLE, "Actualización completa. Reiniciando...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Fallo en OTA. Error: %s", esp_err_to_name(ret));
        ui_set_state(UI_STATE_IDLE, "Fallo en la actualización OTA.");
    }
    
    vTaskDelete(NULL);
}

void start_ota_update(void) {
    xTaskCreatePinnedToCore(&ota_task, "ota_task", 8192, NULL, 5, NULL, 0);
}
