/**
 * @file hal_wifi.cpp
 * @brief WiFi 服务层：WiFiManager 非阻塞配网 + STA 状态维护
 *
 * 工作方式（参考 esp-claw 的 wifi_manager 思路）：
 *   - 热点名固定为 esp-nullpod-XXXXXX（MAC 后 3 字节）
 *   - 门户非阻塞运行，由上层 App 在主循环里调 Wifi_Update() 驱动
 *   - 已保存配置时才尝试 STA 连接，配置写在 NVS 里，掉电保持
 *   - 门户不设超时，何时关闭由 App 决定（连上了就关 AP，省电）
 */
#include "hal/hal.h"
#include "hal/hal_config.h"

#include <WiFi.h>
#include <WiFiManager.h>

// 配网页面风格（注入到 WiFiManager 内置页面的 <head>，暗色 + 红色主色）
static const char kPortalHead[] =
    "<style>"
    "html,body{background:#0a0b0e !important;color:#e8eaf0 !important;"
    "font-family:-apple-system,BlinkMacSystemFont,'Segoe UI','PingFang SC','Microsoft YaHei',sans-serif !important;}"
    ".wrap{background:#161920 !important;border:1px solid rgba(255,255,255,.1) !important;"
    "border-radius:16px !important;box-shadow:0 24px 80px rgba(0,0,0,.28) !important;}"
    "h1,h2,h3,p,label,span,div{color:#e8eaf0 !important;}"
    "button,.btn,input[type=submit]{background:#e8362d !important;color:#fff !important;"
    "border:none !important;border-radius:10px !important;padding:10px 18px !important;}"
    "input,select,textarea{background:#0e1015 !important;color:#e8eaf0 !important;"
    "border:1px solid rgba(255,255,255,.2) !important;border-radius:8px !important;}"
    "a{color:#e8362d !important;}"
    "</style>";

namespace
{
    WiFiManager s_wm;
    bool s_inited = false;
    bool s_portalActive = false;
    char s_apSsid[40] = "esp-nullpod-000000";
    uint32_t s_onlineSince = 0; // 首次观测到已连接的时刻

    // 从 MAC 后 3 字节拼热点名：esp-nullpod-A1B2C3
    void buildApSsid()
    {
        uint8_t mac[6] = {0};
        WiFi.macAddress(mac);
        snprintf(s_apSsid, sizeof(s_apSsid), "esp-nullpod-%02X%02X%02X",
                 mac[3], mac[4], mac[5]);
    }

    void formatMac(char *out, size_t outSize)
    {
        uint8_t mac[6] = {0};
        WiFi.macAddress(mac);
        snprintf(out, outSize, "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
}

void HAL::Wifi_Init()
{
    if (s_inited)
        return;
    s_inited = true;

    buildApSsid();

    s_wm.setConfigPortalBlocking(false); // 非阻塞：门户靠 Wifi_Update() 里的 process() 驱动
    s_wm.setConfigPortalTimeout(0);      // 门户不自动超时，由 App 决定何时关闭
    s_wm.setTitle("NullPod");
    s_wm.setCustomHeadElement(kPortalHead);
    s_wm.setShowInfoErase(false);        // 页面上的"擦除"按钮关掉，避免误操作
    s_wm.setDebugOutput(false);

    Serial.printf("[WiFi] AP hotspot name: %s\n", s_apSsid);
}

void HAL::Wifi_Connect()
{
    if (!s_inited)
        return;

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(); // 用 NVS 里已保存的配置发起连接（没存过就一直保持未连接）
}

void HAL::Wifi_Disconnect()
{
    if (!s_inited)
        return;

    WiFi.disconnect(true); // 关掉 STA 省电，保留 NVS 里的配置
    s_onlineSince = 0;
}

void HAL::Wifi_Update()
{
    if (!s_inited)
        return;

    if (s_portalActive)
    {
        // 门户里手机提交配置后会在这里完成连接，成功后库内部自己关 AP
        s_wm.process();
        if (!s_wm.getConfigPortalActive())
            s_portalActive = false;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        if (s_onlineSince == 0)
            s_onlineSince = millis();
    }
    else
    {
        s_onlineSince = 0;
    }
}

bool HAL::Wifi_IsConnected()
{
    return s_inited && WiFi.status() == WL_CONNECTED;
}

bool HAL::Wifi_IsPortalActive()
{
    return s_inited && s_portalActive;
}

bool HAL::Wifi_HasSaved()
{
    return s_inited && s_wm.getWiFiIsSaved();
}

bool HAL::Wifi_StartPortal()
{
    if (!s_inited)
        return false;
    if (s_portalActive)
        return true;

    // 门户要独占射频：先断开 STA，避免 AP 和 STA 抢信道导致热点出不来
    if (WiFi.status() == WL_CONNECTED)
    {
        WiFi.disconnect(false, false);
        delay(50);
    }

    s_wm.startConfigPortal(s_apSsid, NULL); // 开放热点，无密码
    s_portalActive = s_wm.getConfigPortalActive();

    Serial.printf("[WiFi] portal %s, SSID=%s\n",
                  s_portalActive ? "started" : "failed", s_apSsid);
    return s_portalActive;
}

void HAL::Wifi_StopPortal()
{
    if (!s_inited || !s_portalActive)
        return;

    s_wm.stopConfigPortal(); // 关 AP + web + DNS，STA 连接不受影响
    s_portalActive = false;
    Serial.println("[WiFi] portal stopped");
}

void HAL::Wifi_Forget()
{
    if (!s_inited)
        return;

    if (s_portalActive)
    {
        s_wm.stopConfigPortal();
        s_portalActive = false;
    }

    s_wm.resetSettings();     // 擦掉 NVS 里的网络配置
    WiFi.disconnect(true, true);
    s_onlineSince = 0;
    Serial.println("[WiFi] saved config erased");
}

void HAL::Wifi_GetInfo(WifiInfo_t *out)
{
    if (!out)
        return;

    memset(out, 0, sizeof(*out));
    snprintf(out->apSsid, sizeof(out->apSsid), "%s", s_apSsid);
    formatMac(out->mac, sizeof(out->mac));

    if (!s_inited)
        return;

    out->portalActive = s_portalActive;
    out->hasSaved = s_wm.getWiFiIsSaved();

    if (WiFi.status() != WL_CONNECTED)
        return;

    out->connected = true;
    snprintf(out->ssid, sizeof(out->ssid), "%s", WiFi.SSID().c_str());

    IPAddress ip = WiFi.localIP();
    IPAddress gw = WiFi.gatewayIP();
    snprintf(out->ip, sizeof(out->ip), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    snprintf(out->gateway, sizeof(out->gateway), "%u.%u.%u.%u", gw[0], gw[1], gw[2], gw[3]);

    out->rssi = (int16_t)WiFi.RSSI();
    out->channel = (uint8_t)WiFi.channel();
    if (s_onlineSince)
        out->onlineSec = (millis() - s_onlineSince) / 1000;
}
