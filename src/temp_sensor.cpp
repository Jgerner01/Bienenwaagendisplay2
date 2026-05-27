/**
 * temp_sensor.cpp - DS18B20 Temperatursensor Implementierung
 */

#include "temp_sensor.h"

TempSensor::TempSensor()
    : oneWire(DS18B20_PIN), ds(&oneWire),
      lastRequestMs(0), requestPending(false) {
    data = {0.0f, false, 0};
}

void TempSensor::begin() {
    delay(100);  // Einschaltzeit DS18B20 abwarten, bevor Bus-Scan startet
    ds.begin();

    ds.setResolution(12);           // 12-Bit: 0,0625 °C Auflösung, ~750 ms
    ds.setWaitForConversion(false); // Nicht-blockierend

    uint8_t count = ds.getDeviceCount();

    // Zweiter Versuch falls erster Scan nichts gefunden hat
    if (count == 0) {
        delay(250);
        ds.begin();
        count = ds.getDeviceCount();
    }

    DEBUG_PRINTF("[Temp] DS18B20 an GPIO%d (D2) – Sensoren gefunden: %d\n",
                 DS18B20_PIN, count);
    if (count == 0) {
        DEBUG_PRINTF("[Temp] Kein Sensor – Verkabelung/Pull-up auf GPIO%d (D2) prüfen\n",
                     DS18B20_PIN);
    }
}

void TempSensor::update() {
    unsigned long now = millis();

    if (!requestPending) {
        // Neue Messung anfordern wenn Intervall abgelaufen
        if (lastRequestMs == 0 || (now - lastRequestMs) >= TEMP_READ_INTERVAL) {
            ds.requestTemperatures();
            lastRequestMs  = now;
            requestPending = true;
        }
        return;
    }

    // Warten bis Konversion fertig (~750 ms für 12-Bit)
    if ((now - lastRequestMs) < CONVERSION_MS) return;

    float t = ds.getTempCByIndex(0);
    requestPending = false;

    // DEVICE_DISCONNECTED_C = -127, 85.0 = Power-On-Reset-Wert (Fehler)
    if (t <= DEVICE_DISCONNECTED_C || t == 85.0f) {
        data.isValid = false;
        DEBUG_PRINTLN("[Temp] Sensor nicht erreichbar oder Fehler");
        return;
    }

    data.tempC     = t;
    data.isValid   = true;
    data.timestamp = now;

    DEBUG_PRINTF("[Temp] %.2f C\n", t);
}
