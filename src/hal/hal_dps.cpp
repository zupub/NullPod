#include "hal/hal.h"
#include "hal/hal_config.h"
#include <Adafruit_BMP280.h>

Adafruit_BMP280 bmp;

const float SEA_LEVEL_HPA = 1013.25; // 本地海平面气压，可查天气站
static float local_atmosphere = SEA_LEVEL_HPA;

void HAL::DPS_Init()
{
    Serial.println("BMP280 init...");

    if (!bmp.begin(0x77))
    {
        Serial.println("BMP280 not found! Check wiring / address.");
        return;
    }

    bmp.setSampling(
        Adafruit_BMP280::MODE_NORMAL,  // sleep / forced / normal
        Adafruit_BMP280::SAMPLING_X2,  // temp osrs_t  (PDF 表4 推荐 x2 配 x16)
        Adafruit_BMP280::SAMPLING_X16, // pressure osrs_p
        Adafruit_BMP280::FILTER_X4,    // IIR coeff
        Adafruit_BMP280::STANDBY_MS_63 // t_standby → ODR ≈10Hz
    );
}

void HAL::updateDPS()
{
    float temp = bmp.readTemperature();             // °C
    float pres = bmp.readPressure() / 100.0;        // Pa → hPa
    float alt = bmp.readAltitude(local_atmosphere); // 估算海拔(m)

    Serial.printf("T=%.2f°C  P=%.2fhPa  Alt=%.1fm\n", temp, pres, alt);
}

float HAL::dps_get_temperature()
{
    return bmp.readTemperature(); // °C
}

float HAL::dps_get_pressure()
{
    return bmp.readPressure() / 100.0; // Pa → hPa
}

float HAL::dps_get_altitude()
{
    return bmp.readAltitude(local_atmosphere); // 估算海拔(m)
}

void HAL::dps_set_atmosphere(float atmosphere)
{
    local_atmosphere = atmosphere;
}
