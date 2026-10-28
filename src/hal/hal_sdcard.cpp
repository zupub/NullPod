#include "hal/hal.h"
#include "hal/hal_config.h"
#include <FS.h>
#include <SD.h>

static SPIClass sdSPI(HSPI);

bool _is_sd_card_ready;
void listDir(fs::FS &fs, const char *dirname, uint8_t levels);

bool HAL::SD_isReady(void)
{
    return _is_sd_card_ready;
}

void HAL::SD_Init()
{
    HAL_LOG_INFO("sd card init");

    sdSPI.begin(HAL_PIN_SD_CLK, HAL_PIN_SD_MISO, HAL_PIN_SD_MOSI, HAL_PIN_SD_CS); // SCLK, MISO, MOSI, SS
    if (!SD.begin(HAL_PIN_SD_CS, sdSPI))
    {
        HAL_LOG_ERROR("sd card init failed!");
        Serial.println("SD card mount failed!");
        _is_sd_card_ready = false;
        return;
    }

    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE)
    {
        Serial.println("No SD card attached");
        return;
    }

    Serial.print("SD Card Type: ");
    if (cardType == CARD_MMC)
    {
        Serial.println("MMC");
    }
    else if (cardType == CARD_SD)
    {
        Serial.println("SDSC");
    }
    else if (cardType == CARD_SDHC)
    {
        Serial.println("SDHC");
    }
    else
    {
        Serial.println("UNKNOWN");
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    HAL_LOG_INFO("sd card size: %ldG", SD.cardSize() / 1073741824);
    Serial.printf("SD Card Size: %lluMB\r\n", cardSize);
    _is_sd_card_ready = true;
}

void HAL::SD_ListDir()
{
    if (_is_sd_card_ready)
        listDir(SD, "/", 0);
    else
        Serial.println("SD card init failed or not inserted");
}

bool HAL::tryInitSD()
{
    if (!SD.begin(HAL_PIN_SD_CS, sdSPI))
    {
        Serial.println("SD card mount failed!");
        return false;
    }

    Serial.println("SD card initialized");
    _is_sd_card_ready = true;
    return true;
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if (!root)
    {
        Serial.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if (levels)
            {
                listDir(fs, file.name(), levels - 1);
            }
        }
        else
        {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}
