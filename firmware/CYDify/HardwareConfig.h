#pragma once

#include <Arduino.h>

namespace HardwareConfig {

constexpr const char* APP_NAME = "CYDify";
constexpr const char* APP_VERSION = "0.10.0-web-control-ota";

constexpr int16_t SCREEN_WIDTH = 320;
constexpr int16_t SCREEN_HEIGHT = 240;

constexpr uint8_t TFT_MOSI = 13;
constexpr uint8_t TFT_MISO = 12;
constexpr uint8_t TFT_SCLK = 14;
constexpr uint8_t TFT_CS = 15;
constexpr uint8_t TFT_DC = 2;
constexpr int8_t TFT_RST = -1;
constexpr uint8_t TFT_BL = 21;
// Rotacao 3 deixa a placa na orientacao fisica preferida pelo usuario.
constexpr uint8_t TFT_ROTATION = 3;
// Configuracao confirmada no hardware do usuario. Nao inverter as cores.
constexpr bool TFT_INVERTED = false;

constexpr const char* WIFI_AP_NAME = "CYDify-Setup";
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr uint32_t PLAYER_REFRESH_INTERVAL_MS = 2000;
constexpr uint16_t SERVER_REQUEST_TIMEOUT_MS = 3500;
constexpr uint16_t COVER_REQUEST_TIMEOUT_MS = 8000;
constexpr uint16_t LYRICS_REQUEST_TIMEOUT_MS = 3500;
constexpr uint16_t LYRICS_UI_INTERVAL_MS = 100;
constexpr uint16_t LYRICS_RETRY_INTERVAL_MS = 1500;
constexpr uint32_t SLEEP_MODE_TIMEOUT_MS = 90000;
constexpr uint16_t PLAYER_TASK_STACK_BYTES = 8192;
constexpr uint16_t COVER_TASK_STACK_BYTES = 6144;
constexpr uint16_t COVER_WIDTH = 100;
constexpr uint16_t COVER_HEIGHT = 100;
// O servidor aplica o blur antes do download. O CYD mantem apenas 6.144
// bytes e amplia a miniatura para a tela inteira, sem exigir PSRAM.
constexpr uint16_t BACKGROUND_WIDTH = 64;
constexpr uint16_t BACKGROUND_HEIGHT = 48;

constexpr uint8_t TOUCH_CLK = 25;
constexpr uint8_t TOUCH_MISO = 39;
constexpr uint8_t TOUCH_MOSI = 32;
constexpr uint8_t TOUCH_CS = 33;
constexpr uint8_t TOUCH_IRQ = 36;

constexpr int16_t RAW_X_LEFT = 3780;
constexpr int16_t RAW_X_RIGHT = 250;
constexpr int16_t RAW_Y_TOP = 3800;
constexpr int16_t RAW_Y_BOTTOM = 340;

// Buffer parcial de 2.560 bytes. Quatro linhas preservam a renderizacao
// parcial e liberam 2.560 bytes de DRAM estatica no ESP32 sem PSRAM.
constexpr uint16_t LVGL_BUFFER_ROWS = 4;

}  // namespace HardwareConfig
