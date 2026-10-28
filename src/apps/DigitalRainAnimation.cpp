#include "hal/hal.h"
#include "DigitalRainAnimation.h"

DigitalRainAnimation<LGFX> matrix_effect;

void matrix_rain_init()
{
    HAL::GetDisplay()->fillScreen(TFT_BLACK);
    matrix_effect.init(HAL::GetDisplay());
    matrix_effect.setup(
        10 /* Line Min */,
        30, /* Line Max */
        5,  /* Speed Min */
        25, /* Speed Max */
        30 /* Screen Update Interval */);
}

void matrix_rain_run()
{
    matrix_effect.loop();
    delay(50);
}

void matrix_rain_main()
{
    matrix_rain_init();

    while (1)
    {
        matrix_rain_run();
        if (HAL::isReturn())
            break;
    }

    delay(200);
}
