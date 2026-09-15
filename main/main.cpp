// =============================================================================
// main.cpp — Sistema Asistente IA Edge (ESP32-P4 + HMI LVGL 9 + Audio + WiFi)
// =============================================================================

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "driver/gpio.h"

// Audio & Periféricos
// No ADF includes

#include "audio_manager.h"
#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "driver/i2c_master.h"

// Módulos locales
#include "config.h"
#include "pins_config.h"
#include "UI_Manager.h"
#include "OTA_Manager.h"
#include <atomic>
#include <string>

static const char *TAG = "MAIN";

// ==========================================
// GLOBALS & STATE
// ==========================================
std::atomic<bool> interruptPlayback(false);
std::atomic<bool> isPipelineRunning(false);
QueueHandle_t audioCommandQueue = NULL;

extern "C" void ES8311_Init(i2c_master_bus_handle_t bus_handle);

// ==========================================
// WIFI SETUP (Event-driven, no bloqueante)
// ==========================================
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA iniciado. Conectando a '%s'...", WIFI_SSID);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* disconn = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "WiFi desconectado (Razón ID=%d). Reintentando conexión a '%s'...", 
                 disconn ? disconn->reason : 0, WIFI_SSID);
        ui_update_wifi_status(false, 0);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "¡WiFi Conectado con éxito! IP asignada: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            ui_update_wifi_status(true, ap_info.rssi);
        }
        ui_set_state(UI_STATE_IDLE, "WiFi Conectado exitosamente.\nPresiona el boton 'TOCAR PARA HABLAR' para interactuar con la IA.");
    }
}

void setup_wifi(void) {
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password));
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));

    // Asignar dirección MAC física válida al adaptador WiFi STA (evita MAC 00:00:00:00:00:00 que los routers bloquean)
    uint8_t mac[6] = {0};
    if (esp_efuse_mac_get_default(mac) == ESP_OK) {
        mac[5] ^= 0x02; // Asignar bit local/STA
        ESP_LOGI(TAG, "Asignando MAC STA de hardware: %02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        esp_wifi_set_mac(WIFI_IF_STA, mac);
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// ==========================================
// TAREAS MOVIDAS AL AUDIO_MANAGER
// ==========================================
// Las tareas de audio_task y wakenet_task han sido movidas y
// reimplementadas de forma real en audio_manager.cpp

// ==========================================
// APP MAIN
// ==========================================
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "--- INICIANDO SISTEMA (ESP-IDF v5) ---");

    // 0. Reset de hardware del coprocesador WiFi ESP32-C6 (gestionado por esp_hosted/esp_wifi_remote)
    ESP_LOGI(TAG, "Coprocesador WiFi ESP32-C6 será gestionado por esp_wifi_remote.");

    // 1. Activar amplificador de audio (NS4168 PA en GPIO 11)
    gpio_config_t pa_gpio_config = {
        .pin_bit_mask = (1ULL << GPIO_NUM_11),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pa_gpio_config);
    gpio_set_level(GPIO_NUM_11, 1);
    ESP_LOGI(TAG, "Amplificador PA activado en GPIO 11.");

    // 2. Inicializar Pantalla LVGL 9 mediante BSP oficial
    setupUIManager();

    // 3. Inicializar Codec ES8311 usando el bus maestro I2C oficial del BSP
    i2c_master_bus_handle_t bsp_i2c = bsp_i2c_get_handle();
    if (bsp_i2c != NULL) {
        ES8311_Init(bsp_i2c);
    } else {
        ESP_LOGE(TAG, "Error: BSP I2C bus no disponible para ES8311.");
    }

    // 4. Inicializar NVS y WiFi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    setup_wifi();

    // 5. Inicializar Audio Manager y Colas
    audioCommandQueue = xQueueCreate(5, sizeof(AudioCommand));
    audio_manager_init();

    ESP_LOGI(TAG, "Asistente Inicializado y Operativo.");

    // Mantener viva la tarea principal
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
