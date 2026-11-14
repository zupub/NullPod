#ifndef _WIFI_MANAGER_APP_H_
#define _WIFI_MANAGER_APP_H_

#include "hal/hal.h"
#include "hal/hal_config.h"
#include "common.h"

/**
 * @brief WiFi 配网 + 网络信息（参考 esp-claw 的配网逻辑与页面风格）
 *
 * 三个页面：
 *   - UI_PROMPT：未连接提示，倒计时结束自动进配网（OK 立即进入）
 *   - UI_PORTAL：AP 配网页，手机/PC 连热点 esp-nullpod-XXXXXX，浏览器打开 192.168.4.1
 *   - UI_INFO  ：已连接，显示 SSID / IP / 网关 / 信道 / 信号 / 在线时长
 *
 * 按键：OK 进入配网或重新配网，长按 OK 取消配网 / 忘记网络，电源键退出 App。
 * 离开 App 会关掉射频省电，网络配置保留在 NVS，下次进入自动重连。
 */
class WiFiManagerApp
{
public:
    void Main();

protected:
    void Init();
    void Update();
    void Exit();

private:
    // 当前页面
    enum UiMode : uint8_t
    {
        UI_PROMPT = 0, // 未连接提示 / 正在连接
        UI_PORTAL,     // AP 配网页
        UI_INFO        // 网络信息页
    };

    LGFX *disp = nullptr;
    LGFX_Sprite *canvas = nullptr;

    UiMode mode = UI_PROMPT;
    uint32_t modeStart = 0;      // 进入当前页面的时刻
    uint32_t lastPoll = 0;       // HAL::Wifi_Update 轮询
    uint32_t lastDraw = 0;       // 页面重绘
    uint32_t promptDelayMs = 0;  // 提示页停留时长（到点自动进配网）
    uint8_t dots = 0;            // 等待动画的点数

    bool exitRequest = false;
    bool powerWasDown = false;

    //-------- 页面切换 --------
    void enterPrompt(bool autoStart = true);
    void enterPortal();
    void enterInfo();

    //-------- 绘制 --------
    void push();
    void drawHeader(const char *title);
    void drawHint(const char *text);
    void drawPill(int x, int y, const char *text, uint16_t color);
    void drawCard(int x, int y, int w, int h);
    void drawWifiGlyph(int cx, int cy, int r, uint16_t color, bool slash);
    void drawSignalBars(int x, int y, int rssi);
    void drawPrompt();
    void drawPortal();
    void drawInfo();

    //-------- 交互 --------
    void handleKey(uint8_t key);
    void handlePower();
};

extern WiFiManagerApp g_WiFiManagerApp;

#endif
