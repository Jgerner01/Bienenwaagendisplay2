/**
 * scale_reader.cpp - HX711 Wägezellen-Implementierung
 */

#include "scale_reader.h"

ScaleReader::ScaleReader()
    : cfg(nullptr), sampleIdx(0), sampleCount(0), fastIdx(0), fastCount(0) {
    memset(&data, 0, sizeof(ScaleData));
    memset(sampleBuf, 0, sizeof(sampleBuf));
    memset(fastBuf,   0, sizeof(fastBuf));
    data.gain                 = HX711_DEFAULT_GAIN;
    data.weightTCorrectedKg   = 0.0f;
    data.weightCorrectedKg    = 0.0f;
    data.fastWeightKg         = 0.0f;
    data.tempCorrectionActive = false;
}

void ScaleReader::begin(ScaleConfig& config) {
    cfg = &config;

    hx711.begin(HX711_DOUT_PIN, HX711_SCK_PIN);
    hx711.set_gain(config.gain);
    hx711.set_offset(config.offset);
    hx711.set_scale(config.calibrationFactor);

    data.offset            = config.offset;
    data.calibrationFactor = config.calibrationFactor;
    data.gain              = config.gain;
    data.hx711Ready        = false;  // wird auf true gesetzt sobald erster Sample ankommt

    clearSampleBuffer();

    DEBUG_PRINTF("[Scale] begin: gain=%d offset=%ld factor=%.2f ready=%s\n",
                 config.gain, config.offset, config.calibrationFactor,
                 hx711.is_ready() ? "ja" : "nein");
}

// Nicht-blockierendes Update: liest genau einen Sample pro Aufruf.
// HX711 bei 10 SPS liefert alle ~100 ms einen neuen Wert; is_ready() ist
// zwischen den Konversionen false. Durch den gleitenden Puffer entspricht
// die Qualität 10 gemittelten Samples, ohne die Loop zu blockieren.
void ScaleReader::update() {
    if (!hx711.is_ready()) {
        // Kein neuer Wert bereit – letztes Ergebnis behalten.
        // Nach 10 s ohne Messung als ungültig markieren.
        if (data.timestamp == 0 || (millis() - data.timestamp) >= 10000UL) {
            data.isValid    = false;
            data.hx711Ready = false;
        }
        return;
    }

    // Einzelnen Sample lesen – is_ready() ist true, daher kein Warten nötig.
    // hx711.read() liefert den rohen 24-Bit-ADC-Wert ohne Offset-Abzug.
    long rawAdc = hx711.read();

    // Alle Bits gesetzt = DOUT dauerhaft HIGH → Verkabelungsfehler
    if (rawAdc == -1L) {
        data.isValid    = false;
        data.hx711Ready = false;
        DEBUG_PRINTLN("[Scale] Rohwert -1: HX711 antwortet nicht");
        return;
    }

    long net = rawAdc - hx711.get_offset();   // Nettowert nach Tara-Abzug

    // Sample in Langzeit-Ringpuffer schreiben
    sampleBuf[sampleIdx] = net;
    sampleIdx = (sampleIdx + 1) % SAMPLE_BUF_SIZE;
    if (sampleCount < SAMPLE_BUF_SIZE) sampleCount++;

    // Sample in Schnell-Puffer schreiben + fastWeightKg aktualisieren
    fastBuf[fastIdx] = net;
    fastIdx = (fastIdx + 1) % FAST_BUF_SIZE;
    if (fastCount < FAST_BUF_SIZE) fastCount++;
    if (data.calibrationFactor != 0.0f) {
        data.fastWeightKg = (float)computeFastMedian() / data.calibrationFactor;
    }

    data.rawValue   = rawAdc;
    data.hx711Ready = true;
    data.timestamp  = millis();

    // Median + Streuung alle 5 Sekunden neu berechnen
    static unsigned long lastMedianMs = 0;
    if (millis() - lastMedianMs >= 5000UL || !data.isValid) {
        lastMedianMs = millis();
        long  median      = 0;
        float spread      = 0.0f;
        float trimmedMean = 0.0f;
        computeStats(median, spread, trimmedMean);
        if (data.calibrationFactor != 0.0f) {
            data.weightKg       = (float)median / data.calibrationFactor;
            data.spreadKg       = spread        / data.calibrationFactor;
            data.trimmedMeanKg  = trimmedMean   / data.calibrationFactor;
            data.isValid        = true;
        } else {
            data.weightKg      = 0.0f;
            data.spreadKg      = 0.0f;
            data.trimmedMeanKg = 0.0f;
            data.isValid       = false;
        }
    }
}

void ScaleReader::tare() {
    // is_ready() ist nur für ~100 µs true – kein Sofort-Check, stattdessen
    // auf das nächste Bereit-Signal warten (hx711.tare() macht das intern).
    // waitReady() schützt vor hängendem Aufruf bei abgezogenem HX711.
    if (!waitReady()) {
        DEBUG_PRINTLN("[Scale] Tara: HX711 nicht bereit (Timeout)");
        return;
    }

    hx711.tare(20);

    long newOffset = hx711.get_offset();
    data.offset = newOffset;
    if (cfg) cfg->offset = newOffset;

    clearSampleBuffer();

    DEBUG_PRINTF("[Scale] Tara gesetzt: offset=%ld\n", newOffset);
}

bool ScaleReader::calibrate(float knownWeightKg) {
    if (knownWeightKg <= 0.0f) {
        DEBUG_PRINTLN("[Scale] Kalibrierung: ungültiges Gewicht");
        return false;
    }
    if (!waitReady()) {
        DEBUG_PRINTLN("[Scale] Kalibrierung: HX711 nicht bereit (Timeout)");
        return false;
    }

    hx711.set_scale(1.0f);
    float rawNet = hx711.get_value(20);

    if (rawNet == 0.0f) {
        DEBUG_PRINTLN("[Scale] Kalibrierung: Rohwert 0");
        return false;
    }

    float newFactor = rawNet / knownWeightKg;
    hx711.set_scale(newFactor);

    data.calibrationFactor = newFactor;
    data.isValid           = true;
    if (cfg) cfg->calibrationFactor = newFactor;

    clearSampleBuffer();

    DEBUG_PRINTF("[Scale] Kalibriert: rawNet=%.0f kg=%.3f factor=%.2f\n",
                 rawNet, knownWeightKg, newFactor);
    return true;
}

void ScaleReader::setGain(uint8_t gain) {
    if (gain != 32 && gain != 64 && gain != 128) {
        DEBUG_PRINTF("[Scale] Ungültiger Gain: %d\n", gain);
        return;
    }

    hx711.set_gain(gain);
    data.gain = gain;
    if (cfg) cfg->gain = gain;

    clearSampleBuffer();

    DEBUG_PRINTF("[Scale] Gain gesetzt: %d\n", gain);
}

void ScaleReader::resetToDefaults() {
    hx711.set_offset(0);
    hx711.set_scale(1.0f);
    hx711.set_gain(HX711_DEFAULT_GAIN);

    data.offset            = 0;
    data.calibrationFactor = 1.0f;
    data.gain              = HX711_DEFAULT_GAIN;
    data.weightKg          = 0.0f;
    data.trimmedMeanKg     = 0.0f;
    data.spreadKg          = 0.0f;
    data.isValid           = false;

    if (cfg) {
        cfg->offset            = 0;
        cfg->calibrationFactor = 1.0f;
        cfg->gain              = HX711_DEFAULT_GAIN;
    }

    clearSampleBuffer();

    DEBUG_PRINTLN("[Scale] Werkseinstellungen wiederhergestellt");
}

bool ScaleReader::isReady() {
    return hx711.is_ready();
}

static int cmpLong(const void* a, const void* b) {
    long la = *(const long*)a;
    long lb = *(const long*)b;
    return (la > lb) - (la < lb);
}

void ScaleReader::computeStats(long& outMedian, float& outSpread, float& outTrimmedMean) {
    if (sampleCount == 0) {
        outMedian      = 0;
        outSpread      = 0.0f;
        outTrimmedMean = 0.0f;
        return;
    }

    // Statisch im BSS-Segment statt auf dem Stack (ESP8266-Stack ist nur 4 KB).
    static long tmp[SAMPLE_BUF_SIZE];
    memcpy(tmp, sampleBuf, sampleCount * sizeof(long));
    qsort(tmp, sampleCount, sizeof(long), cmpLong);

    // Median aller Samples
    outMedian = tmp[sampleCount / 2];

    // Standardabweichung aller Samples als Streuungsmaß
    float sum = 0.0f;
    for (uint16_t i = 0; i < sampleCount; i++) sum += (float)tmp[i];
    float mean = sum / (float)sampleCount;

    float var = 0.0f;
    for (uint16_t i = 0; i < sampleCount; i++) {
        float d = (float)tmp[i] - mean;
        var += d * d;
    }
    outSpread = sqrtf(var / (float)sampleCount);

    // Trimmed Mean: je TRIM_COUNT Ausreißer oben/unten entfernen, Rest mitteln
    uint16_t lo = 0, hi = sampleCount;
    if (sampleCount > 2u * TRIM_COUNT) {
        lo = TRIM_COUNT;
        hi = sampleCount - TRIM_COUNT;
    }
    float tsum = 0.0f;
    for (uint16_t i = lo; i < hi; i++) tsum += (float)tmp[i];
    outTrimmedMean = tsum / (float)(hi - lo);
}

long ScaleReader::computeFastMedian() {
    if (fastCount == 0) return 0;
    long tmp[FAST_BUF_SIZE];
    memcpy(tmp, fastBuf, fastCount * sizeof(long));
    qsort(tmp, fastCount, sizeof(long), cmpLong);
    return tmp[fastCount / 2];
}

void ScaleReader::clearSampleBuffer() {
    memset(sampleBuf, 0, sizeof(sampleBuf));
    sampleIdx   = 0;
    sampleCount = 0;
    memset(fastBuf, 0, sizeof(fastBuf));
    fastIdx   = 0;
    fastCount = 0;
    data.fastWeightKg = 0.0f;
}

bool ScaleReader::waitReady(uint16_t timeoutMs) {
    unsigned long deadline = millis() + timeoutMs;
    while (!hx711.is_ready()) {
        if (millis() >= deadline) return false;
        yield();
    }
    return true;
}
