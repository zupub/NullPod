/**
 * @file hal_rgb.cpp
 * @brief 板载 WS2812（单颗三色 LED）驱动
 *
 * 全工程只有这里持有一个 Adafruit_NeoPixel 实例：
 * 多个 App（BreathDemo / CircularDial）共用同一颗灯，各自 new 一个实例会
 * 重复申请 RMT 通道，且前后 App 的亮度/颜色会互相覆盖。
 */
#include "hal/hal.h"
#include "hal/hal_config.h"
#include <Adafruit_NeoPixel.h>
#include <math.h>

#define RGB_LED_COUNT 1

static Adafruit_NeoPixel rgbLed(RGB_LED_COUNT, HAL_PIN_RGB, NEO_GRB + NEO_KHZ800);
static bool rgbInited = false;

void HAL::RGB_Init(void)
{
    if (rgbInited)
        return;
    rgbLed.begin();
    rgbLed.setBrightness(255); // 亮度由调用方的 RGB 值控制，这里不做全局缩放
    rgbLed.clear();
    rgbLed.show();
    rgbInited = true;
}

void HAL::RGB_Set(uint8_t r, uint8_t g, uint8_t b)
{
    RGB_Init();
    rgbLed.setPixelColor(0, r, g, b);
    rgbLed.show();
}

void HAL::RGB_Off(void)
{
    RGB_Set(0, 0, 0);
}

// HSV → RGB：h 0~359，s/v 0~100
void HAL::RGB_SetHSV(uint16_t h, uint8_t s, uint8_t v)
{
    const float H = (float)(h % 360);
    const float S = s / 100.0f;
    const float V = v / 100.0f;

    const float C = V * S;
    const float X = C * (1.0f - fabsf(fmodf(H / 60.0f, 2.0f) - 1.0f));
    const float m = V - C;

    float r = 0, g = 0, b = 0;
    if (H < 60)
    {
        r = C;
        g = X;
    }
    else if (H < 120)
    {
        r = X;
        g = C;
    }
    else if (H < 180)
    {
        g = C;
        b = X;
    }
    else if (H < 240)
    {
        g = X;
        b = C;
    }
    else if (H < 300)
    {
        r = X;
        b = C;
    }
    else
    {
        r = C;
        b = X;
    }

    RGB_Set((uint8_t)((r + m) * 255.0f + 0.5f),
            (uint8_t)((g + m) * 255.0f + 0.5f),
            (uint8_t)((b + m) * 255.0f + 0.5f));
}

// 调试用色卡：0~7 依次为红/橙/黄/绿/青/蓝/紫/白
void HAL::RGB_Demo(uint8_t index)
{
    static const uint8_t presets[8][3] = {
        {255, 0, 0}, {255, 64, 0}, {255, 255, 0}, {0, 255, 0},
        {0, 255, 255}, {0, 0, 255}, {128, 0, 255}, {255, 255, 255}};
    const uint8_t *p = presets[index % 8];
    RGB_Set(p[0], p[1], p[2]);
}
