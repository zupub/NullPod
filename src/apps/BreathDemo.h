#ifndef _BREATH_DEMO_H_
#define _BREATH_DEMO_H_

#include "hal/hal.h"
#include "hal/hal_config.h"
#include "common.h"
#include "app_state.h"

// ============ 呼吸灯区域 ============
#define BD_STATUS_H 28
#define BD_WAVE_Y 40
#define BD_WAVE_H 120
#define BD_WAVE_X 10
#define BD_WAVE_W 220
#define BD_PARAM_Y 170
#define BD_PARAM_H 80
#define BD_HINT_Y 250
#define BD_HINT_H 30


enum WaveType
{
    WAVE_SINE = 0,
    WAVE_TRIANGLE,
    WAVE_PULSE,
    WAVE_NOISE,
    WAVE_COUNT
};

enum EditMode
{
    EDIT_FREQ = 0,
    EDIT_SAT,
    EDIT_MIN_BRIGHT,
    EDIT_HUE,
    EDIT_WAVE,
    EDIT_COUNT
};

struct BreathParams
{
    float frequency;
    uint8_t saturation;
    uint8_t minBrightness;
    uint8_t hue;
    WaveType waveType;
    bool paused;
};

class BreathDemo
{
public:
    void Main();

protected:
    void Init();
    void Update();
    void Draw();
    void Exit();

    void handleRotate(int dir);
    void handleShortPress();
    void handleLongPress();

    void BD_DrawStatusBar(void);
    void BD_DrawWaveform(void);
    void BD_DrawParamArea(void);
    void BD_DrawHintBar(void);

    float getWaveValue(float ph, WaveType type);
    uint16_t hsv2rgb565(uint16_t h, uint8_t s, uint8_t v);
    void SetLED(uint8_t r, uint8_t g, uint8_t b);

private:
    BreathParams params;
    int currentEdit = EDIT_FREQ;
    float breathPhase = 0.0f;
    uint8_t currentBrightness = 0;
    bool guideMode = false;
    unsigned long guidePhaseStart = 0;
    bool guideInhale = true;
    bool breath_active = false;

    unsigned long last_enc_event_time = 0;
    int accel_multiplier = 1;
    unsigned long last_bd_draw = 0;
    unsigned long last_frame = 0;

    float noiseLUT[64];
};

extern BreathDemo g_breathDemo;
extern const char *waveNames[];
extern const char *editLabels[];

#endif
