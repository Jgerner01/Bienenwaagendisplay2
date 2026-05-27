/**
 * display.h - I2C LCD 16x2 Interface
 * Zeigt 30 s die IP-Adresse an, danach das Gewicht
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include "config.h"
#include "scale_reader.h"
#include "temp_sensor.h"

class DisplayManager {
public:
    DisplayManager();

    // Wire.begin + LCD initialisieren
    void begin();

    // Anzeige aktualisieren (im loop aufrufen)
    void update(const ScaleData& scaleData, const TempData& tempData);

    void setIpAddress(const String& ip);
    void setApMode(bool apMode);

    // Temporäre Meldung (z.B. Tara, OTA)
    void showMessage(const String& line1, const String& line2);

private:
    LiquidCrystal_I2C* lcd;
    String   ipAddress;
    bool     apMode;
    bool     showingIp;             // true = IP-Anzeige aktiv
    unsigned long startTime;        // millis() bei begin()
    unsigned long lastWeightUpdate; // Zeitstempel letzte Gewichtsanzeige

    void showIp();
    void showWeight(float kg, bool weightValid, float tempC, bool tempValid);

    // Hilfsfunktion: String auf LCD-Breite padden/kürzen
    String padLine(const String& s, uint8_t width = LCD_COLS);
};

#endif // DISPLAY_H
