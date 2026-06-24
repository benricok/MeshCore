#include "ObserverMQTT.h"
#include "MyMesh.h"
#include <base64.hpp>

extern MyMesh the_mesh;

ObserverMQTT observerMQTT;

String ObserverMQTT::getTopicPrefix() {
    String pubkey = "";
    for (int i=0; i<PUB_KEY_SIZE; i++) {
        if (the_mesh.self_id.pub_key[i] < 16) pubkey += "0";
        pubkey += String(the_mesh.self_id.pub_key[i], HEX);
    }
    return String("meshcore/") + OBSERVER_IATA + "/" + pubkey + "/";
}


ObserverMQTT::ObserverMQTT() : localMqttClient(wifiClient), timeClient(ntpUDP, "pool.ntp.org", 0, 60000) {
    lastLocalReconnectAttempt = 0;
    lastMmReconnectAttempt = 0;
    ntpSynced = false;
}

void ObserverMQTT::begin() {
    Serial.println("Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // Local Broker setup
    localMqttClient.setServer(MQTT_SERVER, atoi(MQTT_PORT));
    localMqttClient.setBufferSize(2048);
    localMqttClient.setCallback(ObserverMQTT::mqttCallback);
    
    // Start NTP Client
    timeClient.begin();
    
    // MeshMapper WebSockets setup
    wsClient.beginSSL("mqtt.meshmapper.net", 443, "/mqtt", NULL, "mqtt");
    wsClient.setReconnectInterval(5000);
    mmClient.begin(wsClient);
    mmClient.setTimeout(15000);
    
    // Setup MeshMapper callback using lambda
    mmClient.subscribe([](const String& topic, const String& payload, const size_t size) {
        observerMQTT.handleMessage((char*)topic.c_str(), (byte*)payload.c_str(), size);
    });
}

void ObserverMQTT::reconnectLocal() {
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

    if (!localMqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastLocalReconnectAttempt > 5000) {
            lastLocalReconnectAttempt = now;
            Serial.println("[Local Broker] Attempting MQTT connection...");
            Serial.print("[Local Broker] MQTT Server: ");
            Serial.println(MQTT_SERVER);
            Serial.print("[Local Broker] MQTT Port: ");
            Serial.println(MQTT_PORT);
            Serial.print("[Local Broker] MQTT User: ");
            Serial.println(MQTT_USER);
            // Serial.print("MQTT PASS: ");
            // Serial.println(MQTT_PASS);

            String clientId = "MeshcoreRepeater-";
            clientId += String(random(0xffff), HEX);
            
            bool connected = false;
            if (strlen(MQTT_USER) > 0) {
                connected = localMqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PWD);
            } else {
                connected = localMqttClient.connect(clientId.c_str());
            }
            
            if (connected) {
                Serial.println("[Local Broker] MQTT connected successfully!");
                // Once connected, publish an announcement...
                localMqttClient.publish((getTopicPrefix() + "status").c_str(), "online");
                // Subscribe to time endpoints
                localMqttClient.subscribe((getTopicPrefix() + "time/read").c_str());
                localMqttClient.subscribe((getTopicPrefix() + "time/update").c_str());
                lastLocalReconnectAttempt = 0;
    lastMmReconnectAttempt = 0;
            } else {
                Serial.print("[Local Broker] MQTT connection failed, rc=");
                Serial.println(localMqttClient.state());
            }
        }
    }
}

void ObserverMQTT::reconnectMeshMapper() {
    if (!mmClient.isConnected()) {
        // Wait at least 2 seconds after boot for network to settle
        if (millis() < 2000) return;

        unsigned long now = millis();
        if (now - lastMmReconnectAttempt > 5000) {
            lastMmReconnectAttempt = now;
            Serial.println("[MeshMapper] Attempting WSS MQTT connection...");
            
            IPAddress mmIP;
            if (WiFi.hostByName("mqtt.meshmapper.net", mmIP)) {
                Serial.print("[MeshMapper] Resolved mqtt.meshmapper.net to IP: ");
                Serial.println(mmIP);
            } else {
                Serial.println("[MeshMapper] DNS resolution failed for mqtt.meshmapper.net!");
            }
            
            String clientId = "MeshcoreRepeater-";
            clientId += String(random(0xffff), HEX);
            
            // Generate fresh token
            String token = generateMeshMapperToken();
            
            // Username must be v1_<PUBLIC_KEY>
            String pubkey = "";
            for (int i = 0; i < PUB_KEY_SIZE; i++) {
                if (the_mesh.self_id.pub_key[i] < 16) pubkey += "0";
                pubkey += String(the_mesh.self_id.pub_key[i], HEX);
            }
            pubkey.toUpperCase();
            String mqttUsername = "v1_" + pubkey;
            
            Serial.println("[MeshMapper JWT] Username: " + mqttUsername);
            Serial.println("[MeshMapper JWT] Token Length: " + String(token.length()));
            Serial.println("[MeshMapper JWT] Token: " + token);
            
            if (mmClient.connect(clientId.c_str(), mqttUsername.c_str(), token.c_str())) {
                Serial.println("[MeshMapper] MQTT connected successfully!");
                // No MeshMapper subscriptions needed (MeshMapper is receive-only for packets, status, adverts)
                lastMmReconnectAttempt = 0;
            } else {
                Serial.print("[MeshMapper] MQTT connection failed (lwmqtt_err=");
                Serial.print((int)mmClient.getLastError());
                Serial.print(", return_code=");
                Serial.print((int)mmClient.getReturnCode());
                Serial.println(")");
            }
        }
    }
}
void ObserverMQTT::loop() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!ntpSynced) {
            bool success = timeClient.update();
            if (success && timeClient.getEpochTime() > 1700000000) {
                ntpSynced = true;
                unsigned long epoch = timeClient.getEpochTime();
                the_mesh.getRTCClock()->setCurrentTime(epoch);
                
                IPAddress ntpIP;
                if (WiFi.hostByName("pool.ntp.org", ntpIP)) {
                    Serial.print("[NTPClient] pool.ntp.org resolved to: ");
                    Serial.println(ntpIP);
                }
                Serial.print("[NTPClient] Synced local RTC to: ");
                Serial.println(epoch);
            } else {
                static unsigned long lastNtpPrint = 0;
                if (millis() - lastNtpPrint > 5000) {
                    Serial.println("[NTPClient] Waiting for NTP sync from pool.ntp.org...");
                    lastNtpPrint = millis();
                }
            }
        }

        if (!localMqttClient.connected()) {
            reconnectLocal();
        } else {
            localMqttClient.loop();
        }
    }
    
    wsClient.loop();
    mmClient.update();
    reconnectMeshMapper();
}

void ObserverMQTT::publishToBoth(const String& topic, const String& payload) {
    if (localMqttClient.connected()) {
        if (localMqttClient.publish(topic.c_str(), payload.c_str())) {
            Serial.println("[Local Broker] Published to " + topic);
        } else {
            Serial.println("[Local Broker] Failed to publish to " + topic);
        }
    }
    
    if (mmClient.isConnected()) {
        if (mmClient.publish(topic.c_str(), payload.c_str())) {
            Serial.println("[MeshMapper] Published to " + topic);
        } else {
            Serial.println("[MeshMapper] Failed to publish to " + topic);
        }
    }
}
void ObserverMQTT::publishPacket(const uint8_t* payload, size_t len, int rssi, float snr) {
    if (localMqttClient.connected()) {
        JsonDocument doc;
        
        // Add origin id for observer format
        String pubkey = "";
        for (int i=0; i<PUB_KEY_SIZE; i++) {
            if (the_mesh.self_id.pub_key[i] < 16) pubkey += "0";
            pubkey += String(the_mesh.self_id.pub_key[i], HEX);
        }
        doc["origin_id"] = pubkey;
        doc["timestamp"] = the_mesh.getRTCClock()->getCurrentTime();
        
        doc["rssi"] = rssi;
        doc["snr"] = snr;
        doc["raw_len"] = len;
        
        mesh::Packet pkt;
        if (pkt.readFrom(payload, len)) {
            doc["is_flood"] = pkt.isRouteFlood();
            doc["is_direct"] = pkt.isRouteDirect();
            doc["payload_type"] = pkt.getPayloadType();
            
            const char* type_name = "unknown";
            switch(pkt.getPayloadType()) {
                case PAYLOAD_TYPE_REQ: type_name = "req"; break;
                case PAYLOAD_TYPE_RESPONSE: type_name = "response"; break;
                case PAYLOAD_TYPE_TXT_MSG: type_name = "txt_msg"; break;
                case PAYLOAD_TYPE_ACK: type_name = "ack"; break;
                case PAYLOAD_TYPE_ADVERT: type_name = "advert"; break;
                case PAYLOAD_TYPE_GRP_TXT: type_name = "grp_txt"; break;
                case PAYLOAD_TYPE_GRP_DATA: type_name = "grp_data"; break;
                case PAYLOAD_TYPE_ANON_REQ: type_name = "anon_req"; break;
                case PAYLOAD_TYPE_PATH: type_name = "path"; break;
                case PAYLOAD_TYPE_TRACE: type_name = "trace"; break;
                case PAYLOAD_TYPE_MULTIPART: type_name = "multipart"; break;
                case PAYLOAD_TYPE_CONTROL: type_name = "control"; break;
                case PAYLOAD_TYPE_RAW_CUSTOM: type_name = "raw_custom"; break;
            }
            doc["payload_type_name"] = type_name;
            doc["route_type_name"] = pkt.isRouteFlood() ? "flood" : "direct";
            
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
        
        // Publish to observer topic
        String observerTopic = getTopicPrefix() + "packets";
        publishToBoth(observerTopic, out);
    }
}



void ObserverMQTT::publishConfig(const void* prefsPtr) {
    if (localMqttClient.connected() && prefsPtr != nullptr) {
        const NodePrefs* prefs = (const NodePrefs*)prefsPtr;
        JsonDocument doc;
        String pubkey = "";
        for (int i=0; i<PUB_KEY_SIZE; i++) {
            if (the_mesh.self_id.pub_key[i] < 16) pubkey += "0";
            pubkey += String(the_mesh.self_id.pub_key[i], HEX);
        }
        doc["origin_id"] = pubkey;
        doc["timestamp"] = the_mesh.getRTCClock()->getCurrentTime();
        doc["node_name"] = prefs->node_name;
        doc["timestamp"] = the_mesh.getRTCClock()->getCurrentTime();
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
        doc["tx_delay_factor"] = prefs->tx_delay_factor;
        
        String out;
        serializeJson(doc, out);
        String topic = getTopicPrefix() + "config";
        if (localMqttClient.connected()) {
            if (localMqttClient.publish(topic.c_str(), out.c_str())) {
                Serial.println("[Local Broker] Published to " + topic);
            } else {
                Serial.println("[Local Broker] Failed to publish to " + topic);
            }
        }
    }
}

void ObserverMQTT::publishNeighbors(const void* neighboursPtr, int max_neighbours) {
    if (localMqttClient.connected() && neighboursPtr != nullptr) {
        struct DummyNeighbour {
            uint8_t pub_key[6];
            uint32_t advert_timestamp;
            uint32_t heard_timestamp;
            int8_t snr;
        };
        const DummyNeighbour* neighbours = (const DummyNeighbour*)neighboursPtr;
        
        JsonDocument doc;
        String pubkey = "";
        for (int i=0; i<PUB_KEY_SIZE; i++) {
            if (the_mesh.self_id.pub_key[i] < 16) pubkey += "0";
            pubkey += String(the_mesh.self_id.pub_key[i], HEX);
        }
        doc["origin_id"] = pubkey;
        doc["timestamp"] = the_mesh.getRTCClock()->getCurrentTime();
        
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
                n["snr"] = neighbours[i].snr / 4.0; 
                n["heard_secs_ago"] = (millis() / 1000) - neighbours[i].heard_timestamp; 
            }
        }
        
        String out;
        serializeJson(doc, out);
        String topic = getTopicPrefix() + "neighbors";
        if (localMqttClient.connected()) {
            if (localMqttClient.publish(topic.c_str(), out.c_str())) {
                Serial.println("[Local Broker] Published to " + topic);
            } else {
                Serial.println("[Local Broker] Failed to publish to " + topic);
            }
        }
    }
}

void ObserverMQTT::mqttCallback(char* topic, byte* payload, unsigned int length) {
    observerMQTT.handleMessage(topic, payload, length);
}

// Helper function for base64url encoding
static String base64urlEncode(const String& input) {
    int inputLen = input.length();
    int encodedLen = encode_base64_length(inputLen);
    char* encoded = (char*)malloc(encodedLen + 1);
    encode_base64((const unsigned char*)input.c_str(), inputLen, (unsigned char*)encoded);
    encoded[encodedLen] = '\0';
    
    String out = encoded;
    free(encoded);
    
    out.replace("+", "-");
    out.replace("/", "_");
    out.replace("=", "");
    return out;
}

String ObserverMQTT::generateMeshMapperToken() {
    // Header
    String header = "{\"alg\":\"Ed25519\",\"typ\":\"JWT\"}";
    
    // Payload
    uint32_t now = the_mesh.getRTCClock()->getCurrentTime();
    uint32_t exp = now + 3600; // 1 hour expiration
    
    String pubkey = "";
    for (int i = 0; i < PUB_KEY_SIZE; i++) {
        if (the_mesh.self_id.pub_key[i] < 16) pubkey += "0";
        pubkey += String(the_mesh.self_id.pub_key[i], HEX);
    }
    pubkey.toUpperCase();
    
    String payload;
    if (now > 1700000000) {
        payload = "{\"publicKey\":\"" + pubkey + "\",\"iat\":" + String(now) + ",\"exp\":" + String(exp) + ",\"aud\":\"mqtt.meshmapper.net\"}";
    } else {
        payload = "{\"publicKey\":\"" + pubkey + "\",\"aud\":\"mqtt.meshmapper.net\"}";
    }
    
    Serial.println("[MeshMapper JWT] Header: " + header);
    Serial.println("[MeshMapper JWT] Payload: " + payload);
    
    // Encode
    String headerB64 = base64urlEncode(header);
    String payloadB64 = base64urlEncode(payload);
    
    String signingInput = headerB64 + "." + payloadB64;
    
    // Sign
    uint8_t sig[SIGNATURE_SIZE];
    the_mesh.self_id.sign(sig, (const uint8_t*)signingInput.c_str(), signingInput.length());
    
    // Hex encode signature
    String sigHex = "";
    for (int i = 0; i < SIGNATURE_SIZE; i++) {
        if (sig[i] < 16) sigHex += "0";
        sigHex += String(sig[i], HEX);
    }
    sigHex.toUpperCase();
    
    return signingInput + "." + sigHex;
}

void ObserverMQTT::handleMessage(char* topic, byte* payload, unsigned int length) {
    String topicStr = String(topic);
    if (topicStr == getTopicPrefix() + "time/read") {
        uint32_t rtc_now = the_mesh.getRTCClock()->getCurrentTime();
        JsonDocument doc;
        
        String pubkey = "";
        for (int i=0; i<PUB_KEY_SIZE; i++) {
            if (the_mesh.self_id.pub_key[i] < 16) pubkey += "0";
            pubkey += String(the_mesh.self_id.pub_key[i], HEX);
        }
        doc["origin_id"] = pubkey;
        doc["timestamp"] = rtc_now;
        doc["rtc_time"] = rtc_now;
        
        String out;
        serializeJson(doc, out);
        String topic = getTopicPrefix() + "time/current";
        if (localMqttClient.connected()) {
            if (localMqttClient.publish(topic.c_str(), out.c_str())) {
                Serial.println("[Local Broker] Published to " + topic);
            } else {
                Serial.println("[Local Broker] Failed to publish to " + topic);
            }
        }
        Serial.println("[Local Broker] Published current RTC time: ");
        Serial.print(rtc_now);
    } 
    else if (topicStr == getTopicPrefix() + "time/update") {
        String payloadStr = "";
        for (unsigned int i = 0; i < length; i++) {
            payloadStr += (char)payload[i];
        }
        
        uint32_t new_time = payloadStr.toInt();
        if (new_time > 0) {
            the_mesh.getRTCClock()->setCurrentTime(new_time);
            Serial.print("RTC time updated via MQTT to: ");
            Serial.println(new_time);
            
            uint32_t rtc_now = the_mesh.getRTCClock()->getCurrentTime();
            JsonDocument doc;
            
            String pubkey = "";
            for (int i=0; i<PUB_KEY_SIZE; i++) {
                if (the_mesh.self_id.pub_key[i] < 16) pubkey += "0";
                pubkey += String(the_mesh.self_id.pub_key[i], HEX);
            }
            doc["origin_id"] = pubkey;
            doc["timestamp"] = rtc_now;
            doc["rtc_time"] = rtc_now;
            doc["status"] = "updated";
            
            String out;
            serializeJson(doc, out);
            String topic = getTopicPrefix() + "time/current";
            if (localMqttClient.connected()) {
                localMqttClient.publish(topic.c_str(), out.c_str());
            }
        } else {
            Serial.println("[Local Broker] Invalid time payload received via MQTT");
        }
    }
}


#include "../../src/helpers/AdvertDataHelpers.h"

void ObserverMQTT::publishAdvert(const mesh::Identity& id, uint32_t timestamp, const uint8_t* app_data, size_t app_data_len, int rssi, float snr) {
    if (!localMqttClient.connected()) return;
    
    AdvertDataParser parser(app_data, app_data_len);
    if (!parser.isValid()) return;
    
    JsonDocument doc;
    
    String pubkey = "";
    for (int i=0; i<PUB_KEY_SIZE; i++) {
        if (the_mesh.self_id.pub_key[i] < 16) pubkey += "0";
        pubkey += String(the_mesh.self_id.pub_key[i], HEX);
    }
    doc["origin_id"] = pubkey;
    doc["timestamp"] = timestamp;
    
    String remote_pubkey = "";
    for (int i=0; i<PUB_KEY_SIZE; i++) {
        if (id.pub_key[i] < 16) remote_pubkey += "0";
        remote_pubkey += String(id.pub_key[i], HEX);
    }
    doc["pub_key"] = remote_pubkey;
    
    if (parser.hasName()) doc["node_name"] = parser.getName();
    
    const char* type_name = "unknown";
    switch(parser.getType()) {
        case ADV_TYPE_CHAT: type_name = "chat"; break;
        case ADV_TYPE_REPEATER: type_name = "repeater"; break;
        case ADV_TYPE_ROOM: type_name = "room"; break;
        case ADV_TYPE_SENSOR: type_name = "sensor"; break;
    }
    doc["node_type"] = type_name;
    doc["node_type_id"] = parser.getType();
    
    if (parser.hasLatLon()) {
        doc["latitude"] = parser.getLat();
        doc["longitude"] = parser.getLon();
    }
    
    doc["rssi"] = rssi;
    doc["snr"] = snr;
    
    String out;
    serializeJson(doc, out);
    
    String observerTopic = getTopicPrefix() + "adverts";
    publishToBoth(observerTopic, out);
}


void ObserverMQTT::publishStatus(uint32_t uptime_secs, int wifi_rssi, uint32_t free_heap, uint32_t rx_count, uint32_t tx_count, const uint8_t* telemetry_buf, size_t telemetry_len) {
    if (!localMqttClient.connected()) return;
    
    JsonDocument doc;
    String pubkey = "";
    for (int i=0; i<PUB_KEY_SIZE; i++) {
        if (the_mesh.self_id.pub_key[i] < 16) pubkey += "0";
        pubkey += String(the_mesh.self_id.pub_key[i], HEX);
    }
    doc["origin_id"] = pubkey;
    doc["timestamp"] = the_mesh.getRTCClock()->getCurrentTime();
    doc["firmware_version"] = the_mesh.getFirmwareVer();
    doc["node_name"] = the_mesh.getNodePrefs()->node_name;
    doc["uptime_secs"] = uptime_secs;
    doc["rx_count"] = rx_count;
    doc["tx_count"] = tx_count;
    doc["wifi_rssi"] = wifi_rssi;
    doc["free_heap"] = free_heap;
    
    if (telemetry_buf && telemetry_len > 0) {
        for (size_t i = 0; i < telemetry_len; ) {
            uint8_t channel = telemetry_buf[i++];
            uint8_t type = telemetry_buf[i++];
            if (type == 116) { // Voltage
                uint16_t val = (telemetry_buf[i] << 8) | telemetry_buf[i+1];
                doc["voltage_" + String(channel)] = val / 100.0;
                i += 2;
            } else if (type == 103) { // Temp
                int16_t val = (telemetry_buf[i] << 8) | telemetry_buf[i+1];
                doc["temp_" + String(channel)] = val / 10.0;
                i += 2;
            } else if (type == 104) { // Humidity
                uint8_t val = telemetry_buf[i++];
                doc["humidity_" + String(channel)] = val * 0.5;
            } else if (type == 115) { // Baro
                uint16_t val = (telemetry_buf[i] << 8) | telemetry_buf[i+1];
                doc["baro_" + String(channel)] = val / 10.0;
                i += 2;
            } else {
                break;
            }
        }
    }
    
    String out;
    serializeJson(doc, out);
    
    String observerTopic = getTopicPrefix() + "status";
    publishToBoth(observerTopic, out);
}
