/**
 * ============================================================================
 *  QMC5883P
 * ----------------------------------------------------------------------------
 *  功能：
 *    1. 采集校准数据 (绕各轴旋转，记录 X/Y/Z 最大最小值)
 *    2. 计算硬铁偏移 (hard-iron offset)
 *    3. 保存到 Flash (使用 ESP32 的 Preferences，比 EEPROM 更可靠)
 *    4. 上电自动加载校准参数
 *
 *  使用说明：
 *    - 正常运行时自动加载已保存的校准值
 *    - 需要重新校准时，调用 beginCalibration() 进入校准模式
 *      或定义 CALIBRATE_ON_BOOT 上电即校准
 *    - 旋转传感器画"8"字约 30 秒，然后调用 endCalibration()
 * ============================================================================
 */

#include "hal/hal.h"
#include "hal/hal_config.h"
#include <QMC5883PCompass.h>

/* ==================== 用户配置 ==================== */
#define CALIBRATE_ON_BOOT 0         // 设为 1 则每次上电都先校准
#define CAL_SAMPLE_TIMEOUT_MS 30000 // 校准采集时长 (ms)
#define CAL_MIN_SAMPLES 200         // 最少采样次数
#define CAL_DEVIATION_THRESHOLD 50  // 各轴范围需 > 此值才算有效校准
/* ================================================== */

QMC5883PCompass compass;
static int Mag_x = 0, Mag_y = 0, Mag_z = 0, Mag_deg = 0; // MAG数据
static bool Mag_isCalibrated = false;                    // MAG数据是否矫正

/* 校准结果 */
typedef struct
{
    int16_t x_min, x_max;
    int16_t y_min, y_max;
    int16_t z_min, z_max;
    int16_t x_off, y_off, z_off; // 硬铁偏移
    float x_scl, y_scl, z_scl;   // 软铁缩放
    uint8_t valid;               // 是否有有效校准数据
} mag_cal_t;

static mag_cal_t g_cal;
static bool g_calibrating = false;
static uint32_t g_cal_start = 0;
static int g_cal_count = 0;

Preferences prefs;

/* 从 compass 读取最新原始值 */
static void read_raw(int16_t *x, int16_t *y, int16_t *z)
{
    compass.read();
    *x = compass.getX();
    *y = compass.getY();
    *z = compass.getZ();
}

/* 应用校准：返回校准后的磁场值 (供 getAzimuth 使用) */
void applyCalibration(int16_t x, int16_t y, int16_t z, float *cx, float *cy, float *cz)
{
    if (g_cal.valid)
    {
        *cx = ((float)x - g_cal.x_off) * g_cal.x_scl;
        *cy = ((float)y - g_cal.y_off) * g_cal.y_scl;
        *cz = ((float)z - g_cal.z_off) * g_cal.z_scl;
    }
    else
    {
        *cx = (float)x;
        *cy = (float)y;
        *cz = (float)z;
    }
}

/* ==================== Flash 存储 (Preferences) ==================== */
static const char *PREF_NS = "qmc5883p"; // namespace

static void saveCalibration()
{
    prefs.begin(PREF_NS, false);
    prefs.putShort("x_min", g_cal.x_min);
    prefs.putShort("x_max", g_cal.x_max);
    prefs.putShort("y_min", g_cal.y_min);
    prefs.putShort("y_max", g_cal.y_max);
    prefs.putShort("z_min", g_cal.z_min);
    prefs.putShort("z_max", g_cal.z_max);
    prefs.putShort("x_off", g_cal.x_off);
    prefs.putShort("y_off", g_cal.y_off);
    prefs.putShort("z_off", g_cal.z_off);
    prefs.putFloat("x_scl", g_cal.x_scl);
    prefs.putFloat("y_scl", g_cal.y_scl);
    prefs.putFloat("z_scl", g_cal.z_scl);
    prefs.putUChar("valid", g_cal.valid);
    prefs.end();
    Serial.println("[CAL] 校准参数已保存到 Flash");
}

static void loadCalibration()
{
    prefs.begin(PREF_NS, true); // 只读
    g_cal.x_min = prefs.getShort("x_min", 0);
    g_cal.x_max = prefs.getShort("x_max", 0);
    g_cal.y_min = prefs.getShort("y_min", 0);
    g_cal.y_max = prefs.getShort("y_max", 0);
    g_cal.z_min = prefs.getShort("z_min", 0);
    g_cal.z_max = prefs.getShort("z_max", 0);
    g_cal.x_off = prefs.getShort("x_off", 0);
    g_cal.y_off = prefs.getShort("y_off", 0);
    g_cal.z_off = prefs.getShort("z_off", 0);
    g_cal.x_scl = prefs.getFloat("x_scl", 1.0f);
    g_cal.y_scl = prefs.getFloat("y_scl", 1.0f);
    g_cal.z_scl = prefs.getFloat("z_scl", 1.0f);
    g_cal.valid = prefs.getUChar("valid", 0);
    prefs.end();

    if (g_cal.valid)
    {
        Serial.printf("[CAL] 已加载校准: off=(%d,%d,%d) scl=(%.3f,%.3f,%.3f)\n",
                      g_cal.x_off, g_cal.y_off, g_cal.z_off,
                      g_cal.x_scl, g_cal.y_scl, g_cal.z_scl);
    }
    else
    {
        Serial.println("[CAL] 无有效校准数据，请运行校准");
    }
}

/* 清除校准数据 (调试用) */
void clearCalibration()
{
    prefs.begin(PREF_NS, false);
    prefs.clear();
    prefs.end();
    g_cal.valid = 0;
    Serial.println("[CAL] 校准数据已清除");
}

/* ==================== 校准流程 ==================== */

void beginCalibration()
{
    g_calibrating = true;
    g_cal_count = 0;
    g_cal_start = millis();

    /* 初始化为反向极值，便于第一次采样覆盖 */
    g_cal.x_min = g_cal.y_min = g_cal.z_min = 32767;
    g_cal.x_max = g_cal.y_max = g_cal.z_max = -32768;

    Serial.println("[CAL] >>> 进入校准模式 <<<");
    Serial.println("[CAL] 请缓慢旋转传感器，画'8'字，覆盖各方向约 30 秒...");
}

void endCalibration()
{
    g_calibrating = false;

    int16_t x_range = g_cal.x_max - g_cal.x_min;
    int16_t y_range = g_cal.y_max - g_cal.y_min;
    int16_t z_range = g_cal.z_max - g_cal.z_min;

    Serial.printf("[CAL] 采样次数: %d\n", g_cal_count);
    Serial.printf("[CAL] X: [%d, %d] range=%d\n", g_cal.x_min, g_cal.x_max, x_range);
    Serial.printf("[CAL] Y: [%d, %d] range=%d\n", g_cal.y_min, g_cal.y_max, y_range);
    Serial.printf("[CAL] Z: [%d, %d] range=%d\n", g_cal.z_min, g_cal.z_max, z_range);

    /* 有效性检查 */
    if (g_cal_count < CAL_MIN_SAMPLES ||
        x_range < CAL_DEVIATION_THRESHOLD ||
        y_range < CAL_DEVIATION_THRESHOLD ||
        z_range < CAL_DEVIATION_THRESHOLD)
    {
        Serial.println("[CAL] ✗ 校准失败：采样不足或旋转范围不够，请重新校准");
        return;
    }

    /* 硬铁偏移 = (max + min) / 2 */
    g_cal.x_off = (g_cal.x_max + g_cal.x_min) / 2;
    g_cal.y_off = (g_cal.y_max + g_cal.y_min) / 2;
    g_cal.z_off = (g_cal.z_max + g_cal.z_min) / 2;

    /* 软铁缩放：以平均半径归一化，使各轴灵敏度一致 */
    float avg_range = (x_range + y_range + z_range) / 3.0f;
    g_cal.x_scl = avg_range / (float)x_range;
    g_cal.y_scl = avg_range / (float)y_range;
    g_cal.z_scl = avg_range / (float)z_range;

    g_cal.valid = 1;
    saveCalibration();

    Serial.printf("[CAL] ✓ 校准完成: off=(%d,%d,%d) scl=(%.3f,%.3f,%.3f)\n",
                  g_cal.x_off, g_cal.y_off, g_cal.z_off,
                  g_cal.x_scl, g_cal.y_scl, g_cal.z_scl);
}

/* 校准进行中的采样 (在 loop 中持续调用) */
static void cal_collect(int16_t x, int16_t y, int16_t z)
{
    if (x < g_cal.x_min)
        g_cal.x_min = x;
    if (x > g_cal.x_max)
        g_cal.x_max = x;
    if (y < g_cal.y_min)
        g_cal.y_min = y;
    if (y > g_cal.y_max)
        g_cal.y_max = y;
    if (z < g_cal.z_min)
        g_cal.z_min = z;
    if (z > g_cal.z_max)
        g_cal.z_max = z;
    g_cal_count++;

    /* 每 2 秒打印进度 */
    static uint32_t last_print = 0;
    if (millis() - last_print > 2000)
    {
        last_print = millis();
        Serial.printf("[CAL] 采集中... cnt=%d  cur=(%d,%d,%d)\n", g_cal_count, x, y, z);
    }

    /* 超时自动结束 */
    if (millis() - g_cal_start > CAL_SAMPLE_TIMEOUT_MS)
    {
        Serial.println("[CAL] 采集超时，自动结束");
        endCalibration();
    }
}

/* ==================== 对外 API ==================== */
void HAL::MAG_Init()
{
    Serial.println("Compass QMC5883P init...");

    compass.init();
    compass.setSmoothing(10, true);

    /* 加载已保存的校准数据 */
    loadCalibration();

#if CALIBRATE_ON_BOOT
    beginCalibration();
#endif
}

void HAL::updateMAG()
{
    int16_t rx, ry, rz;
    read_raw(&rx, &ry, &rz);

    /* 校准模式下采集数据 */
    if (g_calibrating)
    {
        cal_collect(rx, ry, rz);
    }

    float cx, cy, cz;
    applyCalibration(rx, ry, rz, &cx, &cy, &cz);

    Mag_x = (int)cx;
    Mag_y = (int)cy;
    Mag_z = (int)cz;

    /* 用校准后的 X/Y 计算方位角 (需 compass 内部使用校准值，见下方说明) */
    Mag_deg = compass.getAzimuth();

    // printf("Compass: raw(%d,%d,%d) cal(%d,%d,%d) deg=%d%s\n",
    //        rx, ry, rz, Mag_x, Mag_y, Mag_z, Mag_deg,
    //        g_calibrating ? " [CAL]" : "");

    delay(50);
}

void HAL::MAG_GetInfo(MAG_Info_t *info)
{
    info->x = Mag_x;
    info->y = Mag_y;
    info->z = Mag_z;
    info->deg = Mag_deg;
    info->isCalibrated = (g_cal.valid == 1);
}

/* 提供给应用层触发的校准控制函数 */
void HAL::MAG_StartCalibration() { beginCalibration(); }
void HAL::MAG_StopCalibration()
{
    if (g_calibrating)
        endCalibration();
}
void HAL::MAG_ClearCalibration() { clearCalibration(); }
void HAL::MAG_GetCalStatus()
{
    Serial.printf("[CAL] valid=%d off=(%d,%d,%d)\n", g_cal.valid, g_cal.x_off, g_cal.y_off, g_cal.z_off);
}
