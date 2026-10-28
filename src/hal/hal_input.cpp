#include "hal/hal.h"
#include "hal/hal_config.h"

std::array<uint8_t, ENCODER::ENCODER_BUTTON_NUM> _gamepad_key_map;
std::array<bool, ENCODER::ENCODER_BUTTON_NUM> _key_state_list;

void HAL::Input_Init()
{
    _gamepad_key_map[ENCODER::BTN_POWER] = HAL_PIN_PWR_KEY;
    _gamepad_key_map[ENCODER::BTN_SELECT] = HAL_PIN_PWR_BOOT;
    _gamepad_key_map[ENCODER::BTN_UP] = HAL_PIN_ENCODER_UP;
    _gamepad_key_map[ENCODER::BTN_LEFT] = HAL_PIN_ENCODER_LEFT;
    _gamepad_key_map[ENCODER::BTN_RIGHT] = HAL_PIN_ENCODER_RIGHT;
    _gamepad_key_map[ENCODER::BTN_DOWN] = HAL_PIN_ENCODER_DOWN;

    // GPIO init
    for (auto &i : _gamepad_key_map)
    {
        gpio_reset_pin((gpio_num_t)i);
        pinMode(i, INPUT_PULLUP);
    }

    _key_state_list.fill(false);
}

bool HAL::GetButton(uint8_t button)
{
    if (!digitalRead(_gamepad_key_map[button]))
    {
        // If just pressed
        if (!_key_state_list[button])
        {
            _key_state_list[button] = true;
            beep(600, 20);
        }

        return true;
    }

    // If just released
    if (_key_state_list[button])
    {
        _key_state_list[button] = false;
        beep(800, 20);
    }

    return false;
}

bool HAL::isReturn()
{
    return GetButton(ENCODER::BTN_POWER);
}

bool HAL::isEnter()
{
    return GetButton(ENCODER::BTN_SELECT);
}
