#include "hal/hal.h"
#include "hal/hal_config.h"
#include "SensorPCF8563.hpp"

#include <time.h>
#include <WiFi.h>

const char *ssid = "hotspot";
const char *password = "password";

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

void HAL::syncNetworkTime()
{
    uint8_t timeout;

    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        timeout++;
        if (timeout > 30)
        {
            Serial.println(" timeout.");
            Serial.println();
            return;
        }
    }

    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("Contacting time server...");

    const long gmtOffset_sec = 8 * 3600; // UTC+8 小时
    const int daylightOffset_sec = 0;    // 无夏令时

    configTime(gmtOffset_sec, daylightOffset_sec, "time.nist.gov", "0.pool.ntp.org", "1.pool.ntp.org"); // 配置NTP
    Serial.println("Waiting for time synchronization...");
    delay(3000);

    struct tm timeinfo;
    timeinfo.tm_year = 0;
    getLocalTime(&timeinfo, 5000);
    Serial.printf("Now is : %d-%02d-%02d %02d:%02d:%02d\n", (timeinfo.tm_year) + 1900, (timeinfo.tm_mon) + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    Serial.println("\nSet time to RTC...");
    rtc.setDateTime((timeinfo.tm_year) + 1900, (timeinfo.tm_mon) + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    Serial.println("Time sync completed, disconnecting WiFi...");
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi disconnected.");
    Serial.println("");
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
