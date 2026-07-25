#include "AlbumArtClient.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include "CydifyNetwork.h"
#include "HardwareConfig.h"

namespace {

constexpr size_t COVER_PIXEL_COUNT =
    HardwareConfig::COVER_WIDTH * HardwareConfig::COVER_HEIGHT;
constexpr size_t COVER_BYTE_COUNT = COVER_PIXEL_COUNT * sizeof(uint16_t);
constexpr size_t BACKGROUND_PIXEL_COUNT =
    HardwareConfig::BACKGROUND_WIDTH * HardwareConfig::BACKGROUND_HEIGHT;
constexpr size_t BACKGROUND_BYTE_COUNT =
    BACKGROUND_PIXEL_COUNT * sizeof(uint16_t);

uint16_t* displayedPixels = nullptr;
uint16_t* pendingPixels = nullptr;
uint16_t* displayedBackgroundPixels = nullptr;
uint16_t* pendingBackgroundPixels = nullptr;
char requestedTrackId[32] = {};
char displayedTrackId[32] = {};
char pendingTrackId[32] = {};
uint32_t pendingDominantColor = 0x1ED760;

SemaphoreHandle_t stateMutex = nullptr;
bool workerRunning = false;
bool downloadRequested = false;
bool resultPending = false;
bool backgroundResultPending = false;
bool clearPending = false;
bool clearBackgroundPending = false;

AlbumArtClient::ImageHandler onImage = nullptr;
AlbumArtClient::ClearHandler onClear = nullptr;
AlbumArtClient::BackgroundHandler onBackground = nullptr;
AlbumArtClient::ClearHandler onClearBackground = nullptr;

template <size_t Size>
void copyText(char (&destination)[Size], const char* source) {
  snprintf(destination, Size, "%s", source == nullptr ? "" : source);
}

String coverEndpoint(const char* baseUrl, const char* trackId) {
  String url;
  url.reserve(strlen(baseUrl) + strlen(trackId) + 32);
  url += baseUrl;
  url += "/api/cover/";
  url += trackId;
  url += ".rgb565";
  return url;
}

String backgroundEndpoint(const char* baseUrl, const char* trackId) {
  String url;
  url.reserve(strlen(baseUrl) + strlen(trackId) + 40);
  url += baseUrl;
  url += "/api/background/";
  url += trackId;
  url += ".rgb565";
  return url;
}

uint16_t* downloadCover(
    const char* baseUrl,
    const char* trackId,
    uint32_t& dominantColor) {
  uint16_t* pixels = static_cast<uint16_t*>(malloc(COVER_BYTE_COUNT));
  if (pixels == nullptr) {
    Serial.printf("Sem heap para a capa (%u bytes necessarios)\n",
                  static_cast<unsigned>(COVER_BYTE_COUNT));
    return nullptr;
  }

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(HardwareConfig::COVER_REQUEST_TIMEOUT_MS);
  http.useHTTP10(true);

  if (!http.begin(client, coverEndpoint(baseUrl, trackId))) {
    free(pixels);
    return nullptr;
  }
  const char* headerKeys[] = {"X-CYDify-Dominant"};
  http.collectHeaders(headerKeys, 1);
  const int statusCode = http.GET();
  if (statusCode != HTTP_CODE_OK) {
    Serial.printf("Capa indisponivel: HTTP %d\n", statusCode);
    http.end();
    free(pixels);
    return nullptr;
  }

  String colorHeader = http.header("X-CYDify-Dominant");
  colorHeader.trim();
  if (colorHeader.startsWith("#")) colorHeader.remove(0, 1);
  if (colorHeader.length() == 6) {
    dominantColor = strtoul(colorHeader.c_str(), nullptr, 16) & 0xFFFFFF;
  }

  const int contentLength = http.getSize();
  if (contentLength >= 0 &&
      contentLength != static_cast<int>(COVER_BYTE_COUNT)) {
    Serial.printf("Tamanho de capa invalido: %d bytes\n", contentLength);
    http.end();
    free(pixels);
    return nullptr;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t* destination = reinterpret_cast<uint8_t*>(pixels);
  size_t received = 0;
  const uint32_t startedAt = millis();
  while (received < COVER_BYTE_COUNT &&
         millis() - startedAt < HardwareConfig::COVER_REQUEST_TIMEOUT_MS) {
    const size_t available = stream->available();
    if (available == 0) {
      if (!http.connected()) break;
      delay(1);
      continue;
    }
    const size_t remaining = COVER_BYTE_COUNT - received;
    const size_t chunk = available < remaining ? available : remaining;
    const int count = stream->readBytes(destination + received, chunk);
    if (count <= 0) break;
    received += static_cast<size_t>(count);
  }
  http.end();

  if (received != COVER_BYTE_COUNT) {
    Serial.printf("Download de capa incompleto: %u/%u bytes\n",
                  static_cast<unsigned>(received),
                  static_cast<unsigned>(COVER_BYTE_COUNT));
    free(pixels);
    return nullptr;
  }
  return pixels;
}

uint16_t* downloadBackground(const char* baseUrl, const char* trackId) {
  uint16_t* pixels =
      static_cast<uint16_t*>(malloc(BACKGROUND_BYTE_COUNT));
  if (pixels == nullptr) {
    Serial.printf(
        "Sem heap para o fundo (%u bytes necessarios)\n",
        static_cast<unsigned>(BACKGROUND_BYTE_COUNT));
    return nullptr;
  }

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(HardwareConfig::COVER_REQUEST_TIMEOUT_MS);
  http.useHTTP10(true);

  if (!http.begin(client, backgroundEndpoint(baseUrl, trackId))) {
    free(pixels);
    return nullptr;
  }
  const int statusCode = http.GET();
  if (statusCode != HTTP_CODE_OK) {
    Serial.printf("Fundo desfocado indisponivel: HTTP %d\n", statusCode);
    http.end();
    free(pixels);
    return nullptr;
  }

  const int contentLength = http.getSize();
  if (contentLength >= 0 &&
      contentLength != static_cast<int>(BACKGROUND_BYTE_COUNT)) {
    Serial.printf("Tamanho de fundo invalido: %d bytes\n", contentLength);
    http.end();
    free(pixels);
    return nullptr;
  }

  WiFiClient* stream = http.getStreamPtr();
  uint8_t* destination = reinterpret_cast<uint8_t*>(pixels);
  size_t received = 0;
  const uint32_t startedAt = millis();
  while (received < BACKGROUND_BYTE_COUNT &&
         millis() - startedAt < HardwareConfig::COVER_REQUEST_TIMEOUT_MS) {
    const size_t available = stream->available();
    if (available == 0) {
      if (!http.connected()) break;
      delay(1);
      continue;
    }
    const size_t remaining = BACKGROUND_BYTE_COUNT - received;
    const size_t chunk = available < remaining ? available : remaining;
    const int count = stream->readBytes(destination + received, chunk);
    if (count <= 0) break;
    received += static_cast<size_t>(count);
  }
  http.end();

  if (received != BACKGROUND_BYTE_COUNT) {
    Serial.printf(
        "Download de fundo incompleto: %u/%u bytes\n",
        static_cast<unsigned>(received),
        static_cast<unsigned>(BACKGROUND_BYTE_COUNT));
    free(pixels);
    return nullptr;
  }
  return pixels;
}

void coverWorker(void*) {
  char trackId[32];
  char baseUrl[128];

  xSemaphoreTake(stateMutex, portMAX_DELAY);
  copyText(trackId, requestedTrackId);
  copyText(baseUrl, CydifyNetwork::serverUrl());
  xSemaphoreGive(stateMutex);

  uint32_t dominantColor = 0x1ED760;
  uint16_t* result = downloadCover(baseUrl, trackId, dominantColor);
  uint16_t* backgroundResult =
      downloadBackground(baseUrl, trackId);

  xSemaphoreTake(stateMutex, portMAX_DELAY);
  if (strcmp(trackId, requestedTrackId) == 0) {
    downloadRequested = false;
    if (result != nullptr) {
      if (pendingPixels != nullptr) free(pendingPixels);
      pendingPixels = result;
      result = nullptr;
      copyText(pendingTrackId, trackId);
      pendingDominantColor = dominantColor;
      resultPending = true;
    } else {
      clearPending = true;
    }
    if (backgroundResult != nullptr) {
      if (pendingBackgroundPixels != nullptr) free(pendingBackgroundPixels);
      pendingBackgroundPixels = backgroundResult;
      backgroundResult = nullptr;
      backgroundResultPending = true;
    } else {
      clearBackgroundPending = true;
    }
  }
  workerRunning = false;
  xSemaphoreGive(stateMutex);

  if (result != nullptr) free(result);
  if (backgroundResult != nullptr) free(backgroundResult);
  vTaskDelete(nullptr);
}

bool startWorker() {
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  if (workerRunning) {
    xSemaphoreGive(stateMutex);
    return false;
  }
  workerRunning = true;
  xSemaphoreGive(stateMutex);

  const BaseType_t created = xTaskCreatePinnedToCore(
      coverWorker,
      "cydify-cover",
      HardwareConfig::COVER_TASK_STACK_BYTES,
      nullptr,
      1,
      nullptr,
      0);
  if (created == pdPASS) return true;

  xSemaphoreTake(stateMutex, portMAX_DELAY);
  workerRunning = false;
  xSemaphoreGive(stateMutex);
  return false;
}

}  // namespace

namespace AlbumArtClient {

void begin(
    ImageHandler imageHandler,
    ClearHandler clearHandler,
    BackgroundHandler backgroundHandler,
    ClearHandler clearBackgroundHandler) {
  onImage = imageHandler;
  onClear = clearHandler;
  onBackground = backgroundHandler;
  onClearBackground = clearBackgroundHandler;
  stateMutex = xSemaphoreCreateMutex();
}

void setTrack(const char* trackId) {
  const char* safeTrackId = trackId == nullptr ? "" : trackId;
  bool changed = false;

  xSemaphoreTake(stateMutex, portMAX_DELAY);
  if (strcmp(requestedTrackId, safeTrackId) != 0) {
    copyText(requestedTrackId, safeTrackId);
    downloadRequested = requestedTrackId[0] != '\0';
    if (pendingPixels != nullptr) {
      free(pendingPixels);
      pendingPixels = nullptr;
    }
    if (pendingBackgroundPixels != nullptr) {
      free(pendingBackgroundPixels);
      pendingBackgroundPixels = nullptr;
    }
    resultPending = false;
    backgroundResultPending = false;
    changed = true;
  }
  xSemaphoreGive(stateMutex);

  if (!changed) return;
  if (onClear != nullptr) onClear();
  if (onClearBackground != nullptr) onClearBackground();
  if (displayedPixels != nullptr) {
    free(displayedPixels);
    displayedPixels = nullptr;
  }
  if (displayedBackgroundPixels != nullptr) {
    free(displayedBackgroundPixels);
    displayedBackgroundPixels = nullptr;
  }
  displayedTrackId[0] = '\0';
}

void update() {
  uint16_t* pixelsToPublish = nullptr;
  uint16_t* backgroundToPublish = nullptr;
  char trackToPublish[32] = {};
  uint32_t colorToPublish = 0x1ED760;
  bool shouldClear = false;
  bool shouldClearBackground = false;
  bool shouldStart = false;

  xSemaphoreTake(stateMutex, portMAX_DELAY);
  if (resultPending) {
    pixelsToPublish = pendingPixels;
    pendingPixels = nullptr;
    copyText(trackToPublish, pendingTrackId);
    colorToPublish = pendingDominantColor;
    resultPending = false;
  }
  if (backgroundResultPending) {
    backgroundToPublish = pendingBackgroundPixels;
    pendingBackgroundPixels = nullptr;
    backgroundResultPending = false;
  }
  if (clearPending) {
    shouldClear = true;
    clearPending = false;
  }
  if (clearBackgroundPending) {
    shouldClearBackground = true;
    clearBackgroundPending = false;
  }
  shouldStart = downloadRequested && !workerRunning &&
      requestedTrackId[0] != '\0' && WiFi.status() == WL_CONNECTED &&
      CydifyNetwork::hasServerUrl();
  xSemaphoreGive(stateMutex);

  if (shouldClear && pixelsToPublish == nullptr && onClear != nullptr) onClear();
  if (shouldClearBackground &&
      backgroundToPublish == nullptr &&
      onClearBackground != nullptr) {
    onClearBackground();
  }
  if (pixelsToPublish != nullptr) {
    if (displayedPixels != nullptr) free(displayedPixels);
    displayedPixels = pixelsToPublish;
    copyText(displayedTrackId, trackToPublish);
    if (onImage != nullptr) {
      onImage(
          displayedPixels,
          HardwareConfig::COVER_WIDTH,
          HardwareConfig::COVER_HEIGHT,
          colorToPublish);
    }
    Serial.printf("Capa carregada sem bloquear UI. Heap livre: %u bytes\n",
                  ESP.getFreeHeap());
  }
  if (backgroundToPublish != nullptr) {
    if (displayedBackgroundPixels != nullptr) {
      free(displayedBackgroundPixels);
    }
    displayedBackgroundPixels = backgroundToPublish;
    if (onBackground != nullptr) {
      onBackground(
          displayedBackgroundPixels,
          HardwareConfig::BACKGROUND_WIDTH,
          HardwareConfig::BACKGROUND_HEIGHT);
    }
    Serial.printf(
        "Fundo blur carregado sem bloquear UI. Heap livre: %u bytes\n",
        ESP.getFreeHeap());
  }
  if (shouldStart && !startWorker()) {
    Serial.println("Falha ao iniciar tarefa da capa.");
  }
}

}  // namespace AlbumArtClient
