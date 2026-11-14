#include "WiFiManagerApp.h"

#include <math.h>

WiFiManagerApp g_WiFiManagerApp;

// ============ 配色（esp-claw 暗色卡片风）============
static const uint16_t COL_BG = rgb565(10, 11, 14);       // #0a0b0e
static const uint16_t COL_SURFACE = rgb565(17, 19, 24);  // #111318
static const uint16_t COL_CARD = rgb565(22, 25, 32);     // #161920
static const uint16_t COL_BORDER = rgb565(45, 49, 58);   // 白 10% 叠在卡片上
static const uint16_t COL_TEXT = rgb565(232, 234, 240);  // #e8eaf0
static const uint16_t COL_TEXT2 = rgb565(154, 160, 176); // #9aa0b0
static const uint16_t COL_MUTED = rgb565(86, 91, 107);   // #565b6b
static const uint16_t COL_ACCENT = rgb565(232, 54, 45);  // #e8362d
static const uint16_t COL_GREEN = rgb565(104, 211, 145); // #68d391
static const uint16_t COL_ORANGE = rgb565(246, 173, 85); // #f6ad55
static const uint16_t COL_DANGER = rgb565(248, 113, 113); // #f87171

// ============ 布局参数 ============
static const int HEADER_H = 44;
static const int MARGIN = 12;
static const int CARD_X = MARGIN;
static const int CARD_W = SCREEN_WIDTH - MARGIN * 2;
static const int CARD_R = 12;
static const int HINT_Y = SCREEN_HEIGHT - 12;

// ============ 时序参数 ============
static const uint32_t POLL_MS = 40;          // HAL 轮询间隔（门户处理 + 连接状态）
static const uint32_t PROMPT_DRAW_MS = 200;  // 提示页重绘（倒计时）
static const uint32_t PORTAL_DRAW_MS = 250;  // 配网页重绘（等待动画）
static const uint32_t INFO_DRAW_MS = 1000;   // 信息页刷新（信号 / 在线时长）
static const uint32_t PROMPT_SAVED_MS = 8000; // 有保存配置：先等 STA 连上
static const uint32_t PROMPT_NONE_MS = 4000;  // 没配置：稍等一下再进配网

// ============ 小工具 ============
// 按可用宽度截断字符串，超出部分换成 ".."
static void fitText(LGFX_Sprite *c, const char *src, int maxW, char *out, size_t outSize)
{
    snprintf(out, outSize, "%s", src ? src : "");
    if (c->textWidth(out) <= maxW)
        return;

    size_t len = strlen(out);
    while (len > 1)
    {
        len--;
        out[len] = '\0';
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%s..", out);
        if (c->textWidth(tmp) <= maxW)
        {
            snprintf(out, outSize, "%s", tmp);
            return;
        }
    }
}

// 秒 -> "1h 02m" / "3m 05s" / "42s"
static void fmtDuration(uint32_t sec, char *buf, size_t bufSize)
{
    if (sec >= 3600)
        snprintf(buf, bufSize, "%uh %02um", (unsigned)(sec / 3600), (unsigned)((sec % 3600) / 60));
    else if (sec >= 60)
        snprintf(buf, bufSize, "%um %02us", (unsigned)(sec / 60), (unsigned)(sec % 60));
    else
        snprintf(buf, bufSize, "%us", (unsigned)sec);
}

// 信号强度 -> 1~4 格
static int rssiLevel(int rssi)
{
    if (rssi >= -55)
        return 4;
    if (rssi >= -65)
        return 3;
    if (rssi >= -75)
        return 2;
    return 1;
}

// ============ 生命周期 ============
void WiFiManagerApp::Main()
{
    Init();

    while (!exitRequest)
    {
        Update();
        delay(10);
    }

    Exit();
}

void WiFiManagerApp::Init()
{
    disp = HAL::GetDisplay();
    canvas = HAL::GetCanvas();

    exitRequest = false;
    powerWasDown = false;
    dots = 0;
    lastPoll = 0;
    lastDraw = 0;

    HAL::Wifi_Init();
    HAL::Wifi_Connect(); // 有保存配置就用它发起连接，没配置则保持未连接

    if (HAL::Wifi_IsConnected())
    {
        mode = UI_INFO;
        modeStart = millis();
        drawInfo();
        push();
    }
    else
    {
        enterPrompt();
    }
}

void WiFiManagerApp::Update()
{
    const uint32_t now = millis();

    // 门户 / 连接状态轮询
    if (now - lastPoll >= POLL_MS)
    {
        lastPoll = now;
        HAL::Wifi_Update();
    }

    // 连上了就切到网络信息页
    if (mode != UI_INFO && HAL::Wifi_IsConnected())
        enterInfo();

    switch (mode)
    {
    case UI_PROMPT:
        if (promptDelayMs > 0 && now - modeStart >= promptDelayMs)
        {
            enterPortal();
        }
        else if (now - lastDraw >= PROMPT_DRAW_MS)
        {
            lastDraw = now;
            drawPrompt();
            push();
        }
        break;

    case UI_PORTAL:
        if (!HAL::Wifi_IsPortalActive())
        {
            enterPrompt(); // 热点没起来 / 被关掉，退回提示页重试
        }
        else if (now - lastDraw >= PORTAL_DRAW_MS)
        {
            lastDraw = now;
            dots = (dots + 1) & 3; // 等待动画步进
            drawPortal();
            push();
        }
        break;

    case UI_INFO:
        if (!HAL::Wifi_IsConnected())
        {
            enterPrompt(); // 断线了：退回提示页，重新尝试或重新配网
        }
        else if (now - lastDraw >= INFO_DRAW_MS)
        {
            lastDraw = now;
            drawInfo();
            push();
        }
        break;

    default:
        break;
    }

    uint8_t key = 0;
    if (HAL::getEncoder(&key))
        handleKey(key);

    // 电源键为电平信号，这里做按下沿检测
    const bool powerDown = HAL::isReturn();
    if (powerDown && !powerWasDown)
        handlePower();
    powerWasDown = powerDown;
}

void WiFiManagerApp::Exit()
{
    // 离开 App 默认关射频省电；Settings 里打开 "Keep WiFi" 时保持连接
    HAL::Wifi_StopPortal();
    if (!HAL::isWifiKeepAlive())
        HAL::Wifi_Disconnect();

    canvas->setTextDatum(0);
}

// ============ 页面切换 ============
void WiFiManagerApp::enterPrompt(bool autoStart)
{
    mode = UI_PROMPT;
    modeStart = millis();
    lastDraw = 0;
    // autoStart=false（手动取消配网 / 忘记网络后）不再自动进配网，等用户按 OK
    promptDelayMs = autoStart ? (HAL::Wifi_HasSaved() ? PROMPT_SAVED_MS : PROMPT_NONE_MS) : 0;

    drawPrompt();
    push();
}

void WiFiManagerApp::enterPortal()
{
    if (!HAL::Wifi_StartPortal())
    {
        // 热点没起来：留在提示页，8 秒后再试一次
        promptDelayMs = 8000;
        modeStart = millis();
        lastDraw = 0;
        drawPrompt();
        push();
        return;
    }

    mode = UI_PORTAL;
    modeStart = millis();
    lastDraw = 0;
    dots = 0;

    HAL::beep(900, 20);
    drawPortal();
    push();
}

void WiFiManagerApp::enterInfo()
{
    HAL::Wifi_StopPortal(); // 连上就关 AP，省电
    HAL::beep(1200, 30);

    mode = UI_INFO;
    modeStart = millis();
    lastDraw = 0;

    drawInfo();
    push();
}

// ============ 交互 ============
void WiFiManagerApp::handleKey(uint8_t key)
{
    switch (key)
    {
    case BTN_OK_SP:
        if (mode == UI_PROMPT)
        {
            HAL::beep(800, 20);
            enterPortal();
        }
        else if (mode == UI_INFO)
        {
            HAL::beep(800, 20);
            enterPortal(); // 重新配网
        }
        // 配网页里短按不动作，避免误触把热点关掉
        break;

    case BTN_OK_LP:
        if (mode == UI_PORTAL)
        {
            // 放弃配网：关热点，回到提示页（不再自动倒计时）
            HAL::beep(500, 40);
            HAL::Wifi_StopPortal();
            HAL::Wifi_Connect();
            enterPrompt(false);
        }
        else if (mode == UI_INFO)
        {
            // 忘记网络：清掉 NVS 里的配置
            HAL::beep(500, 40);
            HAL::Wifi_Forget();
            HAL::Wifi_Connect();
            enterPrompt(false);
        }
        break;

    default:
        break;
    }
}

void WiFiManagerApp::handlePower()
{
    // 配网页里按电源键：关热点后直接退出 App
    if (mode == UI_PORTAL)
    {
        HAL::Wifi_StopPortal();
        HAL::Wifi_Connect();
    }

    exitRequest = true;
}

// ============ 绘制：通用 ============
void WiFiManagerApp::push()
{
    canvas->pushSprite(0, 0);
}

void WiFiManagerApp::drawHeader(const char *title)
{
    canvas->fillScreen(COL_BG);
    canvas->fillRect(0, 0, SCREEN_WIDTH, HEADER_H, COL_SURFACE);

    canvas->setFont(&fonts::FreeSansBold12pt7b);
    canvas->setTextDatum(MC_DATUM);
    canvas->setTextColor(COL_TEXT);
    canvas->drawString(title, SCREEN_WIDTH / 2, HEADER_H / 2);

    canvas->drawFastHLine(0, HEADER_H, SCREEN_WIDTH, COL_BORDER);
}

void WiFiManagerApp::drawHint(const char *text)
{
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextDatum(MC_DATUM);
    canvas->setTextColor(COL_MUTED);
    canvas->drawString(text, SCREEN_WIDTH / 2, HINT_Y);
    canvas->setTextDatum(0);
}

void WiFiManagerApp::drawPill(int x, int y, const char *text, uint16_t color)
{
    canvas->setFont(&fonts::FreeSansBold9pt7b);
    const int w = canvas->textWidth(text) + 30;
    const int h = 22;

    canvas->fillRoundRect(x, y, w, h, h / 2, COL_CARD);
    canvas->drawRoundRect(x, y, w, h, h / 2, color);
    canvas->fillCircle(x + 12, y + h / 2, 3, color);

    canvas->setTextDatum(ML_DATUM);
    canvas->setTextColor(color);
    canvas->drawString(text, x + 20, y + h / 2);
    canvas->setTextDatum(0);
}

void WiFiManagerApp::drawCard(int x, int y, int w, int h)
{
    canvas->fillRoundRect(x, y, w, h, CARD_R, COL_CARD);
    canvas->drawRoundRect(x, y, w, h, CARD_R, COL_BORDER);
}

// 三段圆弧 + 圆心点，模仿 WiFi 扇形图标；slash 为 true 时加一条禁止斜线
void WiFiManagerApp::drawWifiGlyph(int cx, int cy, int r, uint16_t color, bool slash)
{
    for (int ring = 0; ring < 3; ring++)
    {
        const int rad = r - ring * (r / 3);
        // 数学角 225°..315° 覆盖正上方，屏幕 y 向下需取负 sin
        for (float a = 225.0f; a <= 315.0f; a += 1.2f)
        {
            const float ar = a * 0.0174533f;
            const int px = cx + (int)lroundf(cosf(ar) * (float)rad);
            const int py = cy + (int)lroundf(sinf(ar) * (float)rad);
            canvas->fillCircle(px, py, 1, color);
        }
    }

    // 圆心点（扇形发射点）
    canvas->fillCircle(cx, cy, 3, color);

    if (slash)
    {
        // 禁止斜线：从圆心沿 60° 向上，长度等于半径 r，末端正好落到扇形外缘；
        // 宽度由圆心 1.5px 渐粗到圆弧处 4.5px（用楔形线画）
        const float rad = 60.0f * 0.0174533f;
        const float x1 = cx + cosf(rad) * (float)r;
        const float y1 = cy - sinf(rad) * (float)r;
        drawWedgeLine(canvas, cx, cy, x1, y1, 1.5f, 4.5f, COL_DANGER, COL_BG);
    }
}

void WiFiManagerApp::drawSignalBars(int x, int yBottom, int rssi)
{
    const int level = rssiLevel(rssi);
    const int barW = 3;
    const int gap = 2;
    const int heights[4] = {5, 8, 11, 14};

    for (int i = 0; i < 4; i++)
    {
        const int bx = x + i * (barW + gap);
        const int bh = heights[i];
        canvas->fillRect(bx, yBottom - bh, barW, bh, i < level ? COL_GREEN : COL_BORDER);
    }
}

// ============ 绘制：未连接提示页 ============
void WiFiManagerApp::drawPrompt()
{
    drawHeader("WiFi");

    const bool saved = HAL::Wifi_HasSaved();

    drawWifiGlyph(SCREEN_WIDTH / 2, 92, 30, saved ? COL_ORANGE : COL_DANGER, true);

    canvas->setFont(&fonts::FreeSansBold12pt7b);
    canvas->setTextDatum(MC_DATUM);
    canvas->setTextColor(COL_TEXT);
    canvas->drawString("Not connected", SCREEN_WIDTH / 2, 140);

    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextColor(COL_TEXT2);
    canvas->drawString(saved ? "Trying saved network..." : "No network configured",
                       SCREEN_WIDTH / 2, 166);

    // 提示卡片：自动倒计时 / 手动等待
    const int cy = 190;
    const int ch = 48;
    drawCard(CARD_X, cy, CARD_W, ch);

    if (promptDelayMs == 0)
    {
        canvas->setFont(&fonts::FreeSans9pt7b);
        canvas->setTextDatum(MC_DATUM);
        canvas->setTextColor(COL_ACCENT);
        canvas->drawString("Press OK to start AP setup", SCREEN_WIDTH / 2, cy + ch / 2);
    }
    else
    {
        const uint32_t elapsed = millis() - modeStart;
        uint32_t left = 0;
        if (promptDelayMs > elapsed)
            left = (promptDelayMs - elapsed + 999) / 1000;

        char buf[16];
        snprintf(buf, sizeof(buf), "%us", (unsigned)left);

        canvas->setFont(&fonts::FreeSans9pt7b);
        canvas->setTextDatum(ML_DATUM);
        canvas->setTextColor(COL_TEXT2);
        canvas->drawString("AP setup starts in", CARD_X + 14, cy + 20);

        canvas->setFont(&fonts::FreeSansBold9pt7b);
        canvas->setTextDatum(MR_DATUM);
        canvas->setTextColor(COL_ACCENT);
        canvas->drawString(buf, CARD_X + CARD_W - 14, cy + 20);

        // 进度条
        const int barX = CARD_X + 14;
        const int barW = CARD_W - 28;
        const int barY = cy + 36;
        int fillW = 0;
        if (promptDelayMs > 0 && elapsed < promptDelayMs)
            fillW = (int)((uint64_t)barW * elapsed / promptDelayMs);

        canvas->fillRoundRect(barX, barY, barW, 4, 2, COL_BORDER);
        if (fillW > 2)
            canvas->fillRoundRect(barX, barY, fillW, 4, 2, COL_ACCENT);
    }

    drawHint("OK: setup   PWR: back");
}

// ============ 绘制：AP 配网页 ============
void WiFiManagerApp::drawPortal()
{
    drawHeader("WiFi Setup");

    drawPill(MARGIN, 52, "AP ACTIVE", COL_ORANGE);

    WifiInfo_t info;
    HAL::Wifi_GetInfo(&info);

    // 热点信息
    const int c1y = 80;
    const int c1h = 82;
    drawCard(CARD_X, c1y, CARD_W, c1h);

    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextDatum(ML_DATUM);
    canvas->setTextColor(COL_MUTED);
    canvas->drawString("HOTSPOT", CARD_X + 14, c1y + 18);

    // 热点名较长，用 9pt 粗体，避免贴到卡片边缘
    canvas->setFont(&fonts::FreeSansBold9pt7b);
    canvas->setTextColor(COL_TEXT);
    canvas->drawString(info.apSsid, CARD_X + 14, c1y + 40);

    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextColor(COL_MUTED);
    canvas->drawString("No password needed", CARD_X + 14, c1y + 62);

    // 配置网址 + 等待状态
    const int c2y = 168;
    const int c2h = 76;
    drawCard(CARD_X, c2y, CARD_W, c2h);

    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextDatum(ML_DATUM);
    canvas->setTextColor(COL_MUTED);
    canvas->drawString("CONFIG PAGE", CARD_X + 14, c2y + 18);

    canvas->setFont(&fonts::FreeSansBold9pt7b);
    canvas->setTextColor(COL_ACCENT);
    canvas->drawString("http://192.168.4.1", CARD_X + 14, c2y + 40);

    // 等待动画：放在网址卡片底部，避免与底部按键提示挤在一起
    const char *dotsText[] = {"Waiting for client", "Waiting for client.", "Waiting for client..", "Waiting for client..."};
    canvas->setFont(&fonts::FreeSans9pt7b);
    canvas->setTextColor(COL_TEXT2);
    canvas->drawString(dotsText[dots & 3], CARD_X + 14, c2y + 62);

    drawHint("HOLD: cancel   PWR: back");
}

// ============ 绘制：网络信息页 ============
void WiFiManagerApp::drawInfo()
{
    drawHeader("Network");

    WifiInfo_t info;
    HAL::Wifi_GetInfo(&info);

    drawPill(MARGIN, 52, "CONNECTED", COL_GREEN);

    const int cardY = 80;
    const int rowH = 27;
    const int rowCount = 6;
    const int cardH = rowH * rowCount;
    drawCard(CARD_X, cardY, CARD_W, cardH);

    char online[24];
    fmtDuration(info.onlineSec, online, sizeof(online));

    char signal[24];
    snprintf(signal, sizeof(signal), "%d dBm", (int)info.rssi);

    char channel[8];
    snprintf(channel, sizeof(channel), "%u", (unsigned)info.channel);

    const char *labels[rowCount] = {"SSID", "IP", "Gateway", "Channel", "Signal", "Online"};
    const char *values[rowCount] = {
        info.ssid[0] ? info.ssid : "-",
        info.ip[0] ? info.ip : "-",
        info.gateway[0] ? info.gateway : "-",
        channel,
        signal,
        online,
    };

    for (int i = 0; i < rowCount; i++)
    {
        const int cy = cardY + rowH / 2 + i * rowH;

        canvas->setFont(&fonts::FreeSans9pt7b);
        canvas->setTextDatum(ML_DATUM);
        canvas->setTextColor(COL_MUTED);
        canvas->drawString(labels[i], CARD_X + 14, cy);

        // 信号行：右侧画信号格，数值左移
        const int valueRight = CARD_X + CARD_W - 14;
        int textRight = valueRight;
        if (i == 4)
        {
            drawSignalBars(valueRight - 18, cy + 7, info.rssi);
            textRight = valueRight - 28;
        }

        // 可用宽度 = 值区宽度 - 左侧标题宽度 - 间距；截断与绘制用同一字体测量
        int maxW = textRight - (CARD_X + 14 + canvas->textWidth(labels[i]) + 12);
        if (maxW < 40)
            maxW = 40;

        char trimmed[48];
        fitText(canvas, values[i], maxW, trimmed, sizeof(trimmed));

        canvas->setTextDatum(MR_DATUM);
        canvas->setTextColor(i == 0 ? COL_TEXT : COL_TEXT2);
        canvas->drawString(trimmed, textRight, cy);

        if (i < rowCount - 1)
            canvas->drawFastHLine(CARD_X + 14, cardY + (i + 1) * rowH, CARD_W - 28, COL_BORDER);
    }

    drawHint("OK: re-setup   HOLD: forget");
}