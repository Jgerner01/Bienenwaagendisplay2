/**
 * temp_sensor.h - DS18B20 Temperatursensor Interface
 * Pin: D7 (GPIO13), 12-Bit-Auflösung (0,0625 °C), nicht-blockierend
 */

#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"

struct TempData {
    float         tempC;
    bool          isValid;
    unsigned long timestamp;
};

class TempSensor {
public:
    TempSensor();

    void begin();

    // Nicht-blockierend: request → warten → lesen über mehrere loop()-Aufrufe
    void update();

    const TempData& getData() const { return data; }

private:
    OneWire           oneWire;
    DallasTemperature ds;
    TempData          data;
    unsigned long     lastRequestMs;
    bool              requestPending;

    // 12-Bit-Auflösung: ~750 ms Konversionszeit
    static const uint16_t CONVERSION_MS = 800;
};

#endif // TEMP_SENSOR_H
