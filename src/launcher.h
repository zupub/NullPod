#ifndef _LAUNCHER_H_
#define _LAUNCHER_H_

#include "hal/hal.h"
#include "hal/hal_config.h"
#include "common.h"
#include "app_state.h"

// ============ 菜单 ============
#define MENU_COUNT 15
#define SLOT_SPACING 92
#define VISIBLE_SLOTS 5

// ============ 颜色 ============
#define BG_TOP rgb565(26, 26, 46)
#define BG_BOTTOM rgb565(22, 33, 62)
#define TEXT_WHITE 0xFFFF
#define TEXT_DIM 0x7BEF
#define TEXT_GRAY 0x4A4A

// 状态栏专用
#define STATUS_BG BG_BOTTOM
#define STATUS_TIME_COL TEXT_WHITE
#define STATUS_TITLE_COL rgb565(180, 200, 255)
#define WIFI_ON_COL rgb565(0, 200, 100)  // 绿：已连接
#define WIFI_OFF_COL rgb565(120, 60, 60) // 暗红：断开
#define BT_ON_COL rgb565(0, 160, 220)    // 青：已连接
#define BT_OFF_COL rgb565(120, 60, 60)   // 暗红：断开

// ============ 状态栏区域 ============
#define STATUS_H 30
#define STATUS_Y 0

// ============ 菜单区域 ============
#define ICON_AREA_Y 50
#define ICON_AREA_H 160
#define LABEL_AREA_Y 210
#define LABEL_AREA_H 40
#define HINT_AREA_Y 250
#define HINT_AREA_H 30

#define CENTER_X (SCREEN_WIDTH / 2)

// ============ App 状态枚举 ============
enum AppState
{
    STATE_MENU = 0,
    STATE_SCREENSAVER,
    STATE_DIAL,
    STATE_RGB_LED,
    STATE_REMOTE,
    STATE_MUSIC,
    STATE_WEATHER,
    STATE_AIRMOUSE,
    STATE_GYROSCOPE,
    STATE_COMPASS,
    STATE_BAROMETER,
    STATE_GAME,
    STATE_FILE,
    STATE_SDCARD,
    STATE_WIFI,
    STATE_SETTINGS,
    STATE_ABOUT,
    STATE_COUNT
};

// 前向声明（action 回调）
void actionScreenSaverEntry(void);
void actionDialEntry(void);
void actionLedEntry(void);
void actionRemoteEntry(void);
void actionMusic(void);
void actionWeather(void);
void actionAirMouse(void);
void actionCompass(void);
void actionGyroscope(void);
void actionBarometer(void);
void actionGame(void);
void actionFiles(void);
void actionSDCardEntry(void);
void actionWiFiEntry(void);
void actionSettingsEntry(void);
void actionAboutEntry(void);

class Launcher
{
public:
    void Main();

protected:
    void Init();
    void Update();
    void Draw();
    void Exit();

    void handleRotate(int dir);
    void handleShortPress();

    void drawBackground(void);
    void drawStatusBar(void);
    void drawIconArea(void);
    void drawLabelArea(void);
    void drawHintBar(void);

private:
    struct MenuItem
    {
        const char *label;
        uint16_t iconColor;
        const uint16_t *icon;
        void (*action)(void);
    };

    MenuItem menuItems[MENU_COUNT];

    struct SlotParams
    {
        float scale;
        int yOffset;
        uint8_t alpha;
    };

    const SlotParams slotParams[5] = {
        {0.55f, 38, 70},
        {0.75f, 18, 170},
        {1.00f, 0, 255},
        {0.75f, 18, 170},
        {0.55f, 38, 70},
    };

    LGFX_Sprite *canvas = nullptr;
    LGFX *disp = nullptr;

    int selectedIndex = 0;
    float targetScrollOffset = 0.0f;
    float currentScrollOffset = 0.0f;
    unsigned long last_enc_time = 0;
    int accel_multiplier = 1;
    bool active = false;

    // 用于状态栏每秒刷新（时间 + 连接图标）
    unsigned long last_status_draw = 0;

    // 长按关机
    unsigned long powerPressStart = 0;
    bool powerLongPressTriggered = false;
    bool powerHintShown = false; // 标记关机提示是否已显示
};

extern Launcher g_launcher;

#endif
