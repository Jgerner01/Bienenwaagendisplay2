/**
 * config_manager.cpp - Konfigurationsspeicher Implementierung
 */

#include "config_manager.h"
#include <ArduinoJson.h>

ConfigManager::ConfigManager() : initialized(false) {}

bool ConfigManager::begin() {
    if (!LittleFS.begin()) {
        DEBUG_PRINTLN("[Config] LittleFS Mount fehlgeschlagen, formatiere...");
        if (!LittleFS.format()) {
            DEBUG_PRINTLN("[Config] LittleFS Format fehlgeschlagen!");
            return false;
        }
        if (!LittleFS.begin()) {
            DEBUG_PRINTLN("[Config] LittleFS nach Formatierung fehlgeschlagen!");
            return false;
        }
    }
    initialized = true;
    DEBUG_PRINTLN("[Config] LittleFS initialisiert");
    return true;
}

// ============================================================
// WIFI
// ============================================================

bool ConfigManager::loadWifiConfig(WifiConfig& config) {
    memset(&config, 0, sizeof(WifiConfig));
    config.dhcp = true;

    if (!configExists(CONFIG_FILE_WIFI)) return false;

    File file = LittleFS.open(CONFIG_FILE_WIFI, "r");
    if (!file) return false;

    JsonDocument doc;
    if (deserializeJson(doc, file)) { file.close(); return false; }
    file.close();

    if (doc["ssid"].is<const char*>())
        strncpy(config.ssid, doc["ssid"].as<const char*>(), sizeof(config.ssid) - 1);
    if (doc["password"].is<const char*>())
        strncpy(config.password, doc["password"].as<const char*>(), sizeof(config.password) - 1);
    config.dhcp = doc["dhcp"] | true;
    if (doc["ip"].is<const char*>())
        strncpy(config.ip, doc["ip"].as<const char*>(), sizeof(config.ip) - 1);
    if (doc["gateway"].is<const char*>())
        strncpy(config.gateway, doc["gateway"].as<const char*>(), sizeof(config.gateway) - 1);
    if (doc["subnet"].is<const char*>())
        strncpy(config.subnet, doc["subnet"].as<const char*>(), sizeof(config.subnet) - 1);

    DEBUG_PRINTF("[Config] WiFi geladen: SSID=%s\n", config.ssid);
    return true;
}

bool ConfigManager::saveWifiConfig(const WifiConfig& config) {
    JsonDocument doc;
    doc["ssid"]     = config.ssid;
    doc["password"] = config.password;
    doc["dhcp"]     = config.dhcp;
    doc["ip"]       = config.ip;
    doc["gateway"]  = config.gateway;
    doc["subnet"]   = config.subnet;

    String json;
    serializeJsonPretty(doc, json);
    return writeJson(CONFIG_FILE_WIFI, json.c_str());
}

// ============================================================
// MQTT
// ============================================================

bool ConfigManager::loadMqttConfig(MqttConfig& config) {
    memset(&config, 0, sizeof(MqttConfig));
    config.port            = MQTT_DEFAULT_PORT;
    config.publishInterval = MQTT_PUBLISH_INTERVAL / 1000;
    config.autoDiscovery   = true;
    config.enabled         = false;
    strncpy(config.clientId,     DEVICE_ID,         sizeof(config.clientId) - 1);
    strncpy(config.topicPrefix,  MQTT_TOPIC_PREFIX,  sizeof(config.topicPrefix) - 1);

    if (!configExists(CONFIG_FILE_MQTT)) return false;

    File file = LittleFS.open(CONFIG_FILE_MQTT, "r");
    if (!file) return false;

    JsonDocument doc;
    if (deserializeJson(doc, file)) { file.close(); return false; }
    file.close();

    if (doc["broker"].is<const char*>())
        strncpy(config.broker, doc["broker"].as<const char*>(), sizeof(config.broker) - 1);
    config.port = doc["port"] | MQTT_DEFAULT_PORT;
    if (doc["username"].is<const char*>())
        strncpy(config.username, doc["username"].as<const char*>(), sizeof(config.username) - 1);
    if (doc["password"].is<const char*>())
        strncpy(config.password, doc["password"].as<const char*>(), sizeof(config.password) - 1);
    if (doc["clientId"].is<const char*>())
        strncpy(config.clientId, doc["clientId"].as<const char*>(), sizeof(config.clientId) - 1);
    if (doc["topicPrefix"].is<const char*>())
        strncpy(config.topicPrefix, doc["topicPrefix"].as<const char*>(), sizeof(config.topicPrefix) - 1);
    config.publishInterval = doc["publishInterval"] | (uint16_t)(MQTT_PUBLISH_INTERVAL / 1000);
    config.autoDiscovery   = doc["autoDiscovery"] | true;
    config.enabled         = doc["enabled"] | false;

    DEBUG_PRINTF("[Config] MQTT geladen: Broker=%s\n", config.broker);
    return true;
}

bool ConfigManager::saveMqttConfig(const MqttConfig& config) {
    JsonDocument doc;
    doc["broker"]          = config.broker;
    doc["port"]            = config.port;
    doc["username"]        = config.username;
    doc["password"]        = config.password;
    doc["clientId"]        = config.clientId;
    doc["topicPrefix"]     = config.topicPrefix;
    doc["publishInterval"] = config.publishInterval;
    doc["autoDiscovery"]   = config.autoDiscovery;
    doc["enabled"]         = config.enabled;

    String json;
    serializeJsonPretty(doc, json);
    return writeJson(CONFIG_FILE_MQTT, json.c_str());
}

// ============================================================
// SCALE
// ============================================================

bool ConfigManager::loadScaleConfig(ScaleConfig& config) {
    config.calibrationFactor = 1.0f;
    config.offset            = 0;
    config.gain              = HX711_DEFAULT_GAIN;
    config.publishInterval   = MQTT_PUBLISH_INTERVAL / 1000;

    if (!configExists(CONFIG_FILE_SCALE)) return false;

    File file = LittleFS.open(CONFIG_FILE_SCALE, "r");
    if (!file) return false;

    JsonDocument doc;
    if (deserializeJson(doc, file)) { file.close(); return false; }
    file.close();

    config.calibrationFactor = doc["calibrationFactor"] | 1.0f;
    config.offset            = doc["offset"] | (long)0;
    config.gain              = doc["gain"] | (uint8_t)HX711_DEFAULT_GAIN;
    config.publishInterval   = doc["publishInterval"] | (uint16_t)(MQTT_PUBLISH_INTERVAL / 1000);

    DEBUG_PRINTF("[Config] Scale geladen: factor=%.2f offset=%ld gain=%d\n",
                 config.calibrationFactor, config.offset, config.gain);
    return true;
}

bool ConfigManager::saveScaleConfig(const ScaleConfig& config) {
    JsonDocument doc;
    doc["calibrationFactor"] = config.calibrationFactor;
    doc["offset"]            = config.offset;
    doc["gain"]              = config.gain;
    doc["publishInterval"]   = config.publishInterval;

    String json;
    serializeJsonPretty(doc, json);
    bool ok = writeJson(CONFIG_FILE_SCALE, json.c_str());
    if (ok) DEBUG_PRINTF("[Config] Scale gespeichert: factor=%.2f offset=%ld gain=%d\n",
                         config.calibrationFactor, config.offset, config.gain);
    return ok;
}

// ============================================================
// TEMPERATURKORREKTUR
// ============================================================

bool ConfigManager::loadTempCalConfig(TempCalConfig& config) {
    config = TempCalConfig{};  // Defaults: enabled=false, a=b=c=0

    if (!configExists(CONFIG_FILE_TEMPCAL)) return false;

    File file = LittleFS.open(CONFIG_FILE_TEMPCAL, "r");
    if (!file) return false;

    JsonDocument doc;
    if (deserializeJson(doc, file)) { file.close(); return false; }
    file.close();

    config.enabled = doc["enabled"] | false;
    config.a       = doc["a"]       | 0.0f;
    config.b       = doc["b"]       | 0.0f;
    config.c       = doc["c"]       | 0.0f;

    DEBUG_PRINTF("[Config] TempCal geladen: enabled=%d a=%.6f b=%.6f c=%.6f\n",
                 config.enabled, config.a, config.b, config.c);
    return true;
}

bool ConfigManager::saveTempCalConfig(const TempCalConfig& config) {
    JsonDocument doc;
    doc["enabled"] = config.enabled;
    doc["a"]       = config.a;
    doc["b"]       = config.b;
    doc["c"]       = config.c;

    String json;
    serializeJsonPretty(doc, json);
    bool ok = writeJson(CONFIG_FILE_TEMPCAL, json.c_str());
    if (ok) DEBUG_PRINTF("[Config] TempCal gespeichert: enabled=%d a=%.6f b=%.6f c=%.6f\n",
                         config.enabled, config.a, config.b, config.c);
    return ok;
}

// ============================================================
// PT2-KORREKTUR
// ============================================================

bool ConfigManager::loadPT2CalConfig(PT2CalConfig& config) {
    config = PT2CalConfig{};

    if (!configExists(CONFIG_FILE_PT2CAL)) return false;

    File file = LittleFS.open(CONFIG_FILE_PT2CAL, "r");
    if (!file) return false;

    JsonDocument doc;
    if (deserializeJson(doc, file)) { file.close(); return false; }
    file.close();

    config.enabled = doc["enabled"] | false;
    config.T2_min  = doc["T2_min"]  | 240.0f;
    config.D       = doc["D"]       | 0.5f;
    config.a       = doc["a"]       | 0.0f;
    config.b       = doc["b"]       | 0.0f;
    config.c       = doc["c"]       | 0.0f;

    DEBUG_PRINTF("[Config] PT2Cal geladen: enabled=%d T2=%.1f D=%.2f\n",
                 config.enabled, config.T2_min, config.D);
    return true;
}

bool ConfigManager::savePT2CalConfig(const PT2CalConfig& config) {
    JsonDocument doc;
    doc["enabled"] = config.enabled;
    doc["T2_min"]  = config.T2_min;
    doc["D"]       = config.D;
    doc["a"]       = config.a;
    doc["b"]       = config.b;
    doc["c"]       = config.c;

    String json;
    serializeJsonPretty(doc, json);
    bool ok = writeJson(CONFIG_FILE_PT2CAL, json.c_str());
    if (ok) DEBUG_PRINTF("[Config] PT2Cal gespeichert: enabled=%d T2=%.1f D=%.2f\n",
                         config.enabled, config.T2_min, config.D);
    return ok;
}

// ============================================================
// HILFSMETHODEN
// ============================================================

bool ConfigManager::configExists(const char* filename) {
    return LittleFS.exists(filename);
}

bool ConfigManager::deleteConfig(const char* filename) {
    if (LittleFS.exists(filename)) return LittleFS.remove(filename);
    return false;
}

size_t ConfigManager::getFreeSpace() {
    FSInfo fs;
    if (LittleFS.info(fs)) return fs.totalBytes - fs.usedBytes;
    return 0;
}

bool ConfigManager::writeJson(const char* filename, const char* json) {
    File file = LittleFS.open(filename, "w");
    if (!file) {
        DEBUG_PRINTF("[Config] Konnte %s nicht schreiben\n", filename);
        return false;
    }
    file.print(json);
    file.close();
    return true;
}
