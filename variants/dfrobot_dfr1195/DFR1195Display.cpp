#include "DFR1195Display.h"

#ifndef DISPLAY_ROTATION
  #define DISPLAY_ROTATION 1
#endif

bool DFR1195Display::begin() {
  if (!_isOn) {
#ifdef PIN_TFT_VDD_CTL
    pinMode(PIN_TFT_VDD_CTL, OUTPUT);
#ifdef PIN_TFT_VDD_CTL_ACTIVE
    digitalWrite(PIN_TFT_VDD_CTL, PIN_TFT_VDD_CTL_ACTIVE);
#else
    digitalWrite(PIN_TFT_VDD_CTL, HIGH);
#endif
#endif

#ifdef PIN_TFT_LEDA_CTL
    pinMode(PIN_TFT_LEDA_CTL, OUTPUT);
#ifdef PIN_TFT_LEDA_CTL_ACTIVE
    digitalWrite(PIN_TFT_LEDA_CTL, PIN_TFT_LEDA_CTL_ACTIVE);
#else
    digitalWrite(PIN_TFT_LEDA_CTL, HIGH);
#endif
#endif

#ifdef USE_PIN_TFT
    pinMode(PIN_TFT_CS, OUTPUT);
    digitalWrite(PIN_TFT_CS, HIGH);
    displaySPI.begin(PIN_TFT_SCL, -1, PIN_TFT_SDA, PIN_TFT_CS);
#else
    displaySPI.begin();
#endif

    pinMode(PIN_TFT_RST, OUTPUT);
    digitalWrite(PIN_TFT_RST, HIGH);

    display.begin(16000000);
    display.setRotation(DISPLAY_ROTATION);
    display.fillScreen(0x0000);
    display.setTextColor(0xFFFF);
    display.setTextSize(2);

    _isOn = true;
  }
  return true;
}

void DFR1195Display::turnOn() {
  DFR1195Display::begin();
}

void DFR1195Display::turnOff() {
  if (_isOn) {
    digitalWrite(PIN_TFT_RST, LOW);
#ifdef PIN_TFT_LEDA_CTL
#ifdef PIN_TFT_LEDA_CTL_ACTIVE
    digitalWrite(PIN_TFT_LEDA_CTL, !PIN_TFT_LEDA_CTL_ACTIVE);
#else
    digitalWrite(PIN_TFT_LEDA_CTL, LOW);
#endif
#endif
#ifdef PIN_TFT_VDD_CTL
#ifdef PIN_TFT_VDD_CTL_ACTIVE
    digitalWrite(PIN_TFT_VDD_CTL, !PIN_TFT_VDD_CTL_ACTIVE);
#else
    digitalWrite(PIN_TFT_VDD_CTL, LOW);
#endif
#endif
    _isOn = false;
  }
}

void DFR1195Display::clear() {
  display.fillScreen(0x0000);
}

void DFR1195Display::startFrame(Color bkg) {
  (void)bkg;
  display.fillScreen(0x0000);
  display.setTextColor(0xFFFF);
  display.setTextSize(1);
}

void DFR1195Display::setTextSize(int sz) {
  display.setTextSize(sz);
}

void DFR1195Display::setColor(Color c) {
  switch (c) {
    case DisplayDriver::DARK:
      _color = 0x0000;
      break;
    case DisplayDriver::LIGHT:
      _color = 0xFFFF;
      break;
    case DisplayDriver::RED:
      _color = 0xF800;
      break;
    case DisplayDriver::GREEN:
      _color = 0x07E0;
      break;
    case DisplayDriver::BLUE:
      _color = 0x001F;
      break;
    case DisplayDriver::YELLOW:
      _color = 0xFFE0;
      break;
    case DisplayDriver::ORANGE:
      _color = 0xFC00;
      break;
    default:
      _color = 0xFFFF;
      break;
  }
  display.setTextColor(_color);
}

void DFR1195Display::setCursor(int x, int y) {
  display.setCursor(x, y);
}

void DFR1195Display::print(const char* str) {
  display.print(str);
}

void DFR1195Display::fillRect(int x, int y, int w, int h) {
  display.fillRect(x, y, w, h, _color);
}

void DFR1195Display::drawRect(int x, int y, int w, int h) {
  display.drawRect(x, y, w, h, _color);
}

void DFR1195Display::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  display.drawBitmap(x, y, bits, w, h, _color);
}

uint16_t DFR1195Display::getTextWidth(const char* str) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  return w;
}

void DFR1195Display::endFrame() {
  // no-op for the ST7735 driver
}