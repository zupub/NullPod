#include "launcher.h"
#include "assets/icon/icons.h"
#include <math.h>

// ============ 全局实例 ============
Launcher g_launcher;

// ============ Init ============
void Launcher::Init()
{
    if (active)
    {
        // 从 App 返回时强制刷新
        Draw();
        canvas->pushSprite(0, 0);
        return;
    }

    active = true;

    selectedIndex = 0;
    currentScrollOffset = 0.0f;
    targetScrollOffset = 0.0f;

    powerPressStart = 0;
    powerLongPressTriggered = false;

    // 菜单项
    menuItems[0] = {"Matrix", rgb565(0, 168, 107), icon_matrix, &actionScreenSaverEntry};
    menuItems[1] = {"Dashboard", rgb565(46, 134, 222), icon_dashboard, &actionDialEntry};
    menuItems[2] = {"LED", rgb565(0, 200, 180), icon_rgbled, &actionLedEntry};
    menuItems[3] = {"Remote", rgb565(230, 126, 34), icon_remotetv, &actionRemoteEntry};
    menuItems[4] = {"Music", rgb565(231, 76, 60), icon_music, &actionMusic};
    menuItems[5] = {"Weather", rgb565(231, 76, 60), icon_weather, &actionWeather};
    menuItems[6] = {"Compass", rgb565(149, 165, 166), icon_compass, &actionCompass};
    menuItems[7] = {"Air Mouse", rgb565(52, 73, 94), icon_mouse, &actionAirMouse};
    menuItems[8] = {"Barometer", rgb565(180, 100, 255), icon_barometer, &actionBarometer};
    menuItems[9] = {"Game", rgb565(255, 160, 0), icon_game, &actionGame};
    menuItems[10] = {"Files", rgb565(127, 140, 141), icon_files, &actionFiles};
    menuItems[11] = {"SD Card", rgb565(93, 63, 211), icon_sdcard, &actionSDCardEntry};
    menuItems[12] = {"WiFi", rgb565(93, 63, 211), icon_wifi, &actionWiFiEntry};
    menuItems[13] = {"Settings", rgb565(100, 180, 255), icon_settings, &actionSettingsEntry};
    menuItems[14] = {"About", rgb565(255, 80, 120), icon_about, &actionAboutEntry};

    disp = HAL::GetDisplay();
    canvas = HAL::GetCanvas();

    Draw();
    canvas->pushSprite(0, 0);
    Serial.println(">>> Launcher ready");
}

void Launcher::Exit()
{
}

// ============ Main（阻塞式生命周期）============
void Launcher::Main()
{
    Init();

    while (1)
    {
        Update();

        // 检查是否请求切换 App
        if (g_requestScreenSaver || g_requestDial || g_requestLed || g_requestRemote ||
            g_requestMusic || g_requestWeather || g_requestCompass || g_requestAirMouse ||
            g_requestBarometer || g_requestGame || g_requestFile || g_requestSD ||
            g_requestWiFi || g_requestSettings || g_requestAbout || g_requestMenu)
        {
            break;
        }

        delay(5);
    }

    Exit();
}

// ============ Update ============
void Launcher::Update()
{
    if (!active)
        return;

    uint8_t key = 0;
    if (HAL::getEncoder(&key))
    {
        HAL::beep(500, 20);
        switch (key)
        {
        case BTN_ENC_CW:
        case BTN_LEFT_SP:
            handleRotate(-1);
            break;
        case BTN_ENC_CC:
        case BTN_RIGHT_SP:
            handleRotate(+1);
            break;
        case BTN_OK_SP:
            handleShortPress();
            break;
        default:
            break;
        }
    }

    // ===== 长按Power键关机检测 =====
    bool powerPressed = HAL::isReturn();

    if (powerPressed && !powerLongPressTriggered)
    {
        // 刚按下，记录起始时间
        powerPressStart = millis();
        powerLongPressTriggered = true;
        powerHintShown = false;
    }

    if (powerPressed && powerLongPressTriggered)
    {
        unsigned long held = millis() - powerPressStart;
        // 持续按住，检查是否达到长按阈值
        if (held > 2000) // 2秒
        {
            // 关机前反馈：蜂鸣 + 清屏
            HAL::beep(2000, 100);
            delay(150);
            HAL::beep(2000, 100);
            HAL::Backlight_Off();

            canvas->fillScreen(BG_TOP);
            canvas->pushSprite(0, 0);
            delay(300);

            HAL::PowerOff(); // 关机
            while (1)
                ; // 保险：停在这里
        }

        // 超过500ms才开始显示提示和进度条
        if (held > 500 && !powerHintShown)
        {
            powerHintShown = true;
        }
    }

    if (!powerPressed && powerLongPressTriggered)
    {
        // 松开了，重置状态（短按不关机）
        powerLongPressTriggered = false;
        powerHintShown = false; // 重置提示显示状态
        drawHintBar();
        canvas->pushSprite(0, 0);
    }

    // 平滑滚动插值
    float diff = targetScrollOffset - currentScrollOffset;
    bool scrolling = (fabsf(diff) > 0.001f);

    if (scrolling)
    {
        currentScrollOffset += diff * 0.18f;
        if (fabsf(diff) < 0.008f)
            currentScrollOffset = targetScrollOffset;
        Draw();
        canvas->pushSprite(0, 0);
    }
    else
    {
        // 右侧：长按进度指示（如果正在按）
        if (powerLongPressTriggered && powerHintShown)
        {
            unsigned long held = millis() - powerPressStart;
            int barW = ((held - 500) * 80) / 1500; // 从500ms到2000ms，共1500ms内完成进度条
            if (barW > 80)
                barW = 80;

            uint16_t barColor = (held > 1500) ? rgb565(220, 20, 60) : rgb565(255, 165, 0);
            canvas->fillRect(0, HINT_AREA_Y, SCREEN_WIDTH, HINT_AREA_H, BG_BOTTOM);
            canvas->setTextColor(TFT_WHITE);
            canvas->setCursor(40, HINT_AREA_Y + 8);
            canvas->print("Power off");
            canvas->drawRect(SCREEN_WIDTH - 110, HINT_AREA_Y + 10, 80, 10, barColor);
            canvas->fillRect(SCREEN_WIDTH - 110, HINT_AREA_Y + 10, barW, 10, barColor);
            canvas->pushSprite(0, 0);
        }

        // ---- 不滚动时，每秒重绘一次状态栏（时间 + 连接图标会更新）----
        if (millis() - last_status_draw > 1000)
        {
            last_status_draw = millis();
            drawStatusBar(); // 只重画状态栏 + push
            canvas->pushSprite(0, 0);
        }
    }
}

// ============ 编码器处理 ============
void Launcher::handleRotate(int dir)
{
    unsigned long now = millis();
    if (now - last_enc_time < 50)
        accel_multiplier = 2;
    else if (now - last_enc_time > 150)
        accel_multiplier = 1;
    last_enc_time = now;

    int delta = dir * accel_multiplier;
    int newIndex = selectedIndex + delta;
    if (newIndex < 0)
        newIndex = 0;
    if (newIndex >= MENU_COUNT)
        newIndex = MENU_COUNT - 1;

    if (newIndex != selectedIndex)
    {
        currentScrollOffset = -(newIndex - selectedIndex);
        targetScrollOffset = 0.0f;
        selectedIndex = newIndex;
    }
}

void Launcher::handleShortPress()
{
    if (menuItems[selectedIndex].action)
        menuItems[selectedIndex].action();
}

// ============ 绘制分发 ============
void Launcher::Draw()
{
    drawBackground();
    drawStatusBar();
    drawIconArea();
    drawLabelArea();
    drawHintBar();
}

// ============ 背景颜色 ============
void Launcher::drawBackground()
{
    canvas->fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, BG_TOP);
}

/**
 * 电量显示
 * @param s           LGFX_Sprite 引用
 * @param x,y         左上角坐标
 * @param w,h         电池主体宽高
 * @param percent     电量 0~100
 * @param colorBody   边框颜色（RGB565）
 * @param colorBorder 填充颜色（RGB565）
 * @param showCap     是否画正极小帽
 */
void drawBatteryIcon(LGFX_Sprite *s, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent, uint16_t colorBody, uint16_t colorBorder, bool showCap = true)
{
    if (percent > 100)
        percent = 100;

    // === 电池主体 ===
    s->drawRect(x, y, w, h, colorBorder);

    // === 正极小帽 ===
    if (showCap)
    {
        int16_t capW = 2;
        int16_t capH = h / 3;
        int16_t capX = x + w;
        int16_t capY = y + (h - capH) / 2;
        s->fillRect(capX, capY, capW, capH, colorBorder);
    }

    // === 电量填充 ===
    int16_t fillW = ((w - 4) * percent) / 100;
    if (fillW > 0)
    {
        uint16_t color = colorBody;
        if (percent <= 20)
            color = rgb565(220, 20, 60); // 红
        else if (percent <= 50)
            color = rgb565(255, 165, 0); // 橙

        s->fillRect(x + 2, y + 2, fillW, h - 4, color);
    }

    // === 电量数字（可选）===
    // s->setTextColor(colorBody);
    // s->setTextSize(1);
    // s->drawNumber(percent, x + w / 2 - 6, y + h / 2 - 4);
}

// ============ WiFi 小图标（16x16）============
void drawWiFiIcon(LGFX_Sprite *s, int16_t x, int16_t y, bool connected)
{
    uint16_t col = connected ? WIFI_ON_COL : WIFI_OFF_COL;

    int icon_length = 32;
    float scale = 0.6;
    int newSize = (int)(icon_length * scale);

    // const uint16_t *src = icon_wifi_small;
    const uint16_t *src = icon_wifi_thin;

    for (int dy = 0; dy < newSize; dy++)
    {
        for (int dx = 0; dx < newSize; dx++)
        {
            int sx = (dx * icon_length) / newSize;
            int sy = (dy * icon_length) / newSize;

            uint16_t pixel = pgm_read_word(&src[sy * icon_length + sx]);
            if (pixel == 0x0000) // 透明
                continue;

            uint8_t sr = (pixel >> 11) & 0x1F;
            uint8_t sg = (pixel >> 5) & 0x3F;
            uint8_t sb = pixel & 0x1F;

            s->drawPixel(x + dx, y + dy, (sr << 11) | (sg << 5) | sb);
        }
    }
}

// ============ 蓝牙小图标 ============
void drawBTIcon(LGFX_Sprite &s, int16_t x, int16_t y, bool connected)
{
    uint16_t col = connected ? BT_ON_COL : BT_OFF_COL;
}

// ============ 状态栏 ============
void Launcher::drawStatusBar()
{
    canvas->fillRect(0, STATUS_Y, SCREEN_WIDTH, STATUS_H, STATUS_BG);
    canvas->setTextSize(1);
    canvas->setFont(&FreeSans9pt7b);

    // ---- 中间：时间（从 HAL 取真实 RTC/系统时间）----
    tm *now = HAL::GetLocalTime();
    char timeBuf[6];
    if (now && now->tm_year > 100) // 有效时间（year > 2000）
    {
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", now->tm_hour, now->tm_min);
    }
    else // RTC 未初始化，显示 "--:--"
    {
        snprintf(timeBuf, sizeof(timeBuf), "--:--");
    }

    canvas->setTextColor(STATUS_TIME_COL);
    int16_t timeW = canvas->textWidth(timeBuf);
    canvas->setCursor(CENTER_X - timeW / 2, 8);
    canvas->print(timeBuf);

    // 左侧：WiFi + 蓝牙图标
    int16_t rx = 20;
    int16_t iy = 3;

    bool wifiOn = true;
    bool btOn = true;

    drawWiFiIcon(canvas, rx, iy, wifiOn);
    // drawBTIcon(canvas, rx + 24, iy, btOn);

    // 右侧：电量显示
    uint8_t bat = HAL::getVoltage();
    drawBatteryIcon(canvas, SCREEN_WIDTH - 52, 7, 24, 14, bat, rgb565(0, 200, 0), rgb565(200, 200, 200), true);
}

// ============ 图标流 ============
void Launcher::drawIconArea()
{
    canvas->fillRect(0, ICON_AREA_Y, SCREEN_WIDTH, ICON_AREA_H, BG_TOP);

    int centerY = ICON_AREA_Y + ICON_AREA_H / 2;
    float currentSelection = selectedIndex + currentScrollOffset;

    for (int slot = -2; slot <= 2; slot++)
    {
        int itemIndex = (int)roundf(currentSelection) + slot;
        if (itemIndex < 0 || itemIndex >= MENU_COUNT)
            continue;

        int slotIndex = slot + 2;
        if (slotIndex < 0 || slotIndex >= 5)
            continue;
        const SlotParams &p = slotParams[slotIndex];

        float relativePos = itemIndex - currentSelection;
        int x = CENTER_X + (int)(relativePos * SLOT_SPACING);
        int y = centerY + p.yOffset;

        int iconSize = (int)(ICON_SRC_SIZE * p.scale);
        if (x + iconSize < -60 || x > SCREEN_WIDTH + 60)
            continue;

        drawScaledIcon(canvas, x - iconSize / 2, y - iconSize / 2, p.scale, menuItems[itemIndex].icon, false);
    }
}

// ============ 标签 ============
void Launcher::drawLabelArea()
{
    canvas->fillRect(0, LABEL_AREA_Y, SCREEN_WIDTH, LABEL_AREA_H, BG_TOP);

    const char *label = menuItems[selectedIndex].label;
    canvas->setTextColor(TFT_WHITE);
    canvas->setTextSize(1);
    canvas->setFont(&FreeSansBold12pt7b);

    int16_t w = canvas->textWidth(label);
    canvas->setCursor(CENTER_X - w / 2, LABEL_AREA_Y + 10);
    canvas->print(label);
}

// ============ 提示栏 ============
void Launcher::drawHintBar()
{
    canvas->fillRect(0, HINT_AREA_Y, SCREEN_WIDTH, HINT_AREA_H, BG_BOTTOM);
    canvas->setTextColor(TEXT_DIM);
    canvas->setTextSize(1);
    canvas->setFont(&FreeSans9pt7b);
    canvas->setCursor(40, HINT_AREA_Y + 8);
    canvas->print("SP: Select  LP: Exit");
}

// ============ Action 回调 ============
void actionScreenSaverEntry(void) { App_RequestScreenSaver(); }
void actionDialEntry(void) { App_RequestDial(); }
void actionLedEntry(void) { App_RequestLED(); }
void actionRemoteEntry(void) { App_RequestRemote(); }
void actionMusic(void) { App_RequestMusic(); }
void actionWeather(void) { App_RequestWeather(); }
void actionAirMouse(void) { App_RequestAirMouse(); }
void actionGyroscope(void) { App_RequestGyroscope(); }
void actionCompass(void) { App_RequestCompass(); }
void actionBarometer(void) { App_RequestBarometer(); }
void actionGame(void) { App_RequestGame(); }
void actionFiles(void) { App_RequestFile(); }
void actionSDCardEntry(void) { App_RequestSD(); }
void actionWiFiEntry(void) { App_RequestWiFi(); }
void actionSettingsEntry(void) { App_RequestSettings(); }
void actionAboutEntry(void) { App_RequestAbout(); }
