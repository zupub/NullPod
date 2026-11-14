#ifndef _SETTINGS_APP_H_
#define _SETTINGS_APP_H_

#include "hal/hal.h"
#include "hal/hal_config.h"
#include "common.h"

/**
 * @brief 设置菜单（iPhone 风格）
 *
 * 每行一个设置项，右侧按类型显示不同控件：
 *   - 开关（Keep WiFi / Buzzer）：右侧圆角胶囊按钮，绿底白色圆点
 *   - 数值（Brightness）：右侧显示百分比，OK 弹出滑块，左右键/编码器调整，OK 写入 NVS
 *   - 多选一（Time Format）：右侧显示当前值 + 箭头，OK 弹出选择窗口，OK 打钩生效
 *   - 动作（Network Time）：右侧显示对时状态，OK 弹窗用 NTP 校时并写入 RTC
 *
 * Keep WiFi 开关控制 WiFi 配置 App 退出时是否断开射频（写在 NVS，掉电保持）。
 * 行数超过一屏，列表支持上下滚动（光标跟随 + 右侧滚动条）。
 *
 * 按键：编码器旋转 / 上下左右切换，OK 确认，电源键返回上一级（在弹窗内则关闭弹窗）。
 */
class SettingsApp
{
public:
    void Main();

protected:
    void Init();
    void Update();
    void Exit();

private:
    // 行类型
    enum RowType : uint8_t
    {
        ROW_TOGGLE = 0, // 开关
        ROW_CHOICE,     // 多选一
        ROW_VALUE,      // 数值（滑块）
        ROW_ACTION      // 动作（弹窗内执行）
    };

    // 界面模式
    enum UiMode : uint8_t
    {
        MODE_LIST = 0, // 一级列表
        MODE_CHOICE,   // 选择弹窗
        MODE_SLIDER,   // 滑块弹窗
        MODE_NTP       // 网络对时弹窗
    };

    // 网络对时弹窗阶段
    enum NtpPhase : uint8_t
    {
        NTPP_READY = 0, // 等待开始
        NTPP_RUNNING,   // 同步中
        NTPP_OK,        // 成功
        NTPP_FAIL       // 失败
    };

    // 设置项身份（动作分发用，与绘制用的 RowType 解耦）
    enum SettingId : uint8_t
    {
        ID_BUZZER = 0,
        ID_TIME_FORMAT,
        ID_BRIGHTNESS,
        ID_WIFI_KEEP,
        ID_NTP
    };

    struct RowDef
    {
        const char *label;
        RowType type;
        SettingId id;
    };

    static constexpr int ROW_COUNT = 5;
    static constexpr int CHOICE_COUNT = 2;

    LGFX *disp = nullptr;
    LGFX_Sprite *canvas = nullptr;

    const RowDef rows[ROW_COUNT] = {
        {"Keep WiFi", ROW_TOGGLE, ID_WIFI_KEEP},
        {"Buzzer", ROW_TOGGLE, ID_BUZZER},
        {"Brightness", ROW_VALUE, ID_BRIGHTNESS},
        {"Time Format", ROW_CHOICE, ID_TIME_FORMAT},
        {"Network Time", ROW_ACTION, ID_NTP},
    };

    const char *choiceLabels[CHOICE_COUNT] = {"12 Hour", "24 Hour"};

    UiMode mode = MODE_LIST;
    int cursor = 0;       // 列表光标
    int choiceCursor = 0; // 选择弹窗光标

    // 列表滚动（4 行放不进一屏）：scrollY 当前偏移，scrollTargetY 目标值
    float scrollY = 0.0f;
    float scrollTargetY = 0.0f;

    // 设置项当前值
    bool buzzerOn = true;
    int timeFormat = 24;       // 12 或 24
    uint8_t brightness = 75;   // 10~100
    uint8_t brightnessOrig = 0; // 打开滑块前的亮度，取消时还原
    bool wifiKeepOn = false;   // Keep WiFi：App 退出时不断开射频

    // 网络对时
    NtpPhase ntpPhase = NTPP_READY;
    bool timeSynced = false;    // 本次开机是否对时成功过（决定行右侧状态）
    uint32_t ntpDrawMs = 0;     // 同步中进度动画限帧
    char ntpTime[12] = {0};     // 对时成功的时间文本 HH:MM

    bool powerWasDown = false; // 电源键按下沿检测
    bool exitRequest = false;

    //-------- 配置读写 --------
    void loadConfig();
    void saveTimeFormat();

    //-------- 绘制 --------
    void drawList();
    void drawRow(int index);
    void drawScrollBar();
    void drawSwitch(int x, int y, bool on);
    void drawChevron(int x, int y, uint16_t color);
    void drawCheck(int x, int y, uint16_t color);
    void drawDialogFrame(int x, int y, int w, int h, const char *title);
    void dimBackground();
    void drawChoiceDialog();
    void drawSliderDialog();
    void drawNtpDialog();

    //-------- 交互 --------
    void handleKey(uint8_t key);
    void handlePower();
    void activateRow();
    void closeDialog(bool cancel);
    void setBrightnessValue(int value);
    void scrollToCursor();
    void startNtpSync();
    void updateNtp();
};

extern SettingsApp g_settingsApp;

#endif
