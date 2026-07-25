#include "LyricsClient.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "CydifyNetwork.h"
#include "HardwareConfig.h"

namespace {

struct LyricLine {
  uint32_t timeMs = 0;
  char text[LyricsClient::LINE_LENGTH] = {};
};

struct DownloadResult {
  bool success = false;
  bool loading = false;
  bool available = false;
  bool synced = false;
  bool instrumental = false;
  uint16_t count = 0;
  LyricLine* lines = nullptr;
  char message[64] = {};
};

LyricsClient::Window currentWindow;
LyricsClient::WindowHandler onWindow = nullptr;
SemaphoreHandle_t stateMutex = nullptr;
TaskHandle_t downloadTaskHandle = nullptr;

LyricLine* lyricLines = nullptr;
uint16_t lyricCount = 0;
char currentTrackId[32] = {};
char loadedTrackId[32] = {};
char serverBaseUrl[128] = {};
char stateMessage[64] = "BUSCANDO LETRA...";

uint32_t baseProgressMs = 0;
uint32_t playerStateAt = 0;
uint32_t lastUiUpdateAt = 0;
uint32_t nextDownloadAt = 0;
int16_t lastRenderedIndex = -2;

bool isPlaying = false;
bool isActive = false;
bool lyricsAvailable = false;
bool lyricsSynced = false;
bool lyricsInstrumental = false;
bool downloadRequested = false;

template <size_t Size>
void copyText(char (&destination)[Size], const char* source) {
  snprintf(destination, Size, "%s", source == nullptr ? "" : source);
}

uint32_t estimatedProgress() {
  if (!isPlaying) return baseProgressMs;
  return baseProgressMs + (millis() - playerStateAt);
}

void freeResult(DownloadResult& result) {
  if (result.lines != nullptr) {
    free(result.lines);
    result.lines = nullptr;
  }
}

DownloadResult fetchLyrics(const char* trackId, const char* baseUrl) {
  DownloadResult result;
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(HardwareConfig::LYRICS_REQUEST_TIMEOUT_MS);
  http.useHTTP10(true);

  String url;
  url.reserve(strlen(baseUrl) + strlen(trackId) + 32);
  url += baseUrl;
  url += "/api/lyrics/";
  url += trackId;
  url += "/full";

  if (!http.begin(client, url)) {
    copyText(result.message, "URL DE LETRAS INVALIDA");
    return result;
  }

  const int statusCode = http.GET();
  if (statusCode != HTTP_CODE_OK) {
    snprintf(result.message, sizeof(result.message), "LETRAS HTTP %d", statusCode);
    http.end();
    return result;
  }

  JsonDocument document;
  const DeserializationError error = deserializeJson(document, http.getStream());
  http.end();
  if (error) {
    copyText(result.message, "ERRO AO LER LETRA");
    return result;
  }

  result.success = true;
  result.loading = document["loading"] | false;
  result.available = document["available"] | false;
  result.synced = document["synced"] | false;
  result.instrumental = document["instrumental"] | false;
  copyText(result.message, document["message"] | "");

  if (!result.available || result.instrumental) return result;

  const JsonArrayConst lines = document["lines"];
  const uint16_t requestedCount = min(
      static_cast<uint16_t>(lines.size()),
      LyricsClient::MAX_LINES);
  if (requestedCount == 0) return result;

  result.lines = static_cast<LyricLine*>(malloc(requestedCount * sizeof(LyricLine)));
  if (result.lines == nullptr) {
    result.success = false;
    result.available = false;
    copyText(result.message, "SEM MEMORIA PARA LETRA");
    return result;
  }

  for (JsonObjectConst line : lines) {
    if (result.count >= requestedCount) break;
    result.lines[result.count].timeMs = line["time_ms"] | 0UL;
    copyText(result.lines[result.count].text, line["text"] | "");
    result.count++;
  }
  return result;
}

void downloadTask(void*) {
  char trackId[32];
  char baseUrl[128];

  xSemaphoreTake(stateMutex, portMAX_DELAY);
  copyText(trackId, currentTrackId);
  copyText(baseUrl, serverBaseUrl);
  xSemaphoreGive(stateMutex);

  DownloadResult result = fetchLyrics(trackId, baseUrl);

  xSemaphoreTake(stateMutex, portMAX_DELAY);
  if (strcmp(trackId, currentTrackId) == 0) {
    if (result.success && !result.loading) {
      if (lyricLines != nullptr) free(lyricLines);
      lyricLines = result.lines;
      result.lines = nullptr;
      lyricCount = result.count;
      lyricsAvailable = result.available;
      lyricsSynced = result.synced;
      lyricsInstrumental = result.instrumental;
      copyText(loadedTrackId, trackId);
      copyText(
          stateMessage,
          result.message[0] == '\0'
              ? (result.instrumental ? "FAIXA INSTRUMENTAL" : "")
              : result.message);
      downloadRequested = false;
      lastRenderedIndex = -2;
    } else {
      copyText(
          stateMessage,
          result.message[0] == '\0' ? "BUSCANDO LETRA..." : result.message);
      downloadRequested = true;
      nextDownloadAt = millis() + HardwareConfig::LYRICS_RETRY_INTERVAL_MS;
    }
  }
  downloadTaskHandle = nullptr;
  xSemaphoreGive(stateMutex);

  freeResult(result);
  vTaskDelete(nullptr);
}

void startDownloadIfNeeded() {
  if (!downloadRequested || downloadTaskHandle != nullptr ||
      currentTrackId[0] == '\0' || millis() < nextDownloadAt ||
      WiFi.status() != WL_CONNECTED || !CydifyNetwork::hasServerUrl()) {
    return;
  }

  copyText(serverBaseUrl, CydifyNetwork::serverUrl());
  const BaseType_t created = xTaskCreatePinnedToCore(
      downloadTask,
      "cydify-lyrics",
      8192,
      nullptr,
      1,
      &downloadTaskHandle,
      0);
  if (created != pdPASS) {
    downloadTaskHandle = nullptr;
    copyText(stateMessage, "FALHA AO INICIAR LETRAS");
    nextDownloadAt = millis() + HardwareConfig::LYRICS_RETRY_INTERVAL_MS;
  }
}

int16_t findCurrentLine(uint32_t progressMs) {
  if (!lyricsSynced || lyricCount == 0) return 0;
  int16_t low = 0;
  int16_t high = static_cast<int16_t>(lyricCount) - 1;
  int16_t answer = 0;
  while (low <= high) {
    const int16_t middle = low + (high - low) / 2;
    if (lyricLines[middle].timeMs <= progressMs) {
      answer = middle;
      low = middle + 1;
    } else {
      high = middle - 1;
    }
  }
  return answer;
}

void publishCurrentWindow() {
  LyricsClient::Window nextWindow;

  xSemaphoreTake(stateMutex, portMAX_DELAY);
  nextWindow.available = lyricsAvailable;
  nextWindow.synced = lyricsSynced;
  nextWindow.instrumental = lyricsInstrumental;
  copyText(nextWindow.message, stateMessage);

  if (!lyricsAvailable || lyricCount == 0 || lyricLines == nullptr) {
    nextWindow.loading = downloadRequested || downloadTaskHandle != nullptr;
    xSemaphoreGive(stateMutex);
    currentWindow = nextWindow;
    if (onWindow != nullptr) onWindow(currentWindow);
    return;
  }

  const int16_t current = findCurrentLine(estimatedProgress());
  if (current == lastRenderedIndex) {
    xSemaphoreGive(stateMutex);
    return;
  }
  lastRenderedIndex = current;

  const int16_t possibleMaxStart =
      static_cast<int16_t>(lyricCount) -
      static_cast<int16_t>(LyricsClient::WINDOW_SIZE);
  const int16_t maxStart = possibleMaxStart > 0 ? possibleMaxStart : 0;
  const int16_t desiredStart =
      current > 2 ? static_cast<int16_t>(current - 2) : 0;
  const int16_t start = desiredStart < maxStart ? desiredStart : maxStart;
  nextWindow.currentSlot = lyricsSynced ? current - start : 0;

  for (uint8_t slot = 0; slot < LyricsClient::WINDOW_SIZE; slot++) {
    const uint16_t source = start + slot;
    if (source >= lyricCount) break;
    copyText(nextWindow.lines[slot], lyricLines[source].text);
    nextWindow.count++;
  }
  xSemaphoreGive(stateMutex);

  currentWindow = nextWindow;
  if (onWindow != nullptr) onWindow(currentWindow);
}

}  // namespace

namespace LyricsClient {

void begin(WindowHandler handler) {
  onWindow = handler;
  stateMutex = xSemaphoreCreateMutex();
}

void setPlayer(const char* trackId, uint32_t progressMs, bool playing) {
  const char* safeTrackId = trackId == nullptr ? "" : trackId;
  const bool trackChanged = strcmp(currentTrackId, safeTrackId) != 0;
  baseProgressMs = progressMs;
  playerStateAt = millis();
  isPlaying = playing;

  if (!trackChanged) return;

  xSemaphoreTake(stateMutex, portMAX_DELAY);
  copyText(currentTrackId, safeTrackId);
  loadedTrackId[0] = '\0';
  lyricsAvailable = false;
  lyricsSynced = false;
  lyricsInstrumental = false;
  lyricCount = 0;
  if (lyricLines != nullptr) {
    free(lyricLines);
    lyricLines = nullptr;
  }
  copyText(
      stateMessage,
      currentTrackId[0] == '\0' ? "SEM REPRODUCAO" : "BUSCANDO LETRA...");
  downloadRequested = currentTrackId[0] != '\0';
  nextDownloadAt = 0;
  lastRenderedIndex = -2;
  xSemaphoreGive(stateMutex);
}

void setActive(bool active) {
  isActive = active;
}

void update() {
  startDownloadIfNeeded();
  if (!isActive || stateMutex == nullptr) return;

  const uint32_t now = millis();
  if (now - lastUiUpdateAt < HardwareConfig::LYRICS_UI_INTERVAL_MS) return;
  lastUiUpdateAt = now;
  publishCurrentWindow();
}

}  // namespace LyricsClient
