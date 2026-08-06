// =============================================================================
// UI_Manager.cpp — Motor HMI LVGL 9 para ESP32-P4 (Fase 3.1 Industrializada)
// =============================================================================
// ARQUITECTURA:
//   - El puerto LVGL (lvgl_port_v9.c) gestiona su PROPIA tarea y mutex.
//   - Touch GT911 en bus I2C (pines 7 y 8, liberados previamente por Wire.end).
//   - Fondo 100% Obsidian Dark (lv_scr_act) para eliminar márgenes blancos.
//   - Sanitizador UTF-8 para garantizar que fuentes LVGL no muestren recuadros [].
//   - Nomenclatura Estricta: Prohibido el uso del término "ajustes" -> "configurativo/configuración".
// =============================================================================

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
enum AudioCmdType : uint8_t { CMD_START_VOICE_PIPELINE = 0, CMD_PLAY_SPEAKER_PCM };

// Componentes gráficos en memoria LVGL (punteros estáticos)
static lv_obj_t* main_container = NULL;
static lv_obj_t* header_wifi_lbl = NULL;
static lv_obj_t* status_panel = NULL;
static lv_obj_t* state_icon_lbl = NULL;
static lv_obj_t* state_title_lbl = NULL;
static lv_obj_t* ai_spinner = NULL;
static lv_obj_t* info_text_lbl = NULL;
static lv_obj_t* btn_listen = NULL;
static lv_obj_t* btn_lbl = NULL;

// Dimensiones lógicas de LVGL tras rotación 90° (800 ancho × 480 alto)
#define UI_SCREEN_W  800
#define UI_SCREEN_H  480

// Configuración LEDC para control de brillo LCD
#define BSP_LCD_BACKLIGHT   GPIO_NUM_23
#define LCD_LEDC_CH         LEDC_CHANNEL_0

static void lcd_brightness_init() {
    const ledc_channel_config_t backlight_ch = {
        .gpio_num = BSP_LCD_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_1,
        .duty = 1023,
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

// ============================================================
// SANITIZADOR UTF-8 A ASCII (Evita recuadros [] en LVGL)
// ============================================================
static String clean_utf8_for_lvgl(const char* input) {
    if (!input) return String("");
    String out = "";
    size_t len = strlen(input);
    out.reserve(len);

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)input[i];
        if (c < 128) {
            out += (char)c;
        } else if (c == 0xC3 && (i + 1) < len) {
            unsigned char next = (unsigned char)input[++i];
            switch (next) {
                case 0xA1: case 0x81: out += 'a'; break; // á, Á -> a
                case 0xA9: case 0x89: out += 'e'; break; // é, É -> e
                case 0xAD: case 0x8D: out += 'i'; break; // í, Í -> i
                case 0xB3: case 0x93: out += 'o'; break; // ó, Ó -> o
                case 0xBA: case 0x9A: case 0xBC: case 0x9C: out += 'u'; break; // ú, Ú, ü -> u
                case 0xB1: case 0x91: out += 'n'; break; // ñ, Ñ -> n
                default: out += ' '; break;
            }
        } else if (c == 0xC2 && (i + 1) < len) {
            unsigned char next = (unsigned char)input[++i];
            if (next == 0xBF) out += '?'; // ¿ -> ?
            else if (next == 0xA1) out += '!'; // ¡ -> !
            else out += ' ';
        } else if (c >= 0xE0) {
            // Saltar secuencias multibyte de 3 o 4 bytes (ej. emojis 🗣️, ⚙️)
            if (c >= 0xF0) i += 3;
            else i += 2;
            if (i >= len) break;
            // No agregamos recuadros vacíos, simplemente ignoramos el emoji
        }
    }
    return out;
}

// ============================================================
// CALLBACK VSYNC — ¡OBLIGATORIO para prevenir bloqueo en flush!
// ============================================================
IRAM_ATTR static bool mipi_dsi_lcd_on_vsync_event(
    esp_lcd_panel_handle_t panel,
    esp_lcd_dpi_panel_event_data_t *edata,
    void *user_ctx)
{
    return lvgl_port_notify_lcd_vsync();
}

// Callback de botón táctil en pantalla
static void btn_listen_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        Serial.println("\n[UI Táctil] Botón presionado. Disparando al Core 0...");
        Serial.flush();
        interruptPlayback.store(true, std::memory_order_relaxed);
        uint8_t cmd = CMD_START_VOICE_PIPELINE;
        if (audioCommandQueue && xQueueSend(audioCommandQueue, &cmd, pdMS_TO_TICKS(50)) != pdTRUE) {
            Serial.println("[UI Táctil] Advertencia: Cola de audio llena.");
        }
    }
}

// Construcción del diseño visual (Obsidian Dark Mode 100% de pantalla)
static void build_ai_assistant_ui() {
    // 1. Pintar directamente la raíz del display para eliminar márgenes blancos
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x070B14), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // 2. Barra Superior (Header - 760x44)
    lv_obj_t* header = lv_obj_create(scr);
    lv_obj_set_size(header, 760, 44);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x111726), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_color(header, lv_color_hex(0x23314D), LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(header, 8, LV_PART_MAIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title_lbl = lv_label_create(header);
    lv_label_set_text(title_lbl, "EDGE AI VOICE ASSISTANT [P4 HMI]");
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(0x00E5FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 10, 0);

    header_wifi_lbl = lv_label_create(header);
    lv_label_set_text(header_wifi_lbl, "WiFi: ...");
    lv_obj_set_style_text_color(header_wifi_lbl, lv_color_hex(0xFFA500), LV_PART_MAIN);
    lv_obj_set_style_text_font(header_wifi_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(header_wifi_lbl, LV_ALIGN_RIGHT_MID, -10, 0);

    // 3. Panel Izquierdo (Estado Conversacional - 370x390)
    status_panel = lv_obj_create(scr);
    lv_obj_set_size(status_panel, 370, 395);
    lv_obj_align(status_panel, LV_ALIGN_TOP_LEFT, 20, 68);
    lv_obj_set_style_bg_color(status_panel, lv_color_hex(0x0D1220), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(status_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(status_panel, lv_color_hex(0x00E5FF), LV_PART_MAIN);
    lv_obj_set_style_border_width(status_panel, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(status_panel, 16, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(status_panel, lv_color_hex(0x00E5FF), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(status_panel, 15, LV_PART_MAIN);
    lv_obj_clear_flag(status_panel, LV_OBJ_FLAG_SCROLLABLE);

    // Spinner (oculto por defecto)
    ai_spinner = lv_spinner_create(status_panel);
    lv_obj_set_size(ai_spinner, 90, 90);
    lv_obj_align(ai_spinner, LV_ALIGN_CENTER, 0, -45);
    lv_obj_set_style_arc_color(ai_spinner, lv_color_hex(0x1E2B47), LV_PART_MAIN);
    lv_obj_set_style_arc_color(ai_spinner, lv_color_hex(0x9D00FF), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(ai_spinner, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ai_spinner, 8, LV_PART_INDICATOR);
    lv_obj_add_flag(ai_spinner, LV_OBJ_FLAG_HIDDEN);

    // Ícono de Estado
    state_icon_lbl = lv_label_create(status_panel);
    lv_label_set_text(state_icon_lbl, "[ READY ]");
    lv_obj_set_style_text_color(state_icon_lbl, lv_color_hex(0x00E5FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(state_icon_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(state_icon_lbl, LV_ALIGN_CENTER, 0, -45);

    // Título de Estado
    state_title_lbl = lv_label_create(status_panel);
    lv_label_set_text(state_title_lbl, "SISTEMA EN REPOSO");
    lv_obj_set_style_text_color(state_title_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(state_title_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(state_title_lbl, LV_ALIGN_CENTER, 0, 25);

    // Botón de Activación Táctil (330x60)
    btn_listen = lv_btn_create(status_panel);
    lv_obj_set_size(btn_listen, 330, 60);
    lv_obj_align(btn_listen, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(btn_listen, lv_color_hex(0x00FF88), LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_listen, lv_color_hex(0x00CC6A), (lv_style_selector_t)(LV_PART_MAIN | LV_STATE_PRESSED));
    lv_obj_set_style_radius(btn_listen, 30, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(btn_listen, lv_color_hex(0x00FF88), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn_listen, 12, LV_PART_MAIN);

    btn_lbl = lv_label_create(btn_listen);
    lv_label_set_text(btn_lbl, "TOCAR PARA HABLAR");
    lv_obj_set_style_text_color(btn_lbl, lv_color_hex(0x05131A), LV_PART_MAIN);
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(btn_lbl);
    lv_obj_add_event_cb(btn_listen, btn_listen_event_cb, LV_EVENT_CLICKED, NULL);

    // 4. Panel Derecho (Transcripción y Telemetría - 370x390)
    lv_obj_t* info_panel = lv_obj_create(scr);
    lv_obj_set_size(info_panel, 370, 395);
    lv_obj_align(info_panel, LV_ALIGN_TOP_RIGHT, -20, 68);
    lv_obj_set_style_bg_color(info_panel, lv_color_hex(0x101628), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(info_panel, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_color(info_panel, lv_color_hex(0x23314D), LV_PART_MAIN);
    lv_obj_set_style_border_width(info_panel, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(info_panel, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_all(info_panel, 18, LV_PART_MAIN);
    lv_obj_set_scroll_dir(info_panel, LV_DIR_VER);

    info_text_lbl = lv_label_create(info_panel);
    lv_obj_set_width(info_text_lbl, 330);
    lv_label_set_long_mode(info_text_lbl, LV_LABEL_LONG_WRAP);
    
    String msg = clean_utf8_for_lvgl(
        "Sistema HMI iniciado OK con Double Buffer en PSRAM.\n\n"
        "Touch GT911 activo por I2C.\n"
        "Toca el boton en pantalla o envia '1' por serie "
        "para conversar con NVIDIA Nemotron-3 en tiempo real."
    );
    lv_label_set_text(info_text_lbl, msg.c_str());
    lv_obj_set_style_text_color(info_text_lbl, lv_color_hex(0xD1D5DB), LV_PART_MAIN);
    lv_obj_set_style_text_font(info_text_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(info_text_lbl, LV_ALIGN_TOP_LEFT, 0, 0);
}

// ============================================================
// setupUIManager() — Inicialización HMI DSI + Touch GT911 + LVGL
// ============================================================
void setupUIManager() {
    Serial.println("\n[UI_Manager] Inicializando MIPI DSI HMI & Touch GT911...");
    lcd_brightness_init();

    // 1. Iniciar bus maestro I2C para Touch GT911 en pines 7 y 8
    // (Pines liberados previamente por Wire.end al concluir init del ES8311)
    i2c_master_bus_handle_t i2c_handle = NULL;
    i2c_master_bus_config_t i2c_bus_conf = {};
    i2c_bus_conf.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_bus_conf.i2c_port = I2C_NUM_1;
    i2c_bus_conf.sda_io_num = (gpio_num_t)7;
    i2c_bus_conf.scl_io_num = (gpio_num_t)8;
    i2c_bus_conf.flags.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_conf, &i2c_handle));

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
    dpi_config.num_fbs = LVGL_PORT_LCD_BUFFER_NUMS;

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

    // Registrar Callback VSYNC
    esp_lcd_dpi_panel_event_callbacks_t cbs = {};
#if LVGL_PORT_AVOID_TEAR_ENABLE
    cbs.on_refresh_done = mipi_dsi_lcd_on_vsync_event;
#else
    cbs.on_color_trans_done = mipi_dsi_lcd_on_vsync_event;
#endif
    esp_lcd_dpi_panel_register_event_callbacks(disp_panel, &cbs, NULL);

    // 2. Inicializar controlador Touch GT911 en el bus I2C
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_touch_handle_t tp_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_config.scl_speed_hz = 100000;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle));

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

    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp_handle));
    Serial.println("[UI_Manager] Touch GT911 vinculado exitosamente por I2C_NUM_1.");

    // 3. Inicializar puerto LVGL 9 con Display y Touch activos
    lvgl_port_interface_t interface = LVGL_PORT_INTERFACE_MIPI_DSI_DMA;
    ESP_ERROR_CHECK(lvgl_port_init(disp_panel, tp_handle, interface));

    // Encender retroiluminación LCD
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, 1023);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH);

    // Construir UI usando el mutex NATIVO del puerto LVGL
    if (lvgl_port_lock(-1)) {
        build_ai_assistant_ui();
        lvgl_port_unlock();
    }

    Serial.println("[UI_Manager] Motor LVGL 9 iniciado exitosamente en Core 1.");
}

// Función hilo-segura para cambiar estados desde Core 0 (incluye sanitizador ASCII)
void ui_set_state(UIState state, const char* infoText) {
    if (status_panel == NULL) return;
    if (lvgl_port_lock(100)) {
        lv_obj_add_flag(ai_spinner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(state_icon_lbl, LV_OBJ_FLAG_HIDDEN);

        switch(state) {
            case UI_STATE_IDLE:
                lv_obj_set_style_border_color(status_panel, lv_color_hex(0x00E5FF), LV_PART_MAIN);
                lv_obj_set_style_shadow_color(status_panel, lv_color_hex(0x00E5FF), LV_PART_MAIN);
                lv_label_set_text(state_icon_lbl, "[ READY ]");
                lv_obj_set_style_text_color(state_icon_lbl, lv_color_hex(0x00E5FF), LV_PART_MAIN);
                lv_label_set_text(state_title_lbl, "SISTEMA EN REPOSO");
                lv_obj_set_style_bg_color(btn_listen, lv_color_hex(0x00FF88), LV_PART_MAIN);
                lv_label_set_text(btn_lbl, "TOCAR PARA HABLAR");
                break;
            case UI_STATE_LISTENING:
                lv_obj_set_style_border_color(status_panel, lv_color_hex(0x00FF88), LV_PART_MAIN);
                lv_obj_set_style_shadow_color(status_panel, lv_color_hex(0x00FF88), LV_PART_MAIN);
                lv_label_set_text(state_icon_lbl, ">>> STREAMING <<<");
                lv_obj_set_style_text_color(state_icon_lbl, lv_color_hex(0x00FF88), LV_PART_MAIN);
                lv_label_set_text(state_title_lbl, "ESCUCHANDO...");
                lv_obj_set_style_bg_color(btn_listen, lv_color_hex(0xFFA500), LV_PART_MAIN);
                lv_label_set_text(btn_lbl, "GRABANDO AUDIO...");
                break;
            case UI_STATE_THINKING:
                lv_obj_add_flag(state_icon_lbl, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(ai_spinner, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_border_color(status_panel, lv_color_hex(0x9D00FF), LV_PART_MAIN);
                lv_obj_set_style_shadow_color(status_panel, lv_color_hex(0x9D00FF), LV_PART_MAIN);
                lv_label_set_text(state_title_lbl, "PROCESANDO...");
                lv_obj_set_style_bg_color(btn_listen, lv_color_hex(0x9D00FF), LV_PART_MAIN);
                lv_label_set_text(btn_lbl, "NUBE IA...");
                break;
            case UI_STATE_SPEAKING:
                lv_obj_set_style_border_color(status_panel, lv_color_hex(0xFF007F), LV_PART_MAIN);
                lv_obj_set_style_shadow_color(status_panel, lv_color_hex(0xFF007F), LV_PART_MAIN);
                lv_label_set_text(state_icon_lbl, "=== HABLANDO ===");
                lv_obj_set_style_text_color(state_icon_lbl, lv_color_hex(0xFF007F), LV_PART_MAIN);
                lv_label_set_text(state_title_lbl, "MAGPIE TTS");
                lv_obj_set_style_bg_color(btn_listen, lv_color_hex(0xFF007F), LV_PART_MAIN);
                lv_label_set_text(btn_lbl, "REPRODUCIENDO...");
                break;
        }

        if (infoText != NULL) {
            String clean = clean_utf8_for_lvgl(infoText);
            lv_label_set_text(info_text_lbl, clean.c_str());
        }
        lvgl_port_unlock();
    }
}

// Actualización de señal WiFi hilo-seguro
void ui_update_wifi_status(bool connected, int rssi) {
    if (header_wifi_lbl == NULL) return;
    if (lvgl_port_lock(50)) {
        char buf[32];
        if (connected) {
            snprintf(buf, sizeof(buf), "WiFi: %d dBm", rssi);
            lv_label_set_text(header_wifi_lbl, buf);
            lv_obj_set_style_text_color(header_wifi_lbl, lv_color_hex(0x00FF88), LV_PART_MAIN);
        } else {
            lv_label_set_text(header_wifi_lbl, "WiFi: OFF");
            lv_obj_set_style_text_color(header_wifi_lbl, lv_color_hex(0xFF3B30), LV_PART_MAIN);
        }
        lvgl_port_unlock();
    }
}
