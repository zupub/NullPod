/*
 * TV Remote App - 电视遥控器
 *
 * 页面：
 *   TV_HOME  - 环形音量/频道/输入源调节
 *   TV_PAD   - 十字方向键导航
 *   TV_LIST  - 频道列表
 *   TV_MACRO - 情景模式宏
 *   TV_LEARN - 红外学习（接收+波形显示）
 *
 * 布局约定：
 *   顶部 PAGE_HEADER_H(32px) 为页面头部，由 drawPageHeader() 绘制：
 *     左对齐页面标题 + 右侧电源状态灯
 *   各页面内容从 PAGE_HEADER_H 之下开始绘制（原 drawStatusBar 区域）
 *
 * 操作：
 *   旋转编码器 → 切换调节对象 / 导航（HOME/LIST/MACRO 页）
 *   上下左右方向键 → 各页面直接控制（PAD 页发送对应方向 IR 码 + 按钮动画）
 *   BTN_OK_SP (编码器短按) → 发送确认键值(OK) / 学习页开始学习
 *   BTN_OK_LP (编码器长按) → 发送返回键值(left)
 *   BTN_POWER 短按(防抖) → 发送电源键 + 循环切换页面
 *   BTN_POWER 长按(500ms防抖) → 退出 App 返回 Launcher
 *
 * 退出机制：Main 循环检查全局 volatile bool appExitRequested
 *
 * 学习页操作：
 *   进入学习页后自动开始监听 IR 信号
 *   收到信号后显示协议、地址、命令、原始数据及波形
 *   短按 → 重新开始学习
 *   长按 → 返回 HOME
 */

#include "RemoteTV.h"
#include <IRremote.hpp>

#define HAL_PIN_IR_RX HAL_PIN_RGB

// ============ 全局实例 ============
RemoteTV g_remoteTV;
volatile bool appExitRequested = false;

// 输入源名称
static const char *sourceNames[TV_SRC_COUNT] = {"HDMI 1", "HDMI 2", "AV", "TV"};

// 情景宏名称
static const char *macroNames[4] = {"Movie", "Gaming", "Music", "Sleep"};

// 快捷功能标签
static const char *quickLabels[5] = {"PWR", "MUTE", "SRC", "VOL", "CH"};

// 快捷功能颜色
static const uint16_t funcColors[5] = {
    rgb565(255, 80, 80),  // PWR
    rgb565(180, 40, 180), // MUTE
    rgb565(0, 200, 180),  // SRC
    rgb565(0, 168, 107),  // VOL
    rgb565(93, 63, 211),  // CH
};

// PAD 焦点标签
static const char *padLabels[5] = {"UP", "RIGHT", "DOWN", "LEFT", "OK"};

// PAD 焦点状态
static int macroFocus = 0;

// ============ Init ============
void RemoteTV::Init()
{
    if (active)
        return;
    active = true;
    appExitRequested = false;

    disp = HAL::GetDisplay();
    canvas = HAL::GetCanvas();

    page = TV_HOME;
    adjTarget = TV_ADJ_PWR;
    volume = 18;
    channel = 7;
    source = TV_SRC_HDMI1;
    powerOn = true;
    muted = false;
    sendPulse = false;
    last_enc_time = 0;
    accel_multiplier = 1;
    last_dir = 0;
    for (int i = 0; i < PAD_BTN_COUNT; i++)
    {
        padPressed[i] = false;
        padPressedTick[i] = 0;
    }
    learnWaiting = false;
    memset(&learnData, 0, sizeof(learnData));

    // ===== BTN_POWER 防抖状态复位 =====
    powerPressed = false;
    powerPressStart = 0;
    powerLongFired = false;

    // ---- 默认设备：Hisense NEC 码 ----
    device.name = "Hisense TV";
    device.power = {"Power", IR_PROTO_NEC, 0xBF, 0x0D};
    device.mute = {"Mute", IR_PROTO_NEC, 0xBF, 0x0E};
    device.home = {"Home", IR_PROTO_NEC, 0xBF, 0x94};
    device.src = {"Source", IR_PROTO_NEC, 0xBF, 0x12};
    device.menu = {"Menu", IR_PROTO_NEC, 0xBF, 0x14};
    device.volUp = {"Vol+", IR_PROTO_NEC, 0xBF, 0x44};
    device.volDown = {"Vol-", IR_PROTO_NEC, 0xBF, 0x43};
    device.chUp = {"Ch+", IR_PROTO_NEC, 0xBF, 0x00};
    device.chDown = {"Ch-", IR_PROTO_NEC, 0xBF, 0x01};
    device.up = {"Up", IR_PROTO_NEC, 0xBF, 0x16};
    device.down = {"Down", IR_PROTO_NEC, 0xBF, 0x17};
    device.left = {"Left", IR_PROTO_NEC, 0xBF, 0x19};
    device.right = {"Right", IR_PROTO_NEC, 0xBF, 0x18};
    device.ok = {"OK", IR_PROTO_NEC, 0xBF, 0x15};
    device.back = {"Back", IR_PROTO_NEC, 0xBF, 0x48};

    Draw();

    // 初始化 IR 发送
    IrSender.begin(HAL_PIN_IR);

    // 初始化 IR 接收
    IrReceiver.begin(HAL_PIN_IR_RX, DISABLE_LED_FEEDBACK);

    Serial.println(">>> RemoteTV ready");
    Serial.printf(">>> IR Receiver on pin %d\n", HAL_PIN_IR_RX);
}

// ============ Exit ============
void RemoteTV::Exit()
{
    active = false;
    IrReceiver.stop();
    Serial.println(">>> RemoteTV exit");
}

// ============ Main（阻塞式生命周期）============
void RemoteTV::Main()
{
    Init();

    while (!appExitRequested)
    {
        Update();
        delay(5);
    }

    Exit();
}

// ============ Update ============
void RemoteTV::Update()
{
    if (!active)
        return;

    static bool rotating = false;
    uint8_t key = 0;

    if (HAL::getEncoder(&key))
    {
        HAL::buzz(500, 15);
        switch (key)
        {
        case BTN_ENC_CW:
            rotating = true;
            handleRotate(+1);
            break;
        case BTN_ENC_CC:
            rotating = true;
            handleRotate(-1);
            break;
        case BTN_OK_SP:
            if (!rotating)
                handleShortPress();
            rotating = false;
            break;
        case BTN_OK_LP:
            handleLongPress();
            rotating = false;
            break;

        // ===== 方向键直接控制（PAD 页触发按钮动画）=====
        case BTN_UP_SP:
            handleDirectionKey(BTN_UP_SP);
            break;
        case BTN_DOWN_SP:
            handleDirectionKey(BTN_DOWN_SP);
            break;
        case BTN_LEFT_SP:
            handleDirectionKey(BTN_LEFT_SP);
            break;
        case BTN_RIGHT_SP:
            handleDirectionKey(BTN_RIGHT_SP);
            break;

        default:
            break;
        }
    }
    else
    {
        rotating = false;
    }

    // ===== BTN_POWER 短按/长按检测（防抖）=====
    {
        static unsigned long lastDebounce = 0;
        static int lastRaw = HIGH;

        int raw = digitalRead(HAL_PIN_PWR_KEY);
        unsigned long now = millis();

        if (raw != lastRaw)
        {
            lastDebounce = now;
        }
        lastRaw = raw;

        if ((now - lastDebounce) > POWER_DEBOUNCE_MS)
        {
            bool pressed = (raw == LOW);

            if (pressed && !powerPressed)
            {
                // 下降沿：开始计时
                powerPressed = true;
                powerPressStart = now;
                powerLongFired = false;
            }
            else if (!pressed && powerPressed)
            {
                // 上升沿：若未触发长按 → 短按 → 发电源码 + 切换页面
                if (!powerLongFired)
                {
                    Serial.println(">>> BTN_POWER short → next page");
                    HAL::beep(500, 15);
                    switch (page)
                    {
                    case TV_HOME:
                        page = TV_PAD;
                        break;
                    case TV_PAD:
                        page = TV_LIST;
                        break;
                    case TV_LIST:
                        page = TV_MACRO;
                        break;
                    case TV_MACRO:
                        page = TV_LEARN;
                        break;
                    case TV_LEARN:
                        page = TV_HOME;
                        break;
                    }
                    // sendPulse = true;
                    // sendPulseStart = now;
                    Draw();
                }
                powerPressed = false;
                powerLongFired = false;
            }

            // 持续按下超过阈值 → 长按 → 退出 App
            if (powerPressed && !powerLongFired && (now - powerPressStart) > POWER_LONG_MS)
            {
                powerLongFired = true;
                Serial.println(">>> BTN_POWER long → exit App");
                appExitRequested = true;
            }
        }
    }

    // ===== PAD 按钮按动反馈：自动回弹 =====
    // 按下态在按键瞬间已通过 Draw() 生效；这里仅检测超时并回弹，
    // 且仅在状态发生翻转时才重绘一次，避免每帧刷新拖慢响应。
    if (page == TV_PAD)
    {
        unsigned long now = millis();
        bool needRedraw = false;
        for (int i = 0; i < PAD_BTN_COUNT; i++)
        {
            if (padPressed[i] && (now - padPressedTick[i] >= PAD_PRESS_HOLD_MS))
            {
                padPressed[i] = false;
                needRedraw = true;
            }
        }
        if (needRedraw)
        {
            Draw();
        }
    }

    // 发码脉冲动画衰减
    if (sendPulse)
    {
        if (millis() - sendPulseStart > 120)
        {
            sendPulse = false;
            Draw();
        }
    }

    // 学习页：检查是否收到 IR 信号
    if (page == TV_LEARN)
    {
        static unsigned long lastBlinkRedraw = 0;
        checkLearnIR();
        if (learnWaiting && millis() - lastBlinkRedraw > 300)
        {
            lastBlinkRedraw = millis();
            Draw();
        }
    }
}

// ============ 编码器处理 ============
void RemoteTV::handleRotate(int dir)
{
    // 缓存方向，供短按时使用
    last_dir = dir;

    unsigned long now = millis();
    if (now - last_enc_time < 50)
        accel_multiplier = 2;
    else if (now - last_enc_time > 150)
        accel_multiplier = 1;
    last_enc_time = now;

    int delta = dir * accel_multiplier;

    switch (page)
    {
    case TV_HOME:
    {
        // 根据当前选中的功能，旋转编码器执行对应加减
        switch (adjTarget)
        {
        case TV_ADJ_VOL:
            if (delta > 0)
            {
                sendIR(device.volUp);
                volume++;
            }
            else
            {
                sendIR(device.volDown);
                volume--;
            }
            volume = constrain(volume, 0, 100);
            break;
        case TV_ADJ_CH:
            if (delta > 0)
            {
                sendIR(device.chUp);
                channel++;
            }
            else
            {
                sendIR(device.chDown);
                channel--;
            }
            channel = channel < 1 ? 99 : (channel > 99 ? 1 : channel);
            break;
        case TV_ADJ_SRC:
            source = (TVSource)((int)source + delta);
            if (source < 0)
                source = (TVSource)(TV_SRC_COUNT - 1);
            if (source >= TV_SRC_COUNT)
                source = TV_SRC_HDMI1;
            sendIR(device.src);
            break;
        case TV_ADJ_PWR:
        case TV_ADJ_MUTE:
            // PWR/MUTE 不支持旋转，仅切换焦点
            break;
        }
        sendPulse = true;
        sendPulseStart = millis();
        Draw();
        break;
    }
    case TV_PAD:
    {
        if (delta > 0)
        {
            sendIR(device.up);
        }
        else
        {
            sendIR(device.down);
        }
        break;
    }
    case TV_LIST:
    {
        channel += delta;
        channel = channel < 1 ? 99 : (channel > 99 ? 1 : channel);
        Draw();
        break;
    }
    case TV_MACRO:
    {
        macroFocus += delta;
        macroFocus = macroFocus < 0 ? 3 : (macroFocus > 3 ? 0 : macroFocus);
        Draw();
        break;
    }
    case TV_LEARN:
    {
        // 学习页旋转无操作
        break;
    }
    }
}

// ============ 方向键处理 ============
void RemoteTV::handleDirectionKey(uint8_t key)
{
    switch (page)
    {
    case TV_HOME:
    {
        switch (key)
        {
        case BTN_LEFT_SP:
            // 左键：切换到上一个功能
            adjTarget = (TVAdjustTarget)((int)adjTarget - 1);
            if (adjTarget < 0)
                adjTarget = (TVAdjustTarget)(TV_ADJ_COUNT - 1);
            Draw();
            break;
        case BTN_RIGHT_SP:
            // 右键：切换到下一个功能
            adjTarget = (TVAdjustTarget)((int)adjTarget + 1);
            if (adjTarget >= TV_ADJ_COUNT)
                adjTarget = TV_ADJ_PWR;
            Draw();
            break;
        case BTN_UP_SP:
        case BTN_DOWN_SP:
        {
            int dir = (key == BTN_UP_SP) ? +1 : -1;

            switch (adjTarget)
            {
            case TV_ADJ_VOL:
                sendIR(dir > 0 ? device.volUp : device.volDown);
                volume += dir;
                volume = constrain(volume, 0, 100);
                break;

            case TV_ADJ_CH:
                sendIR(dir > 0 ? device.chUp : device.chDown);
                channel += dir;
                channel = channel < 1 ? 99 : (channel > 99 ? 1 : channel);
                break;

            case TV_ADJ_SRC:
                source = (TVSource)((int)source + dir);
                if (source < 0)
                    source = (TVSource)(TV_SRC_COUNT - 1);
                if (source >= TV_SRC_COUNT)
                    source = TV_SRC_HDMI1;
                sendIR(device.src);
                break;

            case TV_ADJ_PWR:
            case TV_ADJ_MUTE:
                // PWR / MUTE 不支持上下调节
                break;
            }
            sendPulse = true;
            sendPulseStart = millis();
            Draw();
            break;
        }
        default:
            break;
        }
        break;
    }
    case TV_PAD:
    {
        // 方向键直接发送对应 IR 码 + 按钮瞬时高亮
        unsigned long now = millis();
        switch (key)
        {
        case BTN_UP_SP:
            padPressed[PAD_BTN_UP] = true;
            padPressedTick[PAD_BTN_UP] = now;
            sendIR(device.up);
            break;
        case BTN_DOWN_SP:
            padPressed[PAD_BTN_DOWN] = true;
            padPressedTick[PAD_BTN_DOWN] = now;
            sendIR(device.down);
            break;
        case BTN_LEFT_SP:
            padPressed[PAD_BTN_LEFT] = true;
            padPressedTick[PAD_BTN_LEFT] = now;
            sendIR(device.left);
            break;
        case BTN_RIGHT_SP:
            padPressed[PAD_BTN_RIGHT] = true;
            padPressedTick[PAD_BTN_RIGHT] = now;
            sendIR(device.right);
            break;
        default:
            break;
        }
        // 按键瞬间重绘一次：高亮色立即生效，之后不再持续刷新
        Draw();
        break;
    }
    case TV_LIST:
    {
        switch (key)
        {
        case BTN_UP_SP:
            channel++;
            if (channel > 99)
                channel = 1;
            break;
        case BTN_DOWN_SP:
            channel--;
            if (channel < 1)
                channel = 99;
            break;
        default:
            break;
        }
        Draw();
        break;
    }
    case TV_MACRO:
    {
        switch (key)
        {
        case BTN_UP_SP:
            macroFocus--;
            if (macroFocus < 0)
                macroFocus = 3;
            break;
        case BTN_DOWN_SP:
            macroFocus++;
            if (macroFocus > 3)
                macroFocus = 0;
            break;
        default:
            break;
        }
        Draw();
        break;
    }
    case TV_LEARN:
    {
        // 学习页方向键无操作
        break;
    }
    }
}

void RemoteTV::handleShortPress()
{
    int dir = last_dir;

    switch (page)
    {
    case TV_HOME:
    {
        switch (adjTarget)
        {
        case TV_ADJ_PWR:
            sendIR(device.power);
            break;
        case TV_ADJ_MUTE:
            sendIR(device.mute);
            muted = !muted;
            break;
        case TV_ADJ_SRC:
        case TV_ADJ_VOL:
        case TV_ADJ_CH:
            // 旋转已处理加减，短按编码器也可触发一次
            if (adjTarget == TV_ADJ_VOL)
            {
                sendIR(last_dir > 0 ? device.volUp : device.volDown);
                volume += last_dir;
                volume = constrain(volume, 0, 100);
            }
            else if (adjTarget == TV_ADJ_CH)
            {
                sendIR(last_dir > 0 ? device.chUp : device.chDown);
                channel += last_dir;
                channel = channel < 1 ? 99 : (channel > 99 ? 1 : channel);
            }
            else if (adjTarget == TV_ADJ_SRC)
            {
                source = (TVSource)((int)source + 1);
                if (source >= TV_SRC_COUNT)
                    source = TV_SRC_HDMI1;
                sendIR(device.src);
            }
            break;
        }
        sendPulse = true;
        sendPulseStart = millis();
        Draw();
        break;
    }
    case TV_PAD:
    {
        // BTN_OK_SP → 发送确认键值 + OK 按钮瞬时高亮（仅颜色变化）
        padPressed[PAD_BTN_OK] = true;
        padPressedTick[PAD_BTN_OK] = millis();
        sendIR(device.ok);
        Draw();
        break;
    }
    case TV_LIST:
    {
        sendIR(dir > 0 ? device.chUp : device.chDown);
        sendPulse = true;
        sendPulseStart = millis();
        Draw();
        break;
    }
    case TV_MACRO:
    {
        sendMacro(macroFocus);
        sendPulse = true;
        sendPulseStart = millis();
        Draw();
        break;
    }
    case TV_LEARN:
    {
        // 短按：重新开始学习（清除旧数据，重新监听）
        learnWaiting = true;
        memset(&learnData, 0, sizeof(learnData));
        IrReceiver.resume();
        Serial.println(">>> Learn: waiting for IR signal...");
        Draw();
        break;
    }
    }
}

void RemoteTV::handleLongPress()
{
    // BTN_OK_LP → 发送"返回"键值（device.back）
    sendIR(device.back);
    sendPulse = true;
    sendPulseStart = millis();
    Draw();
}

// ============ IR 发送 ============
void RemoteTV::sendIR(const IRKey &key)
{
    Serial.printf(">>> IR: %-6s proto=%d addr=0x%02X cmd=0x%02X\n",
                  key.label, key.protocol, key.addr, key.cmd);

    IrSender.sendNEC((uint16_t)(key.addr << 8), key.cmd, 0);
}

void RemoteTV::sendMacro(int idx)
{
    Serial.printf(">>> Macro: %s\n", macroNames[idx]);

    switch (idx)
    {
    case 0: // Movie → 开机 + HDMI1 + 音量18
        sendIR(device.power);
        delay(80);
        sendIR(device.src);
        delay(80);
        for (int i = 0; i < 18; i++)
        {
            sendIR(device.volUp);
            delay(35);
        }
        break;
    case 1: // Gaming → 开机 + HDMI2 + 音量22
        sendIR(device.power);
        delay(80);
        sendIR(device.src);
        delay(80);
        for (int i = 0; i < 22; i++)
        {
            sendIR(device.volUp);
            delay(35);
        }
        break;
    case 2: // Music → 音量降到8
        for (int i = 0; i < 30; i++)
        {
            sendIR(device.volDown);
            delay(35);
        }
        break;
    case 3: // Sleep → 静音 + 关机
        sendIR(device.mute);
        delay(100);
        sendIR(device.power);
        break;
    }
}

// ============ Draw 分发 ============
void RemoteTV::Draw()
{
    canvas->fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, MENU_BG_COLOR);

    switch (page)
    {
    case TV_HOME:
        drawHome();
        break;
    case TV_PAD:
        drawPad();
        break;
    case TV_LIST:
        drawList();
        break;
    case TV_MACRO:
        drawMacro();
        break;
    case TV_LEARN:
        drawLearn();
        break;
    }

    canvas->pushSprite(0, 0);
}

// ============ 通用页面头部 ============
void RemoteTV::drawPageHeader(const char *title)
{
    // 头部背景条
    canvas->fillRect(0, 0, SCREEN_WIDTH, PAGE_HEADER_H, rgb565(28, 28, 50));

    canvas->setTextColor(TFT_WHITE);
    canvas->setTextSize(1);
    canvas->setFont(&FreeSansBold12pt7b);

    // 标题居中
    int16_t w = canvas->textWidth(title);
    canvas->setCursor(CX - w / 2, 8);
    canvas->print(title);

    // 右侧电源状态灯
    uint16_t pwrColor = powerOn ? rgb565(0, 200, 0) : rgb565(180, 0, 0);
    int ledX = SCREEN_WIDTH - 28;
    int ledY = PAGE_HEADER_H / 2;
    canvas->fillCircle(ledX, ledY, 5, pwrColor);
    canvas->drawCircle(ledX, ledY, 5, TFT_WHITE);
}

// ============ 通用提示栏 ============
void RemoteTV::drawHintBar(const char *hint)
{
    canvas->fillRect(0, MENU_STATUS_Y, SCREEN_WIDTH, MENU_STATUS_HEIGHT, rgb565(28, 28, 50));
    canvas->setTextColor(MENU_HINT_COLOR);
    canvas->setTextSize(1);
    canvas->setFont(&FreeSans9pt7b);

    // 文字居中
    int16_t w = canvas->textWidth(hint);
    canvas->setCursor(CX - w / 2, MENU_STATUS_Y + 8);
    canvas->print(hint);
}

// ============ HOME 页 ============
void RemoteTV::drawHome()
{
    drawPageHeader("TV Remote");
    // drawDeviceCard();
    drawCenterDial();
    drawQuickBar();
}

// ============ 设备卡 ============
void RemoteTV::drawDeviceCard()
{
    int y = PAGE_HEADER_H + 6, h = 42;
    canvas->fillRoundRect(10, y, SCREEN_WIDTH - 20, h, 8, rgb565(35, 35, 60));
    canvas->drawRoundRect(10, y, SCREEN_WIDTH - 20, h, 8, rgb565(60, 60, 100));

    // 电视小图标
    canvas->fillRoundRect(20, y + 8, 28, 18, 3, rgb565(46, 134, 222));
    canvas->drawRoundRect(20, y + 8, 28, 18, 3, TFT_WHITE);
    canvas->fillRect(28, y + 28, 12, 4, rgb565(100, 100, 130));

    // 设备名
    canvas->setTextColor(TFT_WHITE);
    canvas->setTextSize(1);
    canvas->setFont(&FreeSansBold12pt7b);
    canvas->setCursor(58, y + 18);
    canvas->print(device.name);

    // 输入源
    canvas->setTextColor(rgb565(0, 200, 180));
    canvas->setFont(&FreeSans9pt7b);
    canvas->setCursor(58, y + 36);
    canvas->print("Src: ");
    canvas->print(sourceNames[source]);

    // 静音指示
    if (muted)
    {
        canvas->setTextColor(rgb565(231, 76, 60));
        canvas->setCursor(SCREEN_WIDTH - 65, y + 36);
        canvas->print("MUTE");
    }
}

// ============ 中心转盘（环形音量/频道）============
void RemoteTV::drawCenterDial()
{
    int cx = CX;
    int cy = CY - 20;
    int r = 64;
    int w = 12;

    // 外圈刻度线
    for (int i = 0; i < 360; i += 3)
    {
        float a = (i - 90) * PI / 180.0f;
        bool major = (i % 45 == 0);
        int r1 = major ? r - 4 : r - 2;
        int x1 = cx + cosf(a) * r1;
        int y1 = cy + sinf(a) * r1;
        int x2 = cx + cosf(a) * r;
        int y2 = cy + sinf(a) * r;
        canvas->drawLine(x1, y1, x2, y2, rgb565(45, 45, 70));
    }

    // 进度弧 - 根据当前功能显示
    uint16_t curColor = funcColors[(int)adjTarget];

    if (adjTarget == TV_ADJ_VOL)
    {
        float pct = volume / 100.0f;
        int arcEnd = (int)(270.0f * pct);
        uint16_t volColor = muted ? rgb565(231, 76, 60) : (volume > 70 ? rgb565(231, 76, 60) : (volume > 30 ? rgb565(230, 126, 34) : rgb565(0, 168, 107)));
        drawArc(cx, cy, r - 8, -135, -135 + arcEnd, volColor, w);
        drawArc(cx, cy, r - 8, -135 + arcEnd, 135, rgb565(30, 30, 50), w);
    }
    else if (adjTarget == TV_ADJ_CH)
    {
        float pct = (channel - 1) / 98.0f;
        int arcEnd = (int)(270.0f * pct);
        drawArc(cx, cy, r - 8, -135, -135 + arcEnd, curColor, w);
        drawArc(cx, cy, r - 8, -135 + arcEnd, 135, rgb565(30, 30, 50), w);
    }
    else if (adjTarget == TV_ADJ_SRC)
    {
        // 输入源：4 段
        for (int i = 0; i < 4; i++)
        {
            float a1 = -135.0f + i * 67.5f;
            float a2 = a1 + 50.0f;
            uint16_t c = (i == (int)source) ? curColor : rgb565(30, 30, 50);
            drawArc(cx, cy, r - 8, a1, a2, c, w);
        }
    }
    else
    {
        // PWR / MUTE：显示完整圆环（静态）
        drawArc(cx, cy, r - 8, -135, 135, curColor, w);
    }

    // 中心圆
    int centerR = sendPulse ? 36 : 32;
    uint16_t centerBg = sendPulse ? rgb565(60, 60, 100) : rgb565(30, 30, 55);
    canvas->fillCircle(cx, cy, centerR, centerBg);
    canvas->drawCircle(cx, cy, centerR, funcColors[(int)adjTarget]);

    // 中心标签
    canvas->setTextColor(funcColors[(int)adjTarget]);
    canvas->setTextSize(1);
    canvas->setFont(&FreeSans9pt7b);
    const char *lbl = (adjTarget == TV_ADJ_PWR)    ? "PWR"
                      : (adjTarget == TV_ADJ_MUTE) ? "MUTE"
                      : (adjTarget == TV_ADJ_SRC)  ? "SRC"
                      : (adjTarget == TV_ADJ_VOL)  ? "VOL"
                                                   : "CH";
    int16_t lw = canvas->textWidth(lbl);
    canvas->setCursor(cx - lw / 2, cy - 6);
    canvas->print(lbl);

    // 中心数值
    canvas->setTextColor(TFT_WHITE);
    canvas->setFont(&FreeSansBold12pt7b);
    char valBuf[8];
    if (adjTarget == TV_ADJ_SRC)
        snprintf(valBuf, sizeof(valBuf), "%d/4", (int)source + 1);
    else if (adjTarget == TV_ADJ_VOL)
        snprintf(valBuf, sizeof(valBuf), "%d", volume);
    else if (adjTarget == TV_ADJ_CH)
        snprintf(valBuf, sizeof(valBuf), "%02d", channel);
    else if (adjTarget == TV_ADJ_PWR || adjTarget == TV_ADJ_MUTE)
    {
        canvas->setFont(&FreeSansBold9pt7b);
        snprintf(valBuf, sizeof(valBuf), "%s", "ON/OFF");
    }

    lw = canvas->textWidth(valBuf);
    canvas->setCursor(cx - lw / 2, cy + 42);
    canvas->print(valBuf);

    // 底部三角指示
    int triY = cy + r + 14;
    for (int i = 0; i < 5; i++)
    {
        int tx = cx - 60 + i * 30;
        bool sel = (i == (int)adjTarget);
        uint16_t tc = sel ? funcColors[i] : rgb565(80, 80, 100);
        int ty = sel ? triY - 3 : triY;
        canvas->fillTriangle(tx - 5, ty, tx + 5, ty, tx, ty + 7, tc);
    }

    // 选中对象名称
    canvas->setTextColor(funcColors[(int)adjTarget]);
    canvas->setFont(&FreeSans9pt7b);
    const char *tgtLbl = (adjTarget == TV_ADJ_PWR)    ? "Power"
                         : (adjTarget == TV_ADJ_MUTE) ? "Mute"
                         : (adjTarget == TV_ADJ_SRC)  ? "Source"
                         : (adjTarget == TV_ADJ_VOL)  ? "Volume"
                                                      : "Channel";
    lw = canvas->textWidth(tgtLbl);
    canvas->setCursor(cx - lw / 2, triY + 14);
    canvas->print(tgtLbl);
}

// ============ 快捷条 ============
void RemoteTV::drawQuickBar()
{
    int y = 240, h = 32;
    canvas->fillRoundRect(10, y, SCREEN_WIDTH - 20, h, 6, rgb565(35, 35, 60));

    int btnW = (SCREEN_WIDTH - 30) / 5;
    for (int i = 0; i < 5; i++)
    {
        int bx = 15 + i * btnW;
        int by = y + 4;
        int bw = btnW - 6;
        int bh = h - 8;

        bool sel = (i == (int)adjTarget);
        uint16_t bg;

        if (sel)
            bg = funcColors[i];
        else
        {
            // 未选中：暗色
            if (i == 0)
                bg = rgb565(100, 30, 30); // PWR 暗红
            else if (i == 1)
                bg = rgb565(80, 20, 80); // MUTE 暗紫
            else if (i == 2)
                bg = rgb565(20, 60, 55); // SRC 暗青
            else if (i == 3)
                bg = rgb565(20, 50, 35); // VOL 暗绿
            else
                bg = rgb565(45, 30, 80); // CH 暗蓝紫
        }

        canvas->fillRoundRect(bx, by, bw, bh, 4, bg);
        canvas->drawRoundRect(bx, by, bw, bh, 4, sel ? TFT_WHITE : rgb565(100, 100, 130));
    }
}

// ============ PAD 页 ============
// 5 个按钮（UP/RIGHT/DOWN/LEFT/OK）以十字布局排列
// padPressed[i] = true 时该按钮显示"被按下"高亮色，无持续缩放/衰减动画
void RemoteTV::drawPad()
{
    drawPageHeader("Direction Pad");

    // 十字布局参数 —— 间距加大
    int cx = CX;
    int cy = 140; // 中心，给头部标题留空间
    int gap = 64; // 按钮间距

    drawPadBtn(cx, cy - gap, "UP", padPressed[PAD_BTN_UP]);
    drawPadBtn(cx + gap, cy, "RIGHT", padPressed[PAD_BTN_RIGHT]);
    drawPadBtn(cx, cy + gap, "DOWN", padPressed[PAD_BTN_DOWN]);
    drawPadBtn(cx - gap, cy, "LEFT", padPressed[PAD_BTN_LEFT]);
    drawPadBtn(cx, cy, "OK", padPressed[PAD_BTN_OK]);

    // 底部提示
    drawHintBar("U/D/L/R:Dir PWR:Page");
}

// 绘制单个 PAD 按钮
// pressed = true 时显示"被按下"的高亮样式
void RemoteTV::drawPadBtn(int x, int y, const char *lbl, bool pressed)
{
    int w = 52, h = 36;

    // 按下时缩小（按压效果）
    if (pressed)
    {
        w = 46;
        h = 32;
    }

    // 背景色：按下时更亮
    uint16_t bg = pressed ? rgb565(0, 150, 240) : rgb565(50, 50, 80);
    uint16_t border = pressed ? rgb565(120, 200, 255) : rgb565(100, 100, 130);

    canvas->fillRoundRect(x - w / 2, y - h / 2, w, h, 8, bg);
    canvas->drawRoundRect(x - w / 2, y - h / 2, w, h, 8, border);

    // 按下时添加内部高光，增强立体感
    if (pressed)
    {
        canvas->drawRoundRect(x - w / 2 + 2, y - h / 2 + 2, w - 4, h - 4, 6, rgb565(80, 180, 255));
    }

    canvas->setTextColor(TFT_WHITE);
    canvas->setTextSize(1);
    canvas->setFont(&FreeMono9pt7b);
    int16_t tw = canvas->textWidth(lbl);
    int textY = pressed ? y - 6 : y - 5; // 按下时文字略微下移，模拟按压
    canvas->setCursor(x - tw / 2, textY);
    canvas->print(lbl);
}

// ============ LIST 页 ============
void RemoteTV::drawList()
{
    drawPageHeader("Channels");

    int centerY = 120; // 中心坐标
    int baseX = 50;
    int lineH = 40; // 行高基准

    for (int i = -2; i <= 3; i++)
    {
        int ch = channel + i;
        if (ch < 1)
            ch += 99;
        if (ch > 99)
            ch -= 99;

        int level = abs(i); // 0,1,2,3

        // ---- 字号：4 级梯度，用你现有的字体 ----
        if (level == 0)
            canvas->setFont(&FreeSans18pt7b);
        else if (level == 1)
            canvas->setFont(&FreeSans12pt7b);
        else if (level == 2)
            canvas->setFont(&FreeSans9pt7b);
        else
            canvas->setFont(&FreeSans9pt7b);

        // ---- 淡出：4 级灰度 ----
        // level 0: 白(255), 1: 浅灰(190), 2: 中灰(130), 3: 暗灰(75)
        static const uint8_t grayTbl[4] = {255, 190, 130, 75};
        uint16_t textColor = rgb565(grayTbl[level], grayTbl[level], grayTbl[level]);

        // ---- 缩进：越往外越往右推，梯度加大 ----
        int indent = level * 26;
        int textX = baseX + indent;

        // ---- Y 位置：间距随 level 压缩，模拟弧形透视 ----
        // 外层行被"压扁"，间距更小
        int spacing = lineH - level * 3; // level0:28, level1:25, level2:22, level3:19
        int y = centerY + i * spacing;

        // ---- 选中行高亮背景 ----
        if (level == 0)
        {
            // 背景条高度适配 24pt 字体
            canvas->fillRoundRect(textX - 8, y + 2, SCREEN_WIDTH - textX - 30, 36, 8, rgb565(0, 100, 160));
        }

        // ---- 绘制文字 ----
        canvas->setTextColor(textColor);
        char buf[10];
        snprintf(buf, sizeof(buf), "CH %02d", ch);
        canvas->setCursor(textX, y + 6);
        canvas->print(buf);

        // ---- 选中行：左侧三角指示 ----
        if (level == 0)
        {
            int triX = textX - 22;
            int triY = y + 15;
            canvas->fillTriangle(triX, triY, triX, triY + 10, triX + 6, triY + 5, TFT_ORANGE);
        }
    }

    drawHintBar("Up/Dn/Rotate:Chx");
}

// ============ MACRO 页 ============
void RemoteTV::drawMacro()
{
    drawPageHeader("Scenes");

    for (int i = 0; i < 4; i++)
    {
        int y = PAGE_HEADER_H + 30 + i * 40;
        bool sel = (i == macroFocus);

        uint16_t bg = sel        ? rgb565(0, 100, 160)
                      : (i == 0) ? rgb565(80, 40, 40)
                      : (i == 1) ? rgb565(40, 60, 80)
                      : (i == 2) ? rgb565(40, 80, 60)
                                 : rgb565(60, 40, 80);
        canvas->fillRoundRect(20, y, SCREEN_WIDTH - 40, 34, 6, bg);
        canvas->drawRoundRect(20, y, SCREEN_WIDTH - 40, 34, 6,
                              sel ? TFT_WHITE : rgb565(100, 100, 130));

        // 图标符号
        canvas->setTextColor(sel ? TFT_WHITE : rgb565(180, 180, 200));
        canvas->setFont(&FreeSansBold12pt7b);
        const char *icon = (i == 0)   ? "M"
                           : (i == 1) ? "G"
                           : (i == 2) ? "~"
                                      : "Z";
        canvas->setCursor(32, y + 11);
        canvas->print(icon);
        canvas->setCursor(70, y + 11);
        canvas->print(macroNames[i]);
    }

    drawHintBar("Up/Dn/Rotate:Pick");
}

// ============ LEARN 页 ============
void RemoteTV::drawLearn()
{
    drawPageHeader("IR Learn");

    int yBase = PAGE_HEADER_H + 8;

    // 状态指示（紧跟标题右侧）
    int indicatorX = SCREEN_WIDTH - 28;
    int indicatorY = PAGE_HEADER_H / 2;
    int r = 5;

    if (learnWaiting)
    {
        // 闪烁的圆点
        bool blink = ((millis() / 300) % 2 == 0);
        uint16_t c = blink ? rgb565(255, 200, 0) : rgb565(80, 60, 0);
        canvas->fillCircle(indicatorX, indicatorY, r, c);
        canvas->drawCircle(indicatorX, indicatorY, r, rgb565(255, 200, 0));
    }
    else if (learnData.valid)
    {
        canvas->fillCircle(indicatorX, indicatorY, r, rgb565(0, 200, 0));
        canvas->drawCircle(indicatorX, indicatorY, r, TFT_WHITE);
    }
    else
    {
        canvas->fillCircle(indicatorX, indicatorY, r, rgb565(80, 80, 80));
        canvas->drawCircle(indicatorX, indicatorY, r, rgb565(120, 120, 120));
    }

    // 信息区域
    int infoY = yBase + 20;
    canvas->fillRoundRect(10, infoY, SCREEN_WIDTH - 20, 70, 6, rgb565(30, 30, 55));
    canvas->drawRoundRect(10, infoY, SCREEN_WIDTH - 20, 70, 6, rgb565(60, 60, 100));

    if (!learnData.valid && !learnWaiting)
    {
        // 初始提示
        canvas->setTextColor(rgb565(130, 130, 160));
        canvas->setFont(&FreeSans9pt7b);
        canvas->setCursor(20, infoY + 14);
        canvas->print("Press OK to start learning");
        canvas->setCursor(20, infoY + 36);
        canvas->print("Point remote & press key");
    }

    if (learnData.valid)
    {
        // 显示协议信息
        canvas->setTextColor(rgb565(0, 200, 180));
        canvas->setFont(&FreeSans9pt7b);
        canvas->setCursor(20, infoY + 10);
        canvas->print("Proto: ");
        canvas->print(learnData.protoText);

        canvas->setTextColor(TFT_WHITE);
        canvas->setCursor(20, infoY + 28);
        canvas->print(learnData.infoLine1);

        canvas->setTextColor(rgb565(200, 200, 220));
        canvas->setCursor(20, infoY + 46);
        canvas->print(learnData.infoLine2);
    }

    // 波形区域
    int waveY = infoY + 80;
    int waveH = 80;
    int waveX = 10;
    int waveW = SCREEN_WIDTH - 20;

    canvas->fillRoundRect(waveX, waveY, waveW, waveH, 6, rgb565(20, 20, 40));
    canvas->drawRoundRect(waveX, waveY, waveW, waveH, 6, rgb565(50, 50, 80));

    // 网格线
    for (int gx = 1; gx < 4; gx++)
    {
        int gxPos = waveX + (waveW * gx / 4);
        canvas->drawLine(gxPos, waveY + 2, gxPos, waveY + waveH - 2, rgb565(35, 35, 60));
    }
    for (int gy = 1; gy < 3; gy++)
    {
        int gyPos = waveY + (waveH * gy / 3);
        canvas->drawLine(waveX + 2, gyPos, waveX + waveW - 2, gyPos, rgb565(35, 35, 60));
    }

    // 波形标签
    canvas->setTextColor(rgb565(100, 100, 130));
    canvas->setFont(&FreeSans9pt7b);

    // 绘制波形
    if (learnData.valid && learnData.rawLen > 2)
    {
        drawIRWaveform(waveX + 2, waveY + 2, waveW - 4, waveH - 4);
    }
    else
    {
        canvas->setTextColor(rgb565(70, 70, 100));
        canvas->setCursor(waveX + 60, waveY + 30);
        canvas->print("No signal yet");
    }

    // 底部提示
    drawHintBar("SP:Learn  LP:Home");
}

// ============ 绘制 IR 包络波形（示波器风格）============
void RemoteTV::drawIRWaveform(int x, int y, int w, int h)
{
    if (learnData.rawLen < 2)
        return;

    // 计算总时长（用于 X 轴缩放）
    uint32_t totalTicks = 0;
    for (uint16_t i = 0; i < learnData.rawLen && i < 80; i++)
    {
        totalTicks += learnData.rawBuf[i];
    }
    if (totalTicks == 0)
        return;

    // mark 画在上方，space 画在下方
    int markY = y + 6;
    int spaceY = y + h - 6;
    int midY = y + h / 2;

    // 零线
    canvas->drawLine(x, midY, x + w, midY, rgb565(50, 50, 70));

    // 统一绿色
    uint16_t waveColor = rgb565(0, 200, 100);

    // 电平标签
    canvas->setTextColor(waveColor);
    canvas->setFont(&FreeSans9pt7b);
    canvas->setCursor(x + 4, markY + 8);
    canvas->print("H");
    canvas->setCursor(x + 4, spaceY - 16);
    canvas->print("L");

    // 逐段绘制
    int curX = x;
    int curY = markY;

    for (uint16_t i = 0; i < learnData.rawLen && i < 80; i++)
    {
        uint16_t ticks = learnData.rawBuf[i];

        int segW = (int)((uint32_t)ticks * w / totalTicks);
        if (segW < 1)
            segW = 1;
        if (curX + segW > x + w)
            segW = x + w - curX;

        int nextY = (i % 2 == 0) ? markY : spaceY;

        // 水平段
        canvas->drawLine(curX, curY, curX + segW, curY, waveColor);

        // 垂直线跳变
        if (i < learnData.rawLen - 1 && curX + segW < x + w)
        {
            int nextX = curX + segW;
            canvas->drawLine(nextX, curY, nextX, nextY, waveColor);
        }

        curX += segW;
        curY = nextY;
    }

    // 总时间（ms）= totalTicks × 50μs = totalTicks / 20
    float totalMs = (float)totalTicks / 20.0f;

    canvas->setTextColor(rgb565(100, 100, 130));
    canvas->setFont(&Font0);

    // 底部刻度线 Y 位置（波形区域最底部再往下 2px）
    int tickY = y + h + 2;
    int tickLen = 4;

    for (int t = 0; t <= 5; t++)
    {
        int tickX = x + (t * w / 5);

        // 刻度短线
        canvas->drawLine(tickX, tickY, tickX, tickY + tickLen, rgb565(100, 100, 130));

        // 时间标注
        float ms = totalMs * t / 5.0f;
        char msBuf[12];
        if (ms == 0 || ms >= 1.0f)
            snprintf(msBuf, sizeof(msBuf), "%.0fms", ms);
        else
            snprintf(msBuf, sizeof(msBuf), "%.1fms", ms); // <1ms 时显示小数

        int16_t tw = canvas->textWidth(msBuf);
        canvas->setCursor(tickX - tw / 2, tickY + tickLen + 10);
        canvas->print(msBuf);
    }
}

// ============ 检查 IR 学习结果 ============
void RemoteTV::checkLearnIR()
{
    if (!learnWaiting)
        return;

    if (IrReceiver.decode())
    {
        processLearnResult();
        IrReceiver.resume(); // 继续等待下一次
    }
}

// ============ 处理学习结果 ============
void RemoteTV::processLearnResult()
{
    Serial.println(">>> IR signal received, processing...");

    // 打印详细调试信息到串口
    IrReceiver.printIRResultShort(&Serial);
    IrReceiver.printIRResultRawFormatted(&Serial, true);

    // 解析协议信息
    decode_type_t proto = IrReceiver.decodedIRData.protocol;

    // 填充 learnData
    memset(&learnData, 0, sizeof(learnData));
    learnData.valid = true;
    learnData.isRepeat = (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) != 0;
    learnData.address = IrReceiver.decodedIRData.address;
    learnData.command = IrReceiver.decodedIRData.command;
    learnData.rawData = IrReceiver.decodedIRData.decodedRawData;
    learnData.numberOfBits = IrReceiver.decodedIRData.numberOfBits;

    // 协议名称
    printProtoName(proto, learnData.protoText, sizeof(learnData.protoText));

    if (proto == UNKNOWN)
    {
        learnData.isUnknown = true;
        snprintf(learnData.infoLine1, sizeof(learnData.infoLine1),
                 "Hash=%08lX", (unsigned long)IrReceiver.decodedIRData.decodedRawData);
        snprintf(learnData.infoLine2, sizeof(learnData.infoLine2),
                 "%d bits raw timing", IrReceiver.decodedIRData.numberOfBits);
    }
    else if (learnData.isRepeat)
    {
        snprintf(learnData.infoLine1, sizeof(learnData.infoLine1),
                 "Repeat frame");
        snprintf(learnData.infoLine2, sizeof(learnData.infoLine2),
                 "Addr=%04X", learnData.address);
    }
    else
    {
        snprintf(learnData.infoLine1, sizeof(learnData.infoLine1),
                 "Addr=%04X  Cmd=%04X",
                 learnData.address, learnData.command);
        snprintf(learnData.infoLine2, sizeof(learnData.infoLine2),
                 "Raw=%08lX  %d bits",
                 (unsigned long)learnData.rawData, learnData.numberOfBits);
    }

    // 复制原始时序数据
    uint16_t copyLen = IrReceiver.irparams.rawlen;
    if (copyLen > 80)
        copyLen = 80;
    for (uint16_t i = 0; i < copyLen; i++)
    {
        learnData.rawBuf[i] = IrReceiver.irparams.rawbuf[i];
    }
    learnData.rawLen = copyLen;

    Serial.printf(">>> Learn OK: proto=%s addr=0x%04X cmd=0x%04X raw=0x%08lX bits=%d rawLen=%d\n",
                  learnData.protoText, learnData.address, learnData.command,
                  (unsigned long)learnData.rawData, learnData.numberOfBits, learnData.rawLen);

    // 刷新显示
    Draw();
}

// ============ 协议名称转字符串 ============
void RemoteTV::printProtoName(uint8_t proto, char *buf, int buflen)
{
    const char *name;

    switch (proto)
    {
    case NEC:
        name = "NEC";
        break;
    case NEC2:
        name = "NEC2";
        break;
    case SONY:
        name = "SONY";
        break;
    case RC5:
        name = "RC5";
        break;
    case RC6:
        name = "RC6";
        break;
    case SAMSUNG:
        name = "SAMSUNG";
        break;
    case LG:
        name = "LG";
        break;
    case JVC:
        name = "JVC";
        break;
    case PANASONIC:
        name = "PANASONIC";
        break;
    case DENON:
        name = "DENON";
        break;
    case SHARP:
        name = "SHARP";
        break;
    case KASEIKYO:
        name = "KASEIKYO";
        break;
    case ONKYO:
        name = "ONKYO";
        break;
    case PULSE_DISTANCE:
        name = "PULSE_DIST";
        break;
    case PULSE_WIDTH:
        name = "PULSE_WIDTH";
        break;
    case UNKNOWN:
        name = "UNKNOWN";
        break;
    default:
        name = "OTHER";
        break;
    }

    strncpy(buf, name, buflen - 1);
    buf[buflen - 1] = '\0';
}

// ============ 工具：画弧线 ============
void RemoteTV::drawArc(int cx, int cy, int r, float startAngle, float endAngle, uint16_t color, int thickness)
{
    // 确保 startAngle <= endAngle
    if (startAngle > endAngle)
    {
        float t = startAngle;
        startAngle = endAngle;
        endAngle = t;
    }

    // 用 float 步进，避免负角度时 int 循环变量导致弧线缺失
    for (float a = startAngle; a <= endAngle; a += 1.0f)
    {
        float rad = (a - 90) * PI / 180.0f;
        int x1 = cx + cosf(rad) * (r - thickness / 2);
        int y1 = cy + sinf(rad) * (r - thickness / 2);
        int x2 = cx + cosf(rad) * (r + thickness / 2);
        int y2 = cy + sinf(rad) * (r + thickness / 2);
        canvas->drawLine(x1, y1, x2, y2, color);
    }
}
