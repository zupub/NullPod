/**
 * @file Button.h
 * @brief 非阻塞按键类 - 支持短按、长按、按住持续触发、NES连发
 * @version 1.0
 * @date 2025-01-20
 *
 * 特性：
 *   - 非阻塞扫描，适合放在 loop() 或定时器回调中
 *   - 支持短按 / 长按识别（可配置时间阈值）
 *   - 支持 held() 持续按住状态检测（NES 风格移动）
 *   - 支持 repeated() 连发模式（首帧延迟 + 固定间隔）
 *   - 支持高/低电平有效（默认低电平有效，适配上拉按键）
 *   - 每个实例独立时间戳，互不干扰
 *
 * 用法示例：
 *   @code
 *   #include "Button.h"
 *
 *   Button upBtn(16, true);       // GPIO16, 低电平有效
 *   upBtn.setShortId(1);          // 短按事件ID
 *   upBtn.setLongId(2);           // 长按事件ID
 *   upBtn.setLongPressTime(300);  // 长按阈值 300ms
 *
 *   void loop() {
 *       upBtn.update();
 *
 *       if (upBtn.pressed()) {
 *           Serial.println(upBtn.event());  // 1=短按, 2=长按
 *       }
 *
 *       if (upBtn.held()) {
 *           // NES 风格：按住持续移动
 *           player_y -= 1;
 *       }
 *
 *       if (upBtn.repeated(200, 50)) {
 *           // 按住 200ms 后开始连发，每 50ms 触发一次
 *           player_y -= 1;
 *       }
 *   }
 *   @endcode
 */

#pragma once

#include <Arduino.h>

class Button
{
public:
    /**
     * @brief 构造函数
     * @param pin        GPIO 引脚号
     * @param activeLow  true=低电平有效（默认，适配 INPUT_PULLUP）
     *                   false=高电平有效
     * @param debounceMs 消抖时间（毫秒），默认 5ms
     */
    explicit Button(uint8_t pin, bool activeLow = true, uint16_t debounceMs = 5);

    /**
     * @brief 析构函数，不负责释放 GPIO
     */
    ~Button() = default;

    /**
     * @brief 非阻塞更新按键状态
     * @note  必须在 loop() 或定时器中周期性调用
     */
    void update();

    // ==================== 事件接口 ====================

    /**
     * @brief 是否有按键事件（短按或长按）
     * @return true  本次 update 检测到一次有效按键
     * @return false 无事件
     * @note  事件一旦读取不会自动清除，需配合 event() 使用
     */
    bool pressed() const;

    /**
     * @brief 获取当前事件 ID
     * @return 短按返回 shortId，长按返回 longId
     * @note  应在 pressed()==true 后立即读取
     */
    uint8_t event() const;

    /**
     * @brief 清除当前事件标志
     * @note  pressed() 返回 true 后，事件会保持直到调用 clear()
     *        或下次 update() 检测到新事件
     */
    void clear();

    // ==================== 状态接口 ====================

    /**
     * @brief 按键是否被持续按住
     * @return true  按键处于按下状态（未释放）
     * @return false 按键已释放或未被按下
     * @note  适合 NES 风格持续移动
     */
    bool held() const;

    /**
     * @brief 连发模式（NES 经典手感）
     * @param firstDelayMs    首次延迟（毫秒），按下后等待多久开始连发
     * @param repeatIntervalMs 连发间隔（毫秒）
     * @return true  本次调用应触发一次动作
     * @return false 未到触发时机
     * @note  典型值：firstDelayMs=200, repeatIntervalMs=50
     *        按下立即响应一次 → 等待 200ms → 每 50ms 触发一次
     */
    bool repeated(uint16_t firstDelayMs, uint16_t repeatIntervalMs);

    /**
     * @brief 当前按键原始状态（未经消抖）
     * @return true  当前被按下
     * @return false 当前未按下
     */
    bool isDown() const;

    // ==================== 配置接口 ====================

    /**
     * @brief 设置短按事件 ID
     * @param id 事件标识
     */
    void setShortId(uint8_t id);

    /**
     * @brief 设置长按事件 ID
     * @param id 事件标识
     */
    void setLongId(uint8_t id);

    /**
     * @brief 设置长按时间阈值
     * @param ms 毫秒数，默认 300ms
     */
    void setLongPressTime(uint16_t ms);

    /**
     * @brief 设置消抖时间
     * @param ms 毫秒数，默认 5ms
     */
    void setDebounceTime(uint16_t ms);

    /**
     * @brief 获取绑定的 GPIO 引脚号
     */
    uint8_t getPin() const;

private:
    uint8_t _pin;         // GPIO 引脚
    bool _activeLow;      // 有效电平：true=LOW, false=HIGH
    uint16_t _debounceMs; // 消抖时间

    // 状态机
    bool _curState = false;       // 当前消抖后的状态（按下=true）
    bool _prevState = false;      // 上次消抖后的状态
    bool _waitingRelease = false; // 等待释放标志
    bool _pressedFlag = false;    // 事件标志
    uint8_t _eventId = 0;         // 当前事件ID

    // 时间
    unsigned long _pressStart = 0; // 按下时刻
    unsigned long _lastCheck = 0;  // 上次消抖检查时刻
    unsigned long _lastRepeat = 0; // 上次连发时刻

    // 事件配置
    uint8_t _shortId = 0; // 短按ID
    uint8_t _longId = 0;  // 长按ID
    uint16_t _longPressMs = 500;

    /**
     * @brief 读取并归一化引脚电平
     * @return true  按键被按下（已按 activeLow 转换）
     * @return false 按键未按下
     */
    bool readPin() const;
};
