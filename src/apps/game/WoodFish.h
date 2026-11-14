#ifndef _WOODFISH_H_
#define _WOODFISH_H_

#include "hal/hal.h"
#include "hal/hal_config.h"

// ==================== 动画参数 ====================
#define WOODFISH_KNOCK_MS 220        // 木鱼放大动画时长 (ms)
#define WOODFISH_FLOAT_MS 800        // "功德+1"飘字时长 (ms)
#define WOODFISH_SCALE_MIN 0.12f     // 最大缩小比例（1.0 - 0.12 = 0.88）
#define WOODFISH_FLOAT_PX 66         // 飘字上升总距离 (px)
#define WOODFISH_SAVE_THRESHOLD 20   // 每敲击 20 次写一次 NVS
#define WOODFISH_PNG_MAX_SIZE 262144 // PNG 文件上限 256KB
#define WOODFISH_FLOAT_UP_OFFSET -5  // 飘字相对木鱼顶部向上偏移（px）

// ==================== IMU 晃动敲击参数 ====================
#define WOODFISH_IMU_KNOCK_DIR -1    // 1=roll增大触发，-1=roll减小触发
#define WOODFISH_IMU_RATE_DPS 140.0f // 触发角速度阈值（度/秒）
#define WOODFISH_IMU_RESET_DPS 40.0f // 反向复位角速度阈值（度/秒）
#define WOODFISH_IMU_COOLDOWN_MS 220 // 触发冷却（ms）

// ==================== NVS ====================
#define WOODFISH_NVS_NS "woodfish"
#define WOODFISH_NVS_MERIT "merit"
#define WOODFISH_NVS_TODAY "today"
#define WOODFISH_NVS_DATE "date"

class WoodFish
{
public:
    // ---- App 生命周期（与 Compass 同构）----
    void Main();   // App 主入口
    void Init();   // 初始化
    void Update(); // 每帧更新
    void Exit();   // 清理

    // ---- 对外只读接口 ----
    long getMerit() const { return g_merit; }
    long getToday() const { return g_today; }

protected:
    // ---- 资源精灵 ----
    LGFX_Sprite *canvas = nullptr;       // 主画布，由 HAL 提供
    LGFX_Sprite *g_fishSprite = nullptr; // 木鱼 PNG 解码缓存
    LGFX_Sprite *g_animSprite = nullptr; // 全屏动画缓冲（解决放大裁剪）

    bool g_pngLoaded = false;
    int g_fishW = 0;
    int g_fishH = 0;

    // ---- 数据 ----
    long g_merit = 0;
    long g_today = 0;
    uint32_t g_lastDayKey = 0;

    // ---- 动画状态 ----
    bool g_knocking = false;
    uint32_t g_knockStart = 0;
    bool g_floating = false;
    uint32_t g_floatStart = 0;
    int g_floatX = 0;
    int g_floatY0 = 0;

    // ---- IMU 晃动状态 ----
    float g_imuLastRoll = 0.0f;
    uint32_t g_imuLastMs = 0;
    bool g_imuArmed = true; // true=允许触发，false=等待反向复位
    uint32_t g_imuLastKnockMs = 0;

    // ---- 内部方法 ----
    bool pngLoaderInit(); // 从 Flash 加载 PNG
    void drawWoodFish();  // 绘制木鱼（含放大动画）
    void drawFloat();     // 绘制飘字
    void drawInfo();      // 绘制底部信息

    void knock(); // 敲击动作
    void handleKey(uint8_t key);
    void handleShortPress();
    void handleLongPress();
    void handleImuKnock(); // IMU 晃动检测

    void loadStats();   // 从 NVS 读取
    void saveStats();   // 写入 NVS
    void checkNewDay(); // 跨天清零今日
};

extern WoodFish g_woodfish;

#endif // _WOODFISH_H_
