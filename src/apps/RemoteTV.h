#ifndef _REMOTE_TV_H_
#define _REMOTE_TV_H_

#include "hal/hal.h"
#include "hal/hal_config.h"
#include "common.h"

// ============ IR 协议类型 ============
#define IR_PROTO_NEC 1
#define IR_PROTO_RC5 2
#define IR_PROTO_SAMSUNG 3
#define IR_PROTO_SONY 4

// ============ TV 遥控 App 页面状态 ============
enum TVPage
{
    TV_HOME = 0, // 设备卡 + 环形音量/频道
    TV_PAD,      // 十字方向键
    TV_LIST,     // 频道列表
    TV_MACRO,    // 情景模式
    TV_LEARN,    // 红外学习（接收+波形显示）
    TV_PAGE_COUNT
};

// ============ 调节对象（HOME 页旋转切换）============
enum TVAdjustTarget
{
    TV_ADJ_PWR = 0, // 电源
    TV_ADJ_MUTE,    // 静音
    TV_ADJ_SRC,     // 输入源
    TV_ADJ_VOL,     // 音量
    TV_ADJ_CH,      // 频道
    TV_ADJ_COUNT
};

// ============ 输入源 ============
enum TVSource
{
    TV_SRC_HDMI1 = 0,
    TV_SRC_HDMI2,
    TV_SRC_AV,
    TV_SRC_TV,
    TV_SRC_COUNT
};

// ============ IR 键值描述 ============
struct IRKey
{
    const char *label;
    uint8_t protocol; // IR_PROTO_xxx
    uint8_t addr;
    uint8_t cmd;
};

// ============ 电视设备描述 ============
struct TVDevice
{
    const char *name;
    IRKey power;
    IRKey mute;
    IRKey home;
    IRKey src;
    IRKey menu;
    IRKey volUp;
    IRKey volDown;
    IRKey chUp;
    IRKey chDown;
    IRKey up;
    IRKey down;
    IRKey left;
    IRKey right;
    IRKey ok;
    IRKey back;
};

// ============ 学习到的 IR 数据 ============
struct IRLearnData
{
    bool valid;            // 是否成功学到
    uint8_t protocol;      // 协议类型 (IR_PROTO_xxx)
    uint16_t address;      // 解码地址
    uint16_t command;      // 解码命令
    uint32_t rawData;      // 32位原始数据
    uint16_t numberOfBits; // 位数
    bool isRepeat;         // 是否为重复帧
    bool isUnknown;        // 是否为未知协议

    // 原始时序数据（用于波形绘制）
    uint16_t rawBuf[80]; // 原始时序缓冲（单位：50us tick）
    uint16_t rawLen;     // 有效长度

    // 协议文本描述
    char protoText[20]; // 协议名称字符串
    char infoLine1[32]; // 第一行信息
    char infoLine2[32]; // 第二行信息
};

// ============ PAD 按钮索引 ============
enum PadBtnIndex
{
    PAD_BTN_UP = 0,
    PAD_BTN_RIGHT,
    PAD_BTN_DOWN,
    PAD_BTN_LEFT,
    PAD_BTN_OK,
    PAD_BTN_COUNT
};

// ============ RemoteTV 类 ============
class RemoteTV
{
public:
    void Main();

protected:
    void Init();
    void Update();
    void Draw();
    void Exit();

    // 页面绘制
    void drawHome(void);
    void drawPad(void);
    void drawList(void);
    void drawMacro(void);
    void drawLearn(void);

    // 通用页面头部、提示栏
    void drawPageHeader(const char *title);
    void drawHintBar(const char *hint);

    // HOME 子绘制
    void drawDeviceCard(void);
    void drawCenterDial(void);
    void drawQuickBar(void);
    void drawPadBtn(int x, int y, const char *lbl, bool pressed);

    // 编码器处理
    void handleRotate(int dir);
    void handleShortPress(void);
    void handleLongPress(void);

    // 方向键处理（新增）
    void handleDirectionKey(uint8_t key);

    // IR 发送
    void sendIR(const IRKey &key);
    void sendMacro(int idx);

    // IR 学习
    void checkLearnIR(void);
    void processLearnResult(void);
    void drawIRWaveform(int x, int y, int w, int h);

    // 工具
    void drawArc(int cx, int cy, int r, float startAngle, float endAngle, uint16_t color, int thickness);
    void printProtoName(uint8_t proto, char *buf, int buflen);

private:
    LGFX *disp = nullptr;
    LGFX_Sprite *canvas = nullptr;

    static constexpr int CX = SCREEN_WIDTH / 2;
    static constexpr int CY = SCREEN_HEIGHT / 2;

    // 页面头部高度（原 MENU_HEADER_HEIGHT 区域），各页内容从此 Y 之下开始
    static constexpr int PAGE_HEADER_H = 32;

    bool active = false;
    TVPage page = TV_HOME;
    TVAdjustTarget adjTarget = TV_ADJ_VOL;

    int volume = 18;
    int channel = 7;
    TVSource source = TV_SRC_HDMI1;
    bool powerOn = true;
    bool muted = false;

    unsigned long last_enc_time = 0;
    int accel_multiplier = 1;

    bool sendPulse = false;
    unsigned long sendPulseStart = 0;

    // 缓存最近一次旋转方向，供短按使用
    int last_dir = 0;

    // IR 学习状态
    IRLearnData learnData;
    bool learnWaiting = false; // 是否在学习页等待信号

    // BTN_POWER 防抖状态
    bool powerPressed = false;                             // 当前是否处于按下状态
    unsigned long powerPressStart = 0;                     // 按下起始时间
    bool powerLongFired = false;                           // 长按事件是否已触发
    static constexpr unsigned long POWER_LONG_MS = 500;    // 长按阈值 500ms
    static constexpr unsigned long POWER_DEBOUNCE_MS = 30; // 防抖 30ms

    // PAD 页面按动反馈
    bool padPressed[PAD_BTN_COUNT] = {false};              // 是否处于按下态
    unsigned long padPressedTick[PAD_BTN_COUNT] = {0};     // 按下时刻
    static constexpr unsigned long PAD_PRESS_HOLD_MS = 90; // 自动回弹时长(ms)

    TVDevice device;
};

extern RemoteTV g_remoteTV;

#endif
