#pragma once

#include <Arduino.h>

namespace LyricsClient {

constexpr uint8_t WINDOW_SIZE = 5;
constexpr size_t LINE_LENGTH = 80;
constexpr uint16_t MAX_LINES = 180;

struct Window {
  bool available = false;
  bool loading = false;
  bool synced = false;
  bool instrumental = false;
  uint8_t count = 0;
  uint8_t currentSlot = 0;
  char lines[WINDOW_SIZE][LINE_LENGTH] = {};
  char message[64] = {};
};

using WindowHandler = void (*)(const Window& window);

void begin(WindowHandler handler);
void setPlayer(const char* trackId, uint32_t progressMs, bool playing);
void setActive(bool active);
void update();

}  // namespace LyricsClient
