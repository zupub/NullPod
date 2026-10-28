#include "BreathDemo.h"
#include <Adafruit_NeoPixel.h>
#include <math.h>

// ============ 全局实例 ============
BreathDemo g_breathDemo;

// ============ 字符串 ============
const char *waveNames[] = {"Sine", "Triangle", "Pulse", "Noise"};
const char *editLabels[] = {"Freq Hz", "Sat %", "Min Bright %", "Hue", "Wave"};

// ============ LED ============
#define WS2812_PIN HAL_PIN_RGB
#define LED_COUNT 1
static Adafruit_NeoPixel led(LED_COUNT, WS2812_PIN, NEO_GRB + NEO_KHZ800);

// ============ Init ============
void BreathDemo::Init()
{
    breath_active = true;
    breathPhase = 0.0f;
    guideMode = false;
    guideInhale = true;
    currentEdit = EDIT_FREQ;
    accel_multiplier = 1;
    last_enc_event_time = 0;
    last_bd_draw = 0;
    last_frame = 0;

    // 默认参数
    params.frequency = 0.5f;
    params.saturation = 85;
    params.minBrightness = 5;
    params.hue = 180;
    params.waveType = WAVE_SINE;
    params.paused = false;

    // 初始化噪声 LUT
    for (int i = 0; i < 64; i++)
        noiseLUT[i] = (float)rand() / RAND_MAX;

    led.begin();
    led.setBrightness(255);
    led.show();

    Serial.println(">>> BreathDemo Init");
    Draw();
    canvas.pushSprite(0, 0);
}

// ============ Exit ============
void BreathDemo::Exit()
{
    breath_active = false;
    // 关灯
    led.setPixelColor(0, 0, 0, 0);
    led.show();
    Serial.println(">>> BreathDemo Exit");
}

// ============ Main（阻塞式生命周期）============
void BreathDemo::Main()
{
    Init();

    while (1)
    {
        Update();

        // GPIO17 返回键检测（持续检测，不会漏）
        if (HAL::isReturn())
        {
            Serial.println(">>> Return key: exit to menu");
            break;
        }

        // 检查是否请求菜单
        if (g_requestMenu)
        {
            g_requestMenu = false;
            break;
        }

        delay(5); // ~200Hz 轮询
    }

    Exit();
}

// ============ Update ============
void BreathDemo::Update()
{
    if (!breath_active)
        return;

    unsigned long now = millis();
    float dt = (last_frame == 0) ? 0.016f : (now - last_frame) / 1000.0f;
    if (dt > 0.1f)
        dt = 0.016f; // clamp 防溢出
    last_frame = now;

    // ===== 编码器事件 =====
    uint8_t key = 0;
    bool touched = HAL::getEncoder(&key);

    if (touched)
    {
        switch (key)
        {
        case BTN_ENC_CW:
            handleRotate(+1);
            break;
        case BTN_ENC_CC:
            handleRotate(-1);
            break;
        case BTN_OK_SP:
            handleShortPress();
            break;
        case BTN_OK_LP:
            handleLongPress();
            break;
        default:
            Serial.printf(">>> Unknown key: 0x%02X\n", key);
            break;
        }
    }

    // ===== 更新呼吸相位 =====
    if (guideMode)
    {
        unsigned long elapsed = millis() - guidePhaseStart;
        float period = 10000.0f; // 10s 一个周期
        float t = fmodf(elapsed / period, 1.0f);

        if (t < 0.4f)
        {
            guideInhale = true;
            float progress = t / 0.4f;
            breathPhase = progress * PI;
        }
        else
        {
            guideInhale = false;
            float progress = (t - 0.4f) / 0.6f;
            breathPhase = PI + progress * PI;
        }
    }
    else if (!params.paused)
    {
        breathPhase += TWO_PI * params.frequency * dt;
        if (breathPhase > TWO_PI)
            breathPhase -= TWO_PI;
    }

    // ===== 计算亮度（与屏幕中间原点对齐，偏移 0.375 个周期）=====
    float ledPhase = breathPhase + TWO_PI * 0.75f * 0.5f;
    if (ledPhase > TWO_PI) ledPhase -= TWO_PI;
    float waveVal = getWaveValue(ledPhase, params.waveType);
    float minB = params.minBrightness / 100.0f;
    float bright01 = minB + waveVal * (1.0f - minB);

    // HSV → RGB565 → 提取分量 → 按亮度缩放
    uint16_t hue16 = guideMode ? 200 : params.hue;
    uint16_t rgb = hsv2rgb565(hue16, params.saturation, (uint8_t)(bright01 * 100));

    uint8_t r5 = (rgb >> 11) & 0x1F;
    uint8_t g6 = (rgb >> 5) & 0x3F;
    uint8_t b5 = rgb & 0x1F;

    uint8_t br = (uint8_t)(bright01 * 255);
    uint8_t rr = (r5 * br) / 31;
    uint8_t gg = (g6 * br) / 63;
    uint8_t bb = (b5 * br) / 31;

    led.setPixelColor(0, rr, gg, bb);
    led.show();

    currentBrightness = br;

    // ===== 屏幕重绘 ~30Hz =====
    if (now - last_bd_draw > 33)
    {
        Draw();
        canvas.pushSprite(0, 0);
        last_bd_draw = now;
    }
}

// ============ 编码器处理 ============
void BreathDemo::handleRotate(int dir)
{
    unsigned long now = millis();

    if (now - last_enc_event_time < 50)
        accel_multiplier = 2;
    else if (now - last_enc_event_time > 150)
        accel_multiplier = 1;
    last_enc_event_time = now;

    int delta = dir * accel_multiplier;

    if (guideMode)
        return; // 引导模式不接受调参

    switch (currentEdit)
    {
    case EDIT_FREQ:
        params.frequency += delta * 0.05f;
        if (params.frequency < 0.05f)
            params.frequency = 0.05f;
        if (params.frequency > 3.0f)
            params.frequency = 3.0f;
        break;

    case EDIT_SAT:
        params.saturation += delta * 2;
        if (params.saturation > 100)
            params.saturation = 100;
        if (params.saturation < 0)
            params.saturation = 0;
        break;

    case EDIT_MIN_BRIGHT:
        params.minBrightness += delta * 2;
        if (params.minBrightness > 50)
            params.minBrightness = 50;
        if (params.minBrightness < 0)
            params.minBrightness = 0;
        break;

    case EDIT_HUE:
        params.hue += delta * 5;
        if (params.hue > 359)
            params.hue -= 360;
        if (params.hue < 0)
            params.hue += 360;
        break;

    case EDIT_WAVE:
        int w = (int)params.waveType + delta;
        if (w < 0)
            w = WAVE_COUNT - 1;
        if (w >= WAVE_COUNT)
            w = 0;
        params.waveType = (WaveType)w;
        break;
    }
}

void BreathDemo::handleShortPress()
{
    if (guideMode)
        return;
    currentEdit++;
    if (currentEdit >= EDIT_COUNT)
        currentEdit = 0;
    Serial.printf(">>> Edit: %s\n", editLabels[currentEdit]);
}

void BreathDemo::handleLongPress()
{
    guideMode = !guideMode;
    guidePhaseStart = millis();
    guideInhale = true;
    if (guideMode)
    {
        params.frequency = 0.1f; // 4s吸 + 6s呼 ≈ 0.1Hz
        Serial.println(">>> Guide mode ON");
    }
    else
    {
        Serial.println(">>> Guide mode OFF");
    }
}

// ============ Draw 分发 ============
void BreathDemo::Draw()
{
    canvas.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, MENU_BG_COLOR);
    BD_DrawStatusBar();
    BD_DrawWaveform();
    BD_DrawParamArea();
    BD_DrawHintBar();
}

// ============ 状态栏 ============
void BreathDemo::BD_DrawStatusBar()
{
    canvas.fillRect(0, 0, SCREEN_WIDTH, BD_STATUS_H, MENU_BG_COLOR);

    canvas.setTextColor(TFT_WHITE);
    canvas.setTextSize(1);
    canvas.setFont(&FreeSans9pt7b);

    canvas.setCursor(8, 20);
    if (guideMode)
    {
        canvas.setTextColor(rgb565(0, 200, 255));
        canvas.print("Guide Mode");
    }
    else
    {
        canvas.setTextColor(TFT_WHITE);
        canvas.print("Breath Demo");
    }

    // LED 实时颜色预览
    uint16_t dotColor = hsv2rgb565(
        guideMode ? 200 : params.hue,
        params.saturation,
        map(currentBrightness, 0, 255, 0, 100));
    canvas.fillCircle(SCREEN_WIDTH - 20, 14,
                      (currentBrightness > 128) ? 6 : 4, dotColor);

    if (params.paused && !guideMode)
    {
        canvas.setCursor(SCREEN_WIDTH - 60, 20);
        canvas.setTextColor(MENU_HINT_COLOR);
        canvas.print("PAUSE");
    }
}

// ============ 波形区 ============
void BreathDemo::BD_DrawWaveform()
{
    canvas.fillRect(BD_WAVE_X, BD_WAVE_Y, BD_WAVE_W, BD_WAVE_H, MENU_BG_COLOR);
    canvas.drawRect(BD_WAVE_X, BD_WAVE_Y, BD_WAVE_W, BD_WAVE_H, MENU_BORDER_COLOR);

    int cx = BD_WAVE_X + BD_WAVE_W / 2;
    int cy = BD_WAVE_Y + BD_WAVE_H / 2;
    int halfH = BD_WAVE_H / 2 - 6;
    int points = BD_WAVE_W - 4;

    // 网格
    canvas.drawFastHLine(BD_WAVE_X + 2, cy, BD_WAVE_W - 4, rgb565(40, 40, 74));
    canvas.drawFastHLine(BD_WAVE_X + 2, cy - halfH / 2, BD_WAVE_W - 4, rgb565(26, 26, 58));
    canvas.drawFastHLine(BD_WAVE_X + 2, cy + halfH / 2, BD_WAVE_W - 4, rgb565(26, 26, 58));

    // 画波形
    uint16_t waveColor = hsv2rgb565(params.hue, params.saturation, 80);
    // 屏幕中间对应 t=0.5，与 LED 使用相同的相位偏移
    float midPh = breathPhase + TWO_PI * 0.75f * 0.5f;
    if (midPh > TWO_PI) midPh -= TWO_PI;
    float currentVal = getWaveValue(midPh, params.waveType);

    int prevY = cy;
    for (int i = 0; i < points; i++)
    {
        float t = (float)i / points;
        float ph = breathPhase + t * TWO_PI * 0.75f;
        if (ph > TWO_PI)
            ph -= TWO_PI;

        float v = getWaveValue(ph, params.waveType);
        int y = cy - (int)((v - 0.5f) * 2 * halfH);

        if (i > 0)
        {
            canvas.drawLine(
                BD_WAVE_X + 2 + i - 1, prevY,
                BD_WAVE_X + 2 + i, y,
                waveColor);
        }
        prevY = y;
    }

    // 中心线 + 当前值指示
    canvas.drawFastVLine(cx, BD_WAVE_Y + 3, BD_WAVE_H - 6, TFT_WHITE);
    int curY = cy - (int)((currentVal - 0.5f) * 2 * halfH);
    canvas.fillCircle(cx, curY, 4, waveColor);
    canvas.fillCircle(cx, curY, 2, TFT_WHITE);

    // 引导模式文字
    if (guideMode)
    {
        canvas.setTextColor(guideInhale ? rgb565(0, 220, 100) : rgb565(255, 120, 0));
        canvas.setTextSize(1);
        canvas.setFont(&FreeSansBold12pt7b);
        const char *text = guideInhale ? "Inhale..." : "Exhale...";
        int16_t textW = canvas.textWidth(text);
        canvas.setCursor(cx - textW / 2, BD_WAVE_Y + BD_WAVE_H + 18);
        canvas.print(text);
    }
}

// ============ 参数区 ============
void BreathDemo::BD_DrawParamArea()
{
    int yBase = BD_PARAM_Y;
    int rowH = 20;

    canvas.fillRect(0, yBase, SCREEN_WIDTH, BD_PARAM_H, MENU_STATUS_COLOR);
    canvas.setTextSize(1);
    canvas.setFont(&FreeSans9pt7b);

    for (int i = 0; i < EDIT_COUNT; i++)
    {
        int row = i / 2;
        int col = i % 2;
        int x = 10 + col * 120;
        int y = yBase + 6 + row * rowH;

        bool active = (i == currentEdit) && !guideMode;

        if (active)
        {
            canvas.setTextColor(rgb565(0, 255, 255));
            canvas.setCursor(x, y);
            canvas.print("> ");
            canvas.setCursor(x + 12, y);
        }
        else
        {
            canvas.setTextColor(MENU_HINT_COLOR);
            canvas.setCursor(x, y);
        }

        switch (i)
        {
        case EDIT_FREQ:
            canvas.printf("Freq %.2fHz", params.frequency);
            break;
        case EDIT_SAT:
            canvas.printf("Sat %d%%", params.saturation);
            break;
        case EDIT_MIN_BRIGHT:
            canvas.printf("Min %d%%", params.minBrightness);
            break;
        case EDIT_HUE:
            canvas.printf("Hue %d", params.hue);
            canvas.fillRect(x + 90, y - 10, 20, 14, hsv2rgb565(params.hue, 80, 80));
            break;
        case EDIT_WAVE:
            canvas.printf("Wave %s", waveNames[params.waveType]);
            break;
        }
    }

    if (params.paused && !guideMode)
    {
        canvas.fillRect(0, yBase + BD_PARAM_H - 4, SCREEN_WIDTH, 4, MENU_HINT_COLOR);
    }
}

// ============ 提示栏 ============
void BreathDemo::BD_DrawHintBar()
{
    canvas.fillRect(0, BD_HINT_Y, SCREEN_WIDTH, BD_HINT_H, MENU_STATUS_COLOR);
    canvas.setTextColor(MENU_HINT_COLOR);
    canvas.setTextSize(1);
    canvas.setFont(&FreeSans9pt7b);

    canvas.setCursor(15, BD_HINT_Y + 20);
    if (guideMode)
        canvas.print("SP:Exit  LP:Normal  GPIO17:Back");
    else
        canvas.print("CW/CC:Adj  SP:Next  LP:Guide  GPIO17:Back");
}

// ============ 波形函数 ============
float BreathDemo::getWaveValue(float ph, WaveType type)
{
    float v;
    switch (type)
    {
    case WAVE_SINE:
        v = (sinf(ph) + 1.0f) * 0.5f;
        break;

    case WAVE_TRIANGLE:
    {
        float norm = ph / TWO_PI;
        if (norm < 0.25f)
            v = norm * 4.0f;
        else if (norm < 0.75f)
            v = 1.0f - (norm - 0.25f) * 2.0f;
        else
            v = (norm - 0.75f) * 4.0f;
        break;
    }

    case WAVE_PULSE:
    {
        float norm = ph / TWO_PI;
        if (norm < 0.3f)
        {
            v = norm / 0.3f;
            v = v * v;
        }
        else
        {
            float decay = (norm - 0.3f) / 0.7f;
            v = 1.0f - decay;
            v = v * v * v;
        }
        break;
    }

    case WAVE_NOISE:
    {
        int idx = (int)(ph * 10.0f) % 64;
        if (idx < 0)
            idx += 64;
        float target = noiseLUT[idx];
        static float lastNoise = 0;
        lastNoise = lastNoise * 0.7f + target * 0.3f;
        v = lastNoise;
        break;
    }

    default:
        v = 0.5f;
    }
    return constrain(v, 0.0f, 1.0f);
}

// ============ HSV → RGB565 ============
uint16_t BreathDemo::hsv2rgb565(uint16_t h, uint8_t s, uint8_t v)
{
    float H = h % 360;
    float S = s / 100.0f;
    float V = v / 100.0f;

    float C = V * S;
    float X = C * (1 - fabsf(fmodf(H / 60.0f, 2) - 1));
    float m = V - C;

    float r, g, b;
    if (H < 60)
    {
        r = C;
        g = X;
        b = 0;
    }
    else if (H < 120)
    {
        r = X;
        g = C;
        b = 0;
    }
    else if (H < 180)
    {
        r = 0;
        g = C;
        b = X;
    }
    else if (H < 240)
    {
        r = 0;
        g = X;
        b = C;
    }
    else if (H < 300)
    {
        r = X;
        g = 0;
        b = C;
    }
    else
    {
        r = C;
        g = 0;
        b = X;
    }

    uint8_t R = (uint8_t)((r + m) * 31);
    uint8_t G = (uint8_t)((g + m) * 63);
    uint8_t B = (uint8_t)((b + m) * 31);

    return (R << 11) | (G << 5) | B;
}

// ============ LED 控制 ============
void BreathDemo::SetLED(uint8_t r, uint8_t g, uint8_t b)
{
    led.setPixelColor(0, r, g, b);
    led.show();
}
