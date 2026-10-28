/**
 * @file Button.cpp
 * @brief 非阻塞按键类实现
 * @version 1.0
 * @date 2025-01-20
 */

#include "Button.h"

// ==================== 构造 / 析构 ====================

Button::Button(uint8_t pin, bool activeLow, uint16_t debounceMs)
    : _pin(pin),
      _activeLow(activeLow),
      _debounceMs(debounceMs)
{
    // 初始化 GPIO
    // 低电平有效时启用内部上拉，高电平有效时设为输入
    pinMode(_pin, _activeLow ? INPUT_PULLUP : INPUT);

    // 同步初始状态，防止首次 update 误触发
    _prevState = readPin();
    _curState = _prevState;
}

// ==================== 核心更新 ====================

void Button::update()
{
    unsigned long now = millis();

    // 消抖：未到时间直接返回
    if (now - _lastCheck < _debounceMs)
        return;
    _lastCheck = now;

    // 读取当前稳定状态
    bool raw = readPin();

    // ---- 按下沿检测（prev=false, cur=true）----
    if (raw && !_prevState)
    {
        _pressStart = now;
        _waitingRelease = true;
        _pressedFlag = false; // 等待释放后再置事件
    }

    // ---- 释放沿检测（prev=true, cur=false）----
    if (!raw && _prevState && _waitingRelease)
    {
        unsigned long duration = now - _pressStart;

        // 判定短按 / 长按
        if (duration < _longPressMs)
        {
            _eventId = _shortId;
        }
        else
        {
            _eventId = _longId;
        }

        _pressedFlag = true;
        _waitingRelease = false;
    }

    // 更新状态
    _prevState = _curState;
    _curState = raw;
}

// ==================== 事件接口 ====================

bool Button::pressed() const
{
    return _pressedFlag;
}

uint8_t Button::event() const
{
    return _eventId;
}

void Button::clear()
{
    _pressedFlag = false;
}

// ==================== 状态接口 ====================

bool Button::held() const
{
    // 正在等待释放 = 当前被按住
    return _waitingRelease;
}

bool Button::repeated(uint16_t firstDelayMs, uint16_t repeatIntervalMs)
{
    if (!held())
    {
        // 未按住，重置连发计时
        _lastRepeat = 0;
        return false;
    }

    unsigned long now = millis();
    unsigned long heldTime = now - _pressStart;

    // 还没到首次延迟
    if (heldTime < firstDelayMs)
        return false;

    // 首次触发（_lastRepeat == 0）
    if (_lastRepeat == 0)
    {
        _lastRepeat = now;
        return true;
    }

    // 间隔触发
    if (now - _lastRepeat >= repeatIntervalMs)
    {
        _lastRepeat = now;
        return true;
    }

    return false;
}

bool Button::isDown() const
{
    return readPin();
}

// ==================== 配置接口 ====================

void Button::setShortId(uint8_t id)
{
    _shortId = id;
}

void Button::setLongId(uint8_t id)
{
    _longId = id;
}

void Button::setLongPressTime(uint16_t ms)
{
    _longPressMs = ms;
}

void Button::setDebounceTime(uint16_t ms)
{
    _debounceMs = ms;
}

uint8_t Button::getPin() const
{
    return _pin;
}

// ==================== 私有方法 ====================

bool Button::readPin() const
{
    int val = digitalRead(_pin);
    return _activeLow ? (val == LOW) : (val == HIGH);
}
