/**
 * temp_cal.h - Temperaturkorrektur für die Wägezelle
 * Korrekturpolynom 2. Ordnung: correction(T) = a*T² + b*T + c
 * Korrigiertes Gewicht = Rohgewicht - correction(T_aktuell)
 */

#ifndef TEMP_CAL_H
#define TEMP_CAL_H

#include <Arduino.h>

struct TempCalConfig {
    bool  enabled = false;
    float a       = 0.0f;  // Koeffizient T²
    float b       = 0.0f;  // Koeffizient T
    float c       = 0.0f;  // Konstante (Offset bei T=0)
};

inline float applyTempCorrection(float rawKg, float tempC, const TempCalConfig& cal) {
    if (!cal.enabled) return rawKg;
    float corr = cal.a * tempC * tempC + cal.b * tempC + cal.c;
    return rawKg - corr;
}

#endif // TEMP_CAL_H
