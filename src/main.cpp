#include "launcher.h"
#include "apps/DigitalRainAnimation.h"
#include "apps/BreathDemo.h"
#include "apps/FileBrowser.h"
#include "apps/FileManager.h"
#include "apps/AirMouse.h"
#include "apps/RemoteTV.h"
#include "apps/Compass.h"
#include "apps/game/Tetris.h"
#include "apps/AboutApp.h"

// ============ App 状态 ============
static AppState appState = STATE_MENU;

// ============ setup ============
void setup()
{
    HAL::Init();
    Serial.println("System ready");
    g_launcher.Main(); // 首次进入菜单
}

// ============ loop ============
void loop()
{
    switch (appState)
    {
    case STATE_MENU:
        g_launcher.Main();
        break;

    case STATE_SCREENSAVER:
        Serial.println(">>> Matrix App");
        matrix_rain_main();
        appState = STATE_MENU;
        break;

    case STATE_DIAL:
        Serial.println(">>> Dashboard App");
        appState = STATE_MENU;
        break;

    case STATE_RGB_LED:
        Serial.println(">>> RGB LED App");
        g_breathDemo.Main();
        appState = STATE_MENU;
        break;

    case STATE_REMOTE:
        Serial.println(">>> Remote App");
        g_remoteTV.Main();
        appState = STATE_MENU;
        break;

    case STATE_MUSIC:
        Serial.println(">>> Music App");
        appState = STATE_MENU;
        break;

    case STATE_WEATHER:
        Serial.println(">>> Weather App");
        appState = STATE_MENU;
        break;

    case STATE_AIRMOUSE:
        Serial.println(">>> Air Mouse App");
        g_airMouseApp.Main();
        appState = STATE_MENU;
        break;

    case STATE_COMPASS:
        Serial.println(">>> Compass App");
        g_compass.Main();
        appState = STATE_MENU;
        break;

    case STATE_BAROMETER:
        Serial.println(">>> Barometer App");
        appState = STATE_MENU;
        break;

    case STATE_GAME:
        Serial.println(">>> Game App");
        g_tetris.Main();
        appState = STATE_MENU;
        break;

    case STATE_FILE:
        Serial.println(">>> File App");
        g_fileManager.Main();
        appState = STATE_MENU;
        break;

    case STATE_SDCARD:
        Serial.println(">>> SD Card App");
        g_fileBrowser.Main();
        appState = STATE_MENU;
        break;

    case STATE_WIFI:
        Serial.println(">>> WiFi App");
        appState = STATE_MENU;
        break;

    case STATE_SETTINGS:
        Serial.println(">>> Settings App");
        HAL::syncNetworkTime();
        appState = STATE_MENU;
        break;

    case STATE_ABOUT:
        Serial.println(">>> About App");
        g_aboutApp.Main();
        appState = STATE_MENU;
        break;

    default:
        appState = STATE_MENU;
        break;
    }

    if (g_requestScreenSaver)
    {
        g_requestScreenSaver = false;
        appState = STATE_SCREENSAVER;
    }
    else if (g_requestDial)
    {
        g_requestDial = false;
        appState = STATE_DIAL;
    }
    else if (g_requestLed)
    {
        g_requestLed = false;
        appState = STATE_RGB_LED;
    }
    else if (g_requestRemote)
    {
        g_requestRemote = false;
        appState = STATE_REMOTE;
    }
    else if (g_requestMusic)
    {
        g_requestMusic = false;
        appState = STATE_MUSIC;
    }
    else if (g_requestWeather)
    {
        g_requestWeather = false;
        appState = STATE_WEATHER;
    }
    else if (g_requestAirMouse)
    {
        g_requestAirMouse = false;
        appState = STATE_AIRMOUSE;
    }
    else if (g_requestGyroscope)
    {
        g_requestGyroscope = false;
        appState = STATE_GYROSCOPE;
    }
    else if (g_requestCompass)
    {
        g_requestCompass = false;
        appState = STATE_COMPASS;
    }
    else if (g_requestBarometer)
    {
        g_requestBarometer = false;
        appState = STATE_BAROMETER;
    }
    else if (g_requestGame)
    {
        g_requestGame = false;
        appState = STATE_GAME;
    }
    else if (g_requestFile)
    {
        g_requestFile = false;
        appState = STATE_FILE;
    }
    else if (g_requestSD)
    {
        g_requestSD = false;
        appState = STATE_SDCARD;
    }
    else if (g_requestWiFi)
    {
        g_requestWiFi = false;
        appState = STATE_WIFI;
    }
    else if (g_requestSettings)
    {
        g_requestSettings = false;
        appState = STATE_SETTINGS;
    }
    else if (g_requestAbout)
    {
        g_requestAbout = false;
        appState = STATE_ABOUT;
    }
}
