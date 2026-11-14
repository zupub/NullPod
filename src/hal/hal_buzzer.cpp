#include "hal/hal.h"
#include "hal/hal_config.h"

// 蜂鸣器总开关（持久化在 NVS）：关闭后 beep()/buzz() 直接返回
static bool buzzer_enabled = true;

void HAL::Buzzer_Init()
{
    Preferences prefs;
    prefs.begin(CONFIG_NAMESPACE);
    buzzer_enabled = prefs.getBool(BUZZER_EN_KEY, true);
    prefs.end();
}

void HAL::setBuzzerEnabled(bool en)
{
    buzzer_enabled = en;

    Preferences prefs;
    prefs.begin(CONFIG_NAMESPACE);
    prefs.putBool(BUZZER_EN_KEY, en);
    prefs.end();
}

bool HAL::isBuzzerEnabled()
{
    return buzzer_enabled;
}

void HAL::beep(float frequency, uint32_t duration)
{
    if (!buzzer_enabled)
        return;

    tone(HAL_PIN_BUZZER, frequency, duration);
}

void HAL::beepStop()
{
    noTone(HAL_PIN_BUZZER);
}

void HAL::buzz(float frequency, uint32_t duration)
{
    if (!buzzer_enabled)
        return;

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
