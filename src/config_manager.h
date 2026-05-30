/**
 * config_manager.h - Konfigurationsspeicher (LittleFS)
 * Speichert und lädt WLAN-, MQTT- und Waagen-Einstellungen
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include "config.h"
#include "scale_reader.h"
#include "temp_cal.h"
// PT2CalConfig is defined in temp_cal.h

struct WifiConfig {
    char ssid[64];
    char password[64];
    bool dhcp;
    char ip[16];
    char gateway[16];
    char subnet[16];
};

struct MqttConfig {
    char     broker[64];
    uint16_t port;
    char     username[32];
    char     password[32];
    char     clientId[32];
    char     topicPrefix[32];
    uint16_t publishInterval;   // Sekunden
    bool     autoDiscovery;
    bool     enabled;
};

class ConfigManager {
public:
    ConfigManager();

    bool begin();

    bool loadWifiConfig(WifiConfig& config);
    bool saveWifiConfig(const WifiConfig& config);

    bool loadMqttConfig(MqttConfig& config);
    bool saveMqttConfig(const MqttConfig& config);

    bool loadScaleConfig(ScaleConfig& config);
    bool saveScaleConfig(const ScaleConfig& config);

    bool loadTempCalConfig(TempCalConfig& config);
    bool saveTempCalConfig(const TempCalConfig& config);

    bool loadPT2CalConfig(PT2CalConfig& config);
    bool savePT2CalConfig(const PT2CalConfig& config);

    bool   configExists(const char* filename);
    bool   deleteConfig(const char* filename);
    size_t getFreeSpace();

private:
    bool initialized;
    bool writeJson(const char* filename, const char* json);
};

#endif // CONFIG_MANAGER_H
