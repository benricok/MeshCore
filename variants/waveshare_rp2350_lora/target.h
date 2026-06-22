#pragma once

#define RADIOLIB_STATIC_ONLY 1

#include <RadioLib.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <helpers/SensorManager.h>
#include <WaveshareBoard.h>

extern WaveshareBoard board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;

#if defined(HAS_DHT11_SENSOR)
#include <DHT.h>

class DHTSensorManager : public SensorManager {
private:
  DHT dht;
  float last_temp;
  float last_hum;
  unsigned long last_read_time;

public:
  DHTSensorManager();
  bool begin() override;
  void loop() override;
  bool querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) override;
};

// Export the custom manager
extern DHTSensorManager sensors;

#else
// Fallback if the sensor flag isn't set in platformio.ini
extern SensorManager sensors;
#endif

bool radio_init();
uint32_t radio_get_rng_seed();
void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr);
void radio_set_tx_power(int8_t dbm);
mesh::LocalIdentity radio_new_identity();
