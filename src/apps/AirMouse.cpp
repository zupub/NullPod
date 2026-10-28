#include "AirMouse.h"
#include <BleMouse.h>
#include "assets/icon/icons.h"

BleMouse bleMouse("ESP32 AirMouse", "Vinex", 100);
AirMouseApp g_airMouseApp;

static bool leftPressed = false;
static bool rightPressed = false;

/* 方向键当前按下的键码（0 = 无），用于释放时对应抬起 */
static uint8_t curArrowKey = 0;

void AirMouseApp::Init()
{
    disp = HAL::GetDisplay();
    canvas = HAL::GetCanvas();

    drawUI();

    Serial.println("Starting BLE Mouse (NimBLE-Mouse lib)...");
    bleMouse.begin();
}

void AirMouseApp::imuCalibrate()
{
    // canvas->fillRect(0, 140, 240, 40, bg_color);
    // canvas->drawString("Calibrating...", 120, 160);
    // canvas->pushSprite(0, 0);
    calibrated = true;
}

void AirMouseApp::drawUI()
{
    canvas->fillSprite(bg_color);

    // ===== 标题栏 =====
    canvas->fillRect(0, 0, SCREEN_WIDTH, MENU_HEADER_HEIGHT, header_color);
    canvas->setTextColor(fg_color);
    canvas->setTextSize(1);
    canvas->setFont(&FreeSansBold9pt7b);
    canvas->setCursor(80, 10);
    canvas->print("Air Mouse");

    if (bleMouse.isConnected())
        drawScaledIcon(canvas, 20, 3, 0.50f, icon_bluetooth, false);

    if (leftPressed)
        drawScaledIcon(canvas, CX - 32, CY - 32, 1.00f, icon_left_click, false);
    else if (rightPressed)
        drawScaledIcon(canvas, CX - 32, CY - 32, 1.00f, icon_right_click, false);
    else
        drawScaledIcon(canvas, CX - 32, CY - 32, 1.00f, icon_mouse, false);

    canvas->pushSprite(0, 0);
}

void AirMouseApp::Update()
{
    static uint32_t last = 0;
    if (millis() - last < 10)
        return;
    last = millis();

    /* ========== IMU ========== */
    HAL::updateImuData();
    HAL::updateMouseDelta(MOUSE_SENSITIVITY);
    // Serial.printf("%.2f, %.2f, %.2f\n", HAL::imu_get_roll(), HAL::imu_get_pitch(), HAL::imu_get_yaw());

    int8_t dx = HAL::imu_get_mouse_dx();
    int8_t dy = HAL::imu_get_mouse_dy();
    int8_t wheel = 0;

    /* Encoder */
    uint8_t key;
    if (HAL::getEncoder(&key))
    {
        HAL::beep(500, 20);
        switch (key)
        {
        case BTN_ENC_CW:
            wheel = 1;
            break;
        case BTN_ENC_CC:
            wheel = -1;
            break;
        case BTN_OK_SP:
            if (!leftPressed)
            {
                bleMouse.press(MOUSE_LEFT);
                leftPressed = true;
                delay(10);
            }
            break;
        case BTN_OK_LP:
            if (!rightPressed)
            {
                bleMouse.press(MOUSE_RIGHT);
                rightPressed = true;
                delay(10);
            }
            break;

        /* ===== 上下左右 → HID 键盘方向键 ===== */
        case BTN_UP_SP:
            dy = -5;
            break;
        case BTN_DOWN_SP:
            dy = 5;
            break;
        case BTN_LEFT_SP:
            dx = -5;
            break;
        case BTN_RIGHT_SP:
            dx = 5;
            break;

        default:
            break;
        }
    }

    /* UI */
    drawUI();

    /* ========== 按键释放 ========== */
    if (leftPressed && !HAL::GetButton(ENCODER::BTN_SELECT))
    {
        bleMouse.release(MOUSE_LEFT);
        leftPressed = false;
        delay(10);
    }

    if (rightPressed && !HAL::GetButton(ENCODER::BTN_SELECT))
    {
        bleMouse.release(MOUSE_RIGHT);
        rightPressed = false;
        delay(10);
    }

    /* ========== 发送 HID ========== */
    if (dx || dy || wheel)
    {
        bleMouse.move(dx, dy, wheel);
        // Serial.printf("Move: dx=%d dy=%d wheel=%d | static=%d\n", dx, dy, wheel, imu_static);
    }
}

void AirMouseApp::Exit()
{
}

void AirMouseApp::Main()
{
    Init();
    imuCalibrate();

    while (true)
    {
        Update();
        if (HAL::isReturn())
            break;
    }
    Exit();
}
