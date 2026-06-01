#include "DFR1195Board.h"

void DFR1195Board::begin() {
  ESP32Board::begin();

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  pinMode(PIN_USER_BTN, INPUT_PULLUP);
#ifdef PIN_BACK_BTN
  pinMode(PIN_BACK_BTN, INPUT_PULLUP);
#endif

#ifdef PIN_TFT_VDD_CTL
  pinMode(PIN_TFT_VDD_CTL, OUTPUT);
#ifdef PIN_TFT_VDD_CTL_ACTIVE
  digitalWrite(PIN_TFT_VDD_CTL, !PIN_TFT_VDD_CTL_ACTIVE);
#else
  digitalWrite(PIN_TFT_VDD_CTL, LOW);
#endif
#endif

#ifdef PIN_TFT_LEDA_CTL
  pinMode(PIN_TFT_LEDA_CTL, OUTPUT);
#ifdef PIN_TFT_LEDA_CTL_ACTIVE
  digitalWrite(PIN_TFT_LEDA_CTL, !PIN_TFT_LEDA_CTL_ACTIVE);
#else
  digitalWrite(PIN_TFT_LEDA_CTL, LOW);
#endif
#endif
}

const char* DFR1195Board::getManufacturerName() const {
  return "DFRobot DFR1195";
}