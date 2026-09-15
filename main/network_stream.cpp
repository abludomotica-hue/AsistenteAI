#include "network_stream.h"
#include "audio_manager.h"
#include "UI_Manager.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "config.h"
#include <atomic>

extern std::atomic<bool> interruptPlayback;

static const char *TAG_NET = "NETWORK_STREAM";
static esp_http_client_handle_t http_client = NULL;

esp_err_t network_stream_init(void) {
    ESP_LOGI(TAG_NET, "Inicializando Network Stream...");
    
    // Configurar cliente HTTP
    esp_http_client_config_t config = {};
    config.url = BRIDGE_BASE_URL "/v1/conversation_stream";
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 10000;
    config.keep_alive_enable = true;
    
    http_client = esp_http_client_init(&config);
    if (!http_client) {
        ESP_LOGE(TAG_NET, "Fallo al inicializar esp_http_client");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t network_stream_start(void) {
    if (!http_client) return ESP_FAIL;
    
    ESP_LOGI(TAG_NET, "Iniciando stream hacia %s", BRIDGE_BASE_URL "/v1/conversation_stream");
    
    // Preparar headers
    esp_http_client_set_header(http_client, "Content-Type", "application/octet-stream");
    
#ifdef BRIDGE_AUTH_TOKEN
    if (strlen(BRIDGE_AUTH_TOKEN) > 0) {
        esp_http_client_set_header(http_client, "X-Bridge-Token", BRIDGE_AUTH_TOKEN);
    }
#endif
    
    // Activar transferencia chunked (se logra pasando -1 como write_len en _open)
    esp_err_t err = esp_http_client_open(http_client, -1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_NET, "Fallo al abrir conexion HTTP: %s", esp_err_to_name(err));
        return err;
    }
    
    return ESP_OK;
}

esp_err_t network_stream_send_chunk(const int16_t *buffer, size_t len_bytes) {
    if (!http_client || len_bytes == 0) return ESP_FAIL;
    
    // Formatear cabecera de chunk en hexadecimal según RFC 7230
    char chunk_hdr[16];
    int hdr_len = snprintf(chunk_hdr, sizeof(chunk_hdr), "%X\r\n", (unsigned int)len_bytes);
    
    if (esp_http_client_write(http_client, chunk_hdr, hdr_len) < 0) {
        ESP_LOGE(TAG_NET, "Error escribiendo chunk header");
        return ESP_FAIL;
    }
    
    if (esp_http_client_write(http_client, (const char *)buffer, len_bytes) < 0) {
        ESP_LOGE(TAG_NET, "Error escribiendo chunk data");
        return ESP_FAIL;
    }
    
    if (esp_http_client_write(http_client, "\r\n", 2) < 0) {
        ESP_LOGE(TAG_NET, "Error escribiendo chunk trailer");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

void network_stream_abort(void) {
    if (http_client) {
        ESP_LOGW(TAG_NET, "Abortando conexión HTTP");
        esp_http_client_close(http_client);
    }
}

esp_err_t network_stream_finish_and_receive(void) {
    if (!http_client) return ESP_FAIL;
    
    ESP_LOGI(TAG_NET, "Finalizando streaming de audio hacia el bridge...");
    
    // Enviar el chunk finalizador según RFC 7230 ("0\r\n\r\n")
    const char *terminating_chunk = "0\r\n\r\n";
    if (esp_http_client_write(http_client, terminating_chunk, 5) < 0) {
        ESP_LOGE(TAG_NET, "Error enviando chunk finalizador");
        esp_http_client_close(http_client);
        return ESP_FAIL;
    }
    
    // Leer headers de la respuesta
    int content_length = esp_http_client_fetch_headers(http_client);
    if (content_length < 0) {
        ESP_LOGE(TAG_NET, "Error leyendo headers de la respuesta HTTP");
        esp_http_client_close(http_client);
        return ESP_FAIL;
    }
    
    int status_code = esp_http_client_get_status_code(http_client);
    ESP_LOGI(TAG_NET, "Respuesta del servidor. Status = %d, Content-Length = %d", status_code, content_length);
    
    if (status_code == 204) {
        ESP_LOGW(TAG_NET, "Servidor detectó silencio / sin voz inteligible.");
        ui_set_state(UI_STATE_IDLE, "No te he entendido");
        esp_http_client_close(http_client);
        return ESP_OK;
    }
    
    if (status_code != 200) {
        ESP_LOGE(TAG_NET, "Error del servidor HTTP. Código = %d", status_code);
        ui_set_state(UI_STATE_IDLE, "Error de conexion");
        esp_http_client_close(http_client);
        return ESP_FAIL;
    }
    
    // Reproducir el audio TTS recibido por el altavoz
    ui_set_state(UI_STATE_SPEAKING, "Hablando...");
    char read_buffer[1024];
    int total_read = 0;
    while (1) {
        if (interruptPlayback.load(std::memory_order_relaxed)) {
            ESP_LOGW(TAG_NET, "Reproducción de voz interrumpida por el usuario (Barge-in).");
            interruptPlayback.store(false, std::memory_order_relaxed);
            break;
        }
        int read_len = esp_http_client_read(http_client, read_buffer, sizeof(read_buffer));
        if (read_len <= 0) {
            if (read_len < 0) ESP_LOGE(TAG_NET, "Error leyendo datos de la respuesta");
            break;
        }
        total_read += read_len;
        audio_manager_play_chunk((const uint8_t*)read_buffer, read_len);
    }
    
    ESP_LOGI(TAG_NET, "Se recibieron y reprodujeron %d bytes de audio TTS.", total_read);
    ui_set_state(UI_STATE_IDLE, "Listo");
    
    esp_http_client_close(http_client);
    return ESP_OK;
}
