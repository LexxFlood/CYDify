#pragma once

#include <Arduino.h>

namespace CydifyNetwork {

using BrightnessGetter = uint8_t (*)();
using BrightnessSetter = void (*)(uint8_t percent);

void begin();
void update();
void startConfigPortal();
void setBrightnessHandlers(
    BrightnessGetter getter,
    BrightnessSetter setter);

bool isConnected();
bool isPortalActive();
bool isUpdating();
const char* statusText();
const char* serverUrl();
bool hasServerUrl();

}  // namespace CydifyNetwork
