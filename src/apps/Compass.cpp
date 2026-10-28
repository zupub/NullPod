#include "Compass.h"
#include "common.h"

Compass g_compass;

const lgfx::IFont *font_small = &fonts::efontCN_16;
const lgfx::IFont *font_medium = &fonts::efontCN_24;
const lgfx::IFont *font_large = &fonts::efontCN_24_b;

extern "C" const uint8_t _binary_src_assets_image_compass_dial_png_start[];
extern "C" const uint8_t _binary_src_assets_image_compass_dial_png_end[];

#define COMPASS_PNG_DATA ((const uint8_t *)_binary_src_assets_image_compass_dial_png_start)
#define COMPASS_PNG_LEN ((size_t)(_binary_src_assets_image_compass_dial_png_end - _binary_src_assets_image_compass_dial_png_start))

// ==================== 页面管理 ====================
void Compass::setPage(CompassPage page)
{
    if (page != g_page)
    {
        g_page = page;
        g_pageChanged = true; // 标记：下一帧整页重绘

        // 进入水平仪页时初始化平滑状态
        if (page == PAGE_LEVEL)
        {
            LevelPage_Reset();
        }
    }
}

void Compass::togglePage()
{
    CompassPage next = (CompassPage)((g_page + 1) % PAGE_COUNT); // 0→1→2→0
    setPage(next);
    Serial.printf("[Compass] 切换到页面 %d\n", (int)next);
}

// ========== 初始化：从 Flash 解码 PNG 到 srcSprite ==========
bool Compass::pngRotatorInit()
{
    const uint8_t *pngData = COMPASS_PNG_DATA;
    size_t fileSize = COMPASS_PNG_LEN;

    Serial.printf("[PNG] 从 Flash 加载: %zu 字节\n", fileSize);

    // 尺寸校验
    if (fileSize == 0 || fileSize > COMPASS_PNG_MAX_SIZE)
    {
        Serial.printf("[PNG] 尺寸非法: %zu (max=%d)\n", fileSize, COMPASS_PNG_MAX_SIZE);
        return false;
    }

    // 校验 PNG 文件签名（前 8 字节）
    static const uint8_t pngSig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (memcmp(pngData, pngSig, 8) != 0)
    {
        Serial.println("[PNG] 文件签名无效，不是有效的 PNG 文件");
        return false;
    }

    // 创建两个 200x200 sprite
    const int SIZE = COMPASS_DIAL_SIZE;

    if (g_srcSprite == nullptr)
    {
        g_srcSprite = new LGFX_Sprite(HAL::GetDisplay());
        g_srcSprite->setPsram(true); // 使用 PSRAM 创建 sprite
        bool created = g_srcSprite->createSprite(SIZE, SIZE);
        Serial.printf("g_srcSprite createSprite: %s\n", created ? "OK" : "FAILED");
    }

    if (g_rotSprite == nullptr)
    {
        g_rotSprite = new LGFX_Sprite(HAL::GetDisplay());
        g_rotSprite->setPsram(true); // 使用 PSRAM 创建 sprite
        bool created = g_rotSprite->createSprite(SIZE, SIZE);
        Serial.printf("g_rotSprite createSprite: %s\n", created ? "OK" : "FAILED");
    }

    // 解码 PNG 到 srcSprite（只做一次）
    // Flash 数据在 0x3Fxxxxxx（DRAM），drawPng 可直接读取，无需 memcpy
    g_srcSprite->fillSprite(TFT_TRANSPARENT);
    bool ok = g_srcSprite->drawPng(pngData, fileSize, 0, 0);

    if (!ok)
    {
        Serial.println("[PNG] decode failed");
        return false;
    }

    g_pngLoaded = true;
    Serial.println("[PNG] 从 Flash 解码成功，rotator initialized OK");
    return true;
}

// ========== 页面 0：绘制罗盘整页 ==========
void Compass::pngRotatorUpdate()
{
    if (!g_pngLoaded)
        return;

    const int SIZE = COMPASS_DIAL_SIZE;
    int cx = SIZE / 2;
    int cy = SIZE / 2;

    // 1. 清空画布
    canvas->fillSprite(TFT_BLACK);

    // 2. 清空 rotSprite 为透明（关键！否则残留叠加）
    g_rotSprite->fillSprite(TFT_TRANSPARENT);

    // 3. 核心：表盘旋转 = 方位角的反向
    //    heading = 0 时表盘不转（N 朝上）；heading 增大时，表盘反向旋转使 N 始终朝上，
    //    而固定在表盘上的指针随之正确指向 heading 方向。
    //    pushRotateZoom 角度为正 = 顺时针；heading 顺时针增加，故取负值。
    float dialAngle = -g_headingSmoothed; // 度

    g_srcSprite->pushRotateZoom(
        g_rotSprite,
        cx, cy,    // 旋转中心 = 图片中心
        dialAngle, // 表盘旋转角度
        1.0f, 1.0f // 不缩放
    );

    // 4. 把旋转结果推到画布中央（透明色不绘制）
    int dstX = (240 - SIZE) / 2; // 屏幕宽 240，居中
    int dstY = 50;               // 偏下，留出顶部标题栏
    g_rotSprite->pushSprite(canvas, dstX, dstY, TFT_TRANSPARENT);

    // 5. 左上角校准状态指示灯 / 右上角方位角 + 方位汉字 / 底部气压与海拔
    drawStatusLed();
    drawHeadingText();

    // 6. 推到屏幕
    canvas->pushSprite(0, 0);
    g_pageChanged = false;
}

// ========== 角度归一化 ==========
static inline float norm360(float a)
{
    while (a >= 360.0f)
        a -= 360.0f;
    while (a < 0.0f)
        a += 360.0f;
    return a;
}

// ========== 左上角校准状态指示灯 ==========
#define CAL_LED_R 6
#define CAL_LED_X 28
#define CAL_LED_Y 28

void Compass::drawStatusLed()
{
    static uint32_t lastToggle = 0;
    static bool blinkOn = true;

    // 每 250ms 切换一次闪烁状态
    if (millis() - lastToggle > 250)
    {
        lastToggle = millis();
        blinkOn = !blinkOn;
    }

    MAG_Info_t info;
    HAL::MAG_GetInfo(&info);

    uint16_t color;
    if (g_isCalibrating)
    {
        color = blinkOn ? TFT_YELLOW : TFT_BLACK; // 校准中：黄闪
    }
    else
    {
        color = info.isCalibrated ? TFT_GREEN : TFT_DARKGREY; // 绿常亮 / 暗灰
    }

    canvas->fillCircle(CAL_LED_X, CAL_LED_Y, CAL_LED_R, color);
    // 外圈描边
    canvas->drawCircle(CAL_LED_X, CAL_LED_Y, CAL_LED_R, TFT_DARKGREY);
}

// ========== 方位角 -> 汉字 ==========
const char *Compass::headingToCn(float h)
{
    static const char *const kCompassNames[] = {"北", "东北", "东", "东南", "南", "西南", "西", "西北"};
    // 以 8 方位划分，每 45° 一档，北为 [337.5, 22.5]
    float a = norm360(h) + 22.5f;
    if (a >= 360.0f)
        a -= 360.0f;

    int idx = (int)(a / 45.0f); // 0=N,1=NE,2=E,...
    return kCompassNames[idx];
}

// ========== 右上角：方位角 + 方位汉字 ==========
#define HEADING_TEXT_X (SCREEN_WIDTH - 18) // 右对齐基准
#define HEADING_TEXT_Y 25

void Compass::drawHeadingText()
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%03.0f° %s", g_headingSmoothed, headingToCn(g_headingSmoothed));
    canvas->setTextColor(MENU_TITLE_COLOR);
    canvas->setTextDatum(MR_DATUM); // 右对齐
    canvas->drawString(buf, HEADING_TEXT_X, HEADING_TEXT_Y, font_small);
    canvas->setTextDatum(0);
}

float Compass::shortestAngleDiff(float from, float to)
{
    float diff = to - from;
    while (diff > 180.0f)
        diff -= 360.0f;
    while (diff <= -180.0f)
        diff += 360.0f;
    return diff;
}

// ========== 从磁力计刷新方位角 ==========
void Compass::refreshHeading()
{
    static uint32_t lastMag = 0;
    if (millis() - lastMag <= COMPASS_MAG_REFRESH_MS)
        return;
    lastMag = millis();

    MAG_Info_t info;
    HAL::MAG_GetInfo(&info);

    if (g_isCalibrating && info.isCalibrated)
    {
        g_isCalibrating = false;
    }

    // 用校准后的 X/Y 计算方位角（已做硬铁/软铁校正）
    // atan2(Y, X)：X 朝"前进方向"，Y 朝左。按你的安装调整 X/Y 映射。
    float heading = atan2f((float)info.y, (float)info.x) * 180.0f / (float)M_PI;
    if (heading < 0)
        heading += 360.0f;

    // 此处可叠加磁偏角（如XX城市 +7°），按需启用：
    heading = norm360(heading - 7.0f);

    g_heading = heading;

    // 指数平滑（处理 0/360 跳变）
    if (g_headingSmoothed == 0 && !info.isCalibrated)
    {
        // 首次或未校准：直接跟随
        g_headingSmoothed = g_heading;
    }
    else
    {
        float diff = shortestAngleDiff(g_headingSmoothed, g_heading);
        g_headingSmoothed = norm360(g_headingSmoothed + diff * COMPASS_SMOOTH_FACTOR);
    }

    // 同步表盘角度（表盘 = -heading，使 N 朝上）
    setAngle(-g_headingSmoothed);
}

// ========== 设置角度（归一化）==========
void Compass::setAngle(float angleDeg)
{
    g_angle = norm360(angleDeg);
}

// ========== 页面 1：绘制环境信息整页 ==========
void Compass::drawEnvPage()
{
    canvas->fillSprite(TFT_BLACK);

    // 顶部标题
    canvas->setTextColor(MENU_TITLE_COLOR);
    canvas->setTextDatum(MC_DATUM);
    canvas->drawString("环境信息", SCREEN_WIDTH / 2, 20, font_small);
    canvas->drawFastHLine(20, 40, SCREEN_WIDTH - 40, TFT_DARKGREY);
    canvas->setTextDatum(0); // 恢复默认

    // 三张"卡片"：气压 / 海拔 / 气温（垂直排列）
    const int cardX = 20;                // 卡片左边界
    const int cardW = SCREEN_WIDTH - 40; // 卡片宽度
    const int cardH = 60;                // 卡片高度
    const int cardGap = 10;              // 卡片间距
    const int startY = 56;               // 第一张卡片顶部 Y

    auto drawCard = [&](int idx, const char *label, const char *value, uint16_t accent)
    {
        int y = startY + idx * (cardH + cardGap);
        // 卡片背景 + 边框
        canvas->fillRoundRect(cardX, y, cardW, cardH, 8, MENU_STATUS_COLOR);
        canvas->drawRoundRect(cardX, y, cardW, cardH, 8, accent);

        // 左侧标签
        canvas->setTextColor(TFT_LIGHTGREY);
        canvas->setTextDatum(ML_DATUM);
        canvas->drawString(label, cardX + 12, y + 20, font_small);
        canvas->setTextDatum(0);

        // 右侧数值（右对齐）
        canvas->setTextColor(accent);
        canvas->setTextDatum(MR_DATUM);
        canvas->drawString(value, cardX + cardW - 16, y + cardH / 2 + 10, font_large);
        canvas->setTextDatum(0);
    };

    // 刷新传感器数据（节流在 1Hz）
    refreshEnvData();

    // 组装字符串
    char bufP[32], bufA[32], bufT[32];
    snprintf(bufP, sizeof(bufP), "%.1f hPa", g_pressure);
    snprintf(bufA, sizeof(bufA), "%.1f m", g_altitude);
    snprintf(bufT, sizeof(bufT), "%.1f %cC", g_temp, 0xB0); // 0xB0 = '°'

    drawCard(0, "气压 Pressure", bufP, TFT_CYAN);
    drawCard(1, "海拔 Altitude", bufA, TFT_GREEN);
    drawCard(2, "温度 Temp", bufT, TFT_ORANGE);

    // 推到屏幕
    canvas->pushSprite(0, 0);
    g_pageChanged = false;
}

// ========== 环境数据刷新（1Hz 节流） ==========
void Compass::refreshEnvData()
{
    if (millis() - g_lastEnvRefresh > ENV_REFRESH_MS)
    {
        g_lastEnvRefresh = millis();
        g_pressure = HAL::dps_get_pressure(); // hPa
        g_altitude = HAL::dps_get_altitude(); // m
        g_temp = HAL::dps_get_temperature();  // °C
        g_envDirty = false;
    }
}

// ==================== 页面 2：气泡水平仪 ====================
// 计算气泡偏移 (bx, by)，单位 px，+x 右 +y 下（屏幕坐标）
// 输入：roll(绕X轴,右倾为正) / pitch(绕Y轴,上仰为正)，单位度
void Compass::drawLevelBubble(float rollDeg, float pitchDeg, float &bx, float &by, bool &isLevel)
{
    // 平滑（抑制抖动）
    g_smoothRoll = g_smoothRoll * (1 - LEVEL_SMOOTH_ALPHA) + rollDeg * LEVEL_SMOOTH_ALPHA;
    g_smoothPitch = g_smoothPitch * (1 - LEVEL_SMOOTH_ALPHA) + pitchDeg * LEVEL_SMOOTH_ALPHA;

    // 减去用户设定的零位偏移，得到“相对水平”的偏差角
    float roll_c = g_smoothRoll - g_levelRollOffset;
    float pitch_c = g_smoothPitch - g_levelPitchOffset;

    // 重力分量（基于校正角）
    float gx = -pitch_c;
    float gy = -roll_c;
    float mag = sqrtf(gx * gx + gy * gy);

    float offset = 0;

    // 死区：校正角 < LEVEL_DEAD_ZONE 视为水平，气泡严格居中
    if (mag >= LEVEL_DEAD_ZONE)
    {
        float norm = mag / LEVEL_SAT_ANGLE;
        if (norm > 1.0f)
            norm = 1.0f;
        offset = LEVEL_MAX_RADIUS * norm;
    }

    if (mag > 0 && offset > 0)
    {
        bx = gx / mag * offset;
        by = gy / mag * offset;
    }
    else
    {
        bx = 0;
        by = 0;
    }

    // 限制在圆内
    float limit = LEVEL_MAX_RADIUS - LEVEL_BUBBLE_RADIUS;
    float dist = sqrtf(bx * bx + by * by);
    if (dist > limit && dist > 0)
    {
        bx = bx / dist * limit;
        by = by / dist * limit;
    }

    // 是否水平：基于校正角判断（归零后必然居中且变绿）
    isLevel = (mag < LEVEL_DEAD_ZONE);
}

// 绘制水平仪整页
void Compass::drawLevelPage()
{
    canvas->fillSprite(TFT_BLACK);

    // ---- 顶部标题 ----
    canvas->setTextColor(MENU_TITLE_COLOR);
    canvas->setTextDatum(MC_DATUM);
    canvas->drawString("水平仪", LEVEL_CENTER_X, 20, font_small);
    canvas->drawFastHLine(20, 42, SCREEN_WIDTH - 40, TFT_DARKGREY);
    canvas->setTextDatum(0);

    // 每帧驱动一次更新，确保切到此页时数据实时
    ImuData_t imu = HAL::getImuData();

    // ---- 计算气泡位置 ----
    float bx = 0, by = 0;
    bool isLevel = false;
    drawLevelBubble(imu.roll, imu.pitch, bx, by, isLevel);

    const int cx = LEVEL_CENTER_X;
    const int cy = LEVEL_CENTER_Y;

    // ---- 外圈（气泡活动边界）----
    canvas->drawCircle(cx, cy, (int)LEVEL_MAX_RADIUS, TFT_LIGHTGREY);

    // ---- 十字准星（水平时气泡对齐于此）----
    canvas->drawFastHLine(cx - (int)LEVEL_MAX_RADIUS, cy, (int)(LEVEL_MAX_RADIUS * 2), TFT_DARKGREY);
    canvas->drawFastVLine(cx, cy - (int)LEVEL_MAX_RADIUS, (int)(LEVEL_MAX_RADIUS * 2), TFT_DARKGREY);
    // 中心点
    canvas->fillCircle(cx, cy, 3, TFT_RED);

    // ---- 气泡本体（向重力方向下沉）----
    uint16_t bubbleColor = isLevel ? TFT_GREEN : TFT_SKYBLUE;
    canvas->fillCircle(cx + (int)bx, cy + (int)by, LEVEL_BUBBLE_RADIUS, bubbleColor);
    // 高光（模拟球面）
    uint16_t hlColor = isLevel ? TFT_GREENYELLOW : TFT_LIGHTGREY;
    canvas->fillCircle(cx + (int)bx - 5, cy + (int)by - 5, LEVEL_BUBBLE_RADIUS / 3, hlColor);

    // ---- 水平提示 ----
    canvas->setTextDatum(MC_DATUM);
    if (isLevel)
    {
        canvas->setTextColor(TFT_GREEN);
        canvas->drawString("已水平", LEVEL_CENTER_X, cy + (int)LEVEL_MAX_RADIUS + 18, font_small);
    }
    else
    {
        canvas->setTextColor(TFT_LIGHTGREY);
        canvas->drawString("请调平", LEVEL_CENTER_X, cy + (int)LEVEL_MAX_RADIUS + 18, font_small);
    }
    canvas->setTextDatum(0);

    // ---- 实时倾角数值 ----
    char buf[48];
    snprintf(buf, sizeof(buf), "Roll:%+6.1f  Pitch:%+6.1f", g_smoothRoll - g_levelRollOffset, g_smoothPitch - g_levelPitchOffset);
    canvas->setTextColor(TFT_LIGHTGREY);
    canvas->setTextDatum(MC_DATUM);
    canvas->drawString(buf, LEVEL_CENTER_X, 55, font_small);
    canvas->setTextDatum(0);

    canvas->pushSprite(0, 0);
    g_pageChanged = false;
}

void Compass::drawLevelStatus(const char *string, uint16_t color)
{
    g_statusSprite->fillSprite(TFT_BLACK);
    g_statusSprite->setTextColor(color);
    g_statusSprite->setTextDatum(MC_DATUM);
    g_statusSprite->drawString(string, LEVEL_CENTER_X, 10, font_small);
    g_statusSprite->setTextDatum(0);
    g_statusSprite->pushSprite(0, 250);
}

// ==================== 水平仪复位 ====================
void Compass::LevelPage_Reset()
{
    ImuData_t imu = HAL::getImuData();
    g_smoothRoll = imu.roll;
    g_smoothPitch = imu.pitch;
}

// ========== 短按 OK（水平仪页）：以当前姿态设为"水平零位" ==========
// 原理：把此刻平滑后的 roll/pitch 记录下来，后续映射时减去该偏移，
//       则该姿态被判定为"水平"，气泡立即居中、"已水平"亮起。
void Compass::resetLevelOffset()
{
    g_levelRollOffset = g_smoothRoll;
    g_levelPitchOffset = g_smoothPitch;
    saveLevelOffset(); // 写入 Flash，重启不丢失

    HAL::beep(1200, 80); // 确认音（短促高音）
    drawLevelStatus("零位已设置", TFT_WHITE);
    Serial.printf("[Level] 零位已设置: roll=%.2f pitch=%.2f\n",
                  g_levelRollOffset, g_levelPitchOffset);
    delay(500);
}

// ========== 长按 OK（水平仪页）：校准陀螺仪零漂 ==========
// 委托 HAL 在设备静止时采集若干秒求均值，写入 BMI270 陀螺仪 offset。
// 校准期间姿态角可能跳变，故复位平滑值让其重新收敛。
void Compass::calibrateGyro()
{
    // 提示音：两短促 = 开始校准
    HAL::beep(400, 100);
    delay(120);
    HAL::beep(400, 100);
    Serial.println("[Level] 开始陀螺仪校准（请保持设备静止）...");
    drawLevelStatus("开始陀螺仪校准", TFT_WHITE);

    HAL::imu_calibrate_gyro();

    // 校准后复位平滑，避免跳变
    LevelPage_Reset();

    HAL::beep(1200, 150); // 长音 = 完成
    Serial.println("[Level] 陀螺仪校准完成");
    drawLevelStatus("陀螺仪校准完成", TFT_WHITE);
    delay(500);
}

// ========== 零位持久化（Preferences，命名空间 "level"） ==========
void Compass::saveLevelOffset()
{
    Preferences prefs;
    if (prefs.begin("level", false))
    {
        prefs.putFloat("roll_off", g_levelRollOffset);
        prefs.putFloat("pitch_off", g_levelPitchOffset);
        prefs.end();
    }
}

void Compass::loadLevelOffset()
{
    Preferences prefs;
    if (prefs.begin("level", true))
    {
        g_levelRollOffset = prefs.getFloat("roll_off", 0.0f);
        g_levelPitchOffset = prefs.getFloat("pitch_off", 0.0f);
        prefs.end();
    }
    Serial.printf("[Level] 加载零位: roll=%.2f pitch=%.2f\n",
                  g_levelRollOffset, g_levelPitchOffset);
}

void Compass::tickPageData()
{
    switch (g_page)
    {
    case PAGE_COMPASS:
        refreshHeading();
        break;
    case PAGE_ENV:
        refreshEnvData();
        break;
    case PAGE_LEVEL:
        HAL::updateImuData();
        break;
    default:
        break;
    }
}

// 绘制
void Compass::drawPage()
{
    switch (g_page)
    {
    case PAGE_COMPASS:
        pngRotatorUpdate();
        break;
    case PAGE_ENV:
        drawEnvPage();
        break;
    case PAGE_LEVEL:
        drawLevelPage();
        break;
    default:
        canvas->fillSprite(TFT_BLACK);
        canvas->pushSprite(0, 0);
        break;
    }
}

void Compass::handleRotate(int dir)
{
    // 手动微调：用于磁偏角/安装角补偿（而非控制角度本身）
    g_headingSmoothed = norm360(g_headingSmoothed - dir * 1.0f);
    setAngle(-g_headingSmoothed);
}

void Compass::handleShortPress()
{
    if (g_page == PAGE_COMPASS)
    {
        if (g_isCalibrating)
        {
            g_isCalibrating = false;
            HAL::MAG_StopCalibration();
        }
        else
        {
            g_headingSmoothed = g_heading;
            setAngle(-g_headingSmoothed);
        }
    }
    else if (g_page == PAGE_LEVEL)
    {
        resetLevelOffset();
    }
}

void Compass::handleLongPress()
{
    if (g_page == PAGE_COMPASS)
    {

        g_isCalibrating = true;
        HAL::MAG_StartCalibration();
    }
    else if (g_page == PAGE_LEVEL)
    {
        calibrateGyro();
    }
}

// ============ 初始化 ============
void Compass::Init()
{
    canvas = HAL::GetCanvas();
    pngRotatorInit();
    loadLevelOffset(); // 启动时恢复已保存的水平零位（未校准则为 0）
    HAL::logMemory();
    HAL::dps_set_atmosphere(1022.0);

    if (g_statusSprite == nullptr)
    {
        g_statusSprite = new LGFX_Sprite(HAL::GetDisplay());
        bool created = g_statusSprite->createSprite(240, 30);
        Serial.printf("g_statusSprite createSprite: %s\n", created ? "OK" : "FAILED");
    }

    Serial.println("[Compass] Init done");
}

void Compass::Update()
{
    uint8_t key = 0;
    if (HAL::getEncoder(&key))
    {
        HAL::beep(500, 20);
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
        case BTN_LEFT_SP:
        case BTN_RIGHT_SP:
            togglePage();
            break;
        default:
            break;
        }
    }

    HAL::updateMAG(); // 驱动磁力计采样
    tickPageData();

    // ---- 绘制（页面切换时强制重绘）----
    static uint32_t lastDraw = 0;
    if (g_pageChanged || millis() - lastDraw > 16)
    {
        lastDraw = millis();
        drawPage();
        g_pageChanged = false;
    }
}

void Compass::Exit()
{
    if (g_srcSprite)
    {
        g_srcSprite->deleteSprite();
        delete g_srcSprite;
        g_srcSprite = nullptr;
    }
    if (g_rotSprite)
    {
        g_rotSprite->deleteSprite();
        delete g_rotSprite;
        g_rotSprite = nullptr;
    }

    g_pngLoaded = false;
    Serial.println("PNG rotator deinitialized");

    if (g_statusSprite)
    {
        g_statusSprite->deleteSprite();
        delete g_statusSprite;
        g_statusSprite = nullptr;
    }
    HAL::logMemory();
}

static void check_serial_command()
{
    if (!Serial.available())
        return;
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();

    if (cmd == "cal" || cmd == "calibrate")
    {
        HAL::MAG_StartCalibration();
    }
    else if (cmd == "stop")
    {
        HAL::MAG_StopCalibration();
    }
    else if (cmd == "clear")
    {
        HAL::MAG_ClearCalibration();
    }
    else if (cmd == "status")
    {
        HAL::MAG_GetCalStatus();
    }
    else if (cmd == "page") // 串口切页面
    {
        g_compass.togglePage();
    }
}

// ============ App 主入口 ============
void Compass::Main()
{
    Init();
    while (1)
    {
        check_serial_command(); // 监听串口命令
        Update();
        if (HAL::isReturn())
            break;
    }
    Exit();
}
