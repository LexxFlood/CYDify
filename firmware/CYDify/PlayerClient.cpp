#include "PlayerClient.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "CydifyNetwork.h"
#include "HardwareConfig.h"

namespace {

PlayerClient::State currentState;
PlayerClient::State pendingState;
PlayerClient::StateHandler onState = nullptr;
PlayerClient::StatusHandler onStatus = nullptr;

SemaphoreHandle_t stateMutex = nullptr;
bool workerRunning = false;
bool statePending = false;
bool statusPending = false;
bool pendingStatusOk = false;
char pendingStatus[40] = {};

uint32_t lastRefreshAt = 0;
bool refreshRequested = true;
bool actionPending = false;
PlayerClient::Action queuedAction = PlayerClient::Action::Play;
bool likedPending = false;
bool queuedLiked = false;

template <size_t Size>
void copyText(char (&destination)[Size], const char* source) {
  snprintf(destination, Size, "%s", source == nullptr ? "" : source);
}

String endpoint(const char* baseUrl, const char* path) {
  String url;
  url.reserve(strlen(baseUrl) + strlen(path) + 1);
  url += baseUrl;
  url += path;
  return url;
}

void setPendingStatus(const char* message, bool ok) {
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  copyText(pendingStatus, message);
  pendingStatusOk = ok;
  statusPending = true;
  xSemaphoreGive(stateMutex);
}

bool parsePlayer(HTTPClient& http, PlayerClient::State& result) {
  JsonDocument filter;
  filter["player"]["available"] = true;
  filter["player"]["playing"] = true;
  filter["player"]["progress_ms"] = true;
  filter["player"]["duration_ms"] = true;
  filter["player"]["volume_percent"] = true;
  filter["player"]["device"]["name"] = true;
  filter["player"]["device"]["type"] = true;
  filter["player"]["track"]["uri"] = true;
  filter["player"]["track"]["id"] = true;
  filter["player"]["track"]["title"] = true;
  filter["player"]["track"]["artist"] = true;
  filter["player"]["track"]["album"] = true;
  filter["player"]["track"]["liked"] = true;
  filter["player"]["track"]["release_date"] = true;
  filter["player"]["track"]["track_number"] = true;
  filter["player"]["track"]["disc_number"] = true;
  filter["player"]["track"]["popularity"] = true;
  filter["statistics"]["today_seconds"] = true;
  filter["statistics"]["week_seconds"] = true;
  filter["statistics"]["tracks_today"] = true;
  filter["statistics"]["artists_today"] = true;
  filter["statistics"]["albums_today"] = true;
  filter["statistics"]["top_artist"] = true;
  filter["statistics"]["top_track"] = true;

  JsonDocument document;
  const DeserializationError error = deserializeJson(
      document,
      http.getStream(),
      DeserializationOption::Filter(filter));
  if (error) {
    Serial.printf("JSON invalido: %s\n", error.c_str());
    return false;
  }

  const JsonObjectConst player = document["player"];
  const JsonObjectConst track = player["track"];
  const JsonObjectConst device = player["device"];
  const JsonObjectConst statistics = document["statistics"];

  result.available = player["available"] | false;
  result.playing = player["playing"] | false;
  result.progressMs = player["progress_ms"] | 0UL;
  result.durationMs = player["duration_ms"] | 0UL;
  result.todayListeningSeconds = statistics["today_seconds"] | 0UL;
  result.weekListeningSeconds = statistics["week_seconds"] | 0UL;
  result.volumePercent = player["volume_percent"] | -1;
  result.popularity = track["popularity"] | -1;
  result.trackNumber = track["track_number"] | 0;
  result.discNumber = track["disc_number"] | 0;
  result.tracksToday = statistics["tracks_today"] | 0;
  result.artistsToday = statistics["artists_today"] | 0;
  result.albumsToday = statistics["albums_today"] | 0;
  result.liked = track["liked"] | false;
  copyText(result.trackId, track["id"] | "");
  copyText(result.uri, track["uri"] | "");
  copyText(result.title, track["title"] | "Nenhuma musica");
  copyText(result.artist, track["artist"] | "Spotify parado");
  copyText(result.album, track["album"] | "");
  copyText(result.device, device["name"] | "");
  copyText(result.deviceType, device["type"] | "");
  copyText(result.releaseDate, track["release_date"] | "");
  copyText(result.topArtist, statistics["top_artist"] | "");
  copyText(result.topTrack, statistics["top_track"] | "");
  return true;
}

bool fetchPlayer(const char* baseUrl, PlayerClient::State& result) {
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(HardwareConfig::SERVER_REQUEST_TIMEOUT_MS);
  http.useHTTP10(true);

  if (!http.begin(client, endpoint(baseUrl, "/api/player"))) return false;
  const int statusCode = http.GET();
  if (statusCode != HTTP_CODE_OK) {
    Serial.printf("Servidor respondeu HTTP %d\n", statusCode);
    http.end();
    return false;
  }

  const bool success = parsePlayer(http, result);
  http.end();
  return success;
}

bool performPost(const char* baseUrl, const char* path, const char* body = nullptr) {
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(HardwareConfig::SERVER_REQUEST_TIMEOUT_MS);
  http.useHTTP10(true);

  if (!http.begin(client, endpoint(baseUrl, path))) return false;
  if (body != nullptr) http.addHeader("Content-Type", "application/json");
  const int statusCode = http.POST(body == nullptr ? "" : body);
  http.end();
  if (statusCode < 200 || statusCode >= 300) {
    Serial.printf("POST %s falhou: HTTP %d\n", path, statusCode);
    return false;
  }
  return true;
}

const char* actionRoute(PlayerClient::Action action) {
  switch (action) {
    case PlayerClient::Action::Play: return "/api/player/play";
    case PlayerClient::Action::Pause: return "/api/player/pause";
    case PlayerClient::Action::Next: return "/api/player/next";
    case PlayerClient::Action::Previous: return "/api/player/previous";
  }
  return nullptr;
}

void playerWorker(void*) {
  char baseUrl[128];
  char likedUri[64];
  bool doAction = false;
  bool doLiked = false;
  bool liked = false;
  PlayerClient::Action action = PlayerClient::Action::Play;

  xSemaphoreTake(stateMutex, portMAX_DELAY);
  copyText(baseUrl, CydifyNetwork::serverUrl());
  if (likedPending) {
    doLiked = true;
    liked = queuedLiked;
    copyText(likedUri, currentState.uri);
    likedPending = false;
  } else if (actionPending) {
    doAction = true;
    action = queuedAction;
    actionPending = false;
  }
  xSemaphoreGive(stateMutex);

  if (WiFi.status() != WL_CONNECTED || baseUrl[0] == '\0') {
    setPendingStatus(WiFi.status() == WL_CONNECTED ? "CONFIGURE SERVER" : "WIFI OFFLINE", false);
  } else if (doLiked) {
    char body[128];
    snprintf(body, sizeof(body), "{\"uri\":\"%s\",\"liked\":%s}",
             likedUri, liked ? "true" : "false");
    const bool success = performPost(baseUrl, "/api/player/liked/set", body);
    setPendingStatus(success ? "SPOTIFY ONLINE" : "LIKE FALHOU", success);
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    refreshRequested = true;
    xSemaphoreGive(stateMutex);
  } else if (doAction) {
    const char* route = actionRoute(action);
    const bool success = route != nullptr && performPost(baseUrl, route);
    setPendingStatus(success ? "SPOTIFY ONLINE" : "CONTROLE FALHOU", success);
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    refreshRequested = true;
    xSemaphoreGive(stateMutex);
  } else {
    PlayerClient::State result;
    const bool success = fetchPlayer(baseUrl, result);
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    if (success) {
      pendingState = result;
      statePending = true;
      copyText(pendingStatus, result.available ? "SPOTIFY ONLINE" : "SEM REPRODUCAO");
      pendingStatusOk = true;
    } else {
      copyText(pendingStatus, "SERVER OFFLINE");
      pendingStatusOk = false;
    }
    lastRefreshAt = millis();
    statusPending = true;
    xSemaphoreGive(stateMutex);
  }

  xSemaphoreTake(stateMutex, portMAX_DELAY);
  workerRunning = false;
  xSemaphoreGive(stateMutex);
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
      playerWorker,
      "cydify-player",
      HardwareConfig::PLAYER_TASK_STACK_BYTES,
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

namespace PlayerClient {

void begin(StateHandler stateHandler, StatusHandler statusHandler) {
  onState = stateHandler;
  onStatus = statusHandler;
  stateMutex = xSemaphoreCreateMutex();
  refreshRequested = true;
}

void update() {
  State stateToPublish;
  bool publishState = false;
  char statusToPublish[40] = {};
  bool publishStatus = false;
  bool statusOk = false;
  bool shouldStart = false;

  xSemaphoreTake(stateMutex, portMAX_DELAY);
  if (statePending) {
    currentState = pendingState;
    stateToPublish = currentState;
    statePending = false;
    publishState = true;
  }
  if (statusPending) {
    copyText(statusToPublish, pendingStatus);
    statusOk = pendingStatusOk;
    statusPending = false;
    publishStatus = true;
  }

  const uint32_t now = millis();
  shouldStart = !workerRunning &&
      (likedPending || actionPending || refreshRequested ||
       now - lastRefreshAt >= HardwareConfig::PLAYER_REFRESH_INTERVAL_MS);
  if (shouldStart && !likedPending && !actionPending) {
    refreshRequested = false;
    lastRefreshAt = now;
  }
  xSemaphoreGive(stateMutex);

  if (publishState && onState != nullptr) onState(stateToPublish);
  if (publishStatus) {
    Serial.printf("Spotify: %s\n", statusToPublish);
    if (onStatus != nullptr) onStatus(statusToPublish, statusOk);
  }
  if (shouldStart && !startWorker()) {
    Serial.println("Falha ao iniciar tarefa do player.");
  }
}

void refreshNow() {
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  refreshRequested = true;
  xSemaphoreGive(stateMutex);
}

bool sendAction(Action action) {
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  queuedAction = action;
  actionPending = true;
  xSemaphoreGive(stateMutex);
  return true;
}

bool setLiked(bool liked) {
  State stateToPublish;
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  if (currentState.uri[0] == '\0') {
    xSemaphoreGive(stateMutex);
    return false;
  }
  queuedLiked = liked;
  likedPending = true;
  currentState.liked = liked;
  stateToPublish = currentState;
  xSemaphoreGive(stateMutex);

  if (onState != nullptr) onState(stateToPublish);
  return true;
}

const State& state() {
  return currentState;
}

}  // namespace PlayerClient
