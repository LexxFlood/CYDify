#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <XPT2046_Touchscreen.h>

constexpr uint8_t TFT_MOSI = 13;
constexpr uint8_t TFT_MISO = 12;
constexpr uint8_t TFT_SCLK = 14;
constexpr uint8_t TFT_CS = 15;
constexpr uint8_t TFT_DC = 2;
constexpr int8_t TFT_RST = -1;
constexpr uint8_t TFT_BL = 21;

constexpr uint8_t TOUCH_CLK = 25;
constexpr uint8_t TOUCH_MISO = 39;
constexpr uint8_t TOUCH_MOSI = 32;
constexpr uint8_t TOUCH_CS = 33;
constexpr uint8_t TOUCH_IRQ = 36;

SPIClass displaySpi(VSPI);
SPIClass touchSpi(HSPI);
Adafruit_ST7789 display(&displaySpi, TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

const char* cornerNames[] = {
    "SUPERIOR ESQUERDO",
    "SUPERIOR DIREITO",
    "INFERIOR DIREITO",
    "INFERIOR ESQUERDO"};

const int16_t targetX[] = {20, 299, 299, 20};
const int16_t targetY[] = {20, 20, 219, 219};

TS_Point measured[4];
uint8_t currentCorner = 0;
bool wasTouched = false;
TS_Point lastPoint;

void drawTarget(uint8_t corner) {
  display.fillScreen(ST77XX_BLACK);
  display.setTextColor(ST77XX_WHITE);
  display.setTextSize(2);
  display.setCursor(40, 90);
  display.print("Toque e solte no");
  display.setTextSize(1);
  display.setCursor(90, 118);
  display.print(cornerNames[corner]);

  const int16_t x = targetX[corner];
  const int16_t y = targetY[corner];
  display.drawCircle(x, y, 10, ST77XX_RED);
  display.drawCircle(x, y, 5, ST77XX_WHITE);
  display.drawLine(x - 14, y, x + 14, y, ST77XX_RED);
  display.drawLine(x, y - 14, x, y + 14, ST77XX_RED);
}

void printResults() {
  Serial.println("================================");
  Serial.println("CALIBRACAO DO TOUCH");
  Serial.println("================================");
  for (uint8_t i = 0; i < 4; i++) {
    Serial.printf(
        "%s: X=%d Y=%d Z=%d\n",
        cornerNames[i],
        measured[i].x,
        measured[i].y,
        measured[i].z);
  }
  Serial.println("================================");

  display.fillScreen(ST77XX_BLACK);
  display.setTextColor(ST77XX_GREEN);
  display.setTextSize(3);
  display.setCursor(76, 45);
  display.print("Concluido");
  display.setTextColor(ST77XX_WHITE);
  display.setTextSize(2);
  display.setCursor(30, 110);
  display.print("Veja o Serial");
}

void setup() {
  Serial.begin(115200);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  displaySpi.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
  display.init(240, 320);
  display.invertDisplay(false);
  display.setRotation(1);

  touchSpi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);

  if (!touch.begin(touchSpi)) {
    Serial.println("ERRO: XPT2046 nao iniciado.");
    while (true) delay(100);
  }

  Serial.println("Toque e solte em cada alvo.");
  drawTarget(currentCorner);
}

void loop() {
  if (currentCorner >= 4) {
    delay(20);
    return;
  }

  const bool isTouched = touch.touched();
  if (isTouched) {
    lastPoint = touch.getPoint();
    wasTouched = true;
  }

  if (!isTouched && wasTouched) {
    measured[currentCorner] = lastPoint;
    currentCorner++;
    wasTouched = false;

    if (currentCorner < 4) {
      delay(300);
      drawTarget(currentCorner);
    } else {
      printResults();
    }
  }

  delay(10);
}
