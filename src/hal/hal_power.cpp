#include "hal/hal.h"
#include "hal/hal_config.h"

// ADC引脚及参数
const float R1 = 56000.0;      // 分压电阻R1=56kΩ
const float R2 = 22000.0;      // 分压电阻R2=22kΩ
const float REF_VOLTAGE = 3.3; // ESP32 ADC参考电压

void HAL::Power_Init()
{
    // Hold power
    gpio_reset_pin((gpio_num_t)HAL_PIN_PWR_HOLD);
    pinMode(HAL_PIN_PWR_HOLD, OUTPUT);
    digitalWrite(HAL_PIN_PWR_HOLD, 1);
}

void HAL::PowerOff()
{
    digitalWrite(HAL_PIN_PWR_HOLD, 0);
    delay(1000);
}

void HAL::Reboot()
{
    esp_restart();
}

// 电压测量函数
uint8_t HAL::getVoltage()
{
    // 读取ADC原始值（取10次平均）
    int adcValue = analogRead(HAL_PIN_BAT_ADC);

    // 计算实际电压百分比
    float voltage = (adcValue * REF_VOLTAGE / 4096) * ((R1 + R2) / R2);
    float percent = (voltage - 2.25) / (3.9 - 2.25) * 100;
    return constrain(percent, 0, 100);
}
