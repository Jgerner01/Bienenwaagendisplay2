# Bienenwaagendisplay 2

ESP8266-basierte Bienenstock-Waage mit Webinterface, MQTT und Temperaturkorrektur.

![Platform](https://img.shields.io/badge/platform-ESP8266-blue)
![Framework](https://img.shields.io/badge/framework-Arduino-teal)
![Board](https://img.shields.io/badge/board-NodeMCU%20v2-orange)
![License](https://img.shields.io/badge/license-MIT-green)

---

## Features

- **Gewichtsmessung** – HX711 mit gleitendem Median-Filter (300 Samples) und Trimmed Mean
- **Temperaturmessung** – DS18B20 OneWire, nicht-blockierend
- **Temperaturkorrektur** – Polynom 2. Ordnung, Koeffizienten per Browser aus Home-Assistant-CSV berechnet
- **LCD-Anzeige** – I²C 16×2, zeigt Temperatur und korrigiertes Gewicht
- **Webinterface** – Konfiguration, Kalibrierung, Gain-Einstellung, T-Korrektur, OTA-Update
- **MQTT** – Gewicht (roh + T-korrigiert), Temperatur, Status; Home Assistant Auto-Discovery
- **WiFi** – AP-Mode für Erstkonfiguration, danach STA-Mode mit automatischem Reconnect
- **OTA** – Firmware-Update direkt über den Browser, kein Kabel nötig
- **LittleFS** – alle Einstellungen persistent im Flash gespeichert

---

## Hardware

### Komponenten

| Komponente | Typ |
|---|---|
| Mikrocontroller | Wemos NodeMCU v2 (ESP-12E, ESP8266) |
| Wägeverstärker | HX711 |
| Display | I²C LCD 16×2 (PCF8574-Backpack, Adresse 0x3F oder 0x27) |
| Temperatursensor | DS18B20 (OneWire) |

### Pin-Belegung

| NodeMCU-Pin | GPIO | Funktion | Hinweis |
|---|---|---|---|
| D1 | GPIO5 | SDA – LCD I²C | |
| D7 | GPIO13 | SCL – LCD I²C | |
| D2 | GPIO4 | DS18B20 OneWire | 4,7 kΩ Pull-up nach 3,3 V |
| D3 | GPIO0 | HX711 CK (Clock) | Strapping-Pin: HIGH beim Boot ✓ |
| D4 | GPIO2 | HX711 DO (Data) | Strapping-Pin: HIGH beim Boot ✓ |

> **Hinweis D3/D4:** GPIO0 und GPIO2 sind Boot-Strapping-Pins des ESP8266 und müssen beim Start HIGH sein. Auf NodeMCU-Boards sind beide mit Pull-ups versehen – der Betrieb als HX711-Leitungen ist daher problemlos.

---

## Inbetriebnahme

### 1. Erster Start – WiFi einrichten

Beim ersten Start (keine WLAN-Zugangsdaten gespeichert) öffnet das Gerät einen Hotspot:

- **SSID:** `Bienenwaage`
- **Passwort:** `12345678`
- **IP:** `192.168.4.1`

Im Browser `192.168.4.1` aufrufen → **WiFi** → Netzwerk auswählen → Speichern.

Nach erfolgreicher Verbindung zeigt das LCD die IP-Adresse des Geräts im Heimnetzwerk.

### 2. Tara setzen

Webinterface → **Waage** → **Tara setzen** (leere Beute auf der Waage).

### 3. Kalibrieren

Webinterface → **Kalibrierung** → bekanntes Gewicht auflegen → Gewicht in kg eingeben → **Kalibrieren**.

### 4. MQTT konfigurieren (optional)

Webinterface → **MQTT** → Broker-IP, Port, Topic-Prefix eintragen → Speichern.

---

## Webinterface

| Route | Methode | Funktion |
|---|---|---|
| `/` | GET | Startseite: Status, Gewicht (roh + korrigiert), Temperatur |
| `/scale` | GET | HX711-Details, Tara-Button, Live-Werte |
| `/tare` | POST | Tara setzen |
| `/calibrate` | GET/POST | Kalibrierseite |
| `/reset-scale` | POST | Waagen-Werkseinstellungen |
| `/gain` | GET/POST | HX711 Gain (32 / 64 / 128) |
| `/tempcal` | GET | Temperaturkorrektur: CSV laden, Polynom berechnen, speichern |
| `/mqtt` | GET/POST | MQTT-Konfiguration |
| `/scan` | GET | WiFi-Netzwerke scannen und verbinden |
| `/ota` | GET/POST | Firmware-Update über Browser |
| `/api/data` | GET | JSON mit aktuellen Mess­werten (für Live-Update) |
| `/api/tempcal` | GET/POST | Temperaturkorrektur-Koeffizienten lesen/schreiben |

---

## MQTT-Topics

Basis-Prefix konfigurierbar (Standard: `bienenwaage/01`).

| Topic | Payload | Beschreibung |
|---|---|---|
| `.../sensors/weight` | `{"value": 26.547, "unit": "kg", "ts": ...}` | Rohgewicht |
| `.../sensors/weight_corrected` | `{"value": 26.521, "unit": "kg", "corrected": true, "ts": ...}` | T-korrigiertes Gewicht |
| `.../sensors/temperature` | `{"value": 18.75, "unit": "C", "ts": ...}` | Temperatur |
| `.../sensors/trimmedmean` | `{"value": 26.544, "unit": "kg", "ts": ...}` | Getrimmter Mittelwert |
| `.../sensors/spread` | `{"value": 0.0021, "unit": "kg", "ts": ...}` | Standardabweichung |
| `.../sensors/raw` | `{"value": 348291, "ts": ...}` | HX711 Rohwert |
| `.../sensors/offset` | `{"value": 12345, "ts": ...}` | Tara-Offset |
| `.../sensors/calibrationfactor` | `{"value": 21.45, "ts": ...}` | Kalibrierfaktor |
| `.../sensors/gain` | `{"value": 128, "ts": ...}` | HX711 Gain |
| `.../status` | `online` / `offline` | Last-Will-Testament |

Home Assistant Auto-Discovery wird beim ersten Verbindungsaufbau automatisch gesendet.

---

## Temperaturkorrektur

Wägezellen driften temperaturabhängig. Das Korrekturpolynom kompensiert diesen Effekt:

```
korrigiertes_Gewicht = rohgewicht − (a·T² + b·T + c)
```

### Vorgehen

1. **Daten aufzeichnen** – Beute über mehrere Stunden/Tage bei konstantem Gewicht loggen (z. B. leere Beute über einen Temperaturbereich von mindestens 10 °C).

2. **CSV exportieren** – aus Home Assistant zwei Dateien aus dem History-Export:
   - Gewicht-CSV: Entity `sensor.bienenwaage_..._gewicht`
   - Temperatur-CSV: Entity `sensor.bienenwaage_..._temperatur`

   Format (wird automatisch erkannt):
   ```
   entity_id,state,last_changed
   sensor.bienenwaage_...,26.547,2026-05-26T23:00:00.000Z
   ```

3. **Koeffizienten berechnen** – Webinterface → **T-Korrektur** → beide CSV-Dateien laden → **Berechnen**. Die Berechnung (Gauß-Elimination, Kleinste-Quadrate-Fit) erfolgt vollständig im Browser.

4. **Ergebnis prüfen** – R² sollte > 0,8 sein. Das Diagramm zeigt Messpunkte (orange) und gefittete Kurve (rot).

5. **Speichern** – Koeffizienten werden in `/tempcal.json` auf dem Gerät gespeichert.

---

## Konfiguration (`src/config.h`)

```cpp
// Display
#define LCD_I2C_ADDR        0x3F    // alternativ 0x27
#define LCD_SDA_PIN         5       // D1 (GPIO5)
#define LCD_SCL_PIN         13      // D7 (GPIO13)

// HX711
#define HX711_DOUT_PIN      2       // D4 (GPIO2)
#define HX711_SCK_PIN       0       // D3 (GPIO0)
#define HX711_DEFAULT_GAIN  128     // Kanal A, höchste Auflösung
#define HX711_SAMPLES       300     // Medianfenster (~30 s bei 10 SPS)

// DS18B20
#define DS18B20_PIN         4       // D2 (GPIO4)
#define TEMP_READ_INTERVAL  10000UL // 10 s zwischen Messungen

// WiFi
#define WIFI_HOSTNAME       "bienenwaage"
#define WIFI_AP_SSID        "Bienenwaage"
#define WIFI_AP_PASSWORD    "12345678"

// MQTT
#define MQTT_TOPIC_PREFIX   "bienenwaage/01"
#define MQTT_PUBLISH_INTERVAL  60000UL  // 60 s

// OTA
#define OTA_PASSWORD        "bienenwaage"
```

---

## Dateistruktur

```
src/
├── config.h              – Konstanten und Pin-Definitionen
├── config_manager.h/.cpp – LittleFS: WiFi/MQTT/Scale/TempCal laden und speichern
├── temp_cal.h            – TempCalConfig, applyTempCorrection()
├── scale_reader.h/.cpp   – HX711: nicht-blockierend, Median, Trimmed Mean
├── temp_sensor.h/.cpp    – DS18B20: nicht-blockierend, Request→Wait→Read
├── display.h/.cpp        – LCD 16×2: Temperatur + korrigiertes Gewicht
├── webserver.h/.cpp      – HTTP-Server: alle Seiten, API, OTA
├── mqtt_client.h/.cpp    – MQTT: publish, HA Auto-Discovery
└── main.cpp              – Setup/Loop, Temperaturkorrektur im Loop
platformio.ini
```

---

## Build & Flash

### Voraussetzungen

- [PlatformIO](https://platformio.org/) (VS Code Extension oder CLI)

### Bauen und flashen

```bash
# Bauen
pio run

# Flashen (USB)
pio run --target upload

# Serieller Monitor
pio device monitor --baud 115200
```

### OTA-Update (nach Erstkonfiguration)

Webinterface → **OTA** → Datei `.pio/build/nodemcuv2/firmware.bin` auswählen → Hochladen.

### Abhängigkeiten (`platformio.ini`)

```ini
[env:nodemcuv2]
platform  = espressif8266
board     = nodemcuv2
framework = arduino

lib_deps =
    marcoschwartz/LiquidCrystal_I2C@^1.1.4
    bogde/HX711@^0.7.5
    knolleary/PubSubClient@^2.8
    bblanchon/ArduinoJson@^7.2.0
    paulstoffregen/OneWire@^2.3.7
    milesburton/DallasTemperature@^3.11.0
```

---

## Bekannte Einschränkungen

- **I²C auf GPIO16 (D0) nicht möglich** – GPIO16 unterstützt kein Open-Drain; SCL daher auf D7 (GPIO13)
- **ESP8266-Stack: 4 KB** – keine großen lokalen Arrays in häufig gerufenen Funktionen (→ `static` verwenden)
- **HX711 `is_ready()`** – ist nur ~100 µs pro Konversion `true`; nie als dauerhaften Status-Indikator nutzen
- **D3/D4 beim Boot** – müssen HIGH bleiben; HX711 darf diese Leitungen beim Bootvorgang nicht auf LOW ziehen

---

## Lizenz

MIT License – frei verwendbar, veränderbar und weitergegeben unter Beibehaltung des Lizenzhinweises.
