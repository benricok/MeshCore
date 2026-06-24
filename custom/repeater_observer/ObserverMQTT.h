#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Identity.h>

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
#define MQTT_PORT "1883"
#endif

#ifndef MQTT_USER
#define MQTT_USER ""
#endif

#ifndef MQTT_PWD
#define MQTT_PWD ""
#endif

#ifndef OBSERVER_IATA
#define OBSERVER_IATA "XXX"
#endif

class ObserverMQTT {
public:
    ObserverMQTT();
    void begin();
    void loop();
    void publishPacket(const uint8_t* payload, size_t len, int rssi, float snr);
    void publishAdvert(const mesh::Identity& id, uint32_t timestamp, const uint8_t* app_data, size_t app_data_len, int rssi, float snr);
    void publishConfig(const void* prefsPtr); // Accepts void* to avoid circular header deps
    void publishNeighbors(const void* neighboursPtr, int max_neighbours);
    void publishStatus(uint32_t uptime_secs, int wifi_rssi, uint32_t free_heap, uint32_t rx_count, uint32_t tx_count, const uint8_t* telemetry_buf, size_t telemetry_len);

private:
    void reconnect();
    
    static void mqttCallback(char* topic, byte* payload, unsigned int length);
    void handleMessage(char* topic, byte* payload, unsigned int length);
    
    String getTopicPrefix();
    
    WiFiClient wifiClient;
    PubSubClient mqttClient;
    unsigned long lastReconnectAttempt;
};

extern ObserverMQTT observerMQTT;
