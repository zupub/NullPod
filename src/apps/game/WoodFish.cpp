#include "WoodFish.h"
#include "common.h"

WoodFish g_woodfish;

// ==================== Flash 中的 PNG 资源 ====================
// 路径：src/assets/image/woodfish.png
// 编译后符号：/ 替换为 _，即 _binary_src_assets_image_woodfish_png_start
extern "C" const uint8_t _binary_src_assets_image_woodfish_png_start[];
extern "C" const uint8_t _binary_src_assets_image_woodfish_png_end[];

#define WOODFISH_PNG_DATA ((const uint8_t *)_binary_src_assets_image_woodfish_png_start)
#define WOODFISH_PNG_LEN ((size_t)(_binary_src_assets_image_woodfish_png_end - _binary_src_assets_image_woodfish_png_start))

// ==================== 工具函数 ====================

// 缓出：先快后慢
static float easeOut(float t)
{
    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;
    return 1.0f - (1.0f - t) * (1.0f - t);
}

// 565 半透明混合（fg 叠加到 bg 上，a 为 fg 的不透明度 0~255）
static uint16_t alpha565(uint16_t fg, uint16_t bg, uint8_t a)
{
    uint8_t r1 = (fg >> 11) & 0x1F, g1 = (fg >> 5) & 0x3F, b1 = fg & 0x1F;
    uint8_t r2 = (bg >> 11) & 0x1F, g2 = (bg >> 5) & 0x3F, b2 = bg & 0x1F;
    uint8_t r = (uint8_t)((r1 * a + r2 * (255 - a)) >> 8);
    uint8_t g = (uint8_t)((g1 * a + g2 * (255 - a)) >> 8);
    uint8_t b = (uint8_t)((b1 * a + b2 * (255 - a)) >> 8);
    return (uint16_t)((r << 11) | (g << 5) | b);
}

// ==================== PNG 加载（参考 Compass::pngRotatorInit） ====================
bool WoodFish::pngLoaderInit()
{
    size_t fileSize = WOODFISH_PNG_LEN;

    Serial.printf("[WoodFish] PNG size: %zu bytes\n", fileSize);
    if (fileSize == 0 || fileSize > WOODFISH_PNG_MAX_SIZE)
    {
        Serial.println("[WoodFish] PNG size invalid");
        return false;
    }

    // 校验 PNG 签名
    static const uint8_t pngSig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (memcmp(WOODFISH_PNG_DATA, pngSig, 8) != 0)
    {
        Serial.println("[WoodFish] PNG signature invalid");
        return false;
    }

    // 从 PNG IHDR 读取真实宽高（大端）
    uint32_t w = (WOODFISH_PNG_DATA[16] << 24) | (WOODFISH_PNG_DATA[17] << 16) |
                 (WOODFISH_PNG_DATA[18] << 8) | WOODFISH_PNG_DATA[19];
    uint32_t h = (WOODFISH_PNG_DATA[20] << 24) | (WOODFISH_PNG_DATA[21] << 16) |
                 (WOODFISH_PNG_DATA[22] << 8) | WOODFISH_PNG_DATA[23];

    Serial.printf("[WoodFish] image %lu x %lu\n", w, h);

    g_fishW = (int)w;
    g_fishH = (int)h;

    // ---- 木鱼精灵：解码一次，之后复用 ----
    if (g_fishSprite == nullptr)
    {
        g_fishSprite = new LGFX_Sprite(HAL::GetDisplay());
        g_fishSprite->setPsram(true);
        if (!g_fishSprite->createSprite(w, h))
        {
            Serial.println("[WoodFish] fish sprite OOM");
            delete g_fishSprite;
            g_fishSprite = nullptr;
            return false;
        }
    }
    g_fishSprite->fillSprite(TFT_TRANSPARENT);

    // 使用 LovyanGFX 内置 PNG 解码，无需 PNGdec
    bool ok = g_fishSprite->drawPng(WOODFISH_PNG_DATA, fileSize, 0, 0);
    if (!ok)
    {
        Serial.println("[WoodFish] PNG decode failed");
        return false;
    }

    // ---- 全屏动画缓冲：缩放时用，避免超出原图被裁剪 ----
    if (g_animSprite == nullptr)
    {
        g_animSprite = new LGFX_Sprite(HAL::GetDisplay());
        g_animSprite->setPsram(true);
        if (!g_animSprite->createSprite(SCREEN_WIDTH, SCREEN_HEIGHT))
        {
            Serial.println("[WoodFish] anim sprite OOM");
            return false;
        }
    }

    g_pngLoaded = true;
    Serial.println("[WoodFish] PNG loaded OK");
    return true;
}

// ==================== 敲击动作 ====================
void WoodFish::knock()
{
    uint32_t now = millis();
    g_merit++;
    g_today++;

    // 触发放大动画
    g_knocking = true;
    g_knockStart = now;

    // 触发飘字
    g_floating = true;
    g_floatStart = now;
    g_floatX = SCREEN_WIDTH / 2 + 50;

    // 飘字起点：木鱼顶部之上
    if (g_pngLoaded && g_fishH > 0)
    {
        g_floatY0 = SCREEN_HEIGHT / 2 - g_fishH / 2 - WOODFISH_FLOAT_UP_OFFSET;
    }
    else
    {
        g_floatY0 = SCREEN_HEIGHT / 2 - 60;
    }

    // 音效：整百高音，其余标准音
    HAL::beep(g_merit % 100 == 0 ? 1319 : 784, 40);

    // 节流保存：每 20 次写一次 NVS
    static long lastSaved = -1;
    if (g_merit - lastSaved >= WOODFISH_SAVE_THRESHOLD)
    {
        saveStats();
        lastSaved = g_merit;
    }

    Serial.printf("[WoodFish] merit=%ld today=%ld\n", g_merit, g_today);
}

// ==================== 绘制木鱼（缩放动画） ====================
void WoodFish::drawWoodFish()
{
    if (!g_pngLoaded)
    {
        // 兜底：图片加载失败时画一个木色圆，保证功能可用
        int cx = SCREEN_WIDTH / 2;
        int cy = SCREEN_HEIGHT / 2 - 20;
        canvas->fillCircle(cx, cy, 70, 0x98C0);
        canvas->drawCircle(cx, cy, 70, TFT_DARKGREY);
        return;
    }

    // 缩放系数：1.00 -> 0.88 -> 1.00（正弦单峰，先快后慢回落）
    float scale = 1.0f;
    if (g_knocking)
    {
        uint32_t t = millis() - g_knockStart;
        float p = (float)t / WOODFISH_KNOCK_MS;
        if (p >= 1.0f)
        {
            g_knocking = false;
            p = 1.0f;
        }
        scale = 1.0f - WOODFISH_SCALE_MIN * sinf(p * PI);
    }

    // 清空动画缓冲
    g_animSprite->fillSprite(TFT_TRANSPARENT);

    // 木鱼中心
    int cx = SCREEN_WIDTH / 2;
    int cy = SCREEN_HEIGHT / 2;

    // 以 (cx, cy) 为中心缩放绘制到动画缓冲
    g_fishSprite->pushRotateZoom(g_animSprite, cx, cy, 0, scale, scale);

    // 将动画缓冲推送到画布（透明色不绘制）
    g_animSprite->pushSprite(canvas, 0, 0, TFT_TRANSPARENT);
}

// ==================== 绘制飘字 ====================
void WoodFish::drawFloat()
{
    if (!g_floating)
        return;

    float p = (float)(millis() - g_floatStart) / WOODFISH_FLOAT_MS;
    if (p >= 1.0f)
    {
        g_floating = false;
        return;
    }

    // 上移 + 渐隐
    int y = g_floatY0 - (int)(WOODFISH_FLOAT_PX * easeOut(p));
    uint8_t a = (uint8_t)(255 * (1.0f - p));
    uint16_t color = alpha565(TFT_GOLD, TFT_BLACK, a);

    canvas->setTextColor(color, TFT_BLACK);
    canvas->setTextDatum(MC_DATUM);
    canvas->drawString("功德+1", g_floatX, y, &fonts::efontCN_16);
    canvas->setTextDatum(0);
}

// ==================== 绘制底部信息 ====================
void WoodFish::drawInfo()
{
    char buf[32];

    // 大字：功德总数
    snprintf(buf, sizeof(buf), "功德 %ld", g_merit);
    canvas->setTextColor(TFT_WHITE, TFT_BLACK);
    canvas->setTextDatum(MC_DATUM);
    canvas->drawString(buf, SCREEN_WIDTH / 2, SCREEN_HEIGHT - 36, &fonts::efontCN_24);
    canvas->setTextDatum(0);

    // 小字：今日
    snprintf(buf, sizeof(buf), "今日 %ld", g_today);
    canvas->setTextColor(TFT_DARKGREY, TFT_BLACK);
    canvas->setTextDatum(MC_DATUM);
    canvas->drawString(buf, SCREEN_WIDTH / 2, SCREEN_HEIGHT - 12, &fonts::efontCN_16);
    canvas->setTextDatum(0);
}

// ==================== 输入处理 ====================
void WoodFish::handleKey(uint8_t key)
{
    switch (key)
    {
    case BTN_ENC_CW: // 编码器右转 = 敲一下
    case BTN_ENC_CC: // 编码器左转 = 敲一下
        knock();
        break;
    case BTN_OK_SP: // OK 短按 = 敲一下
        handleShortPress();
        break;
    case BTN_OK_LP: // OK 长按 = 清零
        handleLongPress();
        break;
    default:
        break;
    }
}

void WoodFish::handleShortPress()
{
    knock();
}

void WoodFish::handleLongPress()
{
    g_merit = 0;
    g_today = 0;
    saveStats();
    HAL::beep(300, 80);
    Serial.println("[WoodFish] Reset");
    HAL::logMemory();
}

// ==================== 持久化 ====================
void WoodFish::loadStats()
{
    Preferences prefs;
    if (prefs.begin(WOODFISH_NVS_NS, true))
    {
        g_merit = (long)prefs.getInt(WOODFISH_NVS_MERIT, 0);
        g_today = (long)prefs.getInt(WOODFISH_NVS_TODAY, 0);
        g_lastDayKey = prefs.getUInt(WOODFISH_NVS_DATE, 0);
        prefs.end();
    }

    if (g_merit < 0)
        g_merit = 0;
    if (g_today < 0)
        g_today = 0;
}

void WoodFish::saveStats()
{
    Preferences prefs;
    if (prefs.begin(WOODFISH_NVS_NS, false))
    {
        prefs.putInt(WOODFISH_NVS_MERIT, (int32_t)g_merit);
        prefs.putInt(WOODFISH_NVS_TODAY, (int32_t)g_today);
        prefs.putUInt(WOODFISH_NVS_DATE, g_lastDayKey);
        prefs.end();
    }
}

// 简化跨天判定：开机后分钟数 / 1440。有 RTC 可换成 getLocalTime
void WoodFish::checkNewDay()
{
    uint32_t dayKey = (millis() / 60000UL) / 1440UL;
    if (g_lastDayKey == 0)
    {
        g_lastDayKey = dayKey;
    }
    else if (dayKey != g_lastDayKey)
    {
        g_lastDayKey = dayKey;
        g_today = 0;
        saveStats();
    }
}

// ==================== IMU 单向敲击检测 ====================
void WoodFish::handleImuKnock()
{
    // 更新 IMU 数据（内部已有 100Hz 节流）
    HAL::updateImuData();

    // 如果设备前后晃动对应的是 pitch，改成 HAL::imu_get_pitch()
    float roll = HAL::imu_get_roll();
    uint32_t now = millis();

    // 首次进入，记录基准
    if (g_imuLastMs == 0)
    {
        g_imuLastRoll = roll;
        g_imuLastMs = now;
        return;
    }

    float dt = (float)(now - g_imuLastMs) / 1000.0f;
    if (dt < 0.005f)
        return; // 帧间隔太短，跳过
    if (dt > 0.2f)
        dt = 0.2f; // 防止卡顿导致突变

    // 计算 roll 角速度（度/秒）
    float rate = (roll - g_imuLastRoll) / dt;
    g_imuLastRoll = roll;
    g_imuLastMs = now;

    // 按配置方向取“有效速率”
    float dirRate = rate * WOODFISH_IMU_KNOCK_DIR;

    if (g_imuArmed)
    {
        // 方向匹配的快速放下 → 触发敲击
        if (dirRate >= WOODFISH_IMU_RATE_DPS &&
            (now - g_imuLastKnockMs) >= WOODFISH_IMU_COOLDOWN_MS)
        {
            g_imuArmed = false;
            g_imuLastKnockMs = now;
            knock();
        }
    }
    else
    {
        // 已触发过，需要反向明显抬起（收回）才重新武装
        if (dirRate <= -WOODFISH_IMU_RESET_DPS)
        {
            g_imuArmed = true;
        }
    }
}

// ==================== 生命周期 ====================
void WoodFish::Init()
{
    canvas = HAL::GetCanvas();
    loadStats();
    checkNewDay();
    pngLoaderInit();

    HAL::updateImuData();
    g_imuLastRoll = HAL::imu_get_roll();
    g_imuLastMs = millis();
    g_imuArmed = true;

    HAL::logMemory();
    Serial.println("[WoodFish] Init done");
}

void WoodFish::Update()
{
    // 读取编码器/按键事件
    uint8_t key = 0;
    if (HAL::getEncoder(&key))
        handleKey(key);

    // 前后晃动设备敲击
    handleImuKnock();

    // 整帧绘制
    canvas->fillSprite(TFT_BLACK);
    drawWoodFish();
    drawFloat();
    drawInfo();
    canvas->pushSprite(0, 0);
}

void WoodFish::Exit()
{
    saveStats();

    if (g_fishSprite)
    {
        g_fishSprite->deleteSprite();
        delete g_fishSprite;
        g_fishSprite = nullptr;
    }

    if (g_animSprite)
    {
        g_animSprite->deleteSprite();
        delete g_animSprite;
        g_animSprite = nullptr;
    }

    g_pngLoaded = false;
    Serial.println("[WoodFish] Exit done");
    HAL::logMemory();
}

void WoodFish::Main()
{
    Init();
    while (1)
    {
        Update();
        if (HAL::isReturn())
            break;
    }
    Exit();
}
