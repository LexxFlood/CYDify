#pragma once

#include <Arduino.h>
#include <lvgl.h>

#include "PlayerClient.h"

namespace DashboardPages {

using BrightnessHandler = void (*)(uint8_t percent);
using WifiHandler = void (*)();

void begin(lv_obj_t* screen);
void setHandlers(
    BrightnessHandler brightnessHandler,
    WifiHandler wifiHandler);
void setBrightness(uint8_t percent);
void updatePlayer(const PlayerClient::State& state);
void noteActivity();
void setNavigationLocked(bool locked);
bool sleepVisible();

}  // namespace DashboardPages
