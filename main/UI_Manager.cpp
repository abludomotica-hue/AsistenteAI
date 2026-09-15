// =============================================================================
// UI_Manager.cpp — Motor HMI LVGL 9 para ESP32-P4 (Obsidian Dark Portrait 480x800)
// =============================================================================
// ARQUITECTURA:
//   - Utiliza el BSP oficial del fabricante Guition (bsp_display_start_with_config)
//   - Resolución Vertical Nativa: 480 px (ancho) x 800 px (alto)
//   - Componentes no superpuestos, diseño industrial ergonómico de 3 zonas
//   - Mutex hilo-seguro: bsp_display_lock() y bsp_display_unlock()
// =============================================================================

#include "UI_Manager.h"
#include <lvgl.h>
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp_board_extra.h"
#include <esp_log.h>
#include <string>
#include <cstring>
#include <atomic>

static const char *TAG = "UI_Manager";

// Declaración externa para activación táctil (Barge-in hacia Core 0)
extern std::atomic<bool> interruptPlayback;
extern QueueHandle_t audioCommandQueue;

// Componentes gráficos en memoria LVGL (punteros estáticos)
static lv_obj_t* header_wifi_lbl = NULL;
static lv_obj_t* status_panel = NULL;
static lv_obj_t* state_icon_lbl = NULL;
static lv_obj_t* state_title_lbl = NULL;
static lv_obj_t* ai_spinner = NULL;
static lv_obj_t* info_text_lbl = NULL;
static lv_obj_t* btn_listen = NULL;
static lv_obj_t* btn_lbl = NULL;

// Sanitizador UTF-8 a ASCII (Evita recuadros [] en fuentes estándar)
static std::string clean_utf8_for_lvgl(const char* input) {
    if (!input) return std::string("");
    std::string out = "";
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
            if (next == 0xBF) out += '?';
            else if (next == 0xA1) out += '!';
            else out += ' ';
        } else if (c >= 0xE0) {
            if (c >= 0xF0) i += 3;
            else i += 2;
            if (i >= len) break;
        }
    }
    return out;
}

// Callback de botón táctil
static void btn_listen_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_PRESSED) {
        ESP_LOGI(TAG, "[UI Táctil] ¡BOTÓN PRESIONADO! Disparando pipeline de voz...");
        interruptPlayback.store(true, std::memory_order_relaxed);
        AudioCommand cmd = CMD_START_PIPELINE;
        if (audioCommandQueue && xQueueSend(audioCommandQueue, &cmd, pdMS_TO_TICKS(50)) != pdTRUE) {
            ESP_LOGW(TAG, "[UI Táctil] Advertencia: Cola de audio llena.");
        }
    }
}

// Depuración global de pulsación táctil
static void screen_touch_debug_cb(lv_event_t *e) {
    lv_indev_t * indev = lv_indev_active();
    if(indev) {
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        ESP_LOGI(TAG, "[Touch Debug] Toque detectado en coordenadas (X=%d, Y=%d)", (int)p.x, (int)p.y);
    }
}

// Construcción del diseño visual ergonómico (Obsidian Dark Mode Vertical 480x800)
static void build_ai_assistant_ui() {
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x070B14), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr, screen_touch_debug_cb, LV_EVENT_PRESSED, NULL);

    // ========================================================
    // 1. BARRA SUPERIOR (Header - 450x44 px)
    // ========================================================
    lv_obj_t* header = lv_obj_create(scr);
    lv_obj_set_size(header, 450, 44);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x111726), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_color(header, lv_color_hex(0x23314D), LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(header, 10, LV_PART_MAIN);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title_lbl = lv_label_create(header);
    lv_label_set_text(title_lbl, "ESP32-P4 + NEMOTRON");
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(0x00E5FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(title_lbl, LV_ALIGN_LEFT_MID, 5, 0);

    header_wifi_lbl = lv_label_create(header);
    lv_label_set_text(header_wifi_lbl, "WiFi: ...");
    lv_obj_set_style_text_color(header_wifi_lbl, lv_color_hex(0xFFA500), LV_PART_MAIN);
    lv_obj_set_style_text_font(header_wifi_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(header_wifi_lbl, LV_ALIGN_RIGHT_MID, -5, 0);

    // ========================================================
    // 2. PANEL DE ESTADO CONVERSACIONAL (Superior - 450x280 px)
    // ========================================================
    status_panel = lv_obj_create(scr);
    lv_obj_set_size(status_panel, 450, 280);
    lv_obj_align(status_panel, LV_ALIGN_TOP_MID, 0, 64);
    lv_obj_set_style_bg_color(status_panel, lv_color_hex(0x0D1220), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(status_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(status_panel, lv_color_hex(0x00E5FF), LV_PART_MAIN);
    lv_obj_set_style_border_width(status_panel, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(status_panel, 16, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(status_panel, lv_color_hex(0x00E5FF), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(status_panel, 12, LV_PART_MAIN);
    lv_obj_clear_flag(status_panel, LV_OBJ_FLAG_SCROLLABLE);

    ai_spinner = lv_spinner_create(status_panel);
    lv_obj_set_size(ai_spinner, 80, 80);
    lv_obj_align(ai_spinner, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_style_arc_color(ai_spinner, lv_color_hex(0x1E2B47), LV_PART_MAIN);
    lv_obj_set_style_arc_color(ai_spinner, lv_color_hex(0x9D00FF), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(ai_spinner, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ai_spinner, 8, LV_PART_INDICATOR);
    lv_obj_add_flag(ai_spinner, LV_OBJ_FLAG_HIDDEN);

    state_icon_lbl = lv_label_create(status_panel);
    lv_label_set_text(state_icon_lbl, "[ LISTO ]");
    lv_obj_set_style_text_color(state_icon_lbl, lv_color_hex(0x00E5FF), LV_PART_MAIN);
    lv_obj_set_style_text_font(state_icon_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(state_icon_lbl, LV_ALIGN_TOP_MID, 0, 45);

    state_title_lbl = lv_label_create(status_panel);
    lv_label_set_text(state_title_lbl, "SISTEMA EN REPOSO");
    lv_obj_set_style_text_color(state_title_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(state_title_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(state_title_lbl, LV_ALIGN_TOP_MID, 0, 115);

    // Botón Táctil Amplio (400x60 px)
    btn_listen = lv_btn_create(status_panel);
    lv_obj_set_size(btn_listen, 400, 60);
    lv_obj_align(btn_listen, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(btn_listen, lv_color_hex(0x00FF88), LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_listen, lv_color_hex(0x00CC6A), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn_listen, 30, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(btn_listen, lv_color_hex(0x00FF88), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn_listen, 10, LV_PART_MAIN);

    btn_lbl = lv_label_create(btn_listen);
    lv_label_set_text(btn_lbl, "TOCAR PARA HABLAR");
    lv_obj_set_style_text_color(btn_lbl, lv_color_hex(0x05131A), LV_PART_MAIN);
    lv_obj_set_style_text_font(btn_lbl, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_center(btn_lbl);
    lv_obj_add_event_cb(btn_listen, btn_listen_event_cb, LV_EVENT_ALL, NULL);

    // ========================================================
    // 3. PANEL DE CONVERSACION Y TELEMETRIA (Inferior - 450x430 px)
    // ========================================================
    lv_obj_t* info_panel = lv_obj_create(scr);
    lv_obj_set_size(info_panel, 450, 430);
    lv_obj_align(info_panel, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_bg_color(info_panel, lv_color_hex(0x101628), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(info_panel, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_border_color(info_panel, lv_color_hex(0x23314D), LV_PART_MAIN);
    lv_obj_set_style_border_width(info_panel, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(info_panel, 16, LV_PART_MAIN);
    lv_obj_set_scroll_dir(info_panel, LV_DIR_VER);
    lv_obj_clear_flag(info_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(info_panel, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_remove_style(info_panel, NULL, LV_PART_SCROLLBAR);

    info_text_lbl = lv_label_create(info_panel);
    lv_label_set_text(info_text_lbl, "Iniciando sistema...\nDi 'Oye Asistente' o presiona el boton verde para interactuar con la IA.");
    lv_obj_set_style_text_color(info_text_lbl, lv_color_hex(0xE0E6ED), LV_PART_MAIN);
    lv_obj_set_style_text_font(info_text_lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_long_mode(info_text_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(info_text_lbl, 410);
    lv_obj_align(info_text_lbl, LV_ALIGN_TOP_LEFT, 5, 5);
}

// ============================================================
// setupUIManager() — Inicialización HMI mediante BSP oficial
// ============================================================
void setupUIManager(i2c_master_bus_handle_t shared_i2c_bus) {
    ESP_LOGI(TAG, "[UI_Manager] Inicializando pantalla con ESP-BSP oficial de fábrica...");

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = 480 * 800,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .sw_rotate = false,
        }
    };
    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    assert(disp != NULL);
    bsp_display_backlight_on();
    bsp_display_brightness_set(100);

    bsp_display_lock(0);
    build_ai_assistant_ui();
    bsp_display_unlock();

    ESP_LOGI(TAG, "[UI_Manager] Motor LVGL 9 y BSP iniciados exitosamente.");
}

void ui_set_state(UIState state, const char* infoText) {
    if (status_panel == NULL) return;
    if (bsp_display_lock(pdMS_TO_TICKS(100))) {
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
            std::string clean = clean_utf8_for_lvgl(infoText);
            lv_label_set_text(info_text_lbl, clean.c_str());
        }
        
        bsp_display_unlock();
    }
}

void ui_update_wifi_status(bool connected, int rssi) {
    if (header_wifi_lbl == NULL) return;
    if (bsp_display_lock(pdMS_TO_TICKS(50))) {
        char buf[32];
        if (connected) {
            snprintf(buf, sizeof(buf), "WiFi: %d dBm", rssi);
            lv_obj_set_style_text_color(header_wifi_lbl, lv_color_hex(0x00FF88), LV_PART_MAIN);
        } else {
            snprintf(buf, sizeof(buf), "WiFi: Reconectando...");
            lv_obj_set_style_text_color(header_wifi_lbl, lv_color_hex(0xFFA500), LV_PART_MAIN);
        }
        lv_label_set_text(header_wifi_lbl, buf);
        bsp_display_unlock();
    }
}
