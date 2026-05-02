#pragma once
#include "thinclock.h"

enum SensorType { SENSOR_NONE, SENSOR_BME280, SENSOR_BMP280, SENSOR_HTU21DF, SENSOR_SHT31 };

struct SensorData {
    float temperature;  // Celsius
    float humidity;     // %RH (0 if sensor doesn't support)
    uint16_t light;     // LDR raw 0-4095
    uint8_t lightPct;   // LDR as 0-100%
    bool hasTempHumidity;
};

class Sensors {
public:
    void begin();
    void read();
    SensorData data;
    SensorType type = SENSOR_NONE;
};
