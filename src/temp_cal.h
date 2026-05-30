/**
 * temp_cal.h - Zweistufige Temperaturkorrektur für die Wägezelle
 *
 * Stufe 1 – Poly2 direkt:
 *   correction = a·T² + b·T + c
 *
 * Stufe 2 – PT2-Filter + Poly2:
 *   T_pt2 = PT2-Filter(T, T2_min, D)
 *   correction = a2·T_pt2² + b2·T_pt2 + c2
 *
 * PT2-Filter via Euler-Diskretisierung des Zustandsraummodells
 */

#ifndef TEMP_CAL_H
#define TEMP_CAL_H

#include <Arduino.h>

// ============================================================
// Stufe 1: Poly2 direkt auf die Temperatur
// ============================================================

struct TempCalConfig {
    bool  enabled = false;
    float a       = 0.0f;
    float b       = 0.0f;
    float c       = 0.0f;
};

inline float applyTempCorrection(float rawKg, float tempC, const TempCalConfig& cal) {
    if (!cal.enabled) return rawKg;
    float corr = cal.a * tempC * tempC + cal.b * tempC + cal.c;
    return rawKg - corr;
}

// ============================================================
// Stufe 2: PT2-Filter + Poly2
// ============================================================

struct PT2CalConfig {
    bool  enabled = false;
    float T2_min  = 240.0f; // Zeitkonstante T2 in Minuten (Anstiegszeit ~1,8·T2)
    float D       = 0.5f;   // Dämpfungsgrad (0,5–1,0 typisch)
    float a       = 0.0f;
    float b       = 0.0f;
    float c       = 0.0f;
};

struct PT2FilterState {
    float         x1          = 0.0f;
    float         x2          = 0.0f;
    bool          initialized = false;
    unsigned long lastMs      = 0;
};

// Gibt gefilterte Temperatur zurück und aktualisiert den Zustand
inline float updatePT2Filter(PT2FilterState& st, float u, float T2_min, float D) {
    unsigned long now = millis();
    if (!st.initialized) {
        st.x1 = u; st.x2 = 0.0f; st.initialized = true; st.lastMs = now;
        return u;
    }
    float dt_s = (float)(now - st.lastMs) / 1000.0f;
    st.lastMs  = now;
    if (dt_s > 60.0f) dt_s = 60.0f;   // Schutz bei langen Pausen
    if (dt_s <= 0.0f) return st.x1;

    float wn  = 1.0f / (T2_min * 60.0f);
    float wn2 = wn * wn;
    float x1n = st.x1 + dt_s * st.x2;
    float x2n = st.x2 + dt_s * (wn2 * (u - st.x1) - 2.0f * D * wn * st.x2);
    st.x1 = x1n;
    st.x2 = x2n;
    return st.x1;
}

inline float applyPT2Correction(float weightKg, float T_pt2, const PT2CalConfig& cal) {
    if (!cal.enabled) return weightKg;
    float corr = cal.a * T_pt2 * T_pt2 + cal.b * T_pt2 + cal.c;
    return weightKg - corr;
}

#endif // TEMP_CAL_H
