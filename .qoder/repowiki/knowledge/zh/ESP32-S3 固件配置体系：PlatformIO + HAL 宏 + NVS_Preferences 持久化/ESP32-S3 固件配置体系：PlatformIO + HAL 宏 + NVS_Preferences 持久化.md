---
kind: configuration_system
name: ESP32-S3 固件配置体系：PlatformIO + HAL 宏 + NVS/Preferences 持久化
category: configuration_system
scope:
    - '**'
source_files:
    - platformio.ini
    - partitions/custom_16MB_littlefs.csv
    - src/hal/hal_config.h
    - include/lgfx.h
    - src/hal/hal.cpp
    - src/main.cpp
    - src/hal/hal_display.cpp
    - src/apps/game/Tetris.cpp
    - src/apps/game/WoodFish.cpp
---

## 1. 使用的系统与工具

本仓库是一个 PlatformIO 管理的 ESP32-S3 Arduino 工程，配置体系由三层组成：
- **构建期配置**：`platformio.ini` 定义目标板、框架、分区表、编译宏与资源嵌入。
- **硬件抽象层（HAL）宏配置**：`src/hal/hal_config.h` 集中声明所有引脚、屏幕尺寸、NVS key 等硬件相关常量。
- **运行时持久化配置**：通过 ESP-IDF `nvs_flash` 命名空间与 Arduino `Preferences` API 保存用户设置（WiFi SSID/密码、LCD 背光亮度与超时等），以及各 App 独立命名空间的分数/进度数据。

没有发现 `.env`、`.yaml`、`.toml`、`application.properties` 或统一的配置加载库；配置以 C/C++ 宏 + NVS/Preferences 键值对形式存在。

## 2. 关键文件

| 文件 | 作用 |
|---|---|
| `platformio.ini` | 构建环境、编译宏、LittleFS 分区、嵌入式静态资源清单 |
| `partitions/custom_16MB_littlefs.csv` | 16MB Flash 分区布局（双 OTA slot + LittleFS 7.875MB + NVS + coredump） |
| `src/hal/hal_config.h` | 全部 HAL 引脚、屏幕参数、NVS key 常量、日志宏 |
| `include/lgfx.h` | LovyanGFX 显示/触摸驱动初始化（硬编码引脚与面板参数） |
| `src/hal/hal.cpp` | `HAL::Init()` 统一初始化电源、LCD、I2C、输入、编码器、RTC、IMU、SD、磁力计、气压计 |
| `src/main.cpp` | 应用状态机入口，调用 `HAL::Init()` 后进入 `g_launcher.Main()` |
| `src/hal/hal_display.cpp` | 使用 `Preferences` 读写 LCD 背光相关 NVS 配置 |
| `src/apps/game/Tetris.cpp` / `WoodFish.cpp` | 各自用独立 NVS 命名空间持久化最高分/积分 |

## 3. 架构与约定

### 3.1 构建期配置（`platformio.ini`）
- 目标：`espressif32 @ ~6.5.0`，`board = esp32-s3-devkitc-1`，`framework = arduino`。
- PSRAM 启用：`BOARD_HAS_PSRAM`、`ESP32S3_PSRAM_ENABLED` 编译宏。
- USB CDC 启动：`ARDUINO_USB_MODE=1`、`ARDUINO_USB_CDC_ON_BOOT=1`。
- 调试级别：`CORE_DEBUG_LEVEL=3`。
- 文件系统：`board_build.filesystem = littlefs`，分区表指向 `partitions/custom_16MB_littlefs.csv`。
- 嵌入式静态资源：通过 `board_build.embed_files` 将 `compass_dial.png`、`woodfish.png` 直接嵌入固件二进制。

### 3.2 硬件配置（`hal_config.h`）
- 所有引脚以 `HAL_PIN_*` 前缀的 `#define` 集中声明（LCD、Encoder、SD、I2C、Touch、Battery、Buzzer、RGB、IR）。
- 屏幕分辨率与旋转通过 `LCD_H_RES`、`LCD_V_RES`、`LCD_ROTATION` 控制，并被 `SCREEN_WIDTH`/`SCREEN_HEIGHT` 别名暴露给上层。
- NVS 命名空间与 key 集中在同一文件：
  - 命名空间：`CONFIG_NAMESPACE = "config"`
  - Key：`INIT_KEY`、`FFAT_KEY`、`LCD_BK_BRIGHTNESS_KEY`、`LCD_BK_TIME_OUT_KEY`、`WIFI_SSID_KEY`、`WIFI_PASSWORD_KEY`
- 提供 `HAL_LOG*` 系列宏，输出到串口并支持在 LCD 上打印彩色日志。

### 3.3 显示/触摸驱动配置（`lgfx.h`）
- 通过 `#define LGFX_USE_V1`、`PORTRAIT` 切换 LovyanGFX 行为。
- 硬编码 SPI 引脚（MOSI/MISO/SCLK/DC/CS/RST/BL）、I2C 触摸引脚（SDA/SCL/RST/IRQ）与 CST816S 地址 `0x15`。
- `LGFX` 类在构造时依次配置 Bus、Panel、Light、Touch，并 `setPanel` 绑定。

### 3.4 运行时配置持久化（NVS / Preferences）
- 系统级配置（LCD 背光亮度、超时、WiFi SSID/密码）使用 `Preferences` 打开命名空间 `CONFIG_NAMESPACE`（即 `"config"`），key 名来自 `hal_config.h`。
- 游戏级配置（Tetris、WoodFish）各自使用独立的 NVS 命名空间（如 `TETRIS_NVS_NS`、`WOODFISH_NVS_NS`），遵循“每个 App 一个命名空间”的隔离约定。
- 写操作采用节流策略（例如 WoodFish 每 20 次写一次 NVS），避免频繁擦写 Flash。

### 3.5 分区布局约定（`custom_16MB_littlefs.csv`）
- 注释明确说明：双 OTA slot（各 4MB，firmware ~1MB，留 4x headroom），LittleFS 7.875MB 用于 Web 前端、字体、图标和声音。
- SubType 保留为 `spiffs` 以保持工具链兼容，但实际由 LittleFS 挂载。
- 预留 `coredump` 分区用于崩溃转储。

## 4. 约定与约束

- **硬件相关常量必须集中在 `hal_config.h`**：所有引脚、分辨率、NVS key 均以 `#define` 形式声明在该头文件中，HAL 实现与其他模块仅包含该头文件访问这些常量。
- **NVS 命名空间按职责划分**：系统级配置使用 `CONFIG_NAMESPACE`，每个 App 使用自己的命名空间（Tetris、WoodFish 已体现该模式）。
- **NVS key 名称集中管理**：所有 NVS key 字符串常量定义在 `hal_config.h`，避免散落的魔法字符串。
- **LittleFS 作为只读资源盘**：构建期通过 `board_build.filesystem = littlefs` 与 `board_build.embed_files` 将静态资源打包进固件镜像，运行时通过 LittleFS API 读取。
- **PSRAM 必须在构建宏中显式开启**：`BOARD_HAS_PSRAM` 与 `ESP32S3_PSRAM_ENABLED` 两个宏同时出现在 `platformio.ini` 的 `build_flags` 中，缺少任一可能影响内存分配行为。
- **USB CDC 启动宏不可省略**：`ARDUINO_USB_MODE=1` 与 `ARDUINO_USB_CDC_ON_BOOT=1` 共同启用 ESP32-S3 的 USB Serial/JTAG 调试通道。
- **分区表与 `platformio.ini` 必须保持一致**：`board_build.partitions` 指向 `custom_16MB_littlefs.csv`，且 `board_upload.flash_size = 16MB` 需与分区表总容量匹配。