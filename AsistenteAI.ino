#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <HWCDC.h>

HWCDC miPuertoUSB;
#define Serial miPuertoUSB

#include "ES8311_Init.h"
#include "config.h"

// ==========================================
// CONFIGURACIÓN DE RED Y APIS
// Los valores reales viven en config.h (fuera de git).
// Plantilla: config.h.example
// ==========================================
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASS;

const char *API_KEY_LLM = NVIDIA_LLM_API_KEY;

// ==========================================
// PINES DE AUDIO (JC4880P443C)
// ==========================================
#define IIC_DATA 7
#define IIC_CLK 8
#define I2S_MCLK 13
#define I2S_BCLK 12 
#define I2S_LRCK 10 
#define I2S_DOUT 9  
#define I2S_DIN 48  
#define PA_PIN 11

I2SClass i2s;

// ==========================================
// ARQUITECTURA FREERTOS
// ==========================================
TaskHandle_t audioTaskHandle = NULL;
QueueHandle_t audioCommandQueue;

enum AudioCommand {
  CMD_IDLE,
  CMD_START_PIPELINE
};

volatile bool interruptPlayback = false;

void setupWiFi();
void setupAudio();
String recordAndTranscribe();
String getLLMResponse(String promptText);
void synthesizeAndPlay(String textToSpeak);
void generateWavHeader(uint8_t* header, uint32_t wavSize, uint32_t sampleRate);

void audioTask(void *pvParameters) {
  AudioCommand cmd;
  while (1) {
    if (xQueueReceive(audioCommandQueue, &cmd, portMAX_DELAY) == pdTRUE) {
      if (cmd == CMD_START_PIPELINE) {
        interruptPlayback = false; // Resetear la bandera AL INICIO del pipeline
        Serial.println("\n[Core 0] Iniciando Pipeline de Asistente..."); Serial.flush();
        String text = recordAndTranscribe();
        if (text.length() > 0) {
          Serial.println("[Core 0] Usuario dijo: " + text); Serial.flush();
          Serial.println("[Core 0] >>> PENSANDO (Enviando a NVIDIA)..."); Serial.flush();
          String response = getLLMResponse(text);
          if (response.length() > 0) {
            Serial.println("[Core 0] Asistente responde: " + response); Serial.flush();
            Serial.println("[Core 0] >>> HABLANDO..."); Serial.flush();
            synthesizeAndPlay(response);
          }
        } else {
            Serial.println("[Core 0] >>> SILENCIO DETECTADO (Ningún texto reconocido)."); Serial.flush();
        }
        Serial.println("[Core 0] Pipeline finalizado. Volviendo a reposo."); Serial.flush();
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); } 
  delay(1000);
  Serial.println("\n\n--- INICIANDO SISTEMA (Arquitectura FreeRTOS) ---"); Serial.flush();
  
  setupWiFi();
  setupAudio();
  
  audioCommandQueue = xQueueCreate(5, sizeof(AudioCommand));
  
  xTaskCreatePinnedToCore(
    audioTask, "AudioTask", 32768, NULL, 1, &audioTaskHandle, 0
  );
  
  Serial.println("\nAsistente Iniciado."); Serial.flush();
  Serial.println("Envía un '1' por el Monitor Serie para simular el botón de escucha."); Serial.flush();
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == '1') {
      Serial.println("\n[Core 1] Botón presionado. Enviando señal al Core 0..."); Serial.flush();
      interruptPlayback = true; // Señal para detener reproducción actual
      AudioCommand cmd = CMD_START_PIPELINE;
      xQueueSend(audioCommandQueue, &cmd, portMAX_DELAY);
    }
  }
  delay(10);
}

void setupWiFi() {
  Serial.print("Conectando a Wi-Fi"); Serial.flush();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("."); Serial.flush();
  }
  Serial.println("\nWiFi Conectado!"); Serial.flush();
}

void setupAudio() {
  Serial.println("Configurando Audio (I2S + I2C)..."); Serial.flush();
  
  i2s.setPins(I2S_BCLK, I2S_LRCK, I2S_DOUT, I2S_DIN, I2S_MCLK);
  // INICIAMOS I2S PRIMERO PARA DARLE VIDA AL MCLK DEL CODEC
  if (!i2s.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    Serial.println("¡Fallo al inicializar I2S!"); Serial.flush();
  } else {
    Serial.println("I2S inicializado correctamente a 16kHz."); Serial.flush();
  }

  // AHORA SÍ MANDAMOS LOS COMANDOS I2C
  Wire.begin(IIC_DATA, IIC_CLK, 400000);
  ES8311_Init();
  
  pinMode(PA_PIN, OUTPUT);
  digitalWrite(PA_PIN, HIGH);
}

void generateWavHeader(uint8_t* header, uint32_t wavSize, uint32_t sampleRate) {
  uint32_t fileSize = wavSize + 36;
  uint32_t byteRate = sampleRate * 2; // Mono 16-bit (2 bytes por sample)
  const uint8_t wavHeader[44] = {
    'R', 'I', 'F', 'F', 
    (uint8_t)(fileSize & 0xff), (uint8_t)((fileSize >> 8) & 0xff), (uint8_t)((fileSize >> 16) & 0xff), (uint8_t)((fileSize >> 24) & 0xff),
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    16, 0, 0, 0, 
    1, 0, // Mono
    1, 0, // 1 canal
    (uint8_t)(sampleRate & 0xff), (uint8_t)((sampleRate >> 8) & 0xff), (uint8_t)((sampleRate >> 16) & 0xff), (uint8_t)((sampleRate >> 24) & 0xff),
    (uint8_t)(byteRate & 0xff), (uint8_t)((byteRate >> 8) & 0xff), (uint8_t)((byteRate >> 16) & 0xff), (uint8_t)((byteRate >> 24) & 0xff),
    2, 0, // Bloque = 2 bytes
    16, 0,
    'd', 'a', 't', 'a',
    (uint8_t)(wavSize & 0xff), (uint8_t)((wavSize >> 8) & 0xff), (uint8_t)((wavSize >> 16) & 0xff), (uint8_t)((wavSize >> 24) & 0xff)
  };
  memcpy(header, wavHeader, 44);
}


String recordAndTranscribe() {
  if (WiFi.status() != WL_CONNECTED) return "";
  Serial.println(">>> GRABANDO (Hasta 15 Segundos)... Hable ahora."); Serial.flush();
  
  
  uint32_t sampleRate = 16000;
  uint32_t maxDurationSeconds = 15;
  uint32_t maxStereoSize = sampleRate * 4 * maxDurationSeconds; // Estéreo desde Hardware
  uint32_t maxMonoSize = sampleRate * 2 * maxDurationSeconds;   // Mono para NVIDIA
  
  Serial.println(">>> Asignando buffers en PSRAM..."); Serial.flush();
  uint8_t* stereoBuffer = (uint8_t*)heap_caps_malloc(maxStereoSize, MALLOC_CAP_SPIRAM);
  uint8_t* monoBuffer = (uint8_t*)heap_caps_malloc(maxMonoSize, MALLOC_CAP_SPIRAM);
  
  if (!stereoBuffer || !monoBuffer) {
    Serial.println("Error FATAL: No hay memoria PSRAM suficiente."); Serial.flush();
    if (stereoBuffer) heap_caps_free(stereoBuffer);
    if (monoBuffer) heap_caps_free(monoBuffer);
    return "";
  }
  
  i2s.read(); 
  size_t bytesReadTotal = 0;
  Serial.println(">>> Capturando I2S en Estéreo (VAD activo)..."); Serial.flush();
  
  const int16_t VAD_THRESHOLD = 500; // Usaremos el Pico (Max) de amplitud
  const uint32_t SILENCE_MS_TO_STOP = 1200; 
  const uint32_t CHUNK_SIZE = sampleRate * 4 / 10; // ~100ms in bytes (6400)
  uint32_t silenceMs = 0;
  bool userHasSpoken = false;

  while(bytesReadTotal < maxStereoSize) {
    uint32_t bytesToRead = (maxStereoSize - bytesReadTotal) > CHUNK_SIZE ? CHUNK_SIZE : (maxStereoSize - bytesReadTotal);
    size_t bytesRead = i2s.readBytes((char*)(stereoBuffer + bytesReadTotal), bytesToRead);
    
    if (bytesRead > 0) {
      // Calcular Pico Máximo del chunk (canal L)
      int16_t* pChunk = (int16_t*)(stereoBuffer + bytesReadTotal);
      uint32_t chunkSamples = bytesRead / 4; 
      int16_t maxAmp = 0;
      for (uint32_t i = 0; i < chunkSamples; i++) {
        int16_t val = abs(pChunk[i * 2]);
        if (val > maxAmp) maxAmp = val;
      }
      
      if (maxAmp > VAD_THRESHOLD) {
        silenceMs = 0;
        userHasSpoken = true;
      } else {
        silenceMs += 100; // Asumimos ~100ms por chunk
      }
      
      bytesReadTotal += bytesRead;
      
      // Detener si hay silencio prolongado y ya habló algo
      if (userHasSpoken && silenceMs >= SILENCE_MS_TO_STOP) {
        Serial.printf("\nSilencio detectado (%d ms). Deteniendo captura.\n", silenceMs);
        break;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  
  Serial.printf("Grabación finalizada. Bytes leídos: %u de %u\n", bytesReadTotal, maxStereoSize); Serial.flush();
  
  // Ajustar tamaños reales
  uint32_t actualStereoSize = bytesReadTotal;
  uint32_t actualMonoSize = actualStereoSize / 2;
  
  int16_t* pStereo = (int16_t*)stereoBuffer;
  Serial.println("Realizando Downmix de software a Mono..."); Serial.flush();
  
  // DOWNMIX (Estéreo a Mono) - Tomamos solo canal L
  int16_t* pMono = (int16_t*)monoBuffer;
  uint32_t numSamples = actualMonoSize / 2;
  
  for(uint32_t i=0; i<numSamples; i++) {
      int16_t L = pStereo[i*2];
      int16_t R = pStereo[i*2 + 1];
      pMono[i] = (L != 0) ? L : R;
  }
  
  String boundary = "----NvidiaNIMBoundary123456789";
  String head = "--" + boundary + "\r\n"
              + "Content-Disposition: form-data; name=\"model\"\r\n\r\n"
              + "parakeet-1.1b-rnnt-multilingual-asr\r\n"
              + "--" + boundary + "\r\n"
              + "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
              + "Content-Type: audio/wav\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";
  
  uint8_t wavHeader[44];
  generateWavHeader(wavHeader, actualMonoSize, sampleRate);
  
  uint32_t totalLen = head.length() + 44 + actualMonoSize + tail.length();
  uint8_t* fullPayload = (uint8_t*)heap_caps_malloc(totalLen, MALLOC_CAP_SPIRAM);
  
  if(!fullPayload) {
    Serial.println("Error FATAL: Memoria PSRAM insuficiente para payload HTTP."); Serial.flush();
    heap_caps_free(stereoBuffer);
    heap_caps_free(monoBuffer);
    return "";
  }
  
  uint32_t offset = 0;
  memcpy(fullPayload + offset, head.c_str(), head.length()); offset += head.length();
  memcpy(fullPayload + offset, wavHeader, 44); offset += 44;
  memcpy(fullPayload + offset, monoBuffer, actualMonoSize); offset += actualMonoSize;
  memcpy(fullPayload + offset, tail.c_str(), tail.length());
  
  Serial.println("Payload ensamblado. Enviando POST a Debian..."); Serial.flush();
  HTTPClient http;
  http.begin(BRIDGE_BASE_URL "/stt"); 
  http.setTimeout(20000); 
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  
  int httpResponseCode = http.POST(fullPayload, totalLen);
  String transcribedText = "";
  
  if (httpResponseCode == 200) {
    transcribedText = http.getString();
    Serial.println(">>> STT OK."); Serial.flush();
  } else {
    Serial.print("Error STT HTTP (Debian): ");
    Serial.println(httpResponseCode);
    Serial.println(http.getString());
    Serial.flush();
  }
  
  http.end();
  heap_caps_free(stereoBuffer);
  heap_caps_free(monoBuffer);
  heap_caps_free(fullPayload);
  
  Serial.println(">>> Memoria liberada."); Serial.flush();
  return transcribedText;
}

String getLLMResponse(String promptText) {
  if (WiFi.status() != WL_CONNECTED) return "";
  HTTPClient http;
  http.begin("https://integrate.api.nvidia.com/v1/chat/completions");
  http.setTimeout(30000); // 30 segundos de timeout para el LLM
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + API_KEY_LLM);

  JsonDocument payloadDoc;
  payloadDoc["model"] = "nvidia/nemotron-3-nano-omni-30b-a3b-reasoning";
  payloadDoc["max_tokens"] = 1024;
  payloadDoc["temperature"] = 0.6;
  
  JsonArray messages = payloadDoc["messages"].to<JsonArray>();
  JsonObject userMsg = messages.add<JsonObject>();
  userMsg["role"] = "user";
  userMsg["content"] = promptText;
  
  String payload;
  serializeJson(payloadDoc, payload);

  int httpResponseCode = http.POST(payload);
  String responseText = "";

  if (httpResponseCode == 200) {
    String response = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    if (!error) {
      const char* content = doc["choices"][0]["message"]["content"];
      if (content) responseText = String(content);
    }
  } else {
    Serial.print("Error LLM HTTP: ");
    Serial.println(httpResponseCode);
    Serial.flush();
  }
  http.end();
  return responseText;
}

void synthesizeAndPlay(String textToSpeak) {
  if (WiFi.status() != WL_CONNECTED || textToSpeak.length() == 0) return;
  HTTPClient http;
  http.begin(BRIDGE_BASE_URL "/tts"); 
  http.setTimeout(30000); // 30 segundos de timeout
  http.useHTTP10(true); // Fuerza HTTP/1.0 para evitar Chunked Transfer Encoding y corrupción de audio
  http.addHeader("Content-Type", "application/json");

  JsonDocument payloadDoc;
  payloadDoc["input"] = textToSpeak;
  String payload;
  serializeJson(payloadDoc, payload);

  int httpResponseCode = http.POST(payload);
  
  if (httpResponseCode == 200) {
    WiFiClient* stream = http.getStreamPtr();
    size_t size = http.getSize();
    Serial.println("[Core 0] Audio recibido! Duplicando canales y Reproduciendo..."); Serial.flush();
    
    uint8_t mono_buf[1024];
    uint8_t stereo_buf[2048]; // Doble de tamaño para la conversión a Estéreo
    
    bool has_leftover = false;
    uint8_t leftover_byte = 0;

    // Verificar si sigue conectado o si quedan bytes por leer en el buffer LwIP
    while((http.connected() || stream->available() > 0) && (size > 0 || size == (size_t)-1)) {
        if (interruptPlayback) {
            Serial.println("\n[Core 0] Reproducción interrumpida por el usuario."); Serial.flush();
            break;
        }
        
        size_t available = stream->available();
        if(available) {
            // Si tenemos un byte sobrante del chunk anterior, lo ponemos al principio
            int offset = has_leftover ? 1 : 0;
            int max_read = sizeof(mono_buf) - offset;
            int bytes_to_read = min(available, (size_t)max_read);
            
            int c = stream->read(mono_buf + offset, bytes_to_read);
            if (c > 0) {
               int total_bytes = c + offset;
               
               // I2S (16-bit) requiere que los bytes vengan en pares. 
               // Si leemos un número impar de bytes, guardamos el último para la siguiente iteración.
               has_leftover = (total_bytes % 2 != 0);
               if (has_leftover) {
                   leftover_byte = mono_buf[total_bytes - 1];
                   total_bytes -= 1; // Procesamos un número par de bytes
               }
               
               if (total_bytes > 0) {
                   // Duplicación de Mono a Estéreo en tiempo real
                   int16_t* pMono = (int16_t*)mono_buf;
                   int16_t* pStereo = (int16_t*)stereo_buf;
                   int samples = total_bytes / 2;
                   for(int i=0; i<samples; i++){
                       pStereo[i*2] = pMono[i];     // Canal Izquierdo
                       pStereo[i*2 + 1] = pMono[i]; // Canal Derecho
                   }
                   i2s.write(stereo_buf, total_bytes * 2);
                   if(size != (size_t)-1) size -= total_bytes;
               }
               
               // Mover el byte sobrante al principio del buffer para la siguiente lectura
               if (has_leftover) {
                   mono_buf[0] = leftover_byte;
               }
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    Serial.println("[Core 0] Reproducción finalizada."); Serial.flush();
  } else {
    Serial.print("Error TTS HTTP: ");
    Serial.println(httpResponseCode);
    Serial.flush();
  }
  http.end();
}
