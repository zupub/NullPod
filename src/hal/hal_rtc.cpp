#include "hal/hal.h"
#include "hal/hal_config.h"
#include "SensorPCF8563.hpp"

#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>

SensorPCF8563 rtc;
uint32_t lastMillis;
time_t _time_buffer;

void HAL::RTC_Init()
{
    HAL_LOG_INFO("rtc init");
    Serial.println("RTC PCF8563 init...");

    if (!rtc.begin(Wire))
    {
        HAL_LOG_ERROR("rtc init failed!");
        Serial.println("RTC init failed!");
    }

    RTC_DateTime datetime = rtc.getDateTime();
    Serial.printf("Current RTC: %04d-%02d-%02d %02d:%02d:%02d\n",
                  datetime.getYear(), datetime.getMonth(), datetime.getDay(),
                  datetime.getHour(), datetime.getMinute(), datetime.getSecond());

    if (datetime.getYear() < 2025)
    {
        Serial.println("RTC lost power! Setting default time...");
        // rtc.setDateTime(2025, 1, 1, 12, 0, 0);
        setTimeFromCompileTime();
    }

    adjustSysTime();
}

void HAL::adjustSysTime()
{
    struct tm time;
    rtc.getDateTime(&time);
    time_t tv_sec = mktime(&time);
    struct timeval now = {.tv_sec = tv_sec};
    settimeofday(&now, NULL);
}

// ============ NTP 网络对时（非阻塞）============
namespace
{
    HAL::NtpResult s_ntpResult = HAL::NTP_IDLE;
    uint32_t s_ntpStartMs = 0;

    const uint32_t NTP_TIMEOUT_MS = 20000;    // 连 WiFi + 等 SNTP 回包的总超时
    const long NTP_GMT_OFFSET_SEC = 8 * 3600; // UTC+8，无夏令时
}

void HAL::Ntp_Begin()
{
    if (s_ntpResult == NTP_RUNNING)
        return;

    // 复用 WiFi App 存进 NVS 的配置发起连接，不再写死账号
    Wifi_Init();
    if (!Wifi_IsConnected())
        Wifi_Connect();

    // 清掉上一次的同步状态，避免刚进页面就被判成“已同步”
    sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
    configTime(NTP_GMT_OFFSET_SEC, 0, "pool.ntp.org", "ntp.aliyun.com", "time.nist.gov");

    s_ntpStartMs = millis();
    s_ntpResult = NTP_RUNNING;
    Serial.printf("[NTP] begin, wifi=%d\n", (int)Wifi_IsConnected());
}

HAL::NtpResult HAL::Ntp_Update()
{
    if (s_ntpResult != NTP_RUNNING)
        return s_ntpResult;

    // SNTP 在后台回包后状态变为 COMPLETED，此时把时间写进 PCF8563
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED)
    {
        time_t now = time(nullptr);
        struct tm t;
        localtime_r(&now, &t);

        rtc.setDateTime(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                        t.tm_hour, t.tm_min, t.tm_sec);
        adjustSysTime(); // RTC -> 系统时间，保证两边一致

        s_ntpResult = NTP_OK;
        Serial.printf("[NTP] synced: %04d-%02d-%02d %02d:%02d:%02d\n",
                      t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                      t.tm_hour, t.tm_min, t.tm_sec);
        return s_ntpResult;
    }

    if (millis() - s_ntpStartMs >= NTP_TIMEOUT_MS)
    {
        s_ntpResult = NTP_FAIL;
        Serial.printf("[NTP] timeout, wifi=%d\n", (int)Wifi_IsConnected());
    }

    return s_ntpResult;
}

void HAL::Ntp_Reset()
{
    s_ntpResult = NTP_IDLE;
}

void HAL::setTimeFromCompileTime()
{
    // __DATE__ 格式: "Jan 15 2025"
    // __TIME__ 格式: "12:00:00"

    const char *date = __DATE__;
    const char *time_str = __TIME__;

    int year, month, day, hour, minute, second;
    char month_str[4];

    sscanf(date, "%s %d %d", month_str, &day, &year);
    sscanf(time_str, "%d:%d:%d", &hour, &minute, &second);

    // 月份字符串转数字
    const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (month = 0; month < 12; month++)
    {
        if (strcmp(month_str, months[month]) == 0)
            break;
    }
    month++; // 转换为1-12

    rtc.setDateTime(year, month, day, hour, minute, second);
    Serial.printf("Set RTC from compile time: %04d-%02d-%02d %02d:%02d:%02d\n",
                  year, month, day, hour, minute, second);
}

tm *HAL::GetLocalTime()
{
    time(&_time_buffer);
    return localtime(&_time_buffer);
}
