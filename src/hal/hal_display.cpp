#include "hal/hal.h"
#include "hal/hal_config.h"
#include <SD.h>

LGFX display = LGFX();
LGFX_Sprite canvas = LGFX_Sprite(&display);

uint32_t bl_freq = 1200;
int16_t bl_pin = -1;
uint8_t bl_offset = 0;
uint8_t bl_pwm_channel = 7;
uint8_t bl_pwm_bits = 9;
uint8_t bl_backlight = 0;
uint32_t flash_size;

void set_lcd_bk_brightness(uint16_t value);

void HAL::LCD_Init()
{
    bl_pin = HAL_PIN_LCD_BL;
    bl_freq = 44100;
    bl_pwm_channel = 7;

    display.begin();
    display.setRotation(0);
    display.fillScreen(TFT_BLACK);

    Serial.printf("TFT W/H: %d, %d\r\n", display.width(), display.height());
    canvas.createSprite(display.width(), display.height());

    delay(100);
    Backlight_Init();

    HAL_LOGGER_INIT();
    Disp_Logo();
    HAL_LOG_INFO("device init");
    HAL_LOG_INFO("backlight: %d", bl_backlight);
    esp_flash_get_size(NULL, &flash_size);
    HAL_LOG_INFO("FLASH size: %dMB", flash_size / 1048576);
    HAL_LOG_INFO("RAM free size: %dKB", heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024);
    HAL_LOG_INFO("PSRAM free size: %dKB", heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
}

LGFX *HAL::GetDisplay()
{
    return &display;
}

LGFX_Sprite *HAL::GetCanvas()
{
    return &canvas;
}

static const std::string _logo = R"(
     __       _ _   ___          _ 
  /\ \ \_   _| | | / _ \___   __| |
 /  \/ / | | | | |/ /_)/ _ \ / _` |
/ /\  /| |_| | | / ___/ (_) | (_| |
\_\ \/  \__,_|_|_\/    \___/ \__,_|
)";

void HAL::Disp_Logo()
{
    canvas.setFont(&fonts::Font0);
    canvas.setTextColor(TFT_ORANGE, TFT_BLACK);

    for (int i = 0; i < _logo.size(); i++)
    {
        canvas.printf("%c", _logo[i]);
        if (i % 20 == 0)
            canvas.pushSprite(0, 0);
    }

    canvas.printf("\n- @author VINEX\n");
    canvas.printf("\n- @build at %s %s\n\n", __TIME__, __DATE__);
    canvas.pushSprite(0, 0);

    delay(1000);
}

void HAL::Backlight_Init()
{
    ledcSetup(bl_pwm_channel, bl_freq, bl_pwm_bits);
    ledcAttachPin(bl_pin, bl_pwm_channel);
    Preferences prefs;
    prefs.begin(CONFIG_NAMESPACE);
    bl_backlight = prefs.getUShort(LCD_BK_BRIGHTNESS_KEY, 0);
    if (bl_backlight < 10)
        bl_backlight = 10;
    setBrightness(bl_backlight);
    prefs.end();
}

void HAL::Backlight_Off()
{
    ledcWrite(bl_pwm_channel, 0);
}

void HAL::setBrightness(uint8_t brightness)
{
    uint32_t duty = 0;
    if (brightness)
    {
        uint_fast16_t ofs = bl_offset;
        if (ofs)
        {
            ofs = ofs * 259 >> 8;
        }
        duty = brightness * (257 - ofs);
        duty += ofs * 255;
        duty += 1 << (15 - bl_pwm_bits);
        duty >>= 16 - bl_pwm_bits;
    }

    ledcWrite(bl_pwm_channel, duty);
    set_lcd_bk_brightness(brightness);
}

void set_lcd_bk_brightness(uint16_t value)
{
    Preferences prefs;
    prefs.begin(CONFIG_NAMESPACE);
    prefs.putUShort(LCD_BK_BRIGHTNESS_KEY, value);
    prefs.end();
    log_d("set LCD_BK_BRIGHTNESS_KEY Config: %d\n", value);
}
