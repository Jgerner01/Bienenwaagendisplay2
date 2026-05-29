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

enum class DisplayMode {
    NORMAL,           // Temp + normales Gewicht (oder Ertragswert wenn gesetzt)
    SCHNELLMESSUNG,   // Temp + Schnellmessung-Wert (2 Hz)
    CONFIRM_ERTRAG    // Bestätigungsanzeige für Ertragsmessung-Tara (5-s-Fenster)
};

class DisplayManager {
public:
    DisplayManager();

    // Wire.begin + LCD initialisieren
    void begin();

    // Anzeige aktualisieren (im loop aufrufen)
    // ertragsWert: berechneter Ertrag (currentKg - ertragsOffset), NaN wenn nicht gesetzt
    // schnellWert: Schnellmessungs-Wert (currentFastKg - schnellOffset)
    // confirmSecsLeft: verbleibende Sekunden im Bestätigungsfenster (0 = kein Fenster)
    void update(const ScaleData& scaleData, const TempData& tempData,
                DisplayMode mode,
                float ertragsWert, bool ertragsAktiv,
                float schnellWert,
                uint8_t confirmSecsLeft);

    void setIpAddress(const String& ip);
    void setApMode(bool apMode);

    // Temporäre Meldung (z.B. Tara, OTA)
    void showMessage(const String& line1, const String& line2);

private:
    LiquidCrystal_I2C* lcd;
    String   ipAddress;
    bool     apMode;
    bool     showingIp;
    unsigned long startTime;
    unsigned long lastWeightUpdate;

    void showIp();
    void showWeight(float kg, bool weightValid, float tempC, bool tempValid);
    void showErtrag(float ertragsWert, float tempC, bool tempValid);
    void showSchnell(float schnellWert);
    void showConfirm(uint8_t secsLeft);

    String padLine(const String& s, uint8_t width = LCD_COLS);
};

#endif // DISPLAY_H
