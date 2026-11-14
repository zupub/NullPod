#include "CircularDial.h"
#include "common.h"
#include "assets/fonts/font.h"
#include "assets/fonts/sevenSeg.h"
#include <math.h>

// ============ 表盘布局参数 ============
static const int DIAL_R = 240;         // 刻度环半径
static const int DIAL_SX = -150;       // 圆心 X（在原版 240x536 画布中心偏左、位于屏幕外）
static const int DIAL_SY = 160;        // 圆心 Y
static const int TEXT_R = DIAL_R + 28; // 刻度数字所在半径
static const int DIAL_H = 536;         // 原版设计画布高度，用于字号/亮度的透视映射

// 三色 LED 亮度固定 30%，色相跟随刻度角度（angle 本身就是 0~359，直接用）
static const uint8_t LED_SAT = 100;
static const uint8_t LED_VAL = 30;

CircularDial g_circularDial;

// ============ 极坐标预计算 ============
void CircularDial::initGeometry()
{
    for (int i = 0; i < 360; i++)
    {
        float rad = (float)i * DEG_TO_RAD;
        float s = sinf(rad);
        float c = cosf(rad);

        x[i] = DIAL_SX + DIAL_R * c; // 外圈端点
        y[i] = DIAL_SY + DIAL_R * s;

        px[i] = DIAL_SX + (DIAL_R - 14) * c; // 次刻度
        py[i] = DIAL_SY + (DIAL_R - 14) * s;

        lx[i] = DIAL_SX + (DIAL_R - 40) * c; // 主刻度最长
        ly[i] = DIAL_SY + (DIAL_R - 40) * s;

        shx[i] = DIAL_SX + (DIAL_R - 26) * c; // 中刻度
        shy[i] = DIAL_SY + (DIAL_R - 26) * s;

        tx[i] = DIAL_SX + TEXT_R * c; // 刻度数字
        ty[i] = DIAL_SY + TEXT_R * s;
    }
}

void CircularDial::Init()
{
    initGeometry();

    // 灰度阶梯：50 ~ 250
    for (int i = 0; i < 101; i++)
        grays[i] = HAL::GetDisplay()->color565(50 + (i * 2), 50 + (i * 2), 50 + (i * 2));

    HAL::RGB_Init();
    draw();
}

void CircularDial::draw()
{
    LGFX_Sprite *g = HAL::GetCanvas();

    // 读数：角度 0~359 映射到 1000~0（显示时除以 10）
    value = map(angle, 0, 359, 1000, 0);

    g->setTextDatum(ML_DATUM);
    g->fillSprite(background);

    // ===== 刻度环：每 3° 一根，交替为主/中/次刻度 =====
    for (int i = 0; i < 120; i++)
    {
        a = angle + (i * 3);
        if (a > 359)
            a = (angle + (i * 3)) - 360;

        int pos = ty[a];

        if (i % 6 == 0 && pos > 0 && pos < DIAL_H)
        {
            // 主刻度 + 数字：越靠近水平中心线越亮、字号越大（原版的透视效果）
            if (pos < DIAL_SY)
            {
                chosenFont = map(pos, 0, DIAL_SY, 0, 59);
                chosenColor = map(pos, 0, DIAL_SY, 0, 100);
            }
            if (pos >= DIAL_SY)
            {
                chosenFont = map(pos, DIAL_SY, DIAL_H, 59, 0);
                chosenColor = map(pos, DIAL_SY, DIAL_H, 100, 0);
            }

            drawWedgeLine(g, x[a], y[a], lx[a], ly[a], 3, 3, grays[chosenColor], background);

            char buf[8];
            snprintf(buf, sizeof(buf), "%d", (i / 6) * 5);

            g->setFont(myfonts[chosenFont]);
            g->setTextColor(grays[chosenColor], background);
            g->drawString(buf, tx[a], ty[a]);
        }
        else if (i % 3 == 0)
        {
            // 中刻度
            drawWedgeLine(g, x[a], y[a], shx[a], shy[a], 2, 2, grays[chosenColor], background);
        }
        else
        {
            // 次刻度
            drawWedgeLine(g, x[a], y[a], px[a], py[a], 2, 2, grays[chosenColor], background);
        }
    }

    // ===== 标签与读数（右对齐到画布右边缘）=====
    g->setTextColor(grays[30], background);
    g->setFont(myfonts[4]);
    g->setTextDatum(MR_DATUM);
    g->drawString("ANGLE", 240, 50);
    g->drawString("VALUE", 240, 460);

    char buf[16];
    g->setTextColor(grays[50], background);
    g->setFont(&DSEG14_Classic_Regular_28);
    snprintf(buf, sizeof(buf), "%.2f", value / 10.0);
    g->drawString(buf, 240, 490);
    snprintf(buf, sizeof(buf), "%d", angle);
    g->drawString(buf, 240, 82);

    g->setTextColor(0x93E4, background);
    g->drawString("VINEX", 240, 16);

    g->setFont(myfonts[6]);
    snprintf(buf, sizeof(buf), "INCREMENT:%d", inc);
    g->drawString(buf, 240, 526);

    // ===== 固定指针（左侧红色楔形）=====
    drawWedgeLine(g, -20, DIAL_SY, 40, DIAL_SY, 8, 1, TFT_RED, background);

    // ===== 三色 LED：色相 = 刻度角度，亮度固定 30% =====
    HAL::RGB_SetHSV((uint16_t)angle, LED_SAT, LED_VAL);

    // 恢复默认文字对齐（左上），否则会改变后续 App 的文字布局
    g->setTextDatum(0);

    g->pushSprite(0, 0);
}

void CircularDial::Update()
{
    uint8_t key = 0;

    if (HAL::getEncoder(&key))
    {
        switch (key)
        {
        case BTN_ENC_CC:
            HAL::beep(700, 20);
            stepAngle(1); // 编码器细调，一格 1°
            break;

        case BTN_ENC_CW:
            HAL::beep(500, 20);
            stepAngle(-1);
            break;

        case BTN_UP_SP:
            // 按当前增量顺时针滚动
            HAL::beep(700, 20);
            stepAngle(inc);
            break;

        case BTN_DOWN_SP:
            // 按当前增量逆时针滚动
            HAL::beep(500, 20);
            stepAngle(-inc);
            break;

        case BTN_LEFT_SP:
        case BTN_RIGHT_SP:
            // 切换增量：1 / 2 / 4 / 8
            HAL::beep(900, 20);
            inc = (inc >= 8) ? 1 : inc * 2;
            draw();
            prev = angle;
            break;

        case BTN_OK_SP:
            // 表盘复位到 0°（angle 与 prev 不同时会在下面自动重绘）
            HAL::beep(1200, 30);
            angle = 0;
            break;

        default:
            break;
        }
    }

    if (angle != prev)
    {
        draw();
        prev = angle;
    }
}

void CircularDial::stepAngle(int delta)
{
    angle = (angle + delta) % 360;
    if (angle < 0)
        angle += 360; // C 的取模会得到负值，补一圈
}

void CircularDial::Exit()
{
    // 离开 App 关灯，避免在菜单里留一颗亮着的灯
    HAL::RGB_Off();

    // 双重保险：退出时再恢复一次默认文字对齐
    HAL::GetCanvas()->setTextDatum(0);
}

void CircularDial::Main()
{
    Init();

    while (1)
    {
        Update();

        if (HAL::isReturn())
            break;

        delay(5);
    }

    Exit();
}
