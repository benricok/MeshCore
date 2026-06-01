#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <Wire.h>
#include <SPI.h>
#include <DFRobot_GDL.h>

class DFR1195Display : public DisplayDriver {
  SPIClass displaySPI;
  DFRobot_ST7735_80x160_HW_SPI display;
  bool _isOn;
  uint16_t _color;

public:
#ifdef USE_PIN_TFT
  DFR1195Display() : DisplayDriver(160, 80),
      display(PIN_TFT_DC, PIN_TFT_CS, PIN_TFT_RST, PIN_TFT_LEDA_CTL, &displaySPI) {
#else
  DFR1195Display() : DisplayDriver(160, 80),
      display(PIN_TFT_DC, PIN_TFT_CS, PIN_TFT_RST, PIN_TFT_LEDA_CTL, &displaySPI) {
#endif
    _isOn = false;
  }

  bool begin();

  bool isOn() override { return _isOn; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(Color bkg = DARK) override;
  void setTextSize(int sz) override;
  void setColor(Color c) override;
  void setCursor(int x, int y) override;
  void print(const char* str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override;
  uint16_t getTextWidth(const char* str) override;
  void endFrame() override;
};