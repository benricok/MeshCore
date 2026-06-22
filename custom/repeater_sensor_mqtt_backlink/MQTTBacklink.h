#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#ifndef WIFI_SSID
#define WIFI_SSID "YourSSID"
#endif

#ifndef WIFI_PASS
#define WIFI_PASS "YourPass"
#endif

#ifndef MQTT_SERVER
#define MQTT_SERVER "192.168.1.100"
#endif

#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif

class MQTTBacklink {
public:
    MQTTBacklink();
    void begin();
    void loop();

    void publishTelemetry(const String& data);
    void publishPacket(const uint8_t* payload, size_t len, int rssi, float snr);
    void publishStats(uint32_t rxCount, uint32_t txCount);

private:
    void reconnect();
    
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    unsigned long lastReconnectAttempt;
};

extern MQTTBacklink mqttBacklink;
