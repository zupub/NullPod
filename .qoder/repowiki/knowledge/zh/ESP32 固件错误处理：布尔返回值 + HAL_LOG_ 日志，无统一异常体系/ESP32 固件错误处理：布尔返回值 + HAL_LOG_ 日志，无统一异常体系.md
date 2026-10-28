---
kind: error_handling
name: ESP32 固件错误处理：布尔返回值 + HAL_LOG_* 日志，无统一异常体系
category: error_handling
scope:
    - '**'
source_files:
    - src/hal/hal.h
    - src/hal/hal_i2c.cpp
    - src/hal/hal_sdcard.cpp
    - src/hal/hal_config.h
    - src/main.cpp
    - lib/SensorLib/src/SensorBHI260AP.cpp
---

## 1. 采用的方式

该仓库是 PlatformIO / Arduino 风格的 ESP32-S3 固件工程，**没有定义统一的错误类型、错误码枚举或 try/catch/throw 机制**。错误传播主要依赖以下三种手段：

- **布尔返回值**（`bool`）：HAL 层几乎所有可能失败的操作都返回 `bool`，调用方通过 `if (!HAL::xxx())` 判断。
- **C 风格整型返回值**：部分底层库（如 LovyanGFX、Adafruit BusIO）返回 `-1` 表示失败，`nullptr` 表示分配失败。
- **日志输出**：使用项目自定义的 `HAL_LOG_INFO / HAL_LOG_WARN / HAL_LOG_ERROR` 宏（基于 `Serial.printf`），以及第三方库中的 `log_e/log_d`、`ESP_LOGE/ESP_LOGW`。

项目中未发现任何 `try { ... } catch (...)`、`throw`、`Exception`、`ESP_ERROR_CHECK` 的使用，也没有自定义的错误类或错误码 enum。因此，本仓库对 error_handling 的处理属于**“约定式布尔返回 + 串口日志”**模式。

## 2. 关键文件与位置

| 文件 | 作用 |
|---|---|
| `src/hal/hal.h` | HAL 接口声明，所有 I2C/SD/IMU/MAG/DPS/RTC/Power/Audio/BT 等 API 均以 `bool` 或 `void` 暴露；无错误类型定义 |
| `src/hal/hal_i2c.cpp` | I2C 读写封装，`bsp_i2c_reg8_read/write` 在 `Wire.endTransmission()` 出错时打印 `endTransmission: %u` 并返回 `false` |
| `src/hal/hal_sdcard.cpp` | SD 卡初始化，`SD.begin()` 失败时设置 `_is_sd_card_ready = false` 并通过 `HAL_LOG_ERROR("sd card init failed!")` 记录 |
| `src/hal/hal_config.h` | 定义 `HAL_LOG_TAG_START/END/INFO/WARN/ERROR` 宏，集中管理日志前缀与级别 |
| `src/main.cpp` | 应用状态机入口，仅用 `Serial.println` 打印进入各 App 的消息，不捕获/处理子模块错误 |
| `lib/SensorLib/src/SensorBHI260AP.cpp` | 第三方传感器库中使用 `log_e(...)` 报告设备缺失、Flash 未检测到、驱动加载失败等错误 |
| `lib/LovyanGFX/...` | 第三方显示库中广泛使用 `has_error()` / `cpp::fail()` 及 `ESP_LOGE` 进行内部错误处理（不属于本项目代码） |

## 3. 架构与约定

### 3.1 HAL 层的布尔约定

`src/hal/hal.h` 中 HAL 命名空间下的函数签名体现了统一的错误约定：

- 可失败操作返回 `bool`：`bsp_i2c_reg8_read/write`、`SD_isReady`、`tryInitSD`、`BT_Init`、`Audio_PlayMusic`、`GetButton`、`getTouch`、`getGesture` 等。
- 初始化函数返回 `void`，但内部通过 `HAL_LOG_ERROR` 和全局状态变量（如 `_is_sd_card_ready`）表达失败。
- 数据读取函数返回原始数据类型（`float`、`ImuData_t`、`MAG_Info_t`），错误通过调用方检查前置条件（如 `SD_isReady()`）来规避。

示例（`hal_i2c.cpp`）：
```cpp
bool HAL::bsp_i2c_reg8_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint8_t len)
{
    if (bsp_i2c_lock(-1)) {
        Wire.beginTransmission(dev_addr);
        Wire.write(reg_addr);
        error = Wire.endTransmission(true);
        if (error) {
            Serial.printf("endTransmission: %u\n", error);
            bsp_i2c_unlock();
            return false;   // ← 错误以 bool 返回
        }
        ...
        return true;
    }
    return false;
}
```

### 3.2 日志系统

`src/hal/hal_config.h` 定义了日志宏：
- `HAL_LOG_TAG_START()` / `HAL_LOG_TAG_END()`：包裹日志标签前后缀。
- `HAL_LOG_INFO(fmt, args...)` / `HAL_LOG_WARN(fmt, args...)` / `HAL_LOG_ERROR(fmt, args...)`：对应 info/warn/error 级别。

这些宏最终通过 `Serial.printf` 输出，没有结构化日志框架、没有日志级别开关、没有异步队列。

### 3.3 应用层不处理错误

`src/main.cpp` 的 `setup()` 直接调用 `HAL::Init()` 后进入 `g_launcher.Main()`，没有检查返回值；`loop()` 中每个 case 分支也只是打印 `Serial.println(">>> Xxx App")` 然后调用 App 的 `Main()`，App 返回后无条件切回 `STATE_MENU`。错误不会冒泡到主循环，也不会触发恢复逻辑。

### 3.4 第三方库的错误模式

- LovyanGFX 在其平台适配层（`lib/LovyanGFX/src/lgfx/v1/platforms/esp32/common.cpp`）使用 `has_error()` / `cpp::fail()` 传递错误，这是库自身的实现，不是本项目定义的。
- SensorLib 使用 ESP-IDF 风格的 `log_e("...")` 输出错误信息。
- Adafruit BusIO 等库返回 `-1` 作为失败标志。

## 4. 观察到的约定与约束

- **约定（描述性）**：HAL 层所有可能失败的对外 API 均返回 `bool`；初始化失败通过全局状态变量（如 `_is_sd_card_ready`）+ `HAL_LOG_ERROR` 表达；上层调用方通过 `if (!HAL::xxx())` 判断失败。
- **约定（描述性）**：所有日志输出统一通过 `HAL_LOG_INFO/WARN/ERROR` 宏，而非裸 `Serial.printf`（HAL 层内）。
- **约定（描述性）**：I2C 访问通过递归互斥量 `xSemaphoreCreateRecursiveMutex()` 保护，并发冲突以锁获取失败返回 `false` 表达。
- **约束（由实现体现）**：项目中不存在 `throw`、`catch`、`try`、自定义 Exception 类或统一的 Error 类型——所有错误必须通过返回值或日志传播。
- **约束（由实现体现）**：`main.cpp` 不检查 `HAL::Init()` 的返回值（该函数本身为 `void`），也不捕获任何异常路径，因此硬件初始化失败后程序仍会进入 launcher。

## 5. 总结

该仓库没有建立专门的 error_handling 子系统。错误处理是**分散的、按文件约定的**：HAL 层用 `bool` 返回值 + `HAL_LOG_*` 日志，应用层基本不做错误检查，第三方库各自采用其自身模式（`has_error()`、`log_e`、`ESP_LOGE`、`-1`）。这是一种典型的嵌入式 Arduino 风格实践，牺牲了错误信息的结构化与可组合性，换取极简的实现。