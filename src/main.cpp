/**
 * main.cpp - Bienenwaagendisplay
 * Wemos NodeMCU (ESP-12E), I2C LCD 16x2, HX711, WiFi, MQTT, OTA
 *
 * Pin-Belegung:
 *   D1 (GPIO5)  – SDA  (LCD I2C)
 *   D7 (GPIO13) – SCL  (LCD I2C)
 *   D2 (GPIO4)  – DS18B20 (OneWire, 4,7k Pull-up nach 3,3V)
 *   D3 (GPIO0)  – CK   (HX711, Strapping-Pin: HIGH beim Boot)
 *   D4 (GPIO2)  – DO   (HX711, Strapping-Pin: HIGH beim Boot)
 *
 * WiFi-Einrichtung:
 * - Erster Start: AP-Mode "Bienenwaage" (PW: 12345678) → 192.168.4.1
 * - Nach Einrichtung: STA-Mode, IP auf LCD 30 s, dann Gewicht
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "scale_reader.h"
#include "temp_sensor.h"
#include "display.h"
#include "config_manager.h"
#include "webserver.h"
#include "mqtt_client.h"
#include "temp_cal.h"

// ============================================================
// GLOBALE OBJEKTE
// ============================================================

static ScaleReader       scaleReader;
static TempSensor        tempSensor;
static DisplayManager    displayMgr;
static ConfigManager     configMgr;
static WebServerManager  webServer;
static MqttClientManager mqttClient;

static WifiConfig    wifiConfig;
static MqttConfig    mqttConfig;
static ScaleConfig   scaleConfig;
static TempCalConfig tempCalConfig;

static bool          mqttEnabled        = false;
static unsigned long lastMqttPublish    = 0;
static bool          wifiWasConnected   = false;

// ============================================================
// CALLBACKS
// ============================================================

void onWifiSave(const WifiConfig& config) {
    configMgr.saveWifiConfig(config);
    wifiConfig = config;
}

void onMqttSave(const MqttConfig& config) {
    configMgr.saveMqttConfig(config);
    mqttConfig  = config;
    mqttEnabled = config.enabled;
    if (mqttEnabled && strlen(mqttConfig.broker) > 0) {
        mqttClient.begin(mqttConfig);
    } else {
        mqttClient.disconnect();
    }
}

void onScaleSave(const ScaleConfig& config) {
    configMgr.saveScaleConfig(config);
    scaleConfig = config;
}

void onTare() {
    scaleReader.tare();
    configMgr.saveScaleConfig(scaleConfig);   // offset wurde intern in scaleConfig gesetzt
}

bool onCalibrate(float knownKg) {
    bool ok = scaleReader.calibrate(knownKg);
    if (ok) configMgr.saveScaleConfig(scaleConfig);
    return ok;
}

void onIpDisplay(const String& ip) {
    displayMgr.setIpAddress(ip);
}

void onApMode(bool ap) {
    displayMgr.setApMode(ap);
}

void onMqttStatusChange(bool connected) {
    DEBUG_PRINTF("[MQTT] Status: %s\n", connected ? "verbunden" : "getrennt");
}

void onMqttTare() {
    scaleReader.tare();
    configMgr.saveScaleConfig(scaleConfig);
}

void onMqttCalibrate(float knownKg) {
    bool ok = scaleReader.calibrate(knownKg);
    if (ok) configMgr.saveScaleConfig(scaleConfig);
}

void onTempCalSave(const TempCalConfig& config) {
    configMgr.saveTempCalConfig(config);
    tempCalConfig = config;
}

// ============================================================
// MQTT PUBLISH
// ============================================================

void checkMqttPublish() {
    if (!mqttEnabled || !mqttClient.isConnected()) return;

    unsigned long interval = (unsigned long)mqttConfig.publishInterval * 1000UL;
    if (millis() - lastMqttPublish >= interval) {
        const ScaleData& sd = scaleReader.getData();
        const TempData&  td = tempSensor.getData();
        DEBUG_PRINTF("[MQTT] Publish: hx711Ready=%d isValid=%d weight=%.3f\n",
                     sd.hx711Ready, sd.isValid, sd.weightKg);
        if (sd.hx711Ready) mqttClient.publishScaleData(sd);
        mqttClient.publishTempData(td);
        lastMqttPublish = millis();
    }
}

// ============================================================
// OTA SETUP
// ============================================================

void setupOta() {
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([]() {
        DEBUG_PRINTLN("[OTA] Update startet...");
        displayMgr.showMessage("OTA Update", "laeuft...");
    });
    ArduinoOTA.onEnd([]() {
        DEBUG_PRINTLN("[OTA] Fertig – Neustart");
        displayMgr.showMessage("OTA fertig", "Neustart...");
    });
    ArduinoOTA.onError([](ota_error_t error) {
        DEBUG_PRINTF("[OTA] Fehler [%u]\n", error);
        displayMgr.showMessage("OTA Fehler", String(error));
    });

    ArduinoOTA.begin();
    DEBUG_PRINTLN("[OTA] Bereit");
}

// ============================================================
// SETUP
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(500);

    DEBUG_PRINTLN("\n=============================");
    DEBUG_PRINTLN("  Bienenwaagendisplay v" FIRMWARE_VERSION);
    DEBUG_PRINTLN("  Board: Wemos NodeMCU");
    DEBUG_PRINTLN("=============================\n");

    // LittleFS
    if (!configMgr.begin()) {
        DEBUG_PRINTLN("[ERROR] LittleFS fehlgeschlagen!");
    }

    // Konfigurationen laden
    configMgr.loadWifiConfig(wifiConfig);
    configMgr.loadMqttConfig(mqttConfig);
    configMgr.loadScaleConfig(scaleConfig);
    configMgr.loadTempCalConfig(tempCalConfig);

    mqttEnabled = mqttConfig.enabled;

    // Display initialisieren (Wire.begin innen)
    displayMgr.begin();

    // HX711 initialisieren
    scaleReader.begin(scaleConfig);

    // DS18B20 initialisieren
    tempSensor.begin();

    // WebServer + WiFi starten
    webServer.setWifiSaveCallback(onWifiSave);
    webServer.setMqttSaveCallback(onMqttSave);
    webServer.setScaleSaveCallback(onScaleSave);
    webServer.setTareCallback(onTare);
    webServer.setCalibrateCallback(onCalibrate);
    webServer.setDisplayCallback(onIpDisplay);
    webServer.setApModeCallback(onApMode);
    webServer.setTempSensor(&tempSensor);
    webServer.setTempCalConfig(&tempCalConfig);
    webServer.setTempCalSaveCallback(onTempCalSave);

    mqttClient.setTareCallback(onMqttTare);
    mqttClient.setCalibrateCallback(onMqttCalibrate);

    bool connected = webServer.begin(&scaleReader);

    if (connected) {
        wifiWasConnected = true;
        DEBUG_PRINTF("[WiFi] STA: %s\n", webServer.getIp().c_str());

        // OTA nur im STA-Mode sinnvoll
        setupOta();

        // MQTT starten
        if (mqttEnabled && strlen(mqttConfig.broker) > 0) {
            mqttClient.begin(mqttConfig);
            mqttClient.setStatusCallback(onMqttStatusChange);
        }
    } else {
        DEBUG_PRINTLN("[WiFi] AP-Mode aktiv");
    }

    DEBUG_PRINTF("[Scale] Gain=%d Offset=%ld Factor=%.2f\n",
                 scaleConfig.gain, scaleConfig.offset, scaleConfig.calibrationFactor);
    DEBUG_PRINTLN("[Setup] Fertig\n");
}

// ============================================================
// LOOP
// ============================================================

void loop() {
    // 1. OTA
    ArduinoOTA.handle();

    // 2. HX711 + Temperatur lesen
    scaleReader.update();
    tempSensor.update();

    // 2a. Temperaturkorrektur anwenden
    {
        const ScaleData& sd = scaleReader.getData();
        const TempData&  td = tempSensor.getData();
        if (sd.isValid && tempCalConfig.enabled && td.isValid) {
            scaleReader.setWeightCorrected(
                applyTempCorrection(sd.weightKg, td.tempC, tempCalConfig), true);
        } else {
            scaleReader.setWeightCorrected(sd.weightKg, false);
        }
    }

    // 3. WebServer / WiFi
    webServer.loop();

    // 4. WiFi-Status nachverfolgen
    if (webServer.isConnected() && !wifiWasConnected) {
        wifiWasConnected = true;
        setupOta();
        if (mqttEnabled && strlen(mqttConfig.broker) > 0) {
            mqttClient.begin(mqttConfig);
            mqttClient.setStatusCallback(onMqttStatusChange);
            DEBUG_PRINTLN("[MQTT] Nachträglich initialisiert");
        }
    }

    // 5. Display aktualisieren
    displayMgr.update(scaleReader.getData(), tempSensor.getData());

    // 6. MQTT
    if (mqttEnabled) {
        mqttClient.loop();
        checkMqttPublish();
    }

    // 7. Debug alle 10 s
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug > 10000UL) {
        const ScaleData& d = scaleReader.getData();
        DEBUG_PRINTF("[Status] WiFi=%s AP=%s MQTT=%s "
                     "HX711=%s Gewicht=%.3fkg Temp=%.1fC Heap=%d\n",
                     webServer.isConnected() ? "ja" : "nein",
                     webServer.isApMode()    ? "ja" : "nein",
                     (mqttEnabled && mqttClient.isConnected()) ? "ja" : "nein",
                     d.hx711Ready            ? "bereit" : "nicht bereit",
                     d.weightKg,
                     tempSensor.getData().tempC,
                     ESP.getFreeHeap());
        lastDebug = millis();
    }

    delay(10);
}
