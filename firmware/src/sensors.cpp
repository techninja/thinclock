#include "sensors.h"
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_HTU21DF.h>
#include <Adafruit_SHT31.h>

static Adafruit_BME280 bme280;
static Adafruit_BMP280 bmp280;
static Adafruit_HTU21DF htu21df;
static Adafruit_SHT31 sht31;

void Sensors::begin() {
    Wire.begin(I2C_SDA, I2C_SCL);
    pinMode(LDR_PIN, INPUT);

    if (bme280.begin(0x76, &Wire) || bme280.begin(0x77, &Wire)) {
        type = SENSOR_BME280;
        Serial.println("[Sensors] BME280");
    } else if (bmp280.begin(0x76) || bmp280.begin(0x77)) {
        type = SENSOR_BMP280;
        Serial.println("[Sensors] BMP280");
    } else if (htu21df.begin(&Wire)) {
        type = SENSOR_HTU21DF;
        Serial.println("[Sensors] HTU21DF");
    } else if (sht31.begin(0x44)) {
        type = SENSOR_SHT31;
        Serial.println("[Sensors] SHT31");
    } else {
        type = SENSOR_NONE;
        Serial.println("[Sensors] No I2C sensor found");
    }

    data.hasTempHumidity = (type != SENSOR_NONE);
}

void Sensors::read() {
    // LDR
    data.light = analogRead(LDR_PIN);
    data.lightPct = map(data.light, 0, 4095, 0, 100);

    // Temp/humidity
    switch (type) {
        case SENSOR_BME280:
            data.temperature = bme280.readTemperature();
            data.humidity = bme280.readHumidity();
            break;
        case SENSOR_BMP280:
            data.temperature = bmp280.readTemperature();
            data.humidity = 0;
            break;
        case SENSOR_HTU21DF:
            data.temperature = htu21df.readTemperature();
            data.humidity = htu21df.readHumidity();
            break;
        case SENSOR_SHT31:
            data.temperature = sht31.readTemperature();
            data.humidity = sht31.readHumidity();
            break;
        default:
            data.temperature = 0;
            data.humidity = 0;
            break;
    }
}
