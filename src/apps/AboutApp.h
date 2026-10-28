#ifndef _ABOUT_APP_H_
#define _ABOUT_APP_H_

#include "hal/hal.h"
#include "hal/hal_config.h"
#include "common.h"

// 每条环：外半径、环厚、颜色、当前百分比
struct RingDef
{
    int rOuter;
    int thickness;
    uint16_t color;  // RGB565
    uint8_t percent; // 0-100
    const char *label;
};

class AboutApp
{
public:
    void Main();

protected:
    void Init();
    void Update();
    void Exit();

private:
    //-------- 数据 --------
    LGFX *disp = nullptr;
    LGFX_Sprite *canvas = nullptr;

    static constexpr int CX = SCREEN_WIDTH / 2;
    static constexpr int CY = SCREEN_HEIGHT / 2;
    static constexpr int RING_COUNT = 3;

    // 色环定义：外圈 rOuter 大，内圈小；环之间留 2~4px 间隙
    RingDef rings[RING_COUNT] = {
        {104, 22, rgb565(248, 22, 85), 75, "RAM"},
        {78, 22, rgb565(162, 255, 3), 50, "PSRAM"},
        {52, 22, rgb565(0, 226, 245), 30, "Flash"},
    };

    uint16_t dimColors[RING_COUNT];
    float cpuTemp;

    // 动画
    uint8_t targetPercent[RING_COUNT] = {0};
    bool animating = false;
    bool needsRefresh = false; // 标记需要重新读取数据并重播
    uint32_t animStartMs = 0;
    static constexpr uint32_t ANIM_DURATION_MS = 1200;

    //-------- 绘制 --------
    void updateRings();
    void drawRings();
};

extern AboutApp g_aboutApp;

#endif
