#pragma once

#include <Arduino.h>

namespace DisplayPort {

bool begin();
void update();
uint32_t freeHeap();
void setBrightness(uint8_t percent);
uint8_t brightness();

}  // namespace DisplayPort
