#ifndef __LGFX_H__
#define __LGFX_H__

#define LGFX_USE_V1
#define PORTRAIT

#include <LovyanGFX.hpp>

#define LCD_MOSI_PIN 7
#define LCD_MISO_PIN -1
#define LCD_SCLK_PIN 6
#define LCD_DC_PIN 4
#define LCD_CS_PIN 5
#define LCD_RST_PIN 8
#define LCD_BUSY_PIN -1
#define LCD_BL_PIN 15

#define TOUCH_SDA_PIN 11
#define TOUCH_SCL_PIN 10
#define TOUCH_RST_PIN 13
#define TOUCH_IRQ_PIN 14

#define CST816S_ADDRESS 0x15

#ifdef PORTRAIT
static const uint32_t screenWidth = 240;
static const uint32_t screenHeight = 280;
#else
static const uint32_t screenWidth = 280;
static const uint32_t screenHeight = 240;
#endif

#define SCR 30
class LGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI _bus_instance;
    lgfx::Light_PWM _light_instance;
    lgfx::Touch_CST816S _touch_instance;

public:
    LGFX(void)
    {
        {
            auto cfg = _bus_instance.config();

            cfg.pin_mosi = LCD_MOSI_PIN;
            cfg.pin_miso = LCD_MISO_PIN;
            cfg.pin_sclk = LCD_SCLK_PIN;
            cfg.pin_dc = LCD_DC_PIN;
            cfg.freq_write = 40000000;

            _bus_instance.config(cfg);              // Apply the settings to the bus.
            _panel_instance.setBus(&_bus_instance); // Sets the bus to the panel.
        }

        { // Set display panel control.
            auto cfg = _panel_instance.config();

            cfg.invert = true;
            cfg.rgb_order = true;
            cfg.pin_cs = LCD_CS_PIN;
            cfg.pin_rst = LCD_RST_PIN;
            cfg.pin_busy = LCD_BUSY_PIN;
            cfg.panel_width = screenWidth;
            cfg.panel_height = screenHeight;
            cfg.offset_x = 0;
            cfg.offset_y = 20;
#ifdef PORTRAIT
            cfg.offset_rotation = 0;
#else
            cfg.offset_rotation = 1;
#endif
            _panel_instance.config(cfg);
        }

        // {                                        // Set backlight control. (delete if not necessary)
        //     auto cfg = _light_instance.config(); // バックライト設定用の構造体を取得します。

        //     cfg.pin_bl = LCD_BL_PIN; // バックライトが接続されているピン番号
        //     cfg.invert = false;      // バックライトの輝度を反転させる場合 true
        //     // cfg.freq   = 44100;           // バックライトのPWM周波数
        //     cfg.freq = 200;      // バックライトのPWM周波数
        //     cfg.pwm_channel = 7; // 使用するPWMのチャンネル番号

        //     _light_instance.config(cfg);
        //     _panel_instance.setLight(&_light_instance); // バックライトをパネルにセットします。
        // }

        { // Configure settings for touch screen control. (delete if not necessary)
            auto cfg = _touch_instance.config();

            cfg.x_min = 0;                // Minimum X value (raw value) obtained from the touchscreen
            cfg.x_max = screenWidth - 1;  // Maximum X value (raw value) obtained from the touchscreen
            cfg.y_min = 0;                // Minimum Y value obtained from touchscreen (raw value)
            cfg.y_max = screenHeight - 1; // Maximum Y value (raw value) obtained from the touchscreen
            cfg.bus_shared = false;
            cfg.offset_rotation = 0;

            // For I2C connection
            cfg.i2c_port = 0;               // Select I2C to use (0 or 1)
            cfg.i2c_addr = CST816S_ADDRESS; // I2C device address number
            cfg.pin_sda = TOUCH_SDA_PIN;    // pin number where SDA is connected
            cfg.pin_scl = TOUCH_SCL_PIN;    // pin number to which SCL is connected
            cfg.pin_rst = TOUCH_RST_PIN;
            cfg.pin_int = TOUCH_IRQ_PIN; // pin number to which INT is connected
            cfg.freq = 400000;           // set I2C clock

            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance); // Set the touchscreen to the panel.
        }

        setPanel(&_panel_instance); // Sets the panel to use.
    }
};

#endif
