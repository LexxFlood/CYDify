#include "DisplayPort.h"
#include "AlbumArtClient.h"
#include "LyricsClient.h"
#include "HardwareConfig.h"
#include "CydifyNetwork.h"
#include "PlayerClient.h"
#include "UserInterface.h"

namespace {

uint32_t lastNetworkUiUpdate = 0;
char lastTrackId[32] = {};

void openWifiSetup() {
  CydifyNetwork::startConfigPortal();
  UserInterface::setNetworkStatus("WIFI SETUP", false);
}

void updateNetworkIndicator() {
  if (millis() - lastNetworkUiUpdate < 500) return;
  lastNetworkUiUpdate = millis();

  UserInterface::setNetworkStatus(
      CydifyNetwork::statusText(),
      CydifyNetwork::isConnected());
}

void onPlayerState(const PlayerClient::State& state) {
  const char* trackId = state.available ? state.trackId : "";
  const bool trackChanged = strcmp(lastTrackId, trackId) != 0;
  AlbumArtClient::setTrack(trackId);
  if (trackChanged) {
    snprintf(lastTrackId, sizeof(lastTrackId), "%s", trackId);
    UserInterface::setMediaLoading(state.available);
  }
  LyricsClient::setPlayer(
      state.available ? state.trackId : "",
      state.progressMs,
      state.playing);
  UserInterface::updatePlayer(state);
}

void onSpotifyStatus(const char* message, bool ok) {
  UserInterface::setSpotifyStatus(message, ok);
}

void onPlayerAction(const char* action) {
  if (strcmp(action, "play") == 0) {
    PlayerClient::sendAction(PlayerClient::Action::Play);
  } else if (strcmp(action, "pause") == 0) {
    PlayerClient::sendAction(PlayerClient::Action::Pause);
  } else if (strcmp(action, "next") == 0) {
    PlayerClient::sendAction(PlayerClient::Action::Next);
  } else if (strcmp(action, "previous") == 0) {
    PlayerClient::sendAction(PlayerClient::Action::Previous);
  }
}

void onLikedChange(bool liked) {
  PlayerClient::setLiked(liked);
}

void onBrightnessChange(uint8_t percent) {
  DisplayPort::setBrightness(percent);
  UserInterface::syncBrightness(percent);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.printf(
      "%s v%s iniciando...\n",
      HardwareConfig::APP_NAME,
      HardwareConfig::APP_VERSION);

  if (!DisplayPort::begin()) {
    Serial.println("ERRO: falha ao inicializar display, touch ou LVGL.");
    while (true) {
      delay(100);
    }
  }

  UserInterface::create();
  UserInterface::setWifiButtonHandler(openWifiSetup);
  UserInterface::setPlayerHandlers(onPlayerAction, onLikedChange);
  UserInterface::setBrightnessHandler(
      onBrightnessChange,
      DisplayPort::brightness());
  CydifyNetwork::setBrightnessHandlers(
      DisplayPort::brightness,
      onBrightnessChange);
  UserInterface::setNetworkStatus("CONECTANDO", false);
  DisplayPort::update();

  CydifyNetwork::begin();
  updateNetworkIndicator();
  PlayerClient::begin(onPlayerState, onSpotifyStatus);
  AlbumArtClient::begin(
      UserInterface::updateAlbumArt,
      UserInterface::clearAlbumArt,
      UserInterface::updateBackground,
      UserInterface::clearBackground);
  LyricsClient::begin(UserInterface::updateLyrics);

  Serial.printf(
      "Inicializacao concluida. Heap livre: %lu bytes\n",
      static_cast<unsigned long>(DisplayPort::freeHeap()));
}

void loop() {
  CydifyNetwork::update();
  updateNetworkIndicator();
  if (!CydifyNetwork::isUpdating()) {
    PlayerClient::update();
    AlbumArtClient::update();
    LyricsClient::setActive(UserInterface::lyricsVisible());
    LyricsClient::update();
  }
  DisplayPort::update();
}
