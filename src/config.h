/**
 * config.h - Konfigurationskonstanten Bienenwaagendisplay
 * Board: Wemos NodeMCU (ESP-12E / ESP8266)
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// GERÄT
// ============================================================
#define FIRMWARE_VERSION    "1.0.0"
#define DEVICE_NAME         "Bienenwaage"
#define DEVICE_ID           "bienenwaage_01"
#define DEVICE_MANUFACTURER "DIY"
#define DEVICE_MODEL        "NodeMCU-HX711-LCD16x2"

// ============================================================
// I2C LCD 16x2
// ============================================================
#define LCD_I2C_ADDR        0x3F    // alternativ 0x27
#define LCD_COLS            16
#define LCD_ROWS            2
#define LCD_SDA_PIN         5       // D1 (GPIO5)
#define LCD_SCL_PIN         13      // D7 (GPIO13)

// Wie lange die IP-Adresse nach dem Start angezeigt wird (ms)
#define LCD_IP_DISPLAY_MS   5000UL

// ============================================================
// HX711
// ============================================================
#define HX711_DOUT_PIN      2       // D4 (GPIO2)  – DO vom HX711
#define HX711_SCK_PIN       0       // D3 (GPIO0)  – CK vom HX711
#define HX711_DEFAULT_GAIN  128     // Channel A, Gain 128
#define HX711_SAMPLES       300     // Medianfenster (10 SPS × 30 s)

// ============================================================
// DS18B20 Temperatursensor
// ============================================================
#define DS18B20_PIN         4       // D2 (GPIO4)  – kein Boot-Strapping, 4,7k Pull-up problemlos
#define TEMP_READ_INTERVAL  10000UL // ms zwischen Messungen

// ============================================================
// WiFi
// ============================================================
#define WIFI_HOSTNAME       "bienenwaage"
#define WIFI_AP_SSID        "Bienenwaage"
#define WIFI_AP_PASSWORD    "12345678"
#define WIFI_CONNECT_TIMEOUT_MS  20000UL
#define AP_FALLBACK_TIMEOUT_MS   600000UL   // 10 Minuten

// ============================================================
// MQTT
// ============================================================
#define MQTT_DEFAULT_PORT        1883
#define MQTT_TOPIC_PREFIX        "bienenwaage/01"
#define MQTT_TOPIC_DISCOVERY     "homeassistant/sensor/bienenwaage_01"
#define MQTT_PUBLISH_INTERVAL    60000UL    // ms
#define MQTT_RECONNECT_INTERVAL  5000UL
#define MQTT_QOS                 0
#define MQTT_RETAIN              true

// ============================================================
// OTA
// ============================================================
#define OTA_PASSWORD        "bienenwaage"
#define OTA_HOSTNAME        WIFI_HOSTNAME

// ============================================================
// KONFIGURATIONSDATEIEN (LittleFS)
// ============================================================
#define CONFIG_FILE_WIFI    "/wifi.json"
#define CONFIG_FILE_MQTT    "/mqtt.json"
#define CONFIG_FILE_SCALE   "/scale.json"
#define CONFIG_FILE_TEMPCAL "/tempcal.json"

// ============================================================
// TASTER (D8 / GPIO15)
// ============================================================
// GPIO15 ist Strapping-Pin: muss beim Boot LOW sein (externer 10k Pull-Down).
// Taster verbindet GPIO15 mit 3,3V → aktiv HIGH.
#define BUTTON_PIN              15      // D8 (GPIO15), aktiv HIGH
#define BUTTON_ACTIVE_HIGH      1       // 1 = aktiv HIGH, 0 = aktiv LOW
#define BUTTON_DEBOUNCE_MS      50      // Entprellzeit
#define BUTTON_LONG_PRESS_MS    5000UL  // Haltedauer für Ertragsmessung-Tara
#define BUTTON_CONFIRM_WINDOW_MS 5000UL // Bestätigungsfenster nach Loslassen

// ============================================================
// SCHNELLMESSUNG
// ============================================================
#define SCHNELL_SAMPLES         10      // Median-Fenstergröße (~1 s bei 10 SPS)
#define SCHNELL_DISPLAY_INTERVAL_MS 500UL  // 2 Hz Display-Update

// ============================================================
// DEBUG
// ============================================================
#define DEBUG_PRINTLN(x)    Serial.println(x)
#define DEBUG_PRINTF(...)   Serial.printf(__VA_ARGS__)
#define DEBUG_PRINT(x)      Serial.print(x)

#endif // CONFIG_H
