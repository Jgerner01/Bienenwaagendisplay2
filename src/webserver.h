/**
 * webserver.h - WiFiManager + Webinterface Bienenwaage
 * HTTP-Server auf WiFiServer-Basis (keine externen Libs)
 * - AP-Mode für WiFi-Einrichtung
 * - HX711-Daten, Kalibrierung, Gain-Einstellung
 * - MQTT-Konfiguration
 * - OTA-Status
 */

#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include "config.h"
#include "config_manager.h"
#include "scale_reader.h"
#include "temp_sensor.h"
#include "temp_cal.h"

typedef void (*WifiSaveCallback)(const WifiConfig& config);
typedef void (*MqttSaveCallback)(const MqttConfig& config);
typedef void (*ScaleSaveCallback)(const ScaleConfig& config);
typedef void (*TempCalSaveCallback)(const TempCalConfig& config);
typedef void (*TareCallback)();
typedef bool (*CalibrateCallback)(float knownKg);

class WebServerManager {
public:
    WebServerManager();
    ~WebServerManager();

    bool begin(ScaleReader* scaleReader);

    void loop();

    void setWifiSaveCallback(WifiSaveCallback cb)       { wifiSaveCb = cb; }
    void setMqttSaveCallback(MqttSaveCallback cb)       { mqttSaveCb = cb; }
    void setScaleSaveCallback(ScaleSaveCallback cb)       { scaleSaveCb = cb; }
    void setTareCallback(TareCallback cb)                 { tareCb = cb; }
    void setCalibrateCallback(CalibrateCallback cb)       { calibrateCb = cb; }
    void setDisplayCallback(void (*cb)(const String&))    { displayCb = cb; }
    void setApModeCallback(void (*cb)(bool))              { apModeCb = cb; }
    void setTempSensor(TempSensor* ts)                    { tempSensorRef = ts; }
    void setTempCalConfig(TempCalConfig* cfg)             { tempCalRef = cfg; }
    void setTempCalSaveCallback(TempCalSaveCallback cb)   { tempCalSaveCb = cb; }

    // staConnected = STA verbunden; apMode = AP noch aktiv (auch während AP+STA)
    bool   isApMode()    const { return apMode && !staConnected; }
    bool   isConnected() const { return staConnected; }
    String getIp()       const;

private:
    WiFiServer* server;
    DNSServer*  dnsServer;
    bool        apMode;
    bool        staConnected;

    ScaleReader*        scaleReaderRef;
    TempSensor*         tempSensorRef;
    TempCalConfig*      tempCalRef;
    WifiSaveCallback    wifiSaveCb;
    MqttSaveCallback    mqttSaveCb;
    ScaleSaveCallback   scaleSaveCb;
    TempCalSaveCallback tempCalSaveCb;
    TareCallback        tareCb;
    CalibrateCallback   calibrateCb;
    void (*displayCb)(const String&);
    void (*apModeCb)(bool);

    unsigned long connectStartTime;
    unsigned long apShutdownTime;   // millis() wann AP nach STA-Verbindung abschalten

    void startApMode();
    bool startStaMode();
    void handleClient();
    void parseRequest(WiFiClient& client);

    // Seiten
    void handleRoot(WiFiClient& client);
    void handleScale(WiFiClient& client);
    void handleTare(WiFiClient& client);
    void handleCalibratePage(WiFiClient& client);
    void handleCalibratePost(WiFiClient& client, const String& body);
    void handleResetScale(WiFiClient& client);
    void handleOtaPage(WiFiClient& client);
    void handleOtaUpload(WiFiClient& client, int contentLength);
    void handleGainPage(WiFiClient& client);
    void handleGainPost(WiFiClient& client, const String& body);
    void handleMqttPage(WiFiClient& client);
    void handleMqttPost(WiFiClient& client, const String& body);
    void handleTempCalPage(WiFiClient& client);
    void handleTempCalApiGet(WiFiClient& client);
    void handleTempCalApiPost(WiFiClient& client, const String& body);
    void handleWifiScan(WiFiClient& client);
    void handleWifiSave(WiFiClient& client, const String& body);
    void handleStatus(WiFiClient& client);
    void handleReboot(WiFiClient& client);
    void handleApiData(WiFiClient& client);

    // HTTP Helper
    void sendHtml(WiFiClient& client, int code, const String& body);
    void sendJson(WiFiClient& client, const String& json);
    void sendRedirect(WiFiClient& client, const char* location);

    String getRequestParam(const String& req, const char* param);
    String urlDecode(const String& str);

    // HTML Bausteine
    String htmlHead(const String& title, uint16_t refreshSec = 0);
    static const char HTML_END[];
};

#endif // WEBSERVER_H
