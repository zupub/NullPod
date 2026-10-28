#ifndef _COMPASS_H_
#define _COMPASS_H_

#include "hal/hal.h"
#include "hal/hal_config.h"

// 顶部标题栏高度
#define COMPASS_TITLE_HEIGHT 28

// 指南针表盘参数
#define COMPASS_DIAL_SIZE 200 // PNG 底盘尺寸 200x200

// 旋转平滑参数
#define COMPASS_SMOOTH_FACTOR 0.15f // 平滑系数，越小越平滑

// 内存上限（PNG 文件最大 256KB）
#define COMPASS_PNG_MAX_SIZE 262144

// 磁力计数据刷新间隔 (ms)
#define COMPASS_MAG_REFRESH_MS 50

// 环境页刷新间隔（ms）
#define ENV_REFRESH_MS 1000

// ==================== 水平仪参数 ====================
#define LEVEL_CENTER_X (SCREEN_WIDTH / 2)
#define LEVEL_CENTER_Y 160       // 水平仪视觉中心（竖屏居中偏下）
#define LEVEL_MAX_RADIUS 85.0f   // 气泡中心可到达的最大半径（px）
#define LEVEL_BUBBLE_RADIUS 16   // 气泡半径（px）
#define LEVEL_DEAD_ZONE 1.5f     // 死区：倾角小于此值(度)视为水平，气泡严格居中
#define LEVEL_SAT_ANGLE 25.0f    // 倾角达到此值(度)时气泡到达边缘
#define LEVEL_SMOOTH_ALPHA 0.30f // 姿态角平滑系数（0~1）

// ==================== 页面定义 ====================
enum CompassPage
{
    PAGE_COMPASS = 0, // 页面 0：罗盘（表盘 + 方位角 + 校准灯）
    PAGE_ENV = 1,     // 页面 1：环境信息（气压 / 海拔 / 气温）
    PAGE_LEVEL = 2,   // 页面 2：气泡水平仪（十字准星 + 重力气泡）
    PAGE_COUNT = 3
};

class Compass
{
public:
    void Main();   // App 主入口
    void Init();   // 初始化
    void Update(); // 每帧更新
    void Exit();   // 清理

    // ---- 页面管理 ----
    CompassPage getPage() const { return g_page; }
    void setPage(CompassPage page); // 切换页面（含重绘标记）
    void togglePage();              // 在页面间切换（电源键调用）
    void tickPageData();
    void drawPage();

    // ---- 水平仪复位（切到该页时调用，避免气泡跳跃）----
    void LevelPage_Reset();

protected:
    LGFX_Sprite *canvas = nullptr;
    LGFX_Sprite *g_srcSprite = nullptr;    // 原始 PNG（不旋转）
    LGFX_Sprite *g_rotSprite = nullptr;    // 旋转后的输出
    LGFX_Sprite *g_statusSprite = nullptr; // 状态栏

    float g_angle = 0;           // 表盘旋转角度（度，顺时针为正，0 = 表盘N朝上）
    float g_heading = 0;         // 当前方位角（0=北，顺时针增加），来自磁力计
    float g_headingSmoothed = 0; // 指数平滑后的方位角
    bool g_pngLoaded = false;
    bool bAutoRotate = false;     // 无磁力计时的演示回退
    bool g_isCalibrating = false; // 跟踪校准进行中

    // 气压 / 海拔 / 气温
    float g_pressure = 1013.25f; // 大气压 hPa
    float g_altitude = 0.0f;     // 估算海拔 m
    float g_temp = 0.0f;         // 气温 °C
    bool g_envDirty = true;      // 环境数据更新后需要重绘

    // ==================== 页面 0：罗盘 ====================
    void refreshHeading();                         // 从 HAL::MAG_GetInfo 读取并平滑
    float shortestAngleDiff(float from, float to); // 处理 0/360 跳变

    void drawStatusLed();                    // 左上角校准状态指示灯
    void drawHeadingText();                  // 右上角方位角 + 方位汉字
    static const char *headingToCn(float h); // 方位 -> 汉字

    bool pngRotatorInit();
    void setAngle(float angleDeg);
    void pngRotatorUpdate();

    // ==================== 页面 1：环境信息 ====================
    void drawEnvPage();    // 绘制环境信息整页（气压/海拔/气温卡片）
    void refreshEnvData(); // 从 HAL 读取最新传感器数据（受 ENV_REFRESH_MS 节流）

    // ==================== 页面 2：气泡水平仪 ====================
    void drawLevelBubble(float rollDeg, float pitchDeg, float &bx, float &by, bool &isLevel); // 计算气泡偏移
    void drawLevelPage();                                                                     // 绘制整页
    void drawLevelStatus(const char *string, uint16_t color);

    // ==================== 交互 ====================
    void handleRotate(int dir);
    void handleShortPress();
    void handleLongPress();

    // ---- 水平仪专属交互（OK 键在 PAGE_LEVEL 时路由到此）----
    void resetLevelOffset(); // 以当前姿态设为"水平零位"
    void calibrateGyro();    // 校准陀螺仪
    void loadLevelOffset();  // 从 Flash 读取零位
    void saveLevelOffset();  // 将零位写入 Flash

private:
    CompassPage g_page = PAGE_COMPASS; // 当前页面
    bool g_pageChanged = true;         // 页面切换后强制整页重绘

    // 环境数据刷新节流
    uint32_t g_lastEnvRefresh = 0;

    // 水平仪：平滑后的姿态角（静态，跨帧保持）
    float g_smoothRoll = 0.0f;
    float g_smoothPitch = 0.0f;

    // 水平仪零位：短按后以"当前姿态"居中，等价于坐标系平移
    float g_levelRollOffset = 0.0f;
    float g_levelPitchOffset = 0.0f;
};

extern Compass g_compass;

#endif // _COMPASS_H_
