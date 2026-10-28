#include "hal/hal.h"
#include "hal/hal_config.h"

void HAL::beep(float frequency, uint32_t duration)
{
    tone(HAL_PIN_BUZZER, frequency, duration);
}

void HAL::beepStop()
{
    noTone(HAL_PIN_BUZZER);
}

void HAL::buzz(float frequency, uint32_t duration)
{
    const uint32_t halfUs = 500000u / (uint32_t)frequency;
    const uint32_t cycles = (duration * 1000u) / (halfUs * 2);

    pinMode(HAL_PIN_BUZZER, OUTPUT);

    for (uint32_t i = 0; i < cycles; i++)
    {
        digitalWrite(HAL_PIN_BUZZER, HIGH);
        delayMicroseconds(halfUs);
        digitalWrite(HAL_PIN_BUZZER, LOW);
        delayMicroseconds(halfUs);
    }
}
