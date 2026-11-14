#include "hal/hal.h"
#include "hal/hal_config.h"

void HAL::Init()
{
    Serial.begin();
    Serial.printf("\r\n- @NullPod\r\n");
    Serial.printf("- @build at %s %s\r\n", __TIME__, __DATE__);
    Serial.printf("Free RAM size: %d\r\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
    Serial.printf("PSRAM free size: %d\r\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    Power_Init();
    LCD_Init();
    I2C_Init();
    Input_Init();
    Encoder_Init();
    RTC_Init();
    IMU_Init();
    SD_Init();
    MAG_Init();
    DPS_Init();
    // RGB_Init();
    Buzzer_Init();
    beep(700, 20);
    delay(1000);
}

// 12/24 小时制（缓存 + NVS 持久化）
// -1 = 尚未从 NVS 读取
static int time_format_24h = -1;

bool HAL::isTime24Hour()
{
    if (time_format_24h < 0)
    {
        Preferences prefs;
        prefs.begin(CONFIG_NAMESPACE);
        time_format_24h = (prefs.getUChar(TIME_FORMAT_KEY, 24) == 12) ? 0 : 1;
        prefs.end();
    }
    return time_format_24h != 0;
}

void HAL::setTime24Hour(bool on)
{
    time_format_24h = on ? 1 : 0;

    Preferences prefs;
    prefs.begin(CONFIG_NAMESPACE);
    prefs.putUChar(TIME_FORMAT_KEY, on ? 24 : 12);
    prefs.end();
}

// WiFi 保持连接：开启后 WiFi 配置 App 退出时不断开射频（缓存 + NVS 持久化）
// -1 = 尚未从 NVS 读取；默认关闭（保持省电的旧行为）
static int wifi_keep_alive = -1;

bool HAL::isWifiKeepAlive()
{
    if (wifi_keep_alive < 0)
    {
        Preferences prefs;
        prefs.begin(CONFIG_NAMESPACE);
        wifi_keep_alive = prefs.getUChar(WIFI_KEEPALIVE_KEY, 0) ? 1 : 0;
        prefs.end();
    }
    return wifi_keep_alive != 0;
}

void HAL::setWifiKeepAlive(bool on)
{
    wifi_keep_alive = on ? 1 : 0;

    Preferences prefs;
    prefs.begin(CONFIG_NAMESPACE);
    prefs.putUChar(WIFI_KEEPALIVE_KEY, on ? 1 : 0);
    prefs.end();
}

void HAL::Update()
{
    // updateTouch();
    // updateImuData();
    // updateMAG();
    // Encoder_Update();
}

bool HAL::isSwipeOut()
{
    uint16_t tx = 0, ty = 0; // To store the touch coordinates
    TouchGesture gesture;

    if (HAL::getGesture(&tx, &ty, 0, &gesture))
    {
        // 有事件发生（按下确认或抬起手势）
        switch (gesture)
        {
        case GESTURE_SWIPE_LEFT:
            return true;
            break;
        default:
            break;
        }
    }

    return false;
}

void HAL::logMemory()
{
    // 内存状态报告
    Serial.printf("Total heap: %d\r\n", ESP.getHeapSize());
    Serial.printf("Free heap: %d\r\n", ESP.getFreeHeap());
    Serial.printf("Total PSRAM: %d\r\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %d\r\n", ESP.getFreePsram());

    uint32_t flash_size;
    esp_flash_get_size(NULL, &flash_size);
    Serial.printf("FLASH size: %dMB\r\n", flash_size / 1048576);
    Serial.printf("Free RAM size: %dKB (Largest block: %dKB)\n",
                  heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024,
                  heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT) / 1024);
    Serial.printf("PSRAM free size: %dKB\r\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024);
}
