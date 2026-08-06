#include "UI_Manager.h"
#include <lvgl.h>
#include "lvgl_port_v9.h"
#include "pins_config.h"
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <driver/i2c_master.h>
#include <esp_ldo_regulator.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_mipi_dsi.h>
#include "src/touch/esp_lcd_touch_gt911.h"
#include "src/lcd/esp_lcd_st7701.h"
#include <atomic>

// Declaración externa para activación táctil (Barge-in hacia Core 0)
extern std::atomic<bool> interruptPlayback;
extern QueueHandle_t audioCommandQueue;
// Reconstrucción local del enum para encolar sin dependencia circular de AsistenteAI.ino
enum AudioCmdType : uint8_t { CMD_START_VOICE_PIPELINE = 0, CMD_PLAY_SPEAKER_PCM };

TaskHandle_t uiTaskHandle = NULL;
SemaphoreHandle_t xGuiSemaphore = NULL;

// Componentes gráficos en memoria LVGL
static lv_obj_t* main_container = NULL;
static lv_obj_t* header_wifi_lbl = NULL;
static lv_obj_t* status_panel = NULL;
static lv_obj_t* state_icon_lbl = NULL;
static lv_obj_t* state_title_lbl = NULL;
static lv_obj_t* ai_spinner = NULL;
static lv_obj_t* info_text_lbl = NULL;
static lv_obj_t* btn_listen = NULL;
static lv_obj_t* btn_lbl = NULL;

// Configuración LEDC para control de brillo de pantalla MIPI DSI
#define BSP_LCD_BACKLIGHT   GPIO_NUM_23
#define LCD_LEDC_CH         LEDC_CHANNEL_0

static void lcd_brightness_init() {
    const ledc_channel_config_t backlight_ch = {
        .gpio_num = BSP_LCD_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_1,
        .duty = 1023, // 100% de brillo por defecto (10 bits: 0-1023)
        .hpoint = 0
    };
    const ledc_timer_config_t backlight_tmr = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_1,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&backlight_tmr);
    ledc_channel_config(&backlight_ch);
}

// Callback de evento al presionar el botón de escucha en la pantalla táctil
static void btn_listen_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        Serial.println("\n[UI Táctil] Botón de escucha activado en Core 1. Disparando al Core 0...");
        Serial.flush();
        interruptPlayback.store(true, std::memory_order_relaxed); // Señal de Barge-In atómica
        uint8_t cmd = CMD_START_VOICE_PIPELINE;
        if (audioCommandQueue && xQueueSend(audioCommandQueue, &cmd, pdMS_TO_TICKS(50)) != pdTRUE) {
            Serial.println("[UI Táctil] Advertencia: Cola de audio llena.");
        } else {
            ui_set_state(UI_STATE_LISTENING, "Capturando voz por micrófono I2S (Max 5s silencio timeout)...");
        }
    }
}

// Construcción del diseño visual Premium (Glassmorphism Dark Mode)
static void build_ai_assistant_ui() {
    // 1. Contenedor Raíz (Obsidian Dark Mode)
    main_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(main_container, 480, 800);
    lv_obj_set_style_bg_color(main_container, lv_color_hex(0x0A0F1D), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(main_container, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(main_container, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(main_container, 20, LV_PART_MAIN);
    lv_obj_clear_flag(main_container, LV_OBJ_FLAG_SCROLLABLE);

    // 2. Barra Superior (Header - Estado y Sistema)
    lv_obj_t* header = lv_obj_create(main_container);
    lv_obj_set_size(header, 440, 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x151C30), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_color(header, lv_color_hex(0x2E3D5C), LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(header, 12, LV_PART_MAIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title_lbl = lv_label_create(header);
    lv_label_set_text(title_lbl, "EDGE AI VOICE ASSISTANT");
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(0x00E5FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 5, 0);

    header_wifi_lbl = lv_label_create(header);
    lv_label_set_text(header_wifi_lbl, "WiFi: Búsqueda...");
    lv_obj_set_style_text_color(header_wifi_lbl, lv_color_hex(0xFFA500), LV_PART_MAIN);
    lv_obj_set_style_text_font(header_wifi_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(header_wifi_lbl, LV_ALIGN_RIGHT_MID, -5, 0);

    // 3. Panel Central Interactivo y Visualizador (Telemetry & AI State)
    status_panel = lv_obj_create(main_container);
    lv_obj_set_size(status_panel, 440, 380);
    lv_obj_align(status_panel, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_set_style_bg_color(status_panel, lv_color_hex(0x121829), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(status_panel, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_color(status_panel, lv_color_hex(0x00E5FF), LV_PART_MAIN);
    lv_obj_set_style_border_width(status_panel, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(status_panel, 20, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(status_panel, lv_color_hex(0x00E5FF), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(status_panel, 15, LV_PART_MAIN);
    lv_obj_clear_flag(status_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Spinner Animado para Estado "Pensando" (Inicialmente oculto)
    ai_spinner = lv_spinner_create(status_panel);
    lv_obj_set_size(ai_spinner, 100, 100);
    lv_obj_align(ai_spinner, LV_ALIGN_CENTER, 0, -50);
    lv_obj_set_style_arc_color(ai_spinner, lv_color_hex(0x2E3D5C), LV_PART_MAIN);
    lv_obj_set_style_arc_color(ai_spinner, lv_color_hex(0x9D00FF), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(ai_spinner, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ai_spinner, 10, LV_PART_INDICATOR);
    lv_obj_add_flag(ai_spinner, LV_OBJ_FLAG_HIDDEN);

    // Ícono de Estado en Centro del Panel
    state_icon_lbl = lv_label_create(status_panel);
    lv_label_set_text(state_icon_lbl, "[ O READY O ]");
    lv_obj_set_style_text_color(state_icon_lbl, lv_color_hex(0x00E5FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(state_icon_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(state_icon_lbl, LV_ALIGN_CENTER, 0, -50);

    // Título de Estado
    state_title_lbl = lv_label_create(status_panel);
    lv_label_set_text(state_title_lbl, "SISTEMA EN REPOSO");
    lv_obj_set_style_text_color(state_title_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(state_title_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(state_title_lbl, LV_ALIGN_CENTER, 0, 30);

    // 4. Panel Inferior para Transcripción y Respuesta AI
    lv_obj_t* info_panel = lv_obj_create(main_container);
    lv_obj_set_size(info_panel, 440, 200);
    lv_obj_align(info_panel, LV_ALIGN_TOP_MID, 0, 465);
    lv_obj_set_style_bg_color(info_panel, lv_color_hex(0x161D33), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(info_panel, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_color(info_panel, lv_color_hex(0x2D3A5D), LV_PART_MAIN);
    lv_obj_set_style_border_width(info_panel, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(info_panel, 16, LV_PART_MAIN);

    info_text_lbl = lv_label_create(info_panel);
    lv_obj_set_width(info_text_lbl, 400);
    lv_label_set_long_mode(info_text_lbl, LV_LABEL_LONG_WRAP);
    lv_label_set_text(info_text_lbl, "Sistema iniciado en Core 1 con Double Buffer PSRAM a 60 FPS.\n\nPresiona el botón inferior o ingresa '1' en consola para conversar con NVIDIA Nemotron-3 y Parakeet ASR.");
    lv_obj_set_style_text_color(info_text_lbl, lv_color_hex(0xD1D5DB), LV_PART_MAIN);
    lv_obj_set_style_text_font(info_text_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(info_text_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    // 5. Botón de Activación Táctil (WAKE & SPEAK)
    btn_listen = lv_btn_create(main_container);
    lv_obj_set_size(btn_listen, 440, 70);
    lv_obj_align(btn_listen, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(btn_listen, lv_color_hex(0x00FF88), LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_listen, lv_color_hex(0x00CC6A), (lv_style_selector_t)(LV_PART_MAIN | LV_STATE_PRESSED));
    lv_obj_set_style_radius(btn_listen, 35, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(btn_listen, lv_color_hex(0x00FF88), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn_listen, 15, LV_PART_MAIN);

    btn_lbl = lv_label_create(btn_listen);
    lv_label_set_text(btn_lbl, "🎙️ TOCAR PARA HABLAR (INTERACCION)");
    lv_obj_set_style_text_color(btn_lbl, lv_color_hex(0x05131A), LV_PART_MAIN);
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(btn_lbl);

    lv_obj_add_event_cb(btn_listen, btn_listen_event_cb, LV_EVENT_CLICKED, NULL);
}

// Bucle de renderizado dedicado al Core 1
static void uiTask(void *pvParameters) {
    Serial.println("[Core 1] Tarea de interfaz gráfica (UI Task) en ejecución a 60 FPS...");
    while(1) {
        if (xGuiSemaphore != NULL && xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
            lv_timer_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
        vTaskDelay(pdMS_TO_TICKS(5)); // Ceder CPU voluntario y evitar inanición de tareas
    }
}

// Inicialización de hardware y puerto LVGL en ESP32-P4
void setupUIManager() {
    Serial.println("\n[UI_Manager] Inicializando hardware MIPI DSI HMI y panel táctil...");
    lcd_brightness_init();

    // Crear bus maestro I2C para el Touch Panel GT911
    i2c_master_bus_handle_t i2c_handle = NULL;
    i2c_master_bus_config_t i2c_bus_conf = {};
    i2c_bus_conf.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_bus_conf.i2c_port = I2C_NUM_1;
    i2c_bus_conf.sda_io_num = (gpio_num_t)7;
    i2c_bus_conf.scl_io_num = (gpio_num_t)8;
    i2c_new_master_bus(&i2c_bus_conf, &i2c_handle);

    // Alimentar PHY del MIPI DSI (LDO VO3 a 2500 mV)
    static esp_ldo_channel_handle_t phy_pwr_chan = NULL;
    esp_ldo_channel_config_t ldo_cfg = {};
    ldo_cfg.chan_id = 3;
    ldo_cfg.voltage_mv = 2500;
    esp_ldo_acquire_channel(&ldo_cfg, &phy_pwr_chan);

    // Configuración DSI & Panel ST7701
    esp_lcd_dsi_bus_handle_t mipi_dsi_bus = NULL;
    esp_lcd_dsi_bus_config_t bus_config = ST7701_PANEL_BUS_DSI_2CH_CONFIG();
    esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus);

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_dbi_io_config_t dbi_config = ST7701_PANEL_IO_DBI_CONFIG();
    esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &io);

    esp_lcd_panel_handle_t disp_panel = NULL;
    esp_lcd_dpi_panel_config_t dpi_config = ST7701_480_360_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
    dpi_config.num_fbs = 2; // Double buffering para 60 FPS fluidos en PSRAM

    st7701_vendor_config_t vendor_config = {};
    vendor_config.mipi_config.dsi_bus = mipi_dsi_bus;
    vendor_config.mipi_config.dpi_config = &dpi_config;
    vendor_config.flags.use_mipi_interface = 1;

    esp_lcd_panel_dev_config_t lcd_dev_config = {};
    lcd_dev_config.reset_gpio_num = (gpio_num_t)5;
    lcd_dev_config.rgb_ele_order = ESP_LCD_COLOR_SPACE_RGB;
    lcd_dev_config.bits_per_pixel = 16;
    lcd_dev_config.vendor_config = &vendor_config;

    esp_lcd_new_panel_st7701(io, &lcd_dev_config, &disp_panel);
    esp_lcd_panel_reset(disp_panel);
    esp_lcd_panel_init(disp_panel);

    // Touch panel GT911
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_touch_handle_t tp_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_config.scl_speed_hz = 100000;
    esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle);

    esp_lcd_touch_config_t tp_cfg = {};
    tp_cfg.x_max = 480;
    tp_cfg.y_max = 800;
    tp_cfg.rst_gpio_num = GPIO_NUM_NC;
    tp_cfg.int_gpio_num = GPIO_NUM_NC;
    tp_cfg.levels.reset = 0;
    tp_cfg.levels.interrupt = 0;
    tp_cfg.flags.swap_xy = 0;
    tp_cfg.flags.mirror_x = 0;
    tp_cfg.flags.mirror_y = 0;

    esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp_handle);

    // Inicializar puerto LVGL 9 por MIPI DSI DMA
    lvgl_port_interface_t interface = LVGL_PORT_INTERFACE_MIPI_DSI_DMA;
    lvgl_port_init(disp_panel, tp_handle, interface);

    // Crear Mutex e Inicializar Diseño Gráfico
    xGuiSemaphore = xSemaphoreCreateMutex();
    if (xGuiSemaphore != NULL) {
        if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
            build_ai_assistant_ui();
            xSemaphoreGive(xGuiSemaphore);
        }
        // Anclar tarea al Core 1 con pila de 32KB y prioridad 2
        xTaskCreatePinnedToCore(uiTask, "UITask", 32768, NULL, 2, &uiTaskHandle, 1);
        Serial.println("[UI_Manager] Motor LVGL 9 iniciado exitosamente en Core 1.");
    } else {
        Serial.println("[UI_Manager] ¡ERROR FATAL al crear semáforo xGuiSemaphore!");
    }
}

// Función hilo-segura para cambiar estados desde cualquier tarea de Core 0 o 1
void ui_set_state(UIState state, const char* infoText) {
    if (xGuiSemaphore == NULL || status_panel == NULL) return;
    if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
        lv_obj_add_flag(ai_spinner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(state_icon_lbl, LV_OBJ_FLAG_HIDDEN);

        switch(state) {
            case UI_STATE_IDLE:
                lv_obj_set_style_border_color(status_panel, lv_color_hex(0x00E5FF), LV_PART_MAIN);
                lv_obj_set_style_shadow_color(status_panel, lv_color_hex(0x00E5FF), LV_PART_MAIN);
                lv_label_set_text(state_icon_lbl, "[ O READY O ]");
                lv_obj_set_style_text_color(state_icon_lbl, lv_color_hex(0x00E5FF), LV_PART_MAIN);
                lv_label_set_text(state_title_lbl, "SISTEMA EN REPOSO");
                lv_obj_set_style_bg_color(btn_listen, lv_color_hex(0x00FF88), LV_PART_MAIN);
                lv_label_set_text(btn_lbl, "🎙️ TOCAR PARA HABLAR (INTERACCION)");
                break;
            case UI_STATE_LISTENING:
                lv_obj_set_style_border_color(status_panel, lv_color_hex(0x00FF88), LV_PART_MAIN);
                lv_obj_set_style_shadow_color(status_panel, lv_color_hex(0x00FF88), LV_PART_MAIN);
                lv_label_set_text(state_icon_lbl, "[[[ 🎙️ STREAMING ]]]");
                lv_obj_set_style_text_color(state_icon_lbl, lv_color_hex(0x00FF88), LV_PART_MAIN);
                lv_label_set_text(state_title_lbl, "ESCUCHANDO AUDIO I2S...");
                lv_obj_set_style_bg_color(btn_listen, lv_color_hex(0xFFA500), LV_PART_MAIN);
                lv_label_set_text(btn_lbl, "⏳ GRABANDO AUDIO...");
                break;
            case UI_STATE_THINKING:
                lv_obj_add_flag(state_icon_lbl, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ai_spinner, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_border_color(status_panel, lv_color_hex(0x9D00FF), LV_PART_MAIN);
                lv_obj_set_style_shadow_color(status_panel, lv_color_hex(0x9D00FF), LV_PART_MAIN);
                lv_label_set_text(state_title_lbl, "PROCESANDO CON NEMOTRON...");
                lv_obj_set_style_bg_color(btn_listen, lv_color_hex(0x9D00FF), LV_PART_MAIN);
                lv_label_set_text(btn_lbl, "⚙️ CONECTANDO CON NUBE IA...");
                break;
            case UI_STATE_SPEAKING:
                lv_obj_set_style_border_color(status_panel, lv_color_hex(0xFF007F), LV_PART_MAIN);
                lv_obj_set_style_shadow_color(status_panel, lv_color_hex(0xFF007F), LV_PART_MAIN);
                lv_label_set_text(state_icon_lbl, "=== | ||| | ||| | ===");
                lv_obj_set_style_text_color(state_icon_lbl, lv_color_hex(0xFF007F), LV_PART_MAIN);
                lv_label_set_text(state_title_lbl, "HABLANDO (MAGPIE TTS)");
                lv_obj_set_style_bg_color(btn_listen, lv_color_hex(0xFF007F), LV_PART_MAIN);
                lv_label_set_text(btn_lbl, "🔊 REPRODUCIENDO EN ALTAVOZ...");
                break;
        }

        if (infoText != NULL) {
            lv_label_set_text(info_text_lbl, infoText);
        }
        xSemaphoreGive(xGuiSemaphore);
    }
}

// Actualizador de señal WiFi hilo-seguro
void ui_update_wifi_status(bool connected, int rssi) {
    if (xGuiSemaphore == NULL || header_wifi_lbl == NULL) return;
    if (xSemaphoreTake(xGuiSemaphore, portMAX_DELAY) == pdTRUE) {
        char buf[32];
        if (connected) {
            snprintf(buf, sizeof(buf), "WiFi: OK (%d dBm)", rssi);
            lv_label_set_text(header_wifi_lbl, buf);
            lv_obj_set_style_text_color(header_wifi_lbl, lv_color_hex(0x00FF88), LV_PART_MAIN);
        } else {
            lv_label_set_text(header_wifi_lbl, "WiFi: DESCONECTADO");
            lv_obj_set_style_text_color(header_wifi_lbl, lv_color_hex(0xFF3B30), LV_PART_MAIN);
        }
        xSemaphoreGive(xGuiSemaphore);
    }
}
