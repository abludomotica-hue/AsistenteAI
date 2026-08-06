#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <HWCDC.h>
#include <atomic> // (M3 / RISK-003) Concurrencia atómica entre núcleos

HWCDC miPuertoUSB;
#undef Serial
#define Serial miPuertoUSB

// ==========================================
// CONFIGURACIÓN DE DEPURACIÓN Y LOGS (L1)
// 0 = Producción (Arranque rápido y limpio)
// 1 = Debug Verboso (Scans I2C/WiFi, volcado ES8311)
// ==========================================
#ifndef DEBUG_VERBOSE
#define DEBUG_VERBOSE 0
#endif

#include "ES8311_Init.h"
#include "config.h"

// ==========================================
// CONFIGURACIÓN DE RED
// Los valores reales viven en config.h (fuera de git).
// Plantilla: config.h.example
// ==========================================
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASS;

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
// BUFFERS DE AUDIO EN PSRAM (Asignación Estática - M1 / RISK-001)
// Evita fragmentación de PSRAM al eliminar ciclos malloc/free de ~1.9MB
// ==========================================
#define AUDIO_SAMPLE_RATE 16000
#define AUDIO_MAX_DURATION_SEC 15
#define STEREO_BUFFER_SIZE (AUDIO_SAMPLE_RATE * 4 * AUDIO_MAX_DURATION_SEC) // 960 KB
#define MONO_BUFFER_SIZE   (AUDIO_SAMPLE_RATE * 2 * AUDIO_MAX_DURATION_SEC) // 480 KB
#define PAYLOAD_BUFFER_SIZE (MONO_BUFFER_SIZE + 1024)                       // ~481 KB

uint8_t* psramStereoBuffer = NULL;
uint8_t* psramMonoBuffer = NULL;
uint8_t* psramPayloadBuffer = NULL;

// ==========================================
// ARQUITECTURA FREERTOS
// ==========================================
TaskHandle_t audioTaskHandle = NULL;
QueueHandle_t audioCommandQueue;

enum AudioCommand {
  CMD_START_PIPELINE
};

// Protección atómica para concurrencia entre Core 0 y Core 1 (M3 / Mitigación RISK-003)
std::atomic<bool> interruptPlayback(false);

// ==========================================
// TELEMETRÍA Y DIAGNÓSTICO (Tarea M4 / Diagnostics Service)
// Mide latencia por etapa e imprime reporte sin uso de heap.
// ==========================================
struct PipelineMetrics {
  uint32_t pipelineStartMs;
  uint32_t recordStartMs;
  uint32_t recordDurationMs;
  uint32_t sttStartMs;
  uint32_t sttDurationMs;
  uint32_t llmStartMs;
  uint32_t llmDurationMs;
  uint32_t ttsStartMs;
  uint32_t ttsFirstChunkMs;  // Time to First Audio (TTFA)
  uint32_t ttsPlayDurationMs;
  uint32_t totalPipelineMs;
  
  void reset() {
    memset(this, 0, sizeof(PipelineMetrics));
  }
  
  void printReport() {
    Serial.println("\n=======================================================");
    Serial.println("         📊 TELEMETRÍA DEL PIPELINE (M4) 📊          ");
    Serial.println("=======================================================");
    Serial.printf(" 🎤 Captura I2S + Downmix:    %6u ms\n", (uint32_t)recordDurationMs);
    Serial.printf(" 🗣️  STT (Parakeet ASR):      %6u ms\n", (uint32_t)sttDurationMs);
    Serial.printf(" 🧠 LLM (Nemotron-3 30B):     %6u ms\n", (uint32_t)llmDurationMs);
    Serial.printf(" ⚡ TTS Time-to-First-Audio: %6u ms  <-- [Latencia Percibida]\n", (uint32_t)ttsFirstChunkMs);
    Serial.printf(" 🔊 TTS Duración Audio:      %6u ms\n", (uint32_t)ttsPlayDurationMs);
    Serial.println("-------------------------------------------------------");
    Serial.printf(" 🏁 TIEMPO TOTAL E2E:         %6u ms (%.2f s)\n", (uint32_t)totalPipelineMs, totalPipelineMs / 1000.0f);
    Serial.println("=======================================================\n");
    Serial.flush();
  }
};

PipelineMetrics g_metrics;

// ==========================================
// ROBUSTEZ HTTP (H3)
// Se reintenta ante errores de conexión (code < 0) o de servidor (5xx),
// nunca ante 4xx (el request es inválido y fallaría igual).
// ==========================================
#define HTTP_MAX_ATTEMPTS 2
bool httpShouldRetry(int code) { return code < 0 || code >= 500; }

// Autenticación opcional contra el Bridge (H4): si BRIDGE_AUTH_TOKEN está
// definido y no vacío en config.h, se envía el header X-Bridge-Token.
void addBridgeAuthHeader(HTTPClient &http) {
  if (strlen(BRIDGE_AUTH_TOKEN) > 0) {
    http.addHeader("X-Bridge-Token", BRIDGE_AUTH_TOKEN);
  }
}

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
        // DRENADO DE COLA (M3 / RISK-004): Elimina comandos redundantes acumulados durante rebotes o ráfagas
        xQueueReset(audioCommandQueue);
        g_metrics.reset();
        g_metrics.pipelineStartMs = millis();
        interruptPlayback.store(false, std::memory_order_relaxed); // Reset atómico AL INICIO del pipeline
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
        g_metrics.totalPipelineMs = millis() - g_metrics.pipelineStartMs;
        g_metrics.printReport();
        Serial.println("[Core 0] Pipeline finalizado. Volviendo a reposo."); Serial.flush();
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  // Espera acotada al Monitor Serie (H3): sin USB conectado el dispositivo
  // arranca igual tras 3 s (necesario para operación autónoma empotrada).
  uint32_t serialWaitStart = millis();
  while (!Serial && (millis() - serialWaitStart < 3000)) { delay(10); }
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
  static uint32_t lastTriggerMs = 0;
  if (Serial.available()) {
    char c = Serial.read();
    if (c == '1') {
      uint32_t now = millis();
      // DEBOUNCE (M3): Ignora disparos dentro de 300 ms para prevenir rebotes de pulsadores/táctiles
      if (now - lastTriggerMs >= 300) {
        lastTriggerMs = now;
        Serial.println("\n[Core 1] Disparador '1' activado. Enviando señal al Core 0..."); Serial.flush();
        interruptPlayback.store(true, std::memory_order_relaxed); // Señal atómica (Barge-in / RISK-003)
        AudioCommand cmd = CMD_START_PIPELINE;
        // Enviar a cola con timeout acotado (50 ms) para nunca bloquear el Core 1 (UI futura)
        if (xQueueSend(audioCommandQueue, &cmd, pdMS_TO_TICKS(50)) != pdTRUE) {
          Serial.println("[Core 1] Advertencia: Cola de audio llena. Disparador ignorado."); Serial.flush();
        }
      } else {
        Serial.println("[Core 1] Ignorando rebote de disparador (Debounce < 300ms activo - M3)."); Serial.flush();
      }
    }
  }
  delay(10);
}

void setupWiFi() {
  Serial.print("Conectando a Wi-Fi"); Serial.flush();
  // Auto-reconexión en segundo plano si el AP se cae en operación (H3)
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);  // Forzar modo Station puro (evita AP+STA accidental)
  WiFi.begin(ssid, password);

  // Hasta 3 intentos de 10 s al arranque; si falla, el dispositivo arranca
  // igual y el auto-reconnect seguirá intentando en segundo plano.
  const int MAX_ATTEMPTS = 3;
  for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
    uint32_t attemptStart = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - attemptStart < 10000)) {
      delay(500);
      Serial.print("."); Serial.flush();
    }
    if (WiFi.status() == WL_CONNECTED) break;
    Serial.printf("\n[WiFi] Intento %d/%d fallido (timeout 10 s).\n", attempt, MAX_ATTEMPTS);
    Serial.flush();
  }

  if (WiFi.status() == WL_CONNECTED) {
    // Potencia TX equilibrada para evitar caídas de tensión (Brownouts) por USB
    WiFi.setTxPower(WIFI_POWER_15dBm);
    Serial.println("\nWiFi Conectado!"); Serial.flush();
    Serial.println("--- DIAGNÓSTICO WiFi ---");
    Serial.printf("  IP:        %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("  Gateway:   %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("  RSSI:      %d dBm\n", WiFi.RSSI());
    Serial.printf("  Canal:     %d\n", WiFi.channel());
    Serial.printf("  BSSID:     %s\n", WiFi.BSSIDstr().c_str());
    Serial.printf("  TX Power:  %d (x0.25 dBm)\n", WiFi.getTxPower());
    Serial.println("------------------------"); Serial.flush();
  } else {
    Serial.println("\n[WiFi] Sin conexión al arranque; auto-reconexión activa en segundo plano."); Serial.flush();
  }

#if DEBUG_VERBOSE
  // SCAN DE ANTENA: Si TODAS las redes tienen RSSI < -75, la antena tiene problema
  Serial.println("\n--- SCAN REDES WiFi (Diagnóstico Antena) ---");
  int n = WiFi.scanNetworks(false, false, false, 300);  // Scan sincrónico, activo
  if (n > 0) {
    int strongCount = 0;
    for (int i = 0; i < n; i++) {
      int32_t rssi = WiFi.RSSI(i);
      Serial.printf("  [%2d] %-25s  %4d dBm  CH%2d\n", i + 1, WiFi.SSID(i).c_str(), rssi, WiFi.channel(i));
      if (rssi > -70) strongCount++;
    }
    Serial.printf("\n  Total: %d redes | Señal fuerte (>-70): %d\n", n, strongCount);
    if (strongCount == 0) {
      Serial.println("  ⚠️  NINGUNA red con señal fuerte. Posible problema de antena del ESP32-C6.");
    }
  } else {
    Serial.println("  ⚠️  No se encontraron redes WiFi. ¿Antena desconectada?");
  }
  WiFi.scanDelete();
  Serial.println("--- FIN SCAN ---\n"); Serial.flush();
#endif
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

  // ASIGNACIÓN ESTÁTICA EN PSRAM (Tarea M1 / Mitigación RISK-001)
  Serial.println("Asignando buffers estáticos en PSRAM (~1.9 MB)..."); Serial.flush();
  psramStereoBuffer = (uint8_t*)heap_caps_malloc(STEREO_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
  psramMonoBuffer = (uint8_t*)heap_caps_malloc(MONO_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
  psramPayloadBuffer = (uint8_t*)heap_caps_malloc(PAYLOAD_BUFFER_SIZE, MALLOC_CAP_SPIRAM);

  if (!psramStereoBuffer || !psramMonoBuffer || !psramPayloadBuffer) {
    Serial.println("¡ERROR FATAL: Memoria PSRAM insuficiente para buffers estáticos!"); Serial.flush();
  } else {
    Serial.printf("Buffers PSRAM asignados OK. PSRAM libre restante: %u bytes.\n", (uint32_t)heap_caps_get_free_size(MALLOC_CAP_SPIRAM)); Serial.flush();
  }
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
  
  
  uint32_t sampleRate = AUDIO_SAMPLE_RATE;
  uint32_t maxStereoSize = STEREO_BUFFER_SIZE;
  
  if (!psramStereoBuffer || !psramMonoBuffer || !psramPayloadBuffer) {
    Serial.println("Error FATAL: Buffers estáticos en PSRAM no están inicializados."); Serial.flush();
    return "";
  }
  uint8_t* stereoBuffer = psramStereoBuffer;
  uint8_t* monoBuffer = psramMonoBuffer;
  
  g_metrics.recordStartMs = millis();
  size_t bytesReadTotal = 0;
  Serial.println(">>> Capturando I2S en Estéreo (VAD inteligente M2)..."); Serial.flush();
  
  const int16_t VAD_THRESHOLD = 500;                // Umbral de amplitud de voz
  const uint32_t POST_SPEECH_SILENCE_MS = 1200;     // Corte tras hablar
  const uint32_t INITIAL_SILENCE_TIMEOUT_MS = 5000; // Timeout de inactividad inicial (M2)
  const uint32_t CHUNK_SIZE = sampleRate * 4 / 10;  // ~100ms in bytes (6400)
  
  uint32_t silenceMs = 0;
  uint32_t lastChunkTimeMs = millis();
  bool userHasSpoken = false;

  while(bytesReadTotal < maxStereoSize) {
    uint32_t bytesToRead = (maxStereoSize - bytesReadTotal) > CHUNK_SIZE ? CHUNK_SIZE : (maxStereoSize - bytesReadTotal);
    size_t bytesRead = i2s.readBytes((char*)(stereoBuffer + bytesReadTotal), bytesToRead);
    
    if (bytesRead > 0) {
      // Medición diferencial real milisegundal (M2) en lugar de 100ms fijos
      uint32_t now = millis();
      uint32_t deltaMs = now - lastChunkTimeMs;
      lastChunkTimeMs = now;

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
        silenceMs += deltaMs;
      }
      
      bytesReadTotal += bytesRead;
      
      // Caso 1: Detener si el usuario habló y luego hizo silencio >= 1200ms
      if (userHasSpoken && silenceMs >= POST_SPEECH_SILENCE_MS) {
        Serial.printf("\n[VAD] Silencio detectado tras voz (%u ms). Deteniendo captura.\n", (uint32_t)silenceMs);
        break;
      }
      
      // Caso 2: Timeout si nadie habló en los primeros 5000ms (M2)
      if (!userHasSpoken && silenceMs >= INITIAL_SILENCE_TIMEOUT_MS) {
        Serial.printf("\n[VAD] Timeout de inactividad inicial (%u ms sin voz). Abortando.\n", (uint32_t)silenceMs);
        break;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  
  Serial.printf("Grabación finalizada. Bytes leídos: %u de %u\n", bytesReadTotal, maxStereoSize); Serial.flush();

  // Si se abortó por timeout inicial (no hubo voz), evitamos enviar silencio a la nube
  if (!userHasSpoken) {
    Serial.println(">>> Captura descartada por inactividad inicial (sin voz reconocida). Volviendo a reposo."); Serial.flush();
    return "";
  }
  
  // Ajustar tamaños reales
  uint32_t actualStereoSize = bytesReadTotal;
  uint32_t actualMonoSize = actualStereoSize / 2;
  
  int16_t* pStereo = (int16_t*)stereoBuffer;
  Serial.println("Realizando Downmix de software a Mono..."); Serial.flush();
  
  // DOWNMIX (Estéreo a Mono) - Tomamos solo canal L
  int16_t* pMono = (int16_t*)monoBuffer;
  uint32_t numSamples = actualMonoSize / 2;
  
  for(uint32_t i=0; i<numSamples; i++) {
      // Tomamos directamente el canal L (El ADC del ES8311 transmite por el slot L en I2S - L2)
      pMono[i] = pStereo[i*2];
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
  if (totalLen > PAYLOAD_BUFFER_SIZE) {
    Serial.println("Error FATAL: Tamaño de payload excede capacidad del buffer estático en PSRAM."); Serial.flush();
    return "";
  }
  uint8_t* fullPayload = psramPayloadBuffer;
  
  uint32_t offset = 0;
  memcpy(fullPayload + offset, head.c_str(), head.length()); offset += head.length();
  memcpy(fullPayload + offset, wavHeader, 44); offset += 44;
  memcpy(fullPayload + offset, monoBuffer, actualMonoSize); offset += actualMonoSize;
  memcpy(fullPayload + offset, tail.c_str(), tail.length());
  
  g_metrics.recordDurationMs = millis() - g_metrics.recordStartMs;
  Serial.printf("Payload ensamblado (%u bytes). WiFi RSSI: %d dBm. Enviando POST a Debian...\n", totalLen, WiFi.RSSI()); Serial.flush();
  HTTPClient http;
  http.begin(BRIDGE_BASE_URL "/stt"); 
  http.setTimeout(60000);  // 60s (Tope máximo de uint16_t para evitar desbordamiento al compilar)
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  addBridgeAuthHeader(http);
  
  g_metrics.sttStartMs = millis();
  int httpResponseCode = -1;
  String transcribedText = "";
  for (int attempt = 1; attempt <= HTTP_MAX_ATTEMPTS; attempt++) {
    httpResponseCode = http.POST(fullPayload, totalLen);
    if (!httpShouldRetry(httpResponseCode)) break;
    Serial.printf("[STT] Intento %d/%d falló (HTTP %d). Reintentando en 1 s...\n",
                  attempt, HTTP_MAX_ATTEMPTS, httpResponseCode);
    Serial.flush();
    delay(1000);
  }
  g_metrics.sttDurationMs = millis() - g_metrics.sttStartMs;

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
  Serial.println(">>> Captura y procesamiento finalizado (buffers estáticos preservados en PSRAM)."); Serial.flush();
  return transcribedText;
}

String getLLMResponse(String promptText) {
  if (WiFi.status() != WL_CONNECTED) return "";
  HTTPClient http;
  // El LLM se gestiona en el Bridge (H2): historial, system prompt y la API
  // key viven allá. El ESP32 solo envía el texto del usuario.
  http.begin(BRIDGE_BASE_URL "/llm");
  http.setTimeout(45000); // 45s: el Bridge añade un salto hacia NVIDIA
  http.addHeader("Content-Type", "application/json");
  addBridgeAuthHeader(http);

  JsonDocument payloadDoc;
  payloadDoc["input"] = promptText;
  String payload;
  serializeJson(payloadDoc, payload);

  g_metrics.llmStartMs = millis();
  int httpResponseCode = -1;
  String responseText = "";
  for (int attempt = 1; attempt <= HTTP_MAX_ATTEMPTS; attempt++) {
    httpResponseCode = http.POST(payload);
    if (!httpShouldRetry(httpResponseCode)) break;
    Serial.printf("[LLM] Intento %d/%d falló (HTTP %d). Reintentando en 1 s...\n",
                  attempt, HTTP_MAX_ATTEMPTS, httpResponseCode);
    Serial.flush();
    delay(1000);
  }
  g_metrics.llmDurationMs = millis() - g_metrics.llmStartMs;

  if (httpResponseCode == 200) {
    responseText = http.getString();
    Serial.println(">>> LLM OK (vía Bridge).");
    Serial.flush();
  } else {
    Serial.print("Error LLM HTTP (Bridge): ");
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
  addBridgeAuthHeader(http);

  JsonDocument payloadDoc;
  payloadDoc["input"] = textToSpeak;
  String payload;
  serializeJson(payloadDoc, payload);

  g_metrics.ttsStartMs = millis();
  int httpResponseCode = -1;
  for (int attempt = 1; attempt <= HTTP_MAX_ATTEMPTS; attempt++) {
    httpResponseCode = http.POST(payload);
    if (!httpShouldRetry(httpResponseCode)) break;
    Serial.printf("[TTS] Intento %d/%d falló (HTTP %d). Reintentando en 1 s...\n",
                  attempt, HTTP_MAX_ATTEMPTS, httpResponseCode);
    Serial.flush();
    delay(1000);
  }
  
  if (httpResponseCode == 200) {
    WiFiClient* stream = http.getStreamPtr();
    size_t size = http.getSize();
    Serial.println("[Core 0] Audio recibido! Duplicando canales y Reproduciendo..."); Serial.flush();
    
    uint8_t mono_buf[1024];
    uint8_t stereo_buf[2048]; // Doble de tamaño para la conversión a Estéreo
    
    bool has_leftover = false;
    uint8_t leftover_byte = 0;
    bool first_chunk_played = false;
    uint32_t play_start_ms = 0;

    // Verificar si sigue conectado o si quedan bytes por leer en el buffer LwIP
    while((http.connected() || stream->available() > 0) && (size > 0 || size == (size_t)-1)) {
        if (interruptPlayback.load(std::memory_order_relaxed)) {
            Serial.println("\n[Core 0] Reproducción interrumpida al instante por el usuario (Barge-in atómico)."); Serial.flush();
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
                   if (!first_chunk_played) {
                       g_metrics.ttsFirstChunkMs = millis() - g_metrics.ttsStartMs;
                       play_start_ms = millis();
                       first_chunk_played = true;
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
    if (first_chunk_played) {
        g_metrics.ttsPlayDurationMs = millis() - play_start_ms;
    }
    Serial.println("[Core 0] Reproducción finalizada."); Serial.flush();
  } else {
    Serial.print("Error TTS HTTP: ");
    Serial.println(httpResponseCode);
    Serial.flush();
  }
  http.end();
}
