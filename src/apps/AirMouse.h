#ifndef _AIR_MOUSE_APP_H_
#define _AIR_MOUSE_APP_H_

#include "hal/hal.h"
#include "hal/hal_config.h"
#include "common.h"

class AirMouseApp
{
public:
    void Main();

protected:
    void Init();
    void Update();
    void Exit();

private:
    void imuCalibrate();
    void drawUI();

private:
    LGFX *disp = nullptr;
    LGFX_Sprite *canvas = nullptr;

    static constexpr int CX = SCREEN_WIDTH / 2;
    static constexpr int CY = SCREEN_HEIGHT / 2;

    static constexpr int BIG_RADIUS = 80;
    static constexpr int SMALL_RADIUS = 30;

    static constexpr float MOUSE_SENSITIVITY = 1.0f; // Improve sensitivity

    /* State */
    bool calibrated = false;
    bool mouse_left_down = false;

    /* UI */
    uint16_t bg_color = MENU_BG_COLOR;
    uint16_t header_color = MENU_HEADER_COLOR;
    uint16_t fg_color = MENU_TITLE_COLOR;
};

extern AirMouseApp g_airMouseApp;

#endif
