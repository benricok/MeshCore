#include "MQTTBacklink.h"
#include "MyMesh.h"

extern MyMesh the_mesh;

MQTTBacklink mqttBacklink;

MQTTBacklink::MQTTBacklink() : mqttClient(wifiClient) {
    lastReconnectAttempt = 0;
}

void MQTTBacklink::begin() {
    Serial.println("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // Don't block indefinitely here, just start connecting.
    mqttClient.setServer(MQTT_SERVER, atoi(MQTT_PORT));
    mqttClient.setBufferSize(2048); // Increase buffer size for large JSON payloads
    mqttClient.setCallback(MQTTBacklink::mqttCallback);
}

void MQTTBacklink::reconnect() {
    if (WiFi.status() != WL_CONNECTED) {
        unsigned long now = millis();
        static unsigned long lastWiFiPrint = 0;
        if (now - lastWiFiPrint > 5000) {
            lastWiFiPrint = now;
            Serial.print("WiFi not connected. Status: ");
            Serial.println(WiFi.status());
            Serial.print("SSID: ");
            Serial.println(WIFI_SSID);
            // Serial.print("PASS: ");
            // Serial.println(WIFI_PASS);
            
            // If it failed or disconnected, try again
            if (WiFi.status() == WL_CONNECT_FAILED || WiFi.status() == WL_DISCONNECTED || WiFi.status() == WL_CONNECTION_LOST || WiFi.status() == WL_IDLE_STATUS) {
                Serial.println("Attempting WiFi reconnect...");
                WiFi.disconnect();
                delay(100);
                WiFi.mode(WIFI_STA);
                WiFi.begin(WIFI_SSID, WIFI_PASS);
                
                // Wait up to 10 seconds for connection
                int attempts = 0;
                while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                    delay(500);
                    Serial.print(".");
                    attempts++;
                }
                Serial.println();
                
                if (WiFi.status() == WL_CONNECTED) {
                    Serial.println("WiFi Connected!");
                    Serial.print("IP: ");
                    Serial.println(WiFi.localIP());
                } else {
                    Serial.print("WiFi failed to connect. Final Status: ");
                    Serial.println(WiFi.status());
                }
            }
        }
        return; // wait for WiFi
    }

    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;
            Serial.println("Attempting MQTT connection...");
            Serial.print("MQTT Server: ");
            Serial.println(MQTT_SERVER);
            Serial.print("MQTT Port: ");
            Serial.println(MQTT_PORT);
            Serial.print("MQTT User: ");
            Serial.println(MQTT_USER);
            // Serial.print("MQTT PASS: ");
            // Serial.println(MQTT_PASS);

            String clientId = "MeshcoreRepeater-";
            clientId += String(random(0xffff), HEX);
            
            bool connected = false;
            if (strlen(MQTT_USER) > 0) {
                connected = mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PWD);
            } else {
                connected = mqttClient.connect(clientId.c_str());
            }
            
            if (connected) {
                Serial.println("MQTT connected successfully!");
                // Once connected, publish an announcement...
                mqttClient.publish("meshcore/status", "online");
                // Subscribe to time endpoints
                mqttClient.subscribe("meshcore/time/read");
                mqttClient.subscribe("meshcore/time/update");
                lastReconnectAttempt = 0;
            } else {
                Serial.print("MQTT connection failed, rc=");
                Serial.println(mqttClient.state());
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
        if (!mqttClient.publish("meshcore/telemetry", data.c_str())) {
            Serial.print("Failed to publish telemetry! Size: ");
            Serial.println(data.length());
        }
    }
}

void MQTTBacklink::publishPacket(const uint8_t* payload, size_t len, int rssi, float snr) {
    if (mqttClient.connected()) {
        JsonDocument doc;
        doc["rssi"] = rssi;
        doc["snr"] = snr;
        doc["raw_len"] = len;
        
        mesh::Packet pkt;
        if (pkt.readFrom(payload, len)) {
            doc["is_flood"] = pkt.isRouteFlood();
            doc["is_direct"] = pkt.isRouteDirect();
            doc["payload_type"] = pkt.getPayloadType();
            doc["payload_version"] = pkt.getPayloadVer();
            doc["payload_len"] = pkt.payload_len;
            doc["hop_count"] = pkt.getPathHashCount();
            doc["path_hash_size"] = pkt.getPathHashSize();
            doc["do_not_retransmit"] = pkt.isMarkedDoNotRetransmit();
            doc["route_type"] = pkt.getRouteType();
            
            if (pkt.hasTransportCodes()) {
                 doc["transport_code_0"] = pkt.transport_codes[0];
                 doc["transport_code_1"] = pkt.transport_codes[1];
            }
            
            // Extract the path of hashes (sender -> repeater -> repeater ...)
            uint8_t hash_size = pkt.getPathHashSize();
            uint8_t hash_count = pkt.getPathHashCount();
            if (hash_size > 0 && hash_count > 0) {
                JsonArray pathArr = doc["path"].to<JsonArray>();
                for (int i = 0; i < hash_count; i++) {
                    String h = "";
                    for (int j = 0; j < hash_size; j++) {
                        uint8_t b = pkt.path[i * hash_size + j];
                        if (b < 16) h += "0";
                        h += String(b, HEX);
                    }
                    pathArr.add(h);
                }
            }
            
            String innerPayload = "";
            for (size_t i = 0; i < pkt.payload_len; i++) {
                if (pkt.payload[i] < 16) innerPayload += "0";
                innerPayload += String(pkt.payload[i], HEX);
            }
            doc["inner_payload_hex"] = innerPayload;
        }
        
        String hexPayload = "";
        for (size_t i = 0; i < len; i++) {
            if (payload[i] < 16) hexPayload += "0";
            hexPayload += String(payload[i], HEX);
        }
        doc["raw_hex"] = hexPayload;

        String out;
        serializeJson(doc, out);
        if (!mqttClient.publish("meshcore/packet", out.c_str())) {
            Serial.print("Failed to publish packet! Size: ");
            Serial.println(out.length());
        }
    }
}

void MQTTBacklink::publishStats(uint32_t rxCount, uint32_t txCount) {
    if (mqttClient.connected()) {
        JsonDocument doc;
        doc["rx_count"] = rxCount;
        doc["tx_count"] = txCount;

        String out;
        serializeJson(doc, out);
        if (!mqttClient.publish("meshcore/stats", out.c_str())) {
            Serial.print("Failed to publish stats! Size: ");
            Serial.println(out.length());
        }
    }
}

void MQTTBacklink::publishConfig(const void* prefsPtr) {
    if (mqttClient.connected() && prefsPtr != nullptr) {
        const NodePrefs* prefs = (const NodePrefs*)prefsPtr;
        JsonDocument doc;
        doc["node_name"] = prefs->node_name;
        doc["firmware_version"] = the_mesh.getFirmwareVer();
        doc["freq"] = prefs->freq;
        doc["sf"] = prefs->sf;
        doc["bw"] = prefs->bw;
        doc["cr"] = prefs->cr;
        doc["tx_power_dbm"] = prefs->tx_power_dbm;
        doc["advert_interval_mins"] = prefs->advert_interval;
        doc["flood_advert_interval_hrs"] = prefs->flood_advert_interval;
        doc["flood_max"] = prefs->flood_max;
        doc["latitude"] = prefs->node_lat;
        doc["longitude"] = prefs->node_lon;
        doc["powersaving_enabled"] = prefs->powersaving_enabled;
        doc["bridge_enabled"] = prefs->bridge_enabled;
        doc["rtc_time"] = the_mesh.getRTCClock()->getCurrentTime();
        doc["tx_delay_factor"] = prefs->tx_delay_factor;
        
        String out;
        serializeJson(doc, out);
        if (!mqttClient.publish("meshcore/config", out.c_str())) {
            Serial.print("Failed to publish config! Size: ");
            Serial.println(out.length());
        }
    }
}

void MQTTBacklink::publishNeighbors(const void* neighboursPtr, int max_neighbours) {
    if (mqttClient.connected() && neighboursPtr != nullptr) {
        // Assume neighboursPtr is an array of NeighbourInfo structs
        // We'll define a generic struct here to avoid including MyMesh.h directly
        struct DummyNeighbour {
            uint8_t pub_key[6];
            uint32_t advert_timestamp;
            uint32_t heard_timestamp;
            int8_t snr;
        };
        const DummyNeighbour* neighbours = (const DummyNeighbour*)neighboursPtr;
        
        JsonDocument doc;
        JsonArray arr = doc["neighbors"].to<JsonArray>();
        
        for (int i = 0; i < max_neighbours; i++) {
            if (neighbours[i].heard_timestamp > 0) {
                JsonObject n = arr.add<JsonObject>();
                String hexId = "";
                for (int j = 0; j < 6; j++) {
                    if (neighbours[i].pub_key[j] < 16) hexId += "0";
                    hexId += String(neighbours[i].pub_key[j], HEX);
                }
                n["id"] = hexId;
                n["snr"] = neighbours[i].snr / 4.0; // MyMesh scales snr by 4
                n["heard_secs_ago"] = (millis() / 1000) - neighbours[i].heard_timestamp; // rough estimate if using millis, though MyMesh uses rtc
            }
        }
        
        String out;
        serializeJson(doc, out);
        if (!mqttClient.publish("meshcore/neighbors", out.c_str())) {
            Serial.print("Failed to publish neighbors! Size: ");
            Serial.println(out.length());
        }
    }
}

void MQTTBacklink::mqttCallback(char* topic, byte* payload, unsigned int length) {
    mqttBacklink.handleMessage(topic, payload, length);
}

void MQTTBacklink::handleMessage(char* topic, byte* payload, unsigned int length) {
    if (strcmp(topic, "meshcore/time/read") == 0) {
        // Read the current time and publish it
        uint32_t rtc_now = the_mesh.getRTCClock()->getCurrentTime();
        JsonDocument doc;
        doc["rtc_time"] = rtc_now;
        String out;
        serializeJson(doc, out);
        mqttClient.publish("meshcore/time/current", out.c_str());
        Serial.print("Published current RTC time: ");
        Serial.println(rtc_now);
    } 
    else if (strcmp(topic, "meshcore/time/update") == 0) {
        // Update the RTC time from the payload
        String payloadStr = "";
        for (unsigned int i = 0; i < length; i++) {
            payloadStr += (char)payload[i];
        }
        
        uint32_t new_time = payloadStr.toInt();
        if (new_time > 0) {
            the_mesh.getRTCClock()->setCurrentTime(new_time);
            Serial.print("RTC time updated via MQTT to: ");
            Serial.println(new_time);
            
            // Send back the updated time to confirm
            JsonDocument doc;
            doc["rtc_time"] = the_mesh.getRTCClock()->getCurrentTime();
            doc["status"] = "updated";
            String out;
            serializeJson(doc, out);
            mqttClient.publish("meshcore/time/current", out.c_str());
        } else {
            Serial.println("Invalid time payload received via MQTT");
        }
    }
}
