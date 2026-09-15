#include "audio_manager.h"
#include "network_stream.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "UI_Manager.h"
#include <string.h>
#include <atomic>

// Soporte ESP-SR (Audio Front-End y WakeNet)
#if __has_include("esp_afe_sr_models.h")
#define HAVE_ESP_SR 1
#include "esp_afe_sr_models.h"
#include "esp_afe_sr_iface.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#else
#define HAVE_ESP_SR 0
#endif

// Variables compartidas
#include "freertos/queue.h"

extern QueueHandle_t audioCommandQueue;
extern void start_ota_update(void);

static const char *TAG_AM = "AUDIO_MANAGER";

// Canales I2S
static i2s_chan_handle_t rx_chan = NULL;
static i2s_chan_handle_t tx_chan = NULL;

// Control de grabación atómico
static std::atomic<bool> is_recording(false);
static const int RECORD_DURATION_MS = 4000; // Grabar por 4 segundos
static std::atomic<int> record_elapsed_ms(0);

// Buffer estático de reproducción TTS en PSRAM (previene malloc/free en caliente)
static const size_t MAX_TX_STEREO_BUFFER_BYTES = 16384;
static int16_t *s_tx_stereo_buffer = NULL;

#if HAVE_ESP_SR
static esp_afe_sr_iface_t *s_afe_handle = NULL;
static esp_afe_sr_data_t *s_afe_data = NULL;
static volatile bool s_wakenet_enabled = false;
#endif

static void init_i2s(void) {
    ESP_LOGI(TAG_AM, "Inicializando I2S...");
    
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, &rx_chan));

    // ES8311 opera en bus I2S Estéreo (32 bits de frame: 16-bit Left + 16-bit Right)
    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = (gpio_num_t)13,
            .bclk = (gpio_num_t)12,
            .ws   = (gpio_num_t)10,
            .dout = (gpio_num_t)9,
            .din  = (gpio_num_t)48,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    
    // ES8311 requiere MCLK continuo (256 * Fs = 4.096 MHz)
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));

    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
    ESP_LOGI(TAG_AM, "I2S Inicializado con éxito (Modo Estéreo 16kHz).");
}

static void audio_control_task(void *arg) {
    AudioCommand cmd;
    while(1) {
        if (audioCommandQueue && xQueueReceive(audioCommandQueue, &cmd, portMAX_DELAY) == pdTRUE) {
            if (cmd == CMD_START_PIPELINE) {
                if (!is_recording) {
                    audio_manager_trigger_interaction();
                }
            } else if (cmd == CMD_START_OTA) {
                ESP_LOGI(TAG_AM, "Recibido comando OTA en queue");
                start_ota_update();
            }
        }
    }
}

// Tarea principal de recolección de audio I2S y streaming
static void afe_fetch_task(void *arg) {
    ESP_LOGI(TAG_AM, "Iniciando tarea de captura I2S...");
    
    size_t bytes_read = 0;
    const size_t stereo_buffer_size = 16384; // 16KB de datos estéreo (8192 muestras L+R)
    const size_t mono_buffer_size = 8192;    // 8KB de datos mono (4096 muestras = 256ms a 16kHz)
    
    int16_t *stereo_buffer = (int16_t*)heap_caps_malloc(stereo_buffer_size, MALLOC_CAP_SPIRAM);
    int16_t *mono_buffer = (int16_t*)heap_caps_malloc(mono_buffer_size, MALLOC_CAP_SPIRAM);
    
    if (!stereo_buffer || !mono_buffer) {
        ESP_LOGE(TAG_AM, "Fallo al reservar memoria para I2S");
        vTaskDelete(NULL);
    }

    while (1) {
        if (i2s_channel_read(rx_chan, stereo_buffer, stereo_buffer_size, &bytes_read, portMAX_DELAY) == ESP_OK) {
            size_t stereo_samples = bytes_read / sizeof(int16_t);
            size_t mono_samples = stereo_samples / 2;
            
            // Downmix de Estéreo a Mono (extraemos el canal Left del micrófono analógico)
            for (size_t i = 0; i < mono_samples; i++) {
                mono_buffer[i] = stereo_buffer[i * 2]; // Canal Left
            }
            size_t mono_bytes = mono_samples * sizeof(int16_t);

            if (is_recording.load(std::memory_order_relaxed)) {
                // Enviar chunk de audio mono por la red
                if (network_stream_send_chunk(mono_buffer, mono_bytes) != ESP_OK) {
                    ESP_LOGE(TAG_AM, "Error enviando chunk, abortando grabación.");
                    is_recording.store(false, std::memory_order_relaxed);
                    network_stream_abort();
                    ui_set_state(UI_STATE_IDLE, "Error de red");
                    continue;
                }
                
                // Calcular tiempo transcurrido (16kHz 16bit Mono = 32000 bytes/seg)
                int elapsed_chunk_ms = (mono_bytes * 1000) / 32000;
                int current_elapsed = record_elapsed_ms.fetch_add(elapsed_chunk_ms) + elapsed_chunk_ms;
                
                if (current_elapsed >= RECORD_DURATION_MS) {
                    ESP_LOGI(TAG_AM, "Grabación finalizada (%d ms). Solicitando respuesta...", current_elapsed);
                    is_recording.store(false, std::memory_order_relaxed);
                    ui_set_state(UI_STATE_THINKING, "Analizando audio...");
                    
                    // Finaliza el request chunked HTTP POST
                    network_stream_finish_and_receive();
                }
            }
#if HAVE_ESP_SR
            else if (s_wakenet_enabled && s_afe_handle && s_afe_data) {
                // Ingesta continua en AFE para detección de Wake Word en reposo
                s_afe_handle->feed(s_afe_data, mono_buffer);
                afe_fetch_result_t *res = s_afe_handle->fetch(s_afe_data);
                if (res && res->wake_word_detected == WAKENET_DETECTED) {
                    ESP_LOGI(TAG_AM, "¡WAKE WORD DETECTADO POR VOZ! Activando asistente...");
                    audio_manager_trigger_interaction();
                }
            }
#endif
        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void audio_manager_trigger_interaction(void) {
    ESP_LOGI(TAG_AM, "Interacción iniciada (Voz o Botón táctil).");
    ui_set_state(UI_STATE_LISTENING, "Escuchando (4s)...");
    
    if (network_stream_start() == ESP_OK) {
        record_elapsed_ms.store(0, std::memory_order_relaxed);
        is_recording.store(true, std::memory_order_relaxed);
    } else {
        ui_set_state(UI_STATE_IDLE, "Fallo de conexión");
    }
}

esp_err_t audio_manager_init(void) {
    ESP_LOGI(TAG_AM, "Inicializando Audio Manager con I2S Estéreo...");
    
    init_i2s();
    network_stream_init();
    
    if (!s_tx_stereo_buffer) {
        s_tx_stereo_buffer = (int16_t*)heap_caps_malloc(MAX_TX_STEREO_BUFFER_BYTES, MALLOC_CAP_SPIRAM);
        if (!s_tx_stereo_buffer) {
            ESP_LOGE(TAG_AM, "Fallo al pre-asignar buffer TX en PSRAM");
        } else {
            ESP_LOGI(TAG_AM, "Buffer TX estático de %d bytes pre-asignado en PSRAM.", (int)MAX_TX_STEREO_BUFFER_BYTES);
        }
    }

#if HAVE_ESP_SR
    // Configurar Audio Front-End (AFE) para detección de Wake Word
    ESP_LOGI(TAG_AM, "Configurando ESP-SR WakeNet...");
    afe_config_t afe_config = AFE_CONFIG_DEFAULT();
    afe_config.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    afe_config.wakenet_init = true;
    afe_config.voice_communication_init = false;
    afe_config.pcm_config.total_ch_num = 1;
    afe_config.pcm_config.mic_num = 1;
    afe_config.pcm_config.ref_num = 0;
    
    s_afe_handle = (esp_afe_sr_iface_t *)&ESP_AFE_SR_HANDLE;
    if (s_afe_handle) {
        s_afe_data = s_afe_handle->create_from_config(&afe_config);
        if (s_afe_data) {
            s_wakenet_enabled = true;
            ESP_LOGI(TAG_AM, "ESP-SR WakeNet inicializado correctamente. Wake Word activo ('Hi, ESP').");
        } else {
            ESP_LOGW(TAG_AM, "No se pudo crear instancia AFE. Operando en modo manual.");
        }
    }
#endif
    
    xTaskCreatePinnedToCore(afe_fetch_task, "afe_fetch", 8192, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(audio_control_task, "audio_ctrl", 4096, NULL, 5, NULL, 0);
    
    return ESP_OK;
}

esp_err_t audio_manager_play_chunk(const uint8_t *data, size_t length) {
    if (!tx_chan || length == 0) return ESP_FAIL;
    
    // length es en bytes de audio Mono 16-bit
    size_t mono_samples = length / sizeof(int16_t);
    size_t stereo_bytes = mono_samples * 2 * sizeof(int16_t);
    
    const int16_t *mono_in = (const int16_t*)data;
    int16_t *stereo_out = NULL;
    bool dyn_alloc = false;
    
    if (s_tx_stereo_buffer && stereo_bytes <= MAX_TX_STEREO_BUFFER_BYTES) {
        stereo_out = s_tx_stereo_buffer;
    } else {
        // Fallback dinámico solo en caso extraordinario de que el chunk exceda MAX_TX_STEREO_BUFFER_BYTES
        stereo_out = (int16_t*)heap_caps_malloc(stereo_bytes, MALLOC_CAP_SPIRAM);
        dyn_alloc = true;
    }
    
    if (!stereo_out) {
        ESP_LOGE(TAG_AM, "Fallo al reservar memoria para reproducción TTS");
        return ESP_ERR_NO_MEM;
    }
    
    for (size_t i = 0; i < mono_samples; i++) {
        stereo_out[i * 2]     = mono_in[i]; // Left
        stereo_out[i * 2 + 1] = mono_in[i]; // Right
    }
    
    size_t bytes_written = 0;
    esp_err_t err = i2s_channel_write(tx_chan, stereo_out, stereo_bytes, &bytes_written, portMAX_DELAY);
    
    if (dyn_alloc) {
        free(stereo_out);
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG_AM, "Fallo al escribir I2S TX: %s", esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}
