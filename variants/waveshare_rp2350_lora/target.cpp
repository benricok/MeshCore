#include "target.h"

#include <Arduino.h>
#include <helpers/ArduinoHelpers.h>

WaveshareBoard board;

RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, SPI1);
WRAPPER_CLASS radio_driver(radio, board);

VolatileRTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);

// --- Custom Sensor Manager Implementation ---
#if defined(HAS_DHT11_SENSOR)

// Instantiate our custom manager
DHTSensorManager sensors;

DHTSensorManager::DHTSensorManager() 
  : dht(DHT_PIN, DHT_TYPE), last_temp(0), last_hum(0), last_read_time(0) {}

bool DHTSensorManager::begin() {
  dht.begin();
  return true;
}

void DHTSensorManager::loop() {
  // Read the DHT11 every 5 seconds to prevent blocking the radio during queries
  if (millis() - last_read_time > 5000 || last_read_time == 0) {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    
    // Only update if the read was successful
    if (!isnan(t) && !isnan(h)) {
      last_temp = t;
      last_hum = h;
    }
    last_read_time = millis();
  }
}

bool DHTSensorManager::querySensors(uint8_t requester_permissions, CayenneLPP& telemetry) {
  bool data_added = false;

  // Check if the requester is allowed to see environment data
  if (requester_permissions & TELEM_PERM_ENVIRONMENT) {
    // Make sure we have taken at least one valid reading
    if (last_read_time > 0) {
      // TELEM_CHANNEL_SELF is defined in SensorManager.h (Channel 1)
      telemetry.addTemperature(TELEM_CHANNEL_SELF, last_temp);
      
      // Use the next channel up for humidity
      telemetry.addRelativeHumidity(TELEM_CHANNEL_SELF, last_hum);
      
      data_added = true;
    }
  }
  
  return data_added;
}

#else
// Instantiate the default manager if no sensor is defined
SensorManager sensors;
#endif

bool radio_init() {
  rtc_clock.begin(Wire);

  SPI1.setSCK(P_LORA_SCLK);
  SPI1.setTX(P_LORA_MOSI);
  SPI1.setRX(P_LORA_MISO);

  pinMode(P_LORA_NSS, OUTPUT);
  digitalWrite(P_LORA_NSS, HIGH);

  SPI1.begin(false);

  //passing NULL skips init of SPI
  return radio.std_init(NULL);
}

uint32_t radio_get_rng_seed() {
  return radio.random(0x7FFFFFFF);
}

void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr) {
  radio.setFrequency(freq);
  radio.setSpreadingFactor(sf);
  radio.setBandwidth(bw);
  radio.setCodingRate(cr);
}

void radio_set_tx_power(int8_t dbm) {
  radio.setOutputPower(dbm);
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng); // create new random identity
}

