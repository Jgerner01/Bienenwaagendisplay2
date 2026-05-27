/**
 * display.cpp - I2C LCD 16x2 Implementierung
 */

#include "display.h"

DisplayManager::DisplayManager()
    : lcd(nullptr), ipAddress(""), apMode(false),
      showingIp(true), startTime(0), lastWeightUpdate(0) {
}

void DisplayManager::begin() {
    Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);

    lcd = new LiquidCrystal_I2C(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
    lcd->init();
    lcd->backlight();
    lcd->clear();

    startTime = millis();
    showingIp = true;

    lcd->setCursor(0, 0);
    lcd->print(padLine("Bienenwaage"));
    lcd->setCursor(0, 1);
    lcd->print(padLine("Starte..."));

    DEBUG_PRINTLN("[Display] LCD 16x2 initialisiert");
}

void DisplayManager::update(const ScaleData& scaleData, const TempData& tempData) {
    unsigned long elapsed = millis() - startTime;

    if (showingIp && elapsed >= LCD_IP_DISPLAY_MS) {
        showingIp = false;
    }

    if (showingIp) {
        showIp();
    } else {
        // LCD-Ausgabe auf 1 Hz begrenzen
        if (millis() - lastWeightUpdate >= 1000UL) {
            lastWeightUpdate = millis();
            showWeight(scaleData.weightCorrectedKg, scaleData.isValid,
                       tempData.tempC, tempData.isValid);
        }
    }
}

void DisplayManager::setIpAddress(const String& ip) {
    ipAddress = ip;
    if (showingIp) showIp();
}

void DisplayManager::setApMode(bool ap) {
    apMode = ap;
    if (showingIp) showIp();
}

void DisplayManager::showMessage(const String& line1, const String& line2) {
    if (!lcd) return;
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print(padLine(line1));
    lcd->setCursor(0, 1);
    lcd->print(padLine(line2));
}

// ============================================================
// PRIVATE
// ============================================================

void DisplayManager::showIp() {
    if (!lcd) return;
    lcd->setCursor(0, 0);
    if (apMode) {
        lcd->print(padLine("Bienenwaage AP"));
    } else {
        lcd->print(padLine("Bienenwaage"));
    }
    lcd->setCursor(0, 1);
    if (ipAddress.length() > 0) {
        lcd->print(padLine(ipAddress));
    } else {
        lcd->print(padLine("Warte auf WiFi.."));
    }
}

void DisplayManager::showWeight(float kg, bool weightValid, float tempC, bool tempValid) {
    if (!lcd) return;

    // Zeile 1: Temperatur (+ '*' wenn Korrektur aktiv)
    lcd->setCursor(0, 0);
    if (tempValid) {
        char tbuf[17];
        snprintf(tbuf, sizeof(tbuf), "T:%5.1fC", tempC);
        // '*' signalisiert aktive Temperaturkorrektur, rechts aufgefüllt
        lcd->print(padLine(String(tbuf)));
    } else {
        lcd->print(padLine("T: -.-- C"));
    }

    // Zeile 2: korrigiertes Gewicht (ScaleData liefert bereits weightCorrectedKg)
    lcd->setCursor(0, 1);
    if (weightValid) {
        char wbuf[17];
        snprintf(wbuf, sizeof(wbuf), "%8.3f kg", kg);
        lcd->print(padLine(String(wbuf)));
    } else {
        lcd->print(padLine("Warte auf HX711"));
    }
}

String DisplayManager::padLine(const String& s, uint8_t width) {
    String result = s;
    if (result.length() > width) {
        result = result.substring(0, width);
    }
    while (result.length() < width) {
        result += ' ';
    }
    return result;
}
