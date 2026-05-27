/**
 * mqtt_client.cpp - MQTT Client Implementierung (Bienenwaage)
 */

#include "mqtt_client.h"
#include <ArduinoJson.h>

MqttClientManager* MqttClientManager::_instance = nullptr;

MqttClientManager::MqttClientManager()
    : mqttClient(nullptr), connected(false),
      discoverySent(false), lastReconnectMs(0),
      statusCb(nullptr), tareCb(nullptr), calibrateCb(nullptr) {
    memset(&mqttConfig, 0, sizeof(MqttConfig));
}

MqttClientManager::~MqttClientManager() {
    disconnect();
    if (mqttClient) delete mqttClient;
}

bool MqttClientManager::begin(const MqttConfig& config) {
    mqttConfig = config;

    if (mqttClient) { delete mqttClient; mqttClient = nullptr; }

    mqttClient = new PubSubClient(wifiClient);
    mqttClient->setServer(config.broker, config.port);
    mqttClient->setBufferSize(600);  // reicht für Discovery-Payload (~450 Byte)
    mqttClient->setCallback(mqttCallbackStub);
    _instance = this;

    connected      = false;
    discoverySent  = false;

    DEBUG_PRINTF("[MQTT] Initialisiert: %s:%d\n", config.broker, config.port);
    return true;
}

void MqttClientManager::loop() {
    if (!mqttClient || !mqttConfig.enabled) return;

    if (!mqttClient->connected()) {
        if (connected) {
            connected     = false;
            discoverySent = false;
            DEBUG_PRINTLN("[MQTT] Verbindung verloren");
            if (statusCb) statusCb(false);
        }
        if (millis() - lastReconnectMs >= MQTT_RECONNECT_INTERVAL) {
            lastReconnectMs = millis();
            connect();
        }
    } else {
        if (!connected) {
            connected = true;
            DEBUG_PRINTLN("[MQTT] Verbunden");
            if (statusCb) statusCb(true);
            subscribeCommands();
            if (mqttConfig.autoDiscovery && !discoverySent) {
                sendDiscoveryConfig();
            }
        }
        mqttClient->loop();
    }
}

bool MqttClientManager::connect() {
    if (!mqttClient) return false;

    String willTopic = String(mqttConfig.topicPrefix) + "/status";
    bool result;

    if (strlen(mqttConfig.username) > 0) {
        result = mqttClient->connect(
            mqttConfig.clientId, mqttConfig.username, mqttConfig.password,
            willTopic.c_str(), MQTT_QOS, MQTT_RETAIN, "offline");
    } else {
        result = mqttClient->connect(
            mqttConfig.clientId,
            willTopic.c_str(), MQTT_QOS, MQTT_RETAIN, "offline");
    }

    if (result) {
        mqttClient->publish(willTopic.c_str(), "online", MQTT_RETAIN);
    } else {
        DEBUG_PRINTF("[MQTT] Verbindung fehlgeschlagen (rc=%d)\n", mqttClient->state());
    }
    return result;
}

// ============================================================
// DATEN VERÖFFENTLICHEN
// ============================================================

void MqttClientManager::publishScaleData(const ScaleData& data) {
    if (!connected || !mqttClient) {
        DEBUG_PRINTLN("[MQTT] publishScaleData: nicht verbunden");
        return;
    }

    String base  = String(mqttConfig.topicPrefix) + "/sensors";
    String topic = base + "/weight";
    char   payload[64];
    snprintf(payload, sizeof(payload),
             "{\"value\":%.3f,\"unit\":\"kg\",\"ts\":%lu}",
             data.weightKg, millis() / 1000UL);

    bool ok = mqttClient->publish(topic.c_str(), payload, MQTT_RETAIN);
    DEBUG_PRINTF("[MQTT] Weight: %s = %s  [%s]\n",
                 topic.c_str(), payload, ok ? "OK" : "FEHLER");

    // Temperaturkorrigiertes Gewicht
    String corrTopic = base + "/weight_corrected";
    char   corrPayload[72];
    snprintf(corrPayload, sizeof(corrPayload),
             "{\"value\":%.3f,\"unit\":\"kg\",\"corrected\":%s,\"ts\":%lu}",
             data.weightCorrectedKg,
             data.tempCorrectionActive ? "true" : "false",
             millis() / 1000UL);
    mqttClient->publish(corrTopic.c_str(), corrPayload, MQTT_RETAIN);

    // Trimmed Mean (Mittelwert nach Entfernen je 5 Ausreißer oben/unten)
    String tmTopic = base + "/trimmedmean";
    char   tmPayload[48];
    snprintf(tmPayload, sizeof(tmPayload),
             "{\"value\":%.3f,\"unit\":\"kg\",\"ts\":%lu}",
             data.trimmedMeanKg, millis() / 1000UL);
    mqttClient->publish(tmTopic.c_str(), tmPayload, MQTT_RETAIN);

    // Streuung (Standardabweichung aller Samples in kg)
    String spreadTopic = base + "/spread";
    char   spreadPayload[48];
    snprintf(spreadPayload, sizeof(spreadPayload),
             "{\"value\":%.4f,\"unit\":\"kg\",\"ts\":%lu}",
             data.spreadKg, millis() / 1000UL);
    mqttClient->publish(spreadTopic.c_str(), spreadPayload, MQTT_RETAIN);

    // Rohwert (für Diagnose)
    String rawTopic = base + "/raw";
    char   rawPayload[48];
    snprintf(rawPayload, sizeof(rawPayload),
             "{\"value\":%ld,\"ts\":%lu}",
             data.rawValue, millis() / 1000UL);
    mqttClient->publish(rawTopic.c_str(), rawPayload, MQTT_RETAIN);

    // Offset
    String offsetTopic = base + "/offset";
    char   offsetPayload[48];
    snprintf(offsetPayload, sizeof(offsetPayload),
             "{\"value\":%ld,\"ts\":%lu}",
             data.offset, millis() / 1000UL);
    mqttClient->publish(offsetTopic.c_str(), offsetPayload, MQTT_RETAIN);

    // Kalibrierfaktor
    String calTopic = base + "/calibrationfactor";
    char   calPayload[48];
    snprintf(calPayload, sizeof(calPayload),
             "{\"value\":%.2f,\"ts\":%lu}",
             data.calibrationFactor, millis() / 1000UL);
    mqttClient->publish(calTopic.c_str(), calPayload, MQTT_RETAIN);

    // Gain-Faktor
    String gainTopic = base + "/gain";
    char   gainPayload[32];
    snprintf(gainPayload, sizeof(gainPayload),
             "{\"value\":%d,\"ts\":%lu}",
             data.gain, millis() / 1000UL);
    mqttClient->publish(gainTopic.c_str(), gainPayload, MQTT_RETAIN);
}

void MqttClientManager::publishTempData(const TempData& data) {
    if (!connected || !mqttClient || !data.isValid) return;

    String topic = String(mqttConfig.topicPrefix) + "/sensors/temperature";
    char   payload[48];
    snprintf(payload, sizeof(payload),
             "{\"value\":%.2f,\"unit\":\"C\",\"ts\":%lu}",
             data.tempC, millis() / 1000UL);
    mqttClient->publish(topic.c_str(), payload, MQTT_RETAIN);
}

// ============================================================
// HOME ASSISTANT AUTO-DISCOVERY
// ============================================================

void MqttClientManager::sendDiscoveryConfig() {
    if (!connected || !mqttClient) return;

    DEBUG_PRINTLN("[MQTT] Sende HA Auto-Discovery");

    sendSensorDiscovery("weight",          "Bienenstock Gewicht",
                        nullptr,           "measurement", "kg", "mdi:scale");
    sendSensorDiscovery("weight_corrected", "Bienenstock Gewicht (T-korrigiert)",
                        nullptr,           "measurement", "kg", "mdi:scale-balance");
    sendSensorDiscovery("temperature", "Bienenstock Temperatur",
                        "temperature", "measurement", "\xc2\xb0\x43",  // °C
                        "mdi:thermometer");
    sendSensorDiscovery("trimmedmean",       "Gewicht Trimmed Mean",
                        nullptr,             "measurement", "kg", "mdi:approximately-equal");
    sendSensorDiscovery("spread",            "Gewicht Streuung (Stdabw.)",
                        nullptr,             "measurement", "kg", "mdi:sigma");
    sendSensorDiscovery("raw",               "HX711 Rohwert",
                        nullptr,             "measurement", "", "mdi:waveform");
    sendSensorDiscovery("offset",            "HX711 Offset",
                        nullptr,             "measurement", "", "mdi:target");
    sendSensorDiscovery("calibrationfactor", "Kalibrierfaktor",
                        nullptr,             "measurement", "", "mdi:function-variant");
    sendSensorDiscovery("gain",              "HX711 Gain",
                        nullptr,             "measurement", "", "mdi:tune");

    DEBUG_PRINTF("[MQTT] Vor Button/Number Discovery, connected=%d heap=%d\n",
                 mqttClient->connected(), ESP.getFreeHeap());

    String tareTopic = String(mqttConfig.topicPrefix) + "/cmd/tare";
    sendButtonDiscovery("tare", "Tara setzen", tareTopic.c_str(), "mdi:scale-balance");

    String calTopic = String(mqttConfig.topicPrefix) + "/cmd/calibrate";
    sendNumberDiscovery("calibrate", "Kalibrieren", calTopic.c_str(),
                        0.1f, 50.0f, 0.1f, "kg", "mdi:tune-variant");

    discoverySent = true;
}

void MqttClientManager::sendSensorDiscovery(const char* sensorId, const char* name,
                                              const char* deviceClass, const char* stateClass,
                                              const char* unit, const char* icon) {
    String discoveryTopic = String(MQTT_TOPIC_DISCOVERY) + "/" + sensorId + "/config";
    String stateTopic     = String(mqttConfig.topicPrefix) + "/sensors/" + sensorId;
    String availTopic     = String(mqttConfig.topicPrefix) + "/status";
    String uniqueId       = String(DEVICE_ID) + "_" + sensorId;

    JsonDocument doc;
    doc["name"]                  = name;
    doc["unique_id"]             = uniqueId;
    doc["state_topic"]           = stateTopic;
    doc["availability_topic"]    = availTopic;
    doc["payload_available"]     = "online";
    doc["payload_not_available"] = "offline";
    doc["unit_of_measurement"]   = unit;
    if (deviceClass && strlen(deviceClass) > 0)
        doc["device_class"]      = deviceClass;
    doc["state_class"]           = stateClass;
    if (icon && strlen(icon) > 0)
        doc["icon"]              = icon;
    doc["value_template"]        = "{{ value_json.value }}";

    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"][0] = DEVICE_ID;
    device["name"]           = DEVICE_NAME;
    device["model"]          = DEVICE_MODEL;
    device["manufacturer"]   = DEVICE_MANUFACTURER;
    device["sw_version"]     = FIRMWARE_VERSION;

    String payload;
    serializeJson(doc, payload);

    DEBUG_PRINTF("[MQTT] Discovery topic:   %s\n", discoveryTopic.c_str());
    DEBUG_PRINTF("[MQTT] Discovery payload: %s\n", payload.c_str());
    mqttClient->setBufferSize(payload.length() + 128);
    bool ok = mqttClient->publish(discoveryTopic.c_str(), payload.c_str(), true);
    DEBUG_PRINTF("[MQTT] Discovery %s: %s\n", sensorId, ok ? "OK" : "FEHLER");
    delay(100);
}

void MqttClientManager::sendButtonDiscovery(const char* buttonId, const char* name,
                                              const char* commandTopic, const char* icon) {
    DEBUG_PRINTF("[MQTT] sendButtonDiscovery: %s\n", buttonId);
    String discoveryTopic = "homeassistant/button/" + String(DEVICE_ID) + "/" + buttonId + "/config";
    String availTopic     = String(mqttConfig.topicPrefix) + "/status";
    String uniqueId       = String(DEVICE_ID) + "_" + buttonId;

    JsonDocument doc;
    doc["name"]                  = name;
    doc["unique_id"]             = uniqueId;
    doc["command_topic"]         = commandTopic;
    doc["payload_press"]         = "PRESS";
    doc["availability_topic"]    = availTopic;
    doc["payload_available"]     = "online";
    doc["payload_not_available"] = "offline";
    if (icon) doc["icon"]        = icon;

    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"][0] = DEVICE_ID;
    device["name"]           = DEVICE_NAME;
    device["model"]          = DEVICE_MODEL;
    device["manufacturer"]   = DEVICE_MANUFACTURER;
    device["sw_version"]     = FIRMWARE_VERSION;

    String payload;
    serializeJson(doc, payload);

    DEBUG_PRINTF("[MQTT] Discovery topic:   %s\n", discoveryTopic.c_str());
    DEBUG_PRINTF("[MQTT] Discovery payload: %s\n", payload.c_str());
    mqttClient->setBufferSize(payload.length() + 128);
    bool ok = mqttClient->publish(discoveryTopic.c_str(), payload.c_str(), true);
    DEBUG_PRINTF("[MQTT] Discovery %s: %s\n", buttonId, ok ? "OK" : "FEHLER");
    delay(100);
}

void MqttClientManager::sendNumberDiscovery(const char* numberId, const char* name,
                                              const char* commandTopic,
                                              float minVal, float maxVal, float step,
                                              const char* unit, const char* icon) {
    DEBUG_PRINTF("[MQTT] sendNumberDiscovery: %s\n", numberId);
    String discoveryTopic = "homeassistant/number/" + String(DEVICE_ID) + "/" + numberId + "/config";
    String availTopic     = String(mqttConfig.topicPrefix) + "/status";
    String uniqueId       = String(DEVICE_ID) + "_" + numberId;

    JsonDocument doc;
    doc["name"]                  = name;
    doc["unique_id"]             = uniqueId;
    doc["command_topic"]         = commandTopic;
    doc["min"]                   = minVal;
    doc["max"]                   = maxVal;
    doc["step"]                  = step;
    doc["unit_of_measurement"]   = unit;
    doc["mode"]                  = "box";
    doc["availability_topic"]    = availTopic;
    doc["payload_available"]     = "online";
    doc["payload_not_available"] = "offline";
    if (icon) doc["icon"]        = icon;

    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"][0] = DEVICE_ID;
    device["name"]           = DEVICE_NAME;
    device["model"]          = DEVICE_MODEL;
    device["manufacturer"]   = DEVICE_MANUFACTURER;
    device["sw_version"]     = FIRMWARE_VERSION;

    String payload;
    serializeJson(doc, payload);

    mqttClient->setBufferSize(payload.length() + 128);
    bool ok = mqttClient->publish(discoveryTopic.c_str(), payload.c_str(), true);
    DEBUG_PRINTF("[MQTT] Discovery %s: %s\n", numberId, ok ? "OK" : "FEHLER");
    delay(100);
}

bool MqttClientManager::testConnection() {
    if (!mqttClient) return false;
    return connect();
}

bool MqttClientManager::isConnected() const {
    return connected && mqttClient && mqttClient->connected();
}

void MqttClientManager::setStatusCallback(MqttStatusCallback callback) {
    statusCb = callback;
}

void MqttClientManager::setTareCallback(TareCallback callback) {
    tareCb = callback;
}

void MqttClientManager::setCalibrateCallback(MqttCalibrateCallback callback) {
    calibrateCb = callback;
}

// ============================================================
// MQTT INCOMING MESSAGES
// ============================================================

void MqttClientManager::mqttCallbackStub(char* topic, byte* payload, unsigned int length) {
    if (_instance) _instance->handleMessage(topic, payload, length);
}

void MqttClientManager::handleMessage(char* topic, byte* payload, unsigned int length) {
    String base = String(mqttConfig.topicPrefix) + "/cmd";
    String t    = String(topic);

    if (t == base + "/tare") {
        DEBUG_PRINTLN("[MQTT] Tara-Befehl empfangen");
        if (tareCb) tareCb();
    } else if (t == base + "/calibrate" && length > 0) {
        char buf[16] = {};
        memcpy(buf, payload, min((unsigned int)15, length));
        float kg = atof(buf);
        DEBUG_PRINTF("[MQTT] Kalibrieren: %.3f kg\n", kg);
        if (kg > 0.0f && calibrateCb) calibrateCb(kg);
    }
}

void MqttClientManager::subscribeCommands() {
    String base = String(mqttConfig.topicPrefix) + "/cmd";
    mqttClient->subscribe((base + "/tare").c_str());
    mqttClient->subscribe((base + "/calibrate").c_str());
    DEBUG_PRINTLN("[MQTT] Command-Topics abonniert");
}

void MqttClientManager::disconnect() {
    if (mqttClient && mqttClient->connected()) {
        String willTopic = String(mqttConfig.topicPrefix) + "/status";
        mqttClient->publish(willTopic.c_str(), "offline", MQTT_RETAIN);
        mqttClient->disconnect();
    }
    connected = false;
}
