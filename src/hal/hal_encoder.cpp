#include "hal/hal.h"
#include "hal/hal_config.h"
#include "driver/pcnt.h"
#include "hal/Button.h"

// 按键状态结构体
struct
{
    uint8_t id;
    bool pressed;
} volatile btn;

static Button encoderBtn(HAL_PIN_ENCODER_SEL, true);
static Button upBtn(HAL_PIN_ENCODER_UP, true);
static Button downBtn(HAL_PIN_ENCODER_DOWN, true);
static Button leftBtn(HAL_PIN_ENCODER_LEFT, true);
static Button rightBtn(HAL_PIN_ENCODER_RIGHT, true);

/* Getter */
Button &Encoder_getButton() { return encoderBtn; }
Button &UpBtn_getButton() { return upBtn; }
Button &DownBtn_getButton() { return downBtn; }
Button &LeftBtn_getButton() { return leftBtn; }
Button &RightBtn_getButton() { return rightBtn; }

// PCNT 相关静态变量
static int16_t encoder_count = 0;
static int16_t last_count = 0;

// 按键扫描状态机变量
static unsigned long last_btn_check = 0;
static const unsigned long btn_debounce_ms = 5; // 去抖间隔
static bool btn_last_val = HIGH;                // 上一次采样值（默认上拉高电平）
static bool waiting_release = false;            // 正在等待按键释放
static unsigned long press_start_time = 0;      // 按下时刻

void HAL::Encoder_Init(void)
{
    // 配置事件ID
    encoderBtn.setShortId(BTN_OK_SP);
    encoderBtn.setLongId(BTN_OK_LP);
    encoderBtn.setLongPressTime(300);

    upBtn.setShortId(BTN_UP_SP);
    downBtn.setShortId(BTN_DOWN_SP);
    leftBtn.setShortId(BTN_LEFT_SP);
    rightBtn.setShortId(BTN_RIGHT_SP);

    // 配置 PCNT 单元 0，通道 0，2x 解码
    pcnt_config_t pcnt_cfg = {};
    pcnt_cfg.unit = PCNT_UNIT_0;
    pcnt_cfg.channel = PCNT_CHANNEL_0;
    pcnt_cfg.pulse_gpio_num = HAL_PIN_ENCODER_CHA; // A 相 → pulse 输入
    pcnt_cfg.ctrl_gpio_num = HAL_PIN_ENCODER_CHB;  // B 相 → control 输入
    pcnt_cfg.pos_mode = PCNT_COUNT_INC;            // A 上升沿 +1
    pcnt_cfg.neg_mode = PCNT_COUNT_DEC;            // A 下降沿 -1
    pcnt_cfg.lctrl_mode = PCNT_MODE_REVERSE;       // B 低电平 → 反转加减
    pcnt_cfg.hctrl_mode = PCNT_MODE_KEEP;          // B 高电平 → 保持原方向
    pcnt_cfg.counter_h_lim = 32767;
    pcnt_cfg.counter_l_lim = -32768;
    pcnt_unit_config(&pcnt_cfg);

    // 启用毛刺滤波（推荐值 100，约 1.25μs，可根据实际抖动调整）
    pcnt_set_filter_value(PCNT_UNIT_0, 100);
    pcnt_filter_enable(PCNT_UNIT_0);

    // 启动计数器
    pcnt_counter_pause(PCNT_UNIT_0);
    pcnt_counter_clear(PCNT_UNIT_0);
    pcnt_counter_resume(PCNT_UNIT_0);
}

/* 统一事件接口 */
uint8_t HAL::getEncoder(uint8_t *key)
{
    // 非阻塞按键扫描
    encoderBtn.update();
    upBtn.update();
    downBtn.update();
    leftBtn.update();
    rightBtn.update();

    // 读取 PCNT 计数值，计算增量
    pcnt_get_counter_value(PCNT_UNIT_0, &encoder_count);
    int16_t diff = encoder_count - last_count;
    last_count = encoder_count;

    // 优先级：方向键 > 旋转 > 确认键
    if (upBtn.pressed())
    {
        upBtn.clear();
        *key = upBtn.event();
        return true;
    }
    if (downBtn.pressed())
    {
        downBtn.clear();
        *key = downBtn.event();
        return true;
    }
    if (leftBtn.pressed())
    {
        leftBtn.clear();
        *key = leftBtn.event();
        return true;
    }
    if (rightBtn.pressed())
    {
        rightBtn.clear();
        *key = rightBtn.event();
        return true;
    }

    if (diff != 0)
    {
        *key = (diff > 0) ? BTN_ENC_CC : BTN_ENC_CW;
        return true;
    }

    if (encoderBtn.pressed())
    {
        encoderBtn.clear();
        *key = encoderBtn.event();
        return true;
    }

    return false;
}
