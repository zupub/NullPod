---
kind: logging_system
name: 基于 Serial.print 的裸串口调试输出（无结构化日志框架）
category: logging_system
scope:
    - '**'
source_files:
    - platformio.ini
    - src/main.cpp
---

## 1. 使用的系统/方法

本项目**没有引入任何第三方日志框架或结构化日志库**。固件中的“日志”全部通过 Arduino `Serial` 接口的 `print` / `println` 直接输出到 USB CDC 串口，波特率由 PlatformIO 配置为 `monitor_speed = 115200`。ESP-IDF 的 `ESP_LOG*` 宏在本工程源码中未被使用；唯一的 ESP-IDF 相关构建标志是 `-D CORE_DEBUG_LEVEL=3`，但项目 framework 为 `arduino`，且源码未包含 `esp_log.h`，因此该宏仅作为通用编译定义存在，并未被本项目的代码消费。

## 2. 关键文件

- `platformio.ini`：定义 `monitor_speed = 115200`、`framework = arduino`、`board_build.filesystem = littlefs`，以及 `-D CORE_DEBUG_LEVEL=3` 等构建选项。
- `src/main.cpp`：唯一集中出现 `Serial.println(...)` 的位置，用于在应用状态切换时打印形如 `>>> Matrix App`、`>>> Dashboard App`、`>>> WiFi App` 等的状态行。
- `lib/IRremote/src/LocalDebugLevelStart.h`：第三方库内部用 `TRACE_PRINT` / `DEBUG_PRINT` / `INFO_PRINT` 等宏包装 `Serial.print`，属于 vendored 库自身实现，非本项目自定义。

## 3. 架构与约定

- **输出目标单一**：所有调试信息经 USB CDC 串口输出，无文件日志、无网络日志、无多 sink 路由。
- **无日志级别体系**：项目未定义自己的 LOG_DEBUG/INFO/WARN/ERROR 等宏，也没有按严重性分级；所有 `Serial.println` 都是同等级别的即时文本输出。
- **格式约定**：应用状态跳转处的日志统一采用前缀 `>>> <App Name> App` 的字符串常量，便于在串口监视器中快速识别当前激活的应用模块。
- **初始化位置**：仅在 `setup()` 中调用一次 `Serial.println("System ready")`，之后不再重复初始化 Serial。
- **无结构化字段**：日志不包含 JSON、KV 对、时间戳、任务 ID、内存占用等结构化字段；纯人类可读字符串。

## 4. 约定与约束

- **观察到的约定**：
  - 应用状态机入口处使用 `Serial.println(">>> <Name> App")` 标记模块切换（见 `src/main.cpp` 各 case 分支）。
  - 启动阶段使用 `Serial.println("System ready")` 表示 HAL 初始化完成（`src/main.cpp:22`）。
  - 所有日志均为硬编码 C 字符串字面量，未见动态拼接或格式化宏。
- **构建期约束**：`platformio.ini` 固定 `monitor_speed = 115200`，若修改串口速率需同步调整此值。
- **无强制规则**：仓库中没有 lint 规则、文档或头文件强制要求使用某种日志 API；现有模式仅为历史习惯形成的约定，新增代码可自由选择是否沿用 `Serial.println`。

综上，该项目不存在独立的 logging_system 抽象层，日志子系统即“Arduino Serial + 少量约定式字符串”。