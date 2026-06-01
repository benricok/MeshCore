#pragma once

#include <Arduino.h>
#include <helpers/ESP32Board.h>

class DFR1195Board : public ESP32Board {
public:
  void begin();
  const char* getManufacturerName() const override;
};