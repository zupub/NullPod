#pragma once

/**
 * @brief Pin configs
 *
 */
// LCD
#define HAL_PIN_LCD_MOSI 7
#define HAL_PIN_LCD_MISO -1
#define HAL_PIN_LCD_SCLK 6
#define HAL_PIN_LCD_DC 4
#define HAL_PIN_LCD_CS 5
#define HAL_PIN_LCD_RST 8
#define HAL_PIN_LCD_BUSY -1
#define HAL_PIN_LCD_BL 15

#define LCD_ROTATION 4
#define LCD_H_RES 240
#define LCD_V_RES 280

#define SCREEN_WIDTH LCD_H_RES
#define SCREEN_HEIGHT LCD_V_RES

// Power
#define HAL_PIN_PWR_BOOT 0
#define HAL_PIN_PWR_KEY 17
#define HAL_PIN_PWR_HOLD 18

// Encoder
#define HAL_PIN_ENCODER_UP 16
#define HAL_PIN_ENCODER_DOWN 12
#define HAL_PIN_ENCODER_LEFT 21
#define HAL_PIN_ENCODER_RIGHT 2
#define HAL_PIN_ENCODER_CHA 9
#define HAL_PIN_ENCODER_CHB 3
#define HAL_PIN_ENCODER_SEL 0

// SD card
#define HAL_PIN_SD_CS 41
#define HAL_PIN_SD_MOSI 38
#define HAL_PIN_SD_MISO 40
#define HAL_PIN_SD_CLK 39

// I2C
#define HAL_PIN_I2C_SCL 10
#define HAL_PIN_I2C_SDA 11

// Touch
#define HAL_PIN_TOUCH_SCL HAL_PIN_I2C_SCL
#define HAL_PIN_TOUCH_SDA HAL_PIN_I2C_SDA
#define HAL_PIN_TOUCH_RST 13
#define HAL_PIN_TOUCH_IRQ 14

// Battery
#define HAL_PIN_BAT_ADC 1

// Buzzer
#define HAL_PIN_BUZZER 46
#define HAL_BUZZ_CHANNEL 2

// RGB
#define HAL_PIN_RGB 45

// IR remote
#define HAL_PIN_IR 42

// Color
#define TFT_GREY 0x5AEB

// NVS
#define CONFIG_NAMESPACE "config"
#define INIT_KEY "init"
#define FFAT_KEY "ffat"
#define LCD_BK_BRIGHTNESS_KEY "lcd_bk"
#define LCD_BK_TIME_OUT_KEY "lcd_time"
#define WIFI_SSID_KEY "wifi_ssid"
#define WIFI_PASSWORD_KEY "wifi_password"

// HAL uart logger
#define HAL_LOGI(fmt, ...) Serial.printf("[HAL][I] %s:%d " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define HAL_LOGE(fmt, ...) Serial.printf("[HAL][E] %s:%d " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define HAL_LOGW(fmt, ...) Serial.printf("[HAL][W] %s:%d " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

// HAL internal display logger
#define HAL_LOGGER_INIT()  \
    canvas.setTextSize(1); \
    canvas.setCursor(0, 0)

#define HAL_LOG(fmt, args...)                      \
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK); \
    canvas.printf(fmt, ##args);                    \
    canvas.print('\n');                            \
    canvas.pushSprite(0, 0)

#define HAL_LOG_TAG_START()                        \
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK); \
    canvas.print(" [")

#define HAL_LOG_TAG_END()                          \
    canvas.setTextColor(TFT_LIGHTGREY, TFT_BLACK); \
    canvas.print("] ")

#define HAL_LOG_INFO(fmt, args...)                   \
    HAL_LOG_TAG_START();                             \
    canvas.setTextColor(TFT_GREENYELLOW, TFT_BLACK); \
    canvas.print("info");                            \
    HAL_LOG_TAG_END();                               \
    HAL_LOG(fmt, ##args)

#define HAL_LOG_WARN(fmt, args...)              \
    HAL_LOG_TAG_START();                        \
    canvas.setTextColor(TFT_YELLOW, TFT_BLACK); \
    canvas.print("warn");                       \
    HAL_LOG_TAG_END();                          \
    HAL_LOG(fmt, ##args)

#define HAL_LOG_ERROR(fmt, args...)          \
    HAL_LOG_TAG_START();                     \
    canvas.setTextColor(TFT_RED, TFT_BLACK); \
    canvas.print("error");                   \
    HAL_LOG_TAG_END();                       \
    HAL_LOG(fmt, ##args)
