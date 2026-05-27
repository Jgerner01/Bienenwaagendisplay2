/**
 * mqtt_client.h - MQTT Client mit Home Assistant Auto-Discovery
 * Veröffentlicht Wägezellen-Daten
 */

#ifndef MQTT_CLIENT_WRAPPER_H
#define MQTT_CLIENT_WRAPPER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include "config_manager.h"
#include "scale_reader.h"
#include "temp_sensor.h"

typedef void (*MqttStatusCallback)(bool connected);
typedef void (*TareCallback)();
typedef void (*MqttCalibrateCallback)(float knownKg);

class MqttClientManager {
public:
    MqttClientManager();
    ~MqttClientManager();

    bool begin(const MqttConfig& config);
    void loop();

    // Messdaten veröffentlichen
    void publishScaleData(const ScaleData& data);
    void publishTempData(const TempData& data);

    // Home Assistant Auto-Discovery senden
    void sendDiscoveryConfig();

    bool testConnection();
    bool isConnected() const;
    void setStatusCallback(MqttStatusCallback callback);
    void setTareCallback(TareCallback callback);
    void setCalibrateCallback(MqttCalibrateCallback callback);
    void disconnect();

private:
    WiFiClient         wifiClient;
    PubSubClient*      mqttClient;
    MqttConfig         mqttConfig;
    bool               connected;
    bool               discoverySent;
    unsigned long      lastReconnectMs;
    MqttStatusCallback statusCb;
    TareCallback       tareCb;
    MqttCalibrateCallback calibrateCb;

    static MqttClientManager* _instance;
    static void mqttCallbackStub(char* topic, byte* payload, unsigned int length);
    void handleMessage(char* topic, byte* payload, unsigned int length);
    void subscribeCommands();

    bool connect();
    void sendSensorDiscovery(const char* sensorId, const char* name,
                              const char* deviceClass, const char* stateClass,
                              const char* unit, const char* icon = nullptr);
    void sendButtonDiscovery(const char* buttonId, const char* name,
                              const char* commandTopic, const char* icon = nullptr);
    void sendNumberDiscovery(const char* numberId, const char* name,
                              const char* commandTopic,
                              float min, float max, float step,
                              const char* unit, const char* icon = nullptr);
};

#endif // MQTT_CLIENT_WRAPPER_H
