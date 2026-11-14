#include "SettingsApp.h"

#include <math.h>

SettingsApp g_settingsApp;

// ============ 配色（iPhone 深色设置风格）============
static const uint16_t COL_BG = TFT_BLACK;
static const uint16_t COL_HEADER = rgb565(28, 28, 30);
static const uint16_t COL_ROW = rgb565(44, 44, 46);
static const uint16_t COL_ROW_SEL = rgb565(58, 58, 60);
static const uint16_t COL_ACCENT = rgb565(10, 132, 255);   // iOS 蓝
static const uint16_t COL_TEXT_DIM = rgb565(152, 152, 158);
static const uint16_t COL_CHEVRON = rgb565(142, 142, 147);
static const uint16_t COL_HINT = rgb565(110, 110, 115);
static const uint16_t COL_SW_ON = rgb565(52, 199, 89); // iOS 绿
static const uint16_t COL_SW_OFF = rgb565(120, 120, 128);
static const uint16_t COL_DIALOG = rgb565(30, 30, 32);
static const uint16_t COL_BORDER = rgb565(90, 90, 95);
static const uint16_t COL_DIVIDER = rgb565(60, 60, 64);
static const uint16_t COL_ITEM = rgb565(40, 40, 42);
static const uint16_t COL_TRACK = rgb565(70, 70, 75);
static const uint16_t COL_DANGER = rgb565(255, 69, 58); // iOS 红

// ============ 布局参数 ============
static const int HEADER_H = 44;                  // 顶部标题栏
static const int ROW_X = 10;                     // 行左边界
static const int ROW_W = SCREEN_WIDTH - 20;      // 行宽
static const int ROW_H = 56;                     // 行高
static const int ROW_GAP = 8;                    // 行间距
static const int ROW_PITCH = ROW_H + ROW_GAP;    // 行步进
static const int ROW_TOP = HEADER_H + 10;        // 首行顶部
// 列表视口高度（底部留给按键提示）。
// 名字不能用 LIST_H：FreeRTOS 的 freertos/list.h 把它当 include guard 占了，
// ESP32 构建时会被展开成空，native 环境不带 FreeRTOS 所以看不出来
static const int LIST_VIEW_H = SCREEN_HEIGHT - 28 - ROW_TOP;
static const int ROW_RADIUS = 12;                // 行圆角
static const int SW_W = 46;                      // 开关宽
static const int SW_H = 26;                      // 开关高
static const int ITEM_H = 38;                    // 弹窗选项行高
static const int ITEM_GAP = 6;

// 弹窗背景遮罩强度：盖一层黑色，alpha 越大越暗（0~255）
static const uint8_t DIM_ALPHA = 80;

// 亮度范围（与 Backlight_Init 的 10 下限保持一致）
static const int BR_MIN = 10;
static const int BR_MAX = 100;
static const int BR_STEP = 5;
static const int BR_DEF = 75;

// ============ 生命周期 ============
void SettingsApp::Init()
{
    disp = HAL::GetDisplay();
    canvas = HAL::GetCanvas();

    loadConfig();

    // 只初始化 WiFi 服务层（不开射频）：对时弹窗要读已保存的网络配置
    HAL::Wifi_Init();

    brightness = HAL::getBrightness();
    if (brightness < BR_MIN || brightness > BR_MAX)
        brightness = BR_DEF;

    mode = MODE_LIST;
    cursor = 0;
    scrollY = 0.0f;
    scrollTargetY = 0.0f;
    ntpPhase = NTPP_READY;
    powerWasDown = false;
    exitRequest = false;

    drawList();
    canvas->pushSprite(0, 0);
}

void SettingsApp::Main()
{
    Init();

    while (!exitRequest)
    {
        Update();
        delay(10);
    }

    Exit();
}

void SettingsApp::Update()
{
    uint8_t key = 0;
    if (HAL::getEncoder(&key))
    {
        handleKey(key);
    }

    // 列表滚动动画：逐帧逼近目标偏移
    if (mode == MODE_LIST)
    {
        const float diff = scrollTargetY - scrollY;
        if (fabsf(diff) > 0.05f)
        {
            scrollY += diff * 0.25f;
            if (fabsf(scrollTargetY - scrollY) < 0.5f)
                scrollY = scrollTargetY;

            drawList();
            canvas->pushSprite(0, 0);
        }
    }
    else if (mode == MODE_NTP && ntpPhase == NTPP_RUNNING)
    {
        updateNtp(); // 轮询 NTP 状态 + 刷进度动画
    }

    // 电源键在弹窗内是"返回上级"，在列表里是"退出 App"
    bool powerDown = HAL::isReturn();
    if (powerDown && !powerWasDown)
    {
        handlePower();
    }
    powerWasDown = powerDown;
}

void SettingsApp::Exit()
{
    // 恢复默认文字对齐，避免影响其它 App 的排版
    canvas->setTextDatum(0);
}

// ============ 配置读写 ============
void SettingsApp::loadConfig()
{
    timeFormat = HAL::isTime24Hour() ? 24 : 12;
    buzzerOn = HAL::isBuzzerEnabled();
    wifiKeepOn = HAL::isWifiKeepAlive();
}

void SettingsApp::saveTimeFormat()
{
    HAL::setTime24Hour(timeFormat == 24);
}

// ============ 绘制：一级列表 ============
void SettingsApp::drawList()
{
    canvas->fillScreen(COL_BG);

    // 顶部标题栏
    canvas->fillRect(0, 0, SCREEN_WIDTH, HEADER_H, COL_HEADER);
    canvas->setFont(&fonts::FreeSansBold12pt7b);
    canvas->setTextDatum(MC_DATUM);
    canvas->setTextColor(TFT_WHITE);
    canvas->drawString("Settings", SCREEN_WIDTH / 2, HEADER_H / 2);
    canvas->drawFastHLine(0, HEADER_H, SCREEN_WIDTH, COL_DIVIDER);

    // 行列表：裁剪到视口，滚动时不会画到标题栏 / 提示栏上
    canvas->setClipRect(0, ROW_TOP, SCREEN_WIDTH, LIST_VIEW_H);
    for (int i = 0; i < ROW_COUNT; i++)
    {
        drawRow(i);
    }
    canvas->clearClipRect();

    drawScrollBar();

    // 底部按键提示
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextDatum(MC_DATUM);
    canvas->setTextColor(COL_HINT);
    canvas->drawString("OK: select   PWR: back", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 12);

    canvas->setTextDatum(0); // 恢复默认
}

void SettingsApp::drawRow(int index)
{
    const int y = ROW_TOP + index * ROW_PITCH - (int)(scrollY + 0.5f);
    if (y + ROW_H <= ROW_TOP || y >= ROW_TOP + LIST_VIEW_H)
        return; // 完全在视口外，不用画

    const bool selected = (mode == MODE_LIST && index == cursor);
    const int cy = y + ROW_H / 2;
    const int rightEdge = ROW_X + ROW_W - 14;

    canvas->fillRoundRect(ROW_X, y, ROW_W, ROW_H, ROW_RADIUS, selected ? COL_ROW_SEL : COL_ROW);
    if (selected)
        canvas->drawRoundRect(ROW_X, y, ROW_W, ROW_H, ROW_RADIUS, COL_ACCENT);

    // 左侧标题
    canvas->setFont(&fonts::FreeSansBold9pt7b);
    canvas->setTextDatum(ML_DATUM);
    canvas->setTextColor(TFT_WHITE);
    canvas->drawString(rows[index].label, ROW_X + 16, cy);

    switch (rows[index].type)
    {
    case ROW_TOGGLE:
    {
        // 圆角胶囊开关：绿底 + 白色圆钮
        const bool on = (rows[index].id == ID_WIFI_KEEP) ? wifiKeepOn : buzzerOn;
        drawSwitch(rightEdge - SW_W, y + (ROW_H - SW_H) / 2, on);
        break;
    }

    case ROW_CHOICE:
    {
        // 行内只显示短值（12h/24h）："Time Format" 粗体已占 ~114px，
        // 放完整 "12 Hour" 会和标题贴在一起；完整文案在弹窗里展示
        char buf[8];
        snprintf(buf, sizeof(buf), "%dh", timeFormat);
        canvas->setFont(&fonts::FreeSans9pt7b);
        canvas->setTextDatum(MR_DATUM);
        canvas->setTextColor(COL_TEXT_DIM);
        canvas->drawString(buf, rightEdge - 16, cy);
        drawChevron(rightEdge - 5, cy, COL_CHEVRON);
        break;
    }

    case ROW_VALUE:
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u%%", (unsigned)brightness);
        canvas->setFont(&fonts::FreeSans9pt7b);
        canvas->setTextDatum(MR_DATUM);
        canvas->setTextColor(COL_TEXT_DIM);
        canvas->drawString(buf, rightEdge - 16, cy);
        drawChevron(rightEdge - 5, cy, COL_CHEVRON);
        break;
    }

    case ROW_ACTION:
    {
        // 动作型行：右侧直接显示状态，不放箭头
        const char *text;
        uint16_t color;
        if (ntpPhase == NTPP_RUNNING)
        {
            text = "Syncing..";
            color = COL_ACCENT;
        }
        else if (timeSynced)
        {
            text = "Synced";
            color = COL_SW_ON;
        }
        else if (!HAL::Wifi_IsConnected())
        {
            text = "No WiFi";
            color = COL_HINT;
        }
        else
        {
            text = "Sync now";
            color = COL_ACCENT;
        }

        canvas->setFont(&fonts::FreeSans9pt7b);
        canvas->setTextDatum(MR_DATUM);
        canvas->setTextColor(color);
        canvas->drawString(text, rightEdge, cy);
        break;
    }

    default:
        break;
    }
}

void SettingsApp::drawSwitch(int x, int y, bool on)
{
    const int r = SW_H / 2;

    canvas->fillRoundRect(x, y, SW_W, SW_H, r, on ? COL_SW_ON : COL_SW_OFF);

    const int knobX = on ? (x + SW_W - r) : (x + r);
    canvas->fillCircle(knobX, y + r, r - 2, TFT_WHITE);
}

void SettingsApp::drawChevron(int x, int y, uint16_t color)
{
    drawWedgeLine(canvas, x - 4.0f, y - 6.0f, x + 1.0f, y, 2.0f, 2.0f, color, COL_ROW);
    drawWedgeLine(canvas, x + 1.0f, y, x - 4.0f, y + 6.0f, 2.0f, 2.0f, color, COL_ROW);
}

void SettingsApp::drawCheck(int x, int y, uint16_t color)
{
    drawWedgeLine(canvas, x - 8.0f, y, x - 3.0f, y + 6.0f, 2.5f, 2.5f, color, COL_ITEM);
    drawWedgeLine(canvas, x - 3.0f, y + 6.0f, x + 8.0f, y - 7.0f, 2.5f, 2.5f, color, COL_ITEM);
}

// ============ 绘制：弹窗 ============
void SettingsApp::drawDialogFrame(int x, int y, int w, int h, const char *title)
{
    canvas->fillRoundRect(x - 3, y - 3, w + 6, h + 6, 17, COL_BORDER);
    canvas->fillRoundRect(x, y, w, h, 14, COL_DIALOG);

    canvas->setFont(&fonts::FreeSansBold12pt7b);
    canvas->setTextDatum(MC_DATUM);
    canvas->setTextColor(TFT_WHITE);
    canvas->drawString(title, x + w / 2, y + 26);

    canvas->drawFastHLine(x + 12, y + 46, w - 24, COL_DIVIDER);
}

// 弹窗背景压暗：用 LovyanGFX 自带的 alpha 混合盖一层黑色
//  注意：不能直接改精灵帧缓冲像素——rgb565_2Byte 缓冲区是字节交换存的（GGGBBBBB RRRRRGGG），
//  按 RGB565 拆通道会串色（背景变黄、卡片变蓝）。fillRectAlpha 内部走 readRect/writeImage，
//  颜色转换由库完成，与色深/字节序无关。
void SettingsApp::dimBackground()
{
    canvas->fillRectAlpha(0, 0, canvas->width(), canvas->height(), DIM_ALPHA, (uint32_t)0x000000);
}

void SettingsApp::drawChoiceDialog()
{
    const int w = 212;
    const int h = 146;
    const int x = (SCREEN_WIDTH - w) / 2;
    const int y = (SCREEN_HEIGHT - h) / 2;

    drawDialogFrame(x, y, w, h, "Time Format");

    const int iw = w - 20;

    for (int i = 0; i < CHOICE_COUNT; i++)
    {
        const int iy = y + 54 + i * (ITEM_H + ITEM_GAP);
        const int ix = x + 10;
        const int icy = iy + ITEM_H / 2;

        canvas->fillRoundRect(ix, iy, iw, ITEM_H, 10, (i == choiceCursor) ? COL_ROW_SEL : COL_ITEM);

        canvas->setFont(&fonts::FreeSans9pt7b);
        canvas->setTextDatum(ML_DATUM);
        canvas->setTextColor(TFT_WHITE);
        canvas->drawString(choiceLabels[i], ix + 14, icy);

        // 当前生效的选项打钩
        const bool active = (i == 0 && timeFormat == 12) || (i == 1 && timeFormat == 24);
        if (active)
            drawCheck(ix + iw - 22, icy, COL_ACCENT);
    }

    canvas->setTextDatum(0); // 恢复默认
}

void SettingsApp::drawSliderDialog()
{
    const int w = 212;
    const int h = 96;
    const int x = (SCREEN_WIDTH - w) / 2;
    const int y = (SCREEN_HEIGHT - h) / 2;

    drawDialogFrame(x, y, w, h, "Brightness");

    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", (unsigned)brightness);

    // 百分比文字占据右侧固定宽度，滑轨右边界据此内缩，避免与文字/旋钮重叠
    canvas->setFont(&fonts::FreeSansBold9pt7b);
    const int textRight = x + w - 14;
    const int x1 = textRight - (canvas->textWidth("100%") + 16);
    const int x0 = x + 18;
    const int trackY = y + 70;
    const int pos = x0 + (brightness - BR_MIN) * (x1 - x0) / (BR_MAX - BR_MIN);

    // 清掉上一次的百分比文字（不同宽度会留下残影）
    canvas->fillRect(x1 + 6, trackY - 12, textRight - x1 - 6, 24, COL_DIALOG);

    // 滑轨
    canvas->fillRoundRect(x0, trackY - 4, x1 - x0, 8, 4, COL_TRACK);
    if (pos > x0)
        canvas->fillRoundRect(x0, trackY - 4, pos - x0, 8, 4, COL_ACCENT);

    canvas->fillCircle(pos, trackY, 9, TFT_WHITE);
    canvas->drawCircle(pos, trackY, 9, COL_BORDER);

    // 当前百分比：slider 右侧，与滑轨同高
    canvas->setTextDatum(MR_DATUM);
    canvas->setTextColor(COL_ACCENT);
    canvas->drawString(buf, textRight, trackY);

    canvas->setTextDatum(0); // 恢复默认
}

// ============ 绘制：滚动条 ============
// 行数超出一屏时才画，拇指位置跟随 scrollY
void SettingsApp::drawScrollBar()
{
    const int totalH = ROW_COUNT * ROW_PITCH - ROW_GAP;
    if (totalH <= LIST_VIEW_H)
        return;

    const int maxScroll = totalH - LIST_VIEW_H;
    const int thumbH = LIST_VIEW_H * LIST_VIEW_H / totalH;
    float ratio = scrollY / (float)maxScroll;
    if (ratio < 0.0f)
        ratio = 0.0f;
    if (ratio > 1.0f)
        ratio = 1.0f;
    const int thumbY = ROW_TOP + (int)((float)(LIST_VIEW_H - thumbH) * ratio);

    canvas->fillRoundRect(SCREEN_WIDTH - 5, ROW_TOP + 2, 2, LIST_VIEW_H - 4, 1, COL_DIVIDER);
    canvas->fillRoundRect(SCREEN_WIDTH - 5, thumbY, 2, thumbH, 1, COL_CHEVRON);
}

// ============ 绘制：网络对时弹窗 ============
// 自包含（每次先重绘整个弹窗框），可在同步过程中反复调用刷动画
void SettingsApp::drawNtpDialog()
{
    const int w = 212;
    const int h = 176;
    const int x = (SCREEN_WIDTH - w) / 2;
    const int y = (SCREEN_HEIGHT - h) / 2;

    drawDialogFrame(x, y, w, h, "Network Time");

    WifiInfo_t info;
    HAL::Wifi_GetInfo(&info);

    // WiFi 状态行
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextDatum(ML_DATUM);
    canvas->setTextColor(COL_TEXT_DIM);
    canvas->drawString("WiFi", x + 16, y + 66);

    char ssid[20];
    if (info.connected)
        snprintf(ssid, sizeof(ssid), "%.14s", info.ssid);
    else
        snprintf(ssid, sizeof(ssid), "Not connected");

    canvas->setTextDatum(MR_DATUM);
    canvas->setTextColor(info.connected ? COL_SW_ON : COL_DANGER);
    canvas->drawString(ssid, x + w - 16, y + 66);

    // NTP 服务器
    canvas->setTextDatum(ML_DATUM);
    canvas->setTextColor(COL_TEXT_DIM);
    canvas->drawString("Server", x + 16, y + 88);

    canvas->setTextDatum(MR_DATUM);
    canvas->setTextColor(TFT_WHITE);
    canvas->drawString("pool.ntp.org", x + w - 16, y + 88);

    canvas->drawFastHLine(x + 12, y + 102, w - 24, COL_DIVIDER);

    // 状态文字
    char status[32];
    uint16_t statusColor;
    switch (ntpPhase)
    {
    case NTPP_RUNNING:
        snprintf(status, sizeof(status), "%s", info.connected ? "Syncing..." : "Connecting WiFi...");
        statusColor = COL_ACCENT;
        break;
    case NTPP_OK:
        snprintf(status, sizeof(status), "Synced  %s", ntpTime);
        statusColor = COL_SW_ON;
        break;
    case NTPP_FAIL:
        snprintf(status, sizeof(status), "%s", info.connected ? "Sync failed, retry" : "No WiFi, check WiFi app");
        statusColor = COL_DANGER;
        break;
    default:
        snprintf(status, sizeof(status), "%s", info.hasSaved ? "Ready to sync" : "No network saved");
        statusColor = COL_TEXT_DIM;
        break;
    }

    canvas->setFont(&fonts::FreeSansBold9pt7b);
    canvas->setTextDatum(MC_DATUM);
    canvas->setTextColor(statusColor);
    canvas->drawString(status, x + w / 2, y + 122);

    // 进度条：同步中来回扫动，成功后满格
    const int barX = x + 20;
    const int barW = w - 40;
    const int barY = y + 138;
    canvas->fillRoundRect(barX, barY, barW, 6, 3, COL_TRACK);

    if (ntpPhase == NTPP_RUNNING)
    {
        const int segW = barW / 4;
        const int phase = (int)((millis() / 4) % (uint32_t)(barW + segW));
        int sx = barX + phase - segW;
        int ex = barX + phase;
        if (sx < barX)
            sx = barX;
        if (ex > barX + barW)
            ex = barX + barW;
        if (ex > sx)
            canvas->fillRoundRect(sx, barY, ex - sx, 6, 3, COL_ACCENT);
    }
    else if (ntpPhase == NTPP_OK)
    {
        canvas->fillRoundRect(barX, barY, barW, 6, 3, COL_SW_ON);
    }

    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextDatum(MC_DATUM);
    canvas->setTextColor(COL_HINT);
    canvas->drawString(ntpPhase == NTPP_READY ? "OK: sync   PWR: close" : "PWR: close",
                       x + w / 2, y + h - 16);
    canvas->setTextDatum(0);
}

// ============ 交互 ============
void SettingsApp::handleKey(uint8_t key)
{
    switch (mode)
    {
    case MODE_LIST:
        switch (key)
        {
        case BTN_ENC_CW:
        case BTN_UP_SP:
        case BTN_LEFT_SP:
            cursor = (cursor + ROW_COUNT - 1) % ROW_COUNT;
            HAL::beep(600, 15);
            scrollToCursor();
            drawList();
            canvas->pushSprite(0, 0);
            break;

        case BTN_ENC_CC:
        case BTN_DOWN_SP:
        case BTN_RIGHT_SP:
            cursor = (cursor + 1) % ROW_COUNT;
            HAL::beep(600, 15);
            scrollToCursor();
            drawList();
            canvas->pushSprite(0, 0);
            break;

        case BTN_OK_SP:
            activateRow();
            break;

        default:
            break;
        }
        break;

    case MODE_CHOICE:
        switch (key)
        {
        case BTN_ENC_CW:
        case BTN_UP_SP:
        case BTN_LEFT_SP:
            choiceCursor = (choiceCursor + CHOICE_COUNT - 1) % CHOICE_COUNT;
            HAL::beep(600, 15);
            drawChoiceDialog();
            canvas->pushSprite(0, 0);
            break;

        case BTN_ENC_CC:
        case BTN_DOWN_SP:
        case BTN_RIGHT_SP:
            choiceCursor = (choiceCursor + 1) % CHOICE_COUNT;
            HAL::beep(600, 15);
            drawChoiceDialog();
            canvas->pushSprite(0, 0);
            break;

        case BTN_OK_SP:
            timeFormat = (choiceCursor == 0) ? 12 : 24;
            saveTimeFormat();
            HAL::beep(900, 20);
            closeDialog(false);
            break;

        default:
            break;
        }
        break;

    case MODE_SLIDER:
        switch (key)
        {
        case BTN_ENC_CW:
        case BTN_LEFT_SP:
        case BTN_DOWN_SP:
            setBrightnessValue(brightness - BR_STEP);
            break;

        case BTN_ENC_CC:
        case BTN_RIGHT_SP:
        case BTN_UP_SP:
            setBrightnessValue(brightness + BR_STEP);
            break;

        case BTN_OK_SP:
            HAL::setBrightness(brightness); // 写入 NVS，掉电保持
            HAL::beep(900, 20);
            closeDialog(false);
            break;

        default:
            break;
        }
        break;

    case MODE_NTP:
        // 同步中不再响应 OK，避免重复发起；完成后可以再按一次重试
        if (key == BTN_OK_SP && ntpPhase != NTPP_RUNNING)
            startNtpSync();
        break;
    }
}

void SettingsApp::handlePower()
{
    if (mode == MODE_NTP)
        HAL::Ntp_Reset(); // 放弃本次对时，不写 RTC

    if (mode != MODE_LIST)
    {
        // 弹窗内：关闭弹窗返回上一级
        closeDialog(true);
        return;
    }

    exitRequest = true;
}

void SettingsApp::activateRow()
{
    HAL::beep(800, 20);

    switch (rows[cursor].id)
    {
    case ID_BUZZER:
        buzzerOn = !buzzerOn;
        HAL::setBuzzerEnabled(buzzerOn); // 立即生效 + 写入 NVS
        if (buzzerOn)
            HAL::beep(900, 30);
        drawList();
        canvas->pushSprite(0, 0);
        break;

    case ID_WIFI_KEEP:
        wifiKeepOn = !wifiKeepOn;
        HAL::setWifiKeepAlive(wifiKeepOn); // 写入 NVS：下次 WiFi App 退出才生效
        HAL::beep(900, 30);
        drawList();
        canvas->pushSprite(0, 0);
        break;

    case ID_TIME_FORMAT:
        mode = MODE_CHOICE;
        choiceCursor = (timeFormat == 12) ? 0 : 1;
        dimBackground(); // 背景压暗后再盖弹窗
        drawChoiceDialog();
        canvas->pushSprite(0, 0);
        break;

    case ID_BRIGHTNESS:
        mode = MODE_SLIDER;
        brightnessOrig = brightness;
        dimBackground(); // 背景压暗后再盖弹窗
        drawSliderDialog();
        canvas->pushSprite(0, 0);
        break;

    case ID_NTP:
        mode = MODE_NTP;
        ntpPhase = NTPP_READY;
        HAL::Ntp_Reset();
        dimBackground(); // 背景压暗后再盖弹窗
        drawNtpDialog();
        canvas->pushSprite(0, 0);
        break;

    default:
        break;
    }
}

void SettingsApp::closeDialog(bool cancel)
{
    if (mode == MODE_SLIDER && cancel)
    {
        // 取消：还原到打开滑块前的亮度（不写 NVS）
        brightness = brightnessOrig;
        HAL::setBrightnessPreview(brightness);
    }

    mode = MODE_LIST;
    drawList();
    canvas->pushSprite(0, 0);
}

void SettingsApp::setBrightnessValue(int value)
{
    const int v = constrain(value, BR_MIN, BR_MAX);
    if (v == brightness)
        return;

    brightness = (uint8_t)v;
    HAL::setBrightnessPreview(brightness); // 实时预览，确认时才落盘
    HAL::beep(600, 10);

    drawSliderDialog();
    canvas->pushSprite(0, 0);
}

// ============ 列表滚动 ============
// 只调目标偏移，实际滚动由 Update 逐帧插值，保证选中行完整可见
void SettingsApp::scrollToCursor()
{
    const int totalH = ROW_COUNT * ROW_PITCH - ROW_GAP;
    const int maxScroll = (totalH > LIST_VIEW_H) ? (totalH - LIST_VIEW_H) : 0;

    const int rowTop = cursor * ROW_PITCH;
    if (rowTop < scrollTargetY)
        scrollTargetY = (float)rowTop; // 选中行在上方被遮住：向下移到顶
    if (rowTop + ROW_H - scrollTargetY > LIST_VIEW_H)
        scrollTargetY = (float)(rowTop + ROW_H - LIST_VIEW_H); // 下方被遮住：向上移到底

    scrollTargetY = constrain(scrollTargetY, 0.0f, (float)maxScroll);
}

// ============ 网络对时 ============
void SettingsApp::startNtpSync()
{
    HAL::beep(800, 20);
    HAL::Ntp_Reset();
    HAL::Ntp_Begin(); // 非阻塞：内部自己连 WiFi + 启动 SNTP

    ntpPhase = NTPP_RUNNING;
    ntpDrawMs = 0;
    drawNtpDialog();
    canvas->pushSprite(0, 0);
}

// 由 Update 在同步中反复调用：轮询 NTP 状态 + 刷进度动画
void SettingsApp::updateNtp()
{
    const HAL::NtpResult result = HAL::Ntp_Update();

    if (result == HAL::NTP_OK)
    {
        ntpPhase = NTPP_OK;
        timeSynced = true;

        tm *now = HAL::GetLocalTime();
        if (now)
            snprintf(ntpTime, sizeof(ntpTime), "%02d:%02d", now->tm_hour, now->tm_min);

        HAL::beep(1200, 30);
    }
    else if (result == HAL::NTP_FAIL)
    {
        ntpPhase = NTPP_FAIL;
        HAL::beep(400, 60);
    }
    else if (millis() - ntpDrawMs < 120)
    {
        return; // 进度动画限帧，别每轮都重绘
    }

    ntpDrawMs = millis();
    drawNtpDialog();
    canvas->pushSprite(0, 0);
}
