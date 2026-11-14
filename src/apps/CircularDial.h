#ifndef _CIRCULAR_DIAL_H_
#define _CIRCULAR_DIAL_H_

#include "hal/hal.h"
#include "hal/hal_config.h"

/**
 * @brief 圆形刻度仪表盘
 *
 * 编码器旋转时刻度环转动（细调 1°/格）；
 * 上下键按当前增量滚动表盘，左右键切换增量（1/2/4/8），OK 键把表盘复位到 0°。
 * 三色 LED 同步跟随：色相 = 当前角度（0~359），亮度固定 30%，退出时关灯。
 * 布局与原版保持一致（圆心 (-150,160)、半径 240、数字/读数右对齐到 x=240）。
 */
class CircularDial
{
public:
    void Main();

protected:
    void Init();
    void Update();
    void Exit();

private:
    void draw();
    void initGeometry();
    void stepAngle(int delta); // 按增量旋转表盘（自动 0~359 环绕）

    unsigned short grays[101] = {0};
    unsigned short background = TFT_BLACK;

    float x[360];   // 外圈端点（刻度起点，都在同一圈上）
    float y[360];
    float px[360];  // 次刻度终点
    float py[360];
    float lx[360];  // 主刻度终点
    float ly[360];
    float shx[360]; // 中刻度终点
    float shy[360];
    float tx[360];  // 刻度数字位置
    float ty[360];

    int angle = 0; // 当前角度 0~359
    int value = 0; // 角度换算的读数 0~1000（显示时除以 10）
    int inc = 1;   // 增量 1/2/4/8
    int a = 0;     // 当前绘制用的角度索引
    int prev = 0;  // 上一帧角度，用于判断是否需要重绘

    int chosenFont = 0;  // 当前刻度数字的字体索引（myfonts[]）
    int chosenColor = 0; // 当前刻度的灰度索引（grays[]）
};

extern CircularDial g_circularDial;

#endif
