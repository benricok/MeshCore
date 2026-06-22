#include "MQTTBacklink.h"

MQTTBacklink mqttBacklink;

MQTTBacklink::MQTTBacklink() : mqttClient(wifiClient) {
    lastReconnectAttempt = 0;
}

void MQTTBacklink::begin() {
    Serial.println("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // Don't block indefinitely here, just start connecting.
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
}

void MQTTBacklink::reconnect() {
    if (WiFi.status() != WL_CONNECTED) {
        return; // wait for WiFi
    }

    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            Serial.println("Attempting MQTT connection...");
            String clientId = "MeshcoreRepeater-";
            clientId += String(random(0xffff), HEX);
            if (mqttClient.connect(clientId.c_str())) {
                Serial.println("MQTT connected");
                // Once connected, publish an announcement...
                mqttClient.publish("meshcore/status", "online");
                lastReconnectAttempt = 0;
            }
        }
    }
}

void MQTTBacklink::loop() {
    if (!mqttClient.connected()) {
        reconnect();
    } else {
        mqttClient.loop();
    }
}

void MQTTBacklink::publishTelemetry(const String& data) {
    if (mqttClient.connected()) {
        mqttClient.publish("meshcore/telemetry", data.c_str());
    }
}

void MQTTBacklink::publishPacket(const uint8_t* payload, size_t len, int rssi, float snr) {
    if (mqttClient.connected()) {
        JsonDocument doc;
        doc["rssi"] = rssi;
        doc["snr"] = snr;
        doc["len"] = len;

        // Convert payload to hex string
        String hexPayload = "";
        for (size_t i = 0; i < len; i++) {
            if (payload[i] < 16) hexPayload += "0";
            hexPayload += String(payload[i], HEX);
        }
        doc["payload"] = hexPayload;

        String out;
        serializeJson(doc, out);
        mqttClient.publish("meshcore/packet", out.c_str());
    }
}

void MQTTBacklink::publishStats(uint32_t rxCount, uint32_t txCount) {
    if (mqttClient.connected()) {
        JsonDocument doc;
        doc["rx_count"] = rxCount;
        doc["tx_count"] = txCount;

        String out;
        serializeJson(doc, out);
        mqttClient.publish("meshcore/stats", out.c_str());
    }
}
