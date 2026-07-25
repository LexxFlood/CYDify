#include "DisplayPort.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Preferences.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

#include "HardwareConfig.h"

namespace {

SPIClass displaySpi(VSPI);
SPIClass touchSpi(HSPI);

Adafruit_ST7789 display(
    &displaySpi,
    HardwareConfig::TFT_CS,
    HardwareConfig::TFT_DC,
    HardwareConfig::TFT_RST);

XPT2046_Touchscreen touch(
    HardwareConfig::TOUCH_CS,
    HardwareConfig::TOUCH_IRQ);

static uint16_t drawBuffer[
    HardwareConfig::SCREEN_WIDTH * HardwareConfig::LVGL_BUFFER_ROWS];

lv_display_t* lvDisplay = nullptr;
lv_indev_t* lvTouch = nullptr;
Preferences displayPreferences;
uint8_t brightnessPercent = 85;
bool backlightPwmReady = false;

void applyBrightness() {
  const uint8_t duty = map(brightnessPercent, 5, 100, 13, 255);
  if (backlightPwmReady) {
    ledcWrite(HardwareConfig::TFT_BL, duty);
  } else {
    digitalWrite(HardwareConfig::TFT_BL, brightnessPercent > 5 ? HIGH : LOW);
  }
}

int16_t mapTouchX(int16_t rawX) {
  int32_t value;

  if (HardwareConfig::TFT_ROTATION == 3) {
    // A rotacao 3 e a rotacao 1 virada em 180 graus.
    value = map(
        rawX,
        HardwareConfig::RAW_X_RIGHT,
        HardwareConfig::RAW_X_LEFT,
        0,
        HardwareConfig::SCREEN_WIDTH - 1);
  } else {
    value = map(
        rawX,
        HardwareConfig::RAW_X_LEFT,
        HardwareConfig::RAW_X_RIGHT,
        0,
        HardwareConfig::SCREEN_WIDTH - 1);
  }

  return constrain(value, 0, HardwareConfig::SCREEN_WIDTH - 1);
}

int16_t mapTouchY(int16_t rawY) {
  int32_t value;

  if (HardwareConfig::TFT_ROTATION == 3) {
    value = map(
        rawY,
        HardwareConfig::RAW_Y_BOTTOM,
        HardwareConfig::RAW_Y_TOP,
        0,
        HardwareConfig::SCREEN_HEIGHT - 1);
  } else {
    value = map(
        rawY,
        HardwareConfig::RAW_Y_TOP,
        HardwareConfig::RAW_Y_BOTTOM,
        0,
        HardwareConfig::SCREEN_HEIGHT - 1);
  }

  return constrain(value, 0, HardwareConfig::SCREEN_HEIGHT - 1);
}

uint32_t lvglMillis() {
  return millis();
}

void flushDisplay(
    lv_display_t* lvDisplayHandle,
    const lv_area_t* area,
    uint8_t* pixelMap) {
  const int32_t width = area->x2 - area->x1 + 1;
  const int32_t height = area->y2 - area->y1 + 1;

  display.drawRGBBitmap(
      area->x1,
      area->y1,
      reinterpret_cast<uint16_t*>(pixelMap),
      width,
      height);

  lv_display_flush_ready(lvDisplayHandle);
}

void readTouch(lv_indev_t*, lv_indev_data_t* data) {
  if (!touch.touched()) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  const TS_Point point = touch.getPoint();
  data->point.x = mapTouchX(point.x);
  data->point.y = mapTouchY(point.y);
  data->state = LV_INDEV_STATE_PRESSED;
}

}  // namespace

namespace DisplayPort {

bool begin() {
  displayPreferences.begin("cydify-ui", true);
  brightnessPercent = constrain(
      displayPreferences.getUChar("brightness", 85),
      5,
      100);
  displayPreferences.end();

  pinMode(HardwareConfig::TFT_BL, OUTPUT);
  digitalWrite(HardwareConfig::TFT_BL, HIGH);
  backlightPwmReady = ledcAttach(HardwareConfig::TFT_BL, 5000, 8);
  applyBrightness();

  displaySpi.begin(
      HardwareConfig::TFT_SCLK,
      HardwareConfig::TFT_MISO,
      HardwareConfig::TFT_MOSI,
      HardwareConfig::TFT_CS);

  display.init(240, 320);
  display.invertDisplay(HardwareConfig::TFT_INVERTED);
  display.setRotation(HardwareConfig::TFT_ROTATION);

  touchSpi.begin(
      HardwareConfig::TOUCH_CLK,
      HardwareConfig::TOUCH_MISO,
      HardwareConfig::TOUCH_MOSI,
      HardwareConfig::TOUCH_CS);

  if (!touch.begin(touchSpi)) {
    display.fillScreen(ST77XX_RED);
    return false;
  }

  lv_init();
  lv_tick_set_cb(lvglMillis);

  lvDisplay = lv_display_create(
      HardwareConfig::SCREEN_WIDTH,
      HardwareConfig::SCREEN_HEIGHT);

  if (lvDisplay == nullptr) {
    return false;
  }

  lv_display_set_color_format(lvDisplay, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(
      lvDisplay,
      drawBuffer,
      nullptr,
      sizeof(drawBuffer),
      LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(lvDisplay, flushDisplay);

  lvTouch = lv_indev_create();
  if (lvTouch == nullptr) {
    return false;
  }

  lv_indev_set_type(lvTouch, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(lvTouch, readTouch);
  lv_indev_set_display(lvTouch, lvDisplay);

  return true;
}

void update() {
  lv_timer_handler();
  delay(5);
}

uint32_t freeHeap() {
  return ESP.getFreeHeap();
}

void setBrightness(uint8_t percent) {
  const uint8_t safePercent = constrain(percent, 5, 100);
  if (brightnessPercent == safePercent) return;

  brightnessPercent = safePercent;
  applyBrightness();

  displayPreferences.begin("cydify-ui", false);
  displayPreferences.putUChar("brightness", brightnessPercent);
  displayPreferences.end();
  Serial.printf("Brilho salvo: %u%%\n", brightnessPercent);
}

uint8_t brightness() {
  return brightnessPercent;
}

}  // namespace DisplayPort
