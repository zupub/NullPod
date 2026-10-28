#include "app_state.h"

volatile bool g_requestMenu = false;
volatile bool g_requestScreenSaver = false;
volatile bool g_requestDial = false;
volatile bool g_requestLed = false;
volatile bool g_requestRemote = false;
volatile bool g_requestMusic = false;
volatile bool g_requestWeather = false;
volatile bool g_requestAirMouse = false;
volatile bool g_requestGyroscope = false;
volatile bool g_requestCompass = false;
volatile bool g_requestBarometer = false;
volatile bool g_requestGame = false;
volatile bool g_requestFile = false;
volatile bool g_requestSD = false;
volatile bool g_requestWiFi = false;
volatile bool g_requestSettings = false;
volatile bool g_requestAbout = false;

void App_RequestScreenSaver(void) { g_requestScreenSaver = true; }
void App_RequestDial(void) { g_requestDial = true; }
void App_RequestLED(void) { g_requestLed = true; }
void App_RequestRemote(void) { g_requestRemote = true; }
void App_RequestMusic(void) { g_requestMusic = true; }
void App_RequestWeather(void) { g_requestWeather = true; }
void App_RequestAirMouse(void) { g_requestAirMouse = true; }
void App_RequestGyroscope(void) { g_requestGyroscope = true; }
void App_RequestCompass(void) { g_requestCompass = true; }
void App_RequestBarometer(void) { g_requestBarometer = true; }
void App_RequestGame(void) { g_requestGame = true; }
void App_RequestFile(void) { g_requestFile = true; }
void App_RequestSD(void) { g_requestSD = true; }
void App_RequestWiFi(void) { g_requestWiFi = true; }
void App_RequestSettings(void) { g_requestSettings = true; }
void App_RequestAbout(void) { g_requestAbout = true; }
