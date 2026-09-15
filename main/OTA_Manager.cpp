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

#include "esp_ota_ops.h"
#include "esp_app_format.h"

static void ota_task(void *pvParameter) {
    ESP_LOGI(TAG, "Iniciando descarga OTA progresiva desde: %s", OTA_URL);
    
    ui_set_state(UI_STATE_THINKING, "Conectando al servidor OTA...");

    esp_http_client_config_t config = {};
    config.url = OTA_URL;
    config.timeout_ms = 15000;
    config.keep_alive_enable = true;

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &config;
    ota_config.http_client_init_cb = _ota_http_client_init_cb;

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_begin falló: %s", esp_err_to_name(err));
        ui_set_state(UI_STATE_IDLE, "Fallo al iniciar OTA");
        vTaskDelete(NULL);
        return;
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Nuevo firmware verificado - Proyecto: '%s', Versión: '%s'", 
                 app_desc.project_name, app_desc.version);
    }

    int total_size = esp_https_ota_get_image_size(https_ota_handle);
    int last_percent = -1;

    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        
        int bytes_read = esp_https_ota_get_image_len_read(https_ota_handle);
        if (total_size > 0) {
            int percent = (bytes_read * 100) / total_size;
            if (percent != last_percent && (percent % 5 == 0 || percent == 100)) {
                last_percent = percent;
                char msg[64];
                snprintf(msg, sizeof(msg), "Actualizando OTA: %d%%", percent);
                ui_set_state(UI_STATE_THINKING, msg);
                ESP_LOGI(TAG, "Progreso OTA: %d%% (%d / %d bytes)", percent, bytes_read, total_size);
            }
        }
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_perform falló: %s", esp_err_to_name(err));
        esp_https_ota_abort(https_ota_handle);
        ui_set_state(UI_STATE_IDLE, "Fallo en descarga OTA");
        vTaskDelete(NULL);
        return;
    }

    esp_err_t ota_finish_err = esp_https_ota_finish(https_ota_handle);
    if (ota_finish_err == ESP_OK) {
        ESP_LOGI(TAG, "¡Firmware OTA escrito y verificado exitosamente! Reiniciando...");
        ui_set_state(UI_STATE_IDLE, "OTA completado al 100%.\nReiniciando sistema...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "esp_https_ota_finish falló: %s", esp_err_to_name(ota_finish_err));
        ui_set_state(UI_STATE_IDLE, "Error de verificación OTA");
    }
    
    vTaskDelete(NULL);
}

void start_ota_update(void) {
    xTaskCreatePinnedToCore(&ota_task, "ota_task", 8192, NULL, 5, NULL, 0);
}
