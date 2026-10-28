#include "hal/hal.h"
#include "hal/hal_config.h"
#include "Wire.h"

static SemaphoreHandle_t bsp_i2c_mux = NULL;

void bsp_i2c_init(void);
bool bsp_i2c_reg8_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t len);
bool bsp_i2c_reg8_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t len);

bool bsp_i2c_lock(int timeout_ms);
void bsp_i2c_unlock(void);

void HAL::I2C_Init()
{
    Serial.println("Initialize I2C");
    HAL_LOG_INFO("i2c bus init");

    // Init bus
    bsp_i2c_init();
}

void HAL::I2C_Scan(void)
{
    Serial.println("\n========== I2C Scan Start ==========");

    uint8_t found_devices[128];
    int found_count = 0;
    for (uint8_t addr = 1; addr < 127; addr++)
    {
        uint8_t dummy;
        // 尝试读取寄存器 0x00（任意寄存器均可，只要设备会 ACK）
        Serial.printf("Scan: 0x%02X ", addr);
        if (HAL::bsp_i2c_reg8_read(addr, 0x00, &dummy, 1))
        {
            Serial.println("OK");
            found_devices[found_count++] = addr;
        }
    }

    Serial.printf("Found %d I2C device(s):\n", found_count);
    for (int i = 0; i < found_count; i++)
    {
        Serial.printf("  0x%02X", found_devices[i]);
        if ((i + 1) % 8 == 0)
            Serial.println();
    }
    Serial.println("\n========== I2C Scan End ==========\n");
}

bool bsp_i2c_lock(int timeout_ms)
{
    // Convert timeout in milliseconds to FreeRTOS ticks
    // If `timeout_ms` is set to -1, the program will block until the condition is met
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(bsp_i2c_mux, timeout_ticks) == pdTRUE;
}

void bsp_i2c_unlock(void)
{
    xSemaphoreGiveRecursive(bsp_i2c_mux);
}

void bsp_i2c_init(void)
{
    bsp_i2c_mux = xSemaphoreCreateRecursiveMutex();
    Wire.begin(HAL_PIN_I2C_SDA, HAL_PIN_I2C_SCL);
    Wire.setClock(400000); // 400khz clock
}

bool HAL::bsp_i2c_reg8_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t len)
{
    uint8_t error;
    if (bsp_i2c_lock(-1))
    {
        Wire.beginTransmission(dev_addr);
        Wire.write(reg_addr);
        error = Wire.endTransmission(true);
        if (error)
        {
            Serial.printf("endTransmission: %u\n", error);
            bsp_i2c_unlock();
            return false;
        }
        Wire.requestFrom(dev_addr, len);
        for (int i = 0; i < len; i++)
        {
            *data++ = Wire.read();
        }
        bsp_i2c_unlock();
        return true;
    }
    return false;
}

bool HAL::bsp_i2c_reg8_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t len)
{
    uint8_t error;
    if (bsp_i2c_lock(-1))
    {
        Wire.beginTransmission(dev_addr);
        Wire.write(reg_addr);
        for (int i = 0; i < len; i++)
        {
            Wire.write(*data++);
        }
        error = Wire.endTransmission(true);
        bsp_i2c_unlock();
        if (error)
        {
            Serial.printf("endTransmission: %u\n", error);
            return false;
        }
        return true;
    }
    return false;
}
