#ifndef __HAL_H
#define __HAL_H

#include <Arduino.h>
#include "lgfx.h"
#include <Preferences.h>
#include <vector>
#include "hal/Button.h"

extern LGFX display;
extern LGFX_Sprite canvas;

namespace ENCODER
{
    /**
     * @brief Encoder buttons enum
     *
     */
    enum EncoderButton_t
    {
        BTN_POWER = 0,
        BTN_SELECT,
        BTN_UP,
        BTN_LEFT,
        BTN_RIGHT,
        BTN_DOWN,
        ENCODER_BUTTON_NUM,
    };
} // namespace ENCODER

struct ImuData_t
{
    float accelX;
    float accelY;
    float accelZ;
    float gyro_z;
    float gyro_x;
    float gyro_y;
    float angle_x;
    float angle_y;
    float angle_z;
    float roll;  // Y 轴
    float pitch; // X 轴
    float yaw;   // Z 轴
    int mouse_dx;
    int mouse_dy;
};

// IMU安装方向枚举
typedef enum
{
    IMU_MOUNT_NORMAL = 0, // 正面 (Z 朝上)
    IMU_MOUNT_REAR        // 反面 (PCB 反面, 180° 翻转)
} ImuMountDir_t;

/* MAG */
typedef struct
{
    int16_t deg;
    int16_t x;
    int16_t y;
    int16_t z;
    bool isCalibrated;
} MAG_Info_t;

enum TouchGesture : uint8_t
{
    GESTURE_NONE = 0,
    GESTURE_TAP = 1,        // 轻点（短按抬起）
    GESTURE_LONG_PRESS = 2, // 长按（按住超过阈值时间）
    GESTURE_SWIPE_UP = 3,
    GESTURE_SWIPE_DOWN = 4,
    GESTURE_SWIPE_LEFT = 5,
    GESTURE_SWIPE_RIGHT = 6
};

// Encoder ID
enum EncoderEvent
{
    BTN_ENC_CC = 0, // 逆时针旋转
    BTN_ENC_CW,     // 顺时针旋转
    BTN_OK_SP,      // 短按
    BTN_OK_LP,      // 长按
    BTN_UP_SP,      // 短按
    BTN_UP_LP,      // 长按
    BTN_DOWN_SP,    // 短按
    BTN_DOWN_LP,    // 长按
    BTN_LEFT_SP,    // 短按
    BTN_LEFT_LP,    // 长按
    BTN_RIGHT_SP,   // 短按
    BTN_RIGHT_LP    // 长按
};

/* 按键句柄获取接口 */
Button &Encoder_getButton();
Button &UpBtn_getButton();
Button &DownBtn_getButton();
Button &LeftBtn_getButton();
Button &RightBtn_getButton();

namespace HAL
{
    typedef bool (*CommitFunc_t)(void *info, void *userData);

    void Init();
    void Update();

    /* LCD */
    void LCD_Init();
    void Disp_Logo();
    LGFX *GetDisplay();
    LGFX_Sprite *GetCanvas();

    /* I2C */
    void I2C_Init();
    void I2C_Scan();
    bool bsp_i2c_reg8_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t len);
    bool bsp_i2c_reg8_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t len);

    /* Touch */
    void Touch_Init();
    void updateTouch();
    uint8_t getTouch(uint16_t *x, uint16_t *y, uint16_t threshold);
    uint8_t getGesture(uint16_t *x, uint16_t *y, uint16_t threshold, TouchGesture *gesture);

    /* Button */
    void Input_Init();
    bool GetButton(uint8_t button);

    /* IMU */
    void IMU_Init();
    void updateImuData();
    ImuData_t getImuData();
    void updateMouseDelta(float sensitivity);
    void imu_calibrate_gyro();
    float imu_get_roll();
    float imu_get_pitch();
    float imu_get_yaw();
    int imu_get_mouse_dx();
    int imu_get_mouse_dy();

    /* MAG */
    void MAG_Init();
    void updateMAG();
    void MAG_GetInfo(MAG_Info_t *info);
    void MAG_StartCalibration();
    void MAG_StopCalibration();
    void MAG_ClearCalibration();
    void MAG_GetCalStatus();

    /* Digital Pressure Sensor */
    void DPS_Init();
    void updateDPS();
    float dps_get_temperature();
    float dps_get_pressure();
    float dps_get_altitude();
    void dps_set_atmosphere(float atmosphere);

    /* RTC */
    void RTC_Init();
    void adjustSysTime();
    void syncNetworkTime();
    void setTimeFromCompileTime();

    /* SD */
    void SD_Init();
    bool SD_isReady();
    void SD_ListDir();
    bool tryInitSD();

    /* Power */
    void Power_Init();
    void PowerOff();
    void Reboot();
    void Backlight_Init();
    void Backlight_Off();
    void setBrightness(uint8_t brightness);
    uint8_t getVoltage();

    /* Buzzer */
    void beep(float frequency, uint32_t duration);
    void beepStop();
    void buzz(float frequency, uint32_t duration);

    /* RGB LED */
    void RGB_Init(void);
    void RGB_Demo(uint8_t index);

    /* Encoder */
    void Encoder_Init();
    uint8_t getEncoder(uint8_t *key);
    void Encoder_Update();
    int16_t Encoder_GetDiff();
    bool Encoder_GetIsPush();
    void Encoder_SetEnable(bool en);

    /* Audio */
    void Audio_Init();
    void Audio_Update();
    bool Audio_PlayMusic(const char *name);

    /* Bluetooth */
    bool BT_Init();
    void BT_Update();

    /* System */
    tm *GetLocalTime();
    void logMemory();
    bool isReturn();
    bool isEnter();
    bool isSwipeOut();
}

#endif
