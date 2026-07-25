#pragma once

#include <Arduino.h>

namespace PlayerClient {

enum class Action : uint8_t {
  Play,
  Pause,
  Next,
  Previous,
};

struct State {
  bool available = false;
  bool playing = false;
  bool liked = false;
  uint32_t progressMs = 0;
  uint32_t durationMs = 0;
  uint32_t todayListeningSeconds = 0;
  uint32_t weekListeningSeconds = 0;
  int16_t volumePercent = -1;
  int16_t popularity = -1;
  uint16_t trackNumber = 0;
  uint16_t discNumber = 0;
  uint16_t tracksToday = 0;
  uint16_t artistsToday = 0;
  uint16_t albumsToday = 0;
  char trackId[32] = {};
  char uri[64] = {};
  char title[96] = {};
  char artist[96] = {};
  char album[96] = {};
  char device[48] = {};
  char deviceType[24] = {};
  char releaseDate[16] = {};
  char topArtist[64] = {};
  char topTrack[64] = {};
};

using StateHandler = void (*)(const State& state);
using StatusHandler = void (*)(const char* message, bool ok);

void begin(StateHandler stateHandler, StatusHandler statusHandler);
void update();
void refreshNow();
bool sendAction(Action action);
bool setLiked(bool liked);
const State& state();

}  // namespace PlayerClient
