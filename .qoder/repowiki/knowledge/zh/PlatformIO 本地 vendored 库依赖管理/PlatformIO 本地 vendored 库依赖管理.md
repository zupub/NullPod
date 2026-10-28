---
kind: dependency_management
name: PlatformIO 本地 vendored 库依赖管理
category: dependency_management
scope:
    - '**'
source_files:
    - platformio.ini
    - lib/README
---

## 1. 使用的系统/方法

本项目使用 **PlatformIO** 作为构建与依赖管理系统，目标平台为 `espressif32` (ESP32-S3)，框架为 Arduino。依赖管理采用 **本地 vendored（源码内联）** 策略：所有第三方 Arduino/ESP32 库的完整源码直接拷贝到仓库根目录的 `lib/` 下，由 PlatformIO 在构建时编译并链接进固件。

- 平台版本通过 `platformio.ini` 中的 `platform = espressif32 @ ~6.5.0` 声明，使用语义化版本范围约束。
- 项目没有使用 PlatformIO Library Manager (`lib_deps`)、也没有 `platformio.lock` 锁文件；依赖锁定完全依赖 git 跟踪 `lib/` 下的源码快照。

## 2. 关键文件

- `platformio.ini` — 唯一的项目级依赖声明入口，定义 platform/board/framework/build_flags/filesystem/partitions。
- `lib/README` — PlatformIO 默认生成的说明，描述 `lib/` 目录结构与 Library Dependency Finder 行为。
- `lib/<LibraryName>/` — 每个第三方库一个子目录，存放其原始源码（Adafruit BMP280、BusIO、NeoPixel、Unified Sensor、ESP32-NimBLE-Mouse、IRremote、LovyanGFX、NimBLE-Arduino、QMC5883PCompass、SensorLib、SparkFun BMI270、WiFiManager）。
- `partitions/custom_16MB_littlefs.csv` / `custom_8MB_littlefs.csv` — 分区表，决定 LittleFS 文件系统大小，间接影响可嵌入资源的大小上限。

## 3. 架构与约定

- **源码 vendoring**：`lib/` 下每个目录即一个独立库的完整源码树，PlatformIO 会将其编译为静态库并链接。`lib/README` 明确说明 “The source code of each library should be placed in an own separate directory”。
- **自动依赖发现**：PlatformIO Library Dependency Finder 会扫描项目源文件中的 `#include <...>` 来解析库间依赖，无需显式声明。
- **嵌入式资源嵌入**：`board_build.embed_files` 将图片资源直接嵌入固件镜像，而非走 LittleFS。
- **文件系统**：`board_build.filesystem = littlefs` + 指定 partition CSV，用于 SD 卡 Web 管理器等运行时数据。

## 4. 约定与约束

- **依赖来源**：所有第三方库以源码形式 vendored 在 `lib/` 下，未使用 PlatformIO Registry 或私有 registry 拉取。
- **版本锁定方式**：无 `platformio.lock`，依赖版本由 git 提交历史锁定（commit hash），升级需手动替换 `lib/` 下对应目录源码。
- **PlatformIO 平台版本约束**：`platform = espressif32 @ ~6.5.0`，允许 6.5.x 补丁更新，但不接受 6.6+ 主版本变更。
- **库目录结构约定**：遵循 PlatformIO 默认规范——每个库独占 `lib/<name>/` 子目录，可选包含 `library.json` 自定义构建选项（见 `lib/README` 中对 `Bar/library.json` 的示例）。
- **无全局包管理器配置**：未发现 `.gitmodules`、`vendor/`、`package.json`、`go.mod` 等其它语言生态的依赖清单；依赖管理完全集中在 `platformio.ini` + `lib/` 源码快照。