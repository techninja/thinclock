#pragma once
#include "thinclock.h"

enum SensorType { SENSOR_NONE, SENSOR_BME280, SENSOR_BMP280, SENSOR_HTU21DF, SENSOR_SHT31 };

struct SensorData {
    float temperature = 0;
    float humidity = 0;
    uint16_t light = 0;
    uint8_t lightPct = 0;
    bool hasTempHumidity = false;
};

class Sensors {
public:
    void begin();
    void read();
    SensorData data;
    SensorType type = SENSOR_NONE;
};
