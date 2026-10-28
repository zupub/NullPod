---
kind: build_system
name: PlatformIO + ESP32-S3 固件构建与分区管理
category: build_system
scope:
    - '**'
source_files:
    - platformio.ini
    - partitions/custom_16MB_littlefs.csv
    - partitions/custom_8MB_littlefs.csv
    - lib/README
---

## 1. 构建系统与工具链

本项目使用 **PlatformIO**（`platformio.ini`）作为唯一的构建入口，目标平台为 `espressif32 @ ~6.5.0`，框架为 Arduino，板型固定为 `esp32-s3-devkitc-1`。没有发现 Makefile、CMakeLists.txt、Dockerfile 或 CI 配置文件；所有编译、上传、监视行为均由 PlatformIO CLI 驱动。

## 2. 关键文件

- `platformio.ini`：工程唯一构建配置，定义 platform/board/framework、build_flags、文件系统与嵌入资源。
- `partitions/custom_16MB_littlefs.csv`：16MB Flash 的分区表（默认被引用）。
- `partitions/custom_8MB_littlefs.csv`：8MB Flash 的分区表（可切换）。
- `src/assets/image/compass_dial.png`、`src/assets/image/woodfish.png`：通过 `board_build.embed_files` 直接嵌入固件二进制。
- `data/` 目录（app.js、index.html、style.css）：由 PlatformIO 的 LittleFS 插件打包成镜像，挂载到 `/littlefs`。

## 3. 构建架构与约定

### 3.1 编译选项
`build_flags` 中启用了以下宏：
- `ARDUINO_USB_MODE=1` / `ARDUINO_USB_CDC_ON_BOOT=1`：启用 USB CDC 串口。
- `CORE_DEBUG_LEVEL=3`：ESP-IDF 调试级别设为中等。
- `BOARD_HAS_PSRAM` / `ESP32S3_PSRAM_ENABLED`：开启 PSRAM 支持。
- `board_build.arduino.memory_type = qio_opi`：Flash 模式 QIO + OPI PSRAM。

### 3.2 分区布局
两个分区表遵循相同结构：`nvs` → `otadata` → `app0(ota_0)` → `app1(ota_1)` → `littlefs(spiffs)` → `coredump`。区别在于容量分配：
- 16MB 版：双 OTA 各 4MB，LittleFS 7.875MB。
- 8MB 版：双 OTA 各 2.75MB，LittleFS 2.375MB。
注释明确说明 SubType 保留为 `spiffs` 仅为 toolchain 兼容，实际运行时以 LittleFS 挂载。

### 3.3 文件系统与资源嵌入
- `board_build.filesystem = littlefs` 配合 `board_build.partitions` 将 `data/` 目录打包为 LittleFS 镜像并烧录到对应分区。
- 大图片资源（`compass_dial.png`、`woodfish.png`）通过 `board_build.embed_files` 直接嵌入固件段，不走 LittleFS，避免占用文件系统空间。

### 3.4 第三方库组织
`lib/` 目录下存放已解包的 Arduino/ESP32 第三方库源码（Adafruit BMP280、LovyanGFX、NimBLE-Arduino、WiFiManager 等），由 PlatformIO 在构建时自动编译链接。`lib/README` 用于记录库来源。

## 4. 约定与约束

- **单一构建入口**：仓库根仅存在 `platformio.ini`，无其他构建脚本；所有编译/上传/监视命令应通过 `pio run`、`pio device monitor` 等 PlatformIO CLI 执行。
- **板型锁定**：`board = esp32-s3-devkitc-1`，不暴露多环境（env）别名，构建产物始终面向该开发板。
- **分区表选择**：通过修改 `board_build.partitions` 指向 `custom_16MB_littlefs.csv` 或 `custom_8MB_littlefs.csv` 来适配不同 Flash 容量的硬件。
- **LittleFS 分区 SubType 保持 `spiffs`**：分区表中 `littlefs` 分区的 SubType 写为 `spiffs`，这是为了兼容 ESP-IDF/Partition Tool 的解析逻辑，实际由应用层以 LittleFS 方式挂载——此约定在两版分区表中一致。
- **嵌入式资源分类**：小体积头文件类资源走 Arduino 资源机制，大图片走 `embed_files` 直嵌，其余静态 Web 前端资源走 LittleFS 分区，三者路径在 `platformio.ini` 中显式声明。
- **未发现自动化发布/CI**：仓库中没有 `.github/workflows`、Jenkinsfile、Makefile 或 shell 构建脚本，版本与发布流程未在代码中体现。