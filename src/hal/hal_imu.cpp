#include "hal/hal.h"
#include "hal/hal_config.h"
#include "SparkFun_BMI270_Arduino_Library.h"

#define IMU_SAMPLE_RATE_HZ 100.0f // 固定采样率 100Hz
#define IMU_DT (1.0f / IMU_SAMPLE_RATE_HZ)
#define IMU_CALIB_SAMPLES 200 // 上电校准采样次数
#define IMU_CALIB_DELAY_MS 5  // 每次采样间隔(ms)，约 200*5=1s

// 互补滤波系数 (陀螺仪 / 加速度计)
#define COMP_FILT_GYRO_COEF 0.98f
#define COMP_FILT_ACC_COEF 0.02f

ImuData_t _imu_data;
static BMI270 _bmi270;
ImuMountDir_t _mountDir = IMU_MOUNT_REAR;

// 陀螺仪零偏
static float _gyroOffsetX;
static float _gyroOffsetY;
static float _gyroOffsetZ;

// 互补滤波累加角度
static float _angleX;
static float _angleY;
static float _angleZ;

// 时间控制
static uint32_t _lastUpdateMs;

/* 零偏校准：上电静置时采样求平均 */
static void calibrateGyroBias()
{
    Serial.println("IMU calibrating... keep device still.");

    float gxSum = 0, gySum = 0, gzSum = 0;

    for (int i = 0; i < IMU_CALIB_SAMPLES; i++)
    {
        _bmi270.getSensorData();

        gxSum += _bmi270.data.gyroX;
        gySum += _bmi270.data.gyroY;
        gzSum += _bmi270.data.gyroZ;

        delay(IMU_CALIB_DELAY_MS);
    }

    _gyroOffsetX = gxSum / IMU_CALIB_SAMPLES;
    _gyroOffsetY = gySum / IMU_CALIB_SAMPLES;
    _gyroOffsetZ = gzSum / IMU_CALIB_SAMPLES;

    Serial.printf("Gyro offset: X=%.4f Y=%.4f Z=%.4f\n", _gyroOffsetX, _gyroOffsetY, _gyroOffsetZ);
    Serial.println("IMU calibration done.");
}

void HAL::IMU_Init()
{
    HAL_LOG_INFO("imu init");
    Serial.println("IMU BMI270 init...");

    if (_bmi270.beginI2C(BMI2_I2C_PRIM_ADDR) != BMI2_OK)
    {
        HAL_LOG_ERROR("imu init failed!");
        Serial.println("IMU init failed!");
        return;
    }

    /* 启动后先做零偏校准（要求板子静置 ~1 秒） */
    HAL_LOG_INFO("imu calibrating...");
    delay(200);
    calibrateGyroBias();
    _lastUpdateMs = millis();
}

void applyMountTransform(float &ax, float &ay, float &az, float &gx, float &gy, float &gz)
{
    if (_mountDir == IMU_MOUNT_REAR)
    {
        ax = -ax;
        ay = -ay;
        az = -az;
        gx = -gx;
        gy = -gy;
    }
}

void HAL::updateImuData()
{
    uint32_t now = millis();
    float dt = (now - _lastUpdateMs) / 1000.0f;

    // 固定频率控制（100Hz），未到时间直接返回
    if (dt < IMU_DT)
        return;

    _lastUpdateMs = now;

    _bmi270.getSensorData();

    // --- 原始传感器数据 ---
    float ax = _bmi270.data.accelX;
    float ay = _bmi270.data.accelY;
    float az = _bmi270.data.accelZ;

    float gx = _bmi270.data.gyroX;
    float gy = _bmi270.data.gyroY;
    float gz = _bmi270.data.gyroZ;

    // --- 安装方向坐标变换 ---
    applyMountTransform(ax, ay, az, gx, gy, gz);

    // --- 减去陀螺仪零偏 ---
    gx -= _gyroOffsetX;
    gy -= _gyroOffsetY;
    gz -= _gyroOffsetZ;

    // --- 加速度计角度（roll / pitch）---
    // float accRoll = atan2(ay, az + abs(ax)) * 360 / 2.0 / PI;
    // float accPitch = atan2(ax, az + abs(ay)) * 360 / -2.0 / PI;
    // float accRoll = atan2(ay, az + abs(ax)) * RAD_TO_DEG;
    // float accPitch = -atan2(ax, az + abs(ay)) * RAD_TO_DEG;

    float accRoll = atan2(ay, az + fabsf(ax)) * RAD_TO_DEG;
    float accPitch = -atan2(ax, az + fabsf(ay)) * RAD_TO_DEG;
    if (_mountDir == IMU_MOUNT_REAR)
    {
        accRoll = -accRoll;
        accPitch = -accPitch;
    }

    // --- 标准互补滤波 ---
    _angleX = COMP_FILT_GYRO_COEF * (_angleX + gx * dt) + COMP_FILT_ACC_COEF * accRoll;
    _angleY = COMP_FILT_GYRO_COEF * (_angleY + gy * dt) + COMP_FILT_ACC_COEF * accPitch;
    _angleZ += gz * dt; // yaw 无磁力计，仅陀螺仪积分

    // --- 输出数据 ---
    _imu_data.roll = _angleX;
    _imu_data.pitch = _mountDir == IMU_MOUNT_REAR ? -_angleY : _angleY;
    _imu_data.yaw = _mountDir == IMU_MOUNT_REAR ? -_angleZ : _angleZ;

    _imu_data.angle_x = _angleX;
    _imu_data.angle_y = _angleY;
    _imu_data.angle_z = _angleZ;

    _imu_data.gyro_x = gx;
    _imu_data.gyro_y = gy;
    _imu_data.gyro_z = gz;
}

void HAL::updateMouseDelta(float sensitivity)
{
    static float last_angle_x = 0.0f;
    static float last_angle_y = 0.0f;
    static float last_angle_z = 0.0f;

    // 基于「互补滤波角度」算增量
    float delta_x = _imu_data.angle_x - last_angle_x;
    float delta_y = _imu_data.angle_y - last_angle_y;
    float delta_z = _imu_data.angle_z - last_angle_z;

    last_angle_x = _imu_data.angle_x;
    last_angle_y = _imu_data.angle_y;
    last_angle_z = _imu_data.angle_z;

    // 符号按实测方向微调, pitch→dx, roll→dy
    _imu_data.mouse_dx = static_cast<int>(delta_z * sensitivity * 12.0f);
    _imu_data.mouse_dy = static_cast<int>(-delta_x * sensitivity * 12.0f);

    const int max_move = 50;
    _imu_data.mouse_dx = constrain(_imu_data.mouse_dx, -max_move, max_move);
    _imu_data.mouse_dy = constrain(_imu_data.mouse_dy, -max_move, max_move);
}

void HAL::imu_calibrate_gyro() { calibrateGyroBias(); };
ImuData_t HAL::getImuData() { return _imu_data; }
float HAL::imu_get_roll() { return _imu_data.roll; }
float HAL::imu_get_pitch() { return _imu_data.pitch; }
float HAL::imu_get_yaw() { return _imu_data.yaw; }
int HAL::imu_get_mouse_dx() { return _imu_data.mouse_dx; }
int HAL::imu_get_mouse_dy() { return _imu_data.mouse_dy; }
