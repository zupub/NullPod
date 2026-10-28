#ifndef _APP_STATE_H_
#define _APP_STATE_H_

// ===== 请求标志（volatile：跨中断/多线程安全）=====
extern volatile bool g_requestMenu;
extern volatile bool g_requestScreenSaver;
extern volatile bool g_requestDial;
extern volatile bool g_requestLed;
extern volatile bool g_requestRemote;
extern volatile bool g_requestMusic;
extern volatile bool g_requestWeather;
extern volatile bool g_requestAirMouse;
extern volatile bool g_requestGyroscope;
extern volatile bool g_requestCompass;
extern volatile bool g_requestBarometer;
extern volatile bool g_requestGame;
extern volatile bool g_requestFile;
extern volatile bool g_requestSD;
extern volatile bool g_requestWiFi;
extern volatile bool g_requestSettings;
extern volatile bool g_requestAbout;

// ===== 设置请求 =====
void App_RequestScreenSaver(void);
void App_RequestDial(void);
void App_RequestLED(void);
void App_RequestRemote(void);
void App_RequestMusic(void);
void App_RequestWeather(void);
void App_RequestAirMouse(void);
void App_RequestGyroscope(void);
void App_RequestCompass(void);
void App_RequestBarometer(void);
void App_RequestGame(void);
void App_RequestFile(void);
void App_RequestSD(void);
void App_RequestWiFi(void);
void App_RequestSettings(void);
void App_RequestAbout(void);

#endif
