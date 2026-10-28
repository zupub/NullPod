#include "AboutApp.h"
#include "driver/temp_sensor.h"

AboutApp g_aboutApp;

void AboutApp::updateRings()
{
    rings[0].percent = getRamPercent();
    rings[1].percent = getPsramPercent();
    rings[2].percent = getFlashPercent();

    esp_err_t err = temp_sensor_read_celsius(&cpuTemp);
    if (err != ESP_OK)
        Serial.printf("temp_sensor_read_celsius fail: 0x%x\n", err);
}

void AboutApp::drawRings()
{
    canvas->fillScreen(TFT_BLACK);

    constexpr int ANGLE_MAX = 360;
    constexpr int ANGLE_OFFSET = -90;

    for (int i = 0; i < RING_COUNT; i++)
    {
        int rOuter = rings[i].rOuter;
        int rInner = rOuter - rings[i].thickness;
        uint16_t color = rings[i].color;
        uint16_t dim = dimColors[i];
        int percent = constrain(rings[i].percent, 0, 100);
        int sweep = (ANGLE_MAX * percent) / 100;

        // 背景灰环
        canvas->fillArc(CX, CY, rInner, rOuter, ANGLE_OFFSET, ANGLE_OFFSET + ANGLE_MAX, dim);

        // 前景彩色进度
        if (sweep > 0)
        {
            canvas->fillArc(CX, CY, rInner, rOuter, ANGLE_OFFSET, ANGLE_OFFSET + sweep, color);
        }

        // 圆角端点
        int dotR = rings[i].thickness / 2;
        float radStart = ANGLE_OFFSET * M_PI / 180.0f;
        float ringR = rInner + dotR;

        canvas->fillCircle(CX + cosf(radStart) * ringR, CY + sinf(radStart) * ringR, dotR, color);

        if (sweep > 0)
        {
            float radEnd = (ANGLE_OFFSET + sweep) * M_PI / 180.0f;
            canvas->fillCircle(CX + cosf(radEnd) * ringR, CY + sinf(radEnd) * ringR, dotR, color);
        }
    }

    canvas->setTextDatum(textdatum_t::top_right);
    canvas->setFont(&fonts::FreeSans9pt7b);

    // 显示三条环的数值
    char buf[16];
    for (int i = 0; i < RING_COUNT; ++i)
    {
        canvas->setTextColor(rings[i].color);
        snprintf(buf, sizeof(buf), "%s:%3d%%", rings[i].label, rings[i].percent);
        canvas->drawString(buf, CX - 15, CY - rings[i].rOuter + 4);
    }

    canvas->setTextDatum(textdatum_t::middle_center);
    canvas->setTextColor(TFT_GREEN);
    snprintf(buf, sizeof(buf), "%.1f°C", cpuTemp);
    canvas->drawString(buf, CX, CY, &fonts::efontCN_16_b);

    canvas->setTextDatum(0);
}

void AboutApp::Init()
{
    disp = HAL::GetDisplay();
    canvas = HAL::GetCanvas();

    HAL::logMemory();

    // TSENS_DAC_L2 = -10~80℃，误差 <1℃，室温场景最准
    temp_sensor_config_t cfg = TSENS_CONFIG_DEFAULT();
    cfg.dac_offset = TSENS_DAC_L2; // 等价于 TSENS_DAC_DEFAULT
    cfg.clk_div = 6;

    esp_err_t err = temp_sensor_set_config(cfg);
    if (err != ESP_OK)
    {
        Serial.printf("tsens config fail: 0x%x\n", err);
        return;
    }
    temp_sensor_start(); // 使能传感器
    delay(100);          // 稳一下再读

    // 由亮色自动算暗色（factor 0.25 = 比较接近 Apple Watch 观感）
    for (int i = 0; i < RING_COUNT; i++)
    {
        dimColors[i] = dim565((uint16_t)rings[i].color, 0.15f);
    }

    // 读取真实数据，存为目标值，percent 从 0 开始
    updateRings();
    for (int i = 0; i < RING_COUNT; i++)
    {
        targetPercent[i] = rings[i].percent;
        rings[i].percent = 0;
    }
    animating = true;
    animStartMs = millis();

    drawRings();
    canvas->pushSprite(0, 0);

    Serial.println("AboutApp is ready.");
}

void AboutApp::Update()
{
    // 确认键重播动画
    if (HAL::isEnter())
    {
        updateRings();
        for (int i = 0; i < RING_COUNT; i++)
        {
            targetPercent[i] = rings[i].percent;
            rings[i].percent = 0;
        }
        animating = true;
        animStartMs = millis();
        needsRefresh = false;
    }

    // 串口命令仍然保留
    if (Serial.available())
    {
        String s = Serial.readStringUntil('\n');
        s.trim();
        int colon = s.indexOf(':');
        if (colon > 0)
        {
            int idx = s.substring(0, colon).toInt();
            int pct = s.substring(colon + 1).toInt();
            if (idx >= 0 && idx < RING_COUNT)
            {
                rings[idx].percent = pct;
                drawRings();
                canvas->pushSprite(0, 0);
                Serial.printf("Ring %d -> %d%%\n", idx, pct);
            }
        }
    }

    // 动画逻辑
    if (animating)
    {
        uint32_t elapsed = millis() - animStartMs;
        float t = (float)elapsed / (float)ANIM_DURATION_MS;
        if (t > 1.0f)
            t = 1.0f;

        // easeOutCubic
        float ease = 1.0f - powf(1.0f - t, 3.0f);

        bool done = true;
        for (int i = 0; i < RING_COUNT; i++)
        {
            int cur = (int)(targetPercent[i] * ease);
            cur = constrain(cur, 0, 100);
            rings[i].percent = (uint8_t)cur;
            if (cur < targetPercent[i])
                done = false;
        }

        drawRings();
        canvas->pushSprite(0, 0);

        if (t >= 1.0f && done)
        {
            animating = false;
            Serial.println("Animation done.");
        }
    }
}

void AboutApp::Exit()
{
    temp_sensor_stop();
}

void AboutApp::Main()
{
    Init();
    while (1)
    {
        Update();
        if (HAL::isReturn())
            break;
        delay(10);
    }
    Exit();
}
