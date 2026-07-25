#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

constexpr uint8_t TFT_MOSI = 13;
constexpr uint8_t TFT_MISO = 12;
constexpr uint8_t TFT_SCLK = 14;
constexpr uint8_t TFT_CS = 15;
constexpr uint8_t TFT_DC = 2;
constexpr int8_t TFT_RST = -1;
constexpr uint8_t TFT_BL = 21;

SPIClass displaySpi(VSPI);
Adafruit_ST7789 display(&displaySpi, TFT_CS, TFT_DC, TFT_RST);

void showColor(uint16_t color) {
  display.fillScreen(color);
  delay(700);
}

void setup() {
  Serial.begin(115200);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  displaySpi.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
  display.init(240, 320);
  display.invertDisplay(false);
  display.setRotation(1);

  showColor(ST77XX_RED);
  showColor(ST77XX_GREEN);
  showColor(ST77XX_BLUE);
  showColor(ST77XX_WHITE);
  showColor(ST77XX_BLACK);

  display.drawRect(0, 0, 320, 240, ST77XX_RED);
  display.drawRect(4, 4, 312, 232, ST77XX_GREEN);
  display.drawLine(0, 0, 319, 239, ST77XX_BLUE);
  display.drawLine(319, 0, 0, 239, ST77XX_BLUE);

  display.setTextColor(ST77XX_GREEN);
  display.setTextSize(3);
  display.setCursor(100, 40);
  display.print("CYDify");

  display.setTextColor(ST77XX_WHITE);
  display.setTextSize(2);
  display.setCursor(61, 95);
  display.print("Teste ST7789");

  display.setTextSize(1);
  display.setCursor(80, 140);
  display.printf("Resolucao: %d x %d", display.width(), display.height());
  display.setCursor(68, 180);
  display.print(psramFound() ? "PSRAM: detectada" : "PSRAM: nao detectada");
}

void loop() {}
