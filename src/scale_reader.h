/**
 * scale_reader.h - HX711 Wägezellen-Interface
 * Liest Gewichtsdaten vom HX711 und unterstützt Kalibrierung
 */

#ifndef SCALE_READER_H
#define SCALE_READER_H

#include <Arduino.h>
#include <HX711.h>
#include "config.h"

struct ScaleConfig {
    float    calibrationFactor;  // Rohwert pro kg
    long     offset;             // Tara-Offset
    uint8_t  gain;               // 32, 64 oder 128
    uint16_t publishInterval;    // MQTT-Publish-Intervall in Sekunden
};

struct ScaleData {
    float         weightKg;
    float         weightTCorrectedKg;  // Stufe 1: Poly2 direkt (T-Korrektur)
    float         weightCorrectedKg;   // Stufe 1+2: T-Korr + PT2-Korr (vollständig)
    float         trimmedMeanKg;       // Getrimmter Mittelwert (je 5 Ausreißer entfernt) in kg
    float         spreadKg;            // Standardabweichung aller Samples in kg
    float         fastWeightKg;        // Schnell-Median (SCHNELL_SAMPLES, ~1 s Fenster)
    long          rawValue;
    long          offset;
    float         calibrationFactor;
    uint8_t       gain;
    bool          isValid;
    bool          hx711Ready;
    bool          tempCorrectionActive;
    unsigned long timestamp;
};

class ScaleReader {
public:
    ScaleReader();

    void begin(ScaleConfig& config);

    // Messung aktualisieren (im loop aufrufen)
    void update();

    // Tara: aktuellen Rohwert als Nullpunkt setzen, in config speichern
    void tare();

    // Kalibrieren: bekanntes Gewicht aufgelegt → Faktor berechnen
    // Gibt true zurück wenn erfolgreich
    bool calibrate(float knownWeightKg);

    // Verstärkungsfaktor setzen (32, 64 oder 128)
    void setGain(uint8_t gain);

    // Alle Werte auf Standardwerte zurücksetzen
    void resetToDefaults();

    const ScaleData& getData() const { return data; }
    // kgT  = nach Stufe 1 (Poly2 direkt), kgFull = nach Stufe 1+2
    void setWeightCorrected(float kgT, float kgFull, bool active) {
        data.weightTCorrectedKg   = kgT;
        data.weightCorrectedKg    = kgFull;
        data.tempCorrectionActive = active;
    }
    bool isReady();

private:
    HX711     hx711;
    ScaleData data;
    ScaleConfig* cfg;

    // Gleitender Median – ein Sample pro loop()-Aufruf
    static const uint16_t SAMPLE_BUF_SIZE = HX711_SAMPLES;
    static const uint8_t  TRIM_COUNT      = 5;  // je 5 Ausreißer oben/unten für Trimmed Mean
    long     sampleBuf[SAMPLE_BUF_SIZE];
    uint16_t sampleIdx;
    uint16_t sampleCount;

    // Schnell-Puffer für Schnellmessung
    static const uint8_t FAST_BUF_SIZE = SCHNELL_SAMPLES;
    long     fastBuf[FAST_BUF_SIZE];
    uint8_t  fastIdx;
    uint8_t  fastCount;

    // Berechnet Median, Standardabweichung und Trimmed Mean (in Roheinheiten)
    void computeStats(long& outMedian, float& outSpread, float& outTrimmedMean);
    long computeFastMedian();
    void clearSampleBuffer();
    // Wartet bis is_ready() true ist, maximal timeoutMs Millisekunden.
    // Gibt true zurück wenn HX711 bereit, false bei Timeout.
    bool waitReady(uint16_t timeoutMs = 500);
};

#endif // SCALE_READER_H
