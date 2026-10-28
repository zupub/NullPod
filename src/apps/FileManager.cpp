#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>
#include <FS.h>
#include <LittleFS.h>

#include "FileManager.h"
#include "common.h"

//==================== 全局实例 ====================
FileManager g_fileManager;

WebServer server(80);
String currentUploadFilename;
fs::File currentUploadFile;

//==================== 网页资源说明 ====================
// 网页资源已从固件内嵌迁移到 LittleFS 分区（工程 data/ 目录）：
//   data/index.html -> /index.html
//   data/style.css  -> /style.css
//   data/app.js     -> /app.js
// 分区名 "littlefs" 由 partitions/default_8MB_littlefs.csv 定义；
// 上传资源使用: pio run -t uploadfs

//==================== 静态资源服务 ====================

// 根据扩展名返回 Content-Type
String getContentType(const String &path)
{
    String p = path;
    p.toLowerCase();

    if (p.endsWith(".html") || p.endsWith(".htm"))
        return "text/html; charset=utf-8";
    if (p.endsWith(".css"))
        return "text/css; charset=utf-8";
    if (p.endsWith(".js"))
        return "application/javascript; charset=utf-8";
    if (p.endsWith(".json"))
        return "application/json";
    if (p.endsWith(".png"))
        return "image/png";
    if (p.endsWith(".jpg") || p.endsWith(".jpeg"))
        return "image/jpeg";
    if (p.endsWith(".gif"))
        return "image/gif";
    if (p.endsWith(".svg"))
        return "image/svg+xml";
    if (p.endsWith(".ico"))
        return "image/x-icon";
    if (p.endsWith(".woff"))
        return "font/woff";
    if (p.endsWith(".woff2"))
        return "font/woff2";
    if (p.endsWith(".ttf"))
        return "font/ttf";
    if (p.endsWith(".txt"))
        return "text/plain";
    return "application/octet-stream";
}

// 从 LittleFS 流式发送静态文件，成功返回 true
bool serveStaticFile(String path)
{
    // 拒绝路径穿越
    if (path.indexOf("..") >= 0)
        return false;

    if (!path.startsWith("/"))
        path = "/" + path;
    if (path.endsWith("/"))
        path += "index.html";

    if (!LittleFS.exists(path))
    {
        Serial.println("Static file not found: " + path);
        return false;
    }

    fs::File file = LittleFS.open(path, "r");
    if (!file || file.isDirectory())
    {
        if (file)
            file.close();
        Serial.println("Failed to open static file: " + path);
        return false;
    }

    size_t fileSize = file.size();
    server.sendHeader("Cache-Control", "max-age=86400");
    server.streamFile(file, getContentType(path));
    file.close();
    Serial.println("Served: " + path + " (" + String(fileSize) + " bytes)");
    return true;
}

// 注册静态资源 GET 路由
void registerStaticRoute(const char *path)
{
    server.on(path, HTTP_GET, [path]()
              {
        if (!serveStaticFile(String(path)))
            server.send(404, "text/plain", "Not found: " + String(path)); });
}

// 辅助函数：转义JSON字符串中的特殊字符
String escapeJsonString(const String &input)
{
    String output = input;
    output.replace("\\", "\\\\");
    output.replace("\"", "\\\"");
    output.replace("/", "\\/");
    output.replace("\b", "\\b");
    output.replace("\f", "\\f");
    output.replace("\n", "\\n");
    output.replace("\r", "\\r");
    output.replace("\t", "\\t");
    return output;
}

// 文件上传处理
void handleFileUpload()
{
    HTTPUpload &upload = server.upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        // 获取上传路径和文件名
        String filename = upload.filename;
        String path = server.hasArg("path") ? server.arg("path") : "/";

        // 确保路径格式正确
        if (!path.startsWith("/"))
            path = "/" + path;
        if (path.endsWith("/") && path != "/")
            path = path.substring(0, path.length() - 1);

        // 构建完整文件路径
        if (path == "/")
            currentUploadFilename = "/" + filename;
        else
            currentUploadFilename = path + "/" + filename;

        Serial.println("Upload Start: " + currentUploadFilename);

        // 如果文件已存在，先删除
        if (SD.exists(currentUploadFilename))
        {
            SD.remove(currentUploadFilename);
            Serial.println("Deleted existing file: " + currentUploadFilename);
        }

        // 创建文件 - 使用正确的模式
        currentUploadFile = SD.open(currentUploadFilename, FILE_WRITE);
        if (!currentUploadFile)
        {
            Serial.println("Failed to create file for writing: " + currentUploadFilename);
        }
        else
        {
            Serial.println("File opened for writing: " + currentUploadFilename);
        }
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        // 写入数据到文件
        if (currentUploadFile)
        {
            size_t bytesWritten = currentUploadFile.write(upload.buf, upload.currentSize);
            if (bytesWritten == upload.currentSize)
            {
                Serial.println("Written " + String(bytesWritten) + " bytes");
            }
            else
            {
                Serial.println("Write failed: written " + String(bytesWritten) + " of " + String(upload.currentSize));
            }
        }
        else
        {
            Serial.println("Cannot write, file not open");
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        // 关闭文件
        if (currentUploadFile)
        {
            currentUploadFile.close();
            Serial.println("Upload Complete: " + currentUploadFilename);
            Serial.println("Total size: " + String(upload.totalSize));

            // 验证文件是否真的写入
            if (SD.exists(currentUploadFilename))
            {
                fs::File testFile = SD.open(currentUploadFilename);
                if (testFile)
                {
                    Serial.println("File verification - Size: " + String(testFile.size()));
                    testFile.close();
                }
            }
        }
        else
        {
            Serial.println("Upload Complete but file was not open");
        }

        currentUploadFilename = "";
    }
    else if (upload.status == UPLOAD_FILE_ABORTED)
    {
        // 上传中止
        if (currentUploadFile)
        {
            currentUploadFile.close();
        }
        // 删除不完整的文件
        if (SD.exists(currentUploadFilename))
        {
            SD.remove(currentUploadFilename);
        }
        Serial.println("Upload Aborted: " + currentUploadFilename);
        currentUploadFilename = "";
    }
}

// 处理文件列表请求
void handleFileList()
{
    String path = server.hasArg("path") ? server.arg("path") : "/";
    if (!path.startsWith("/"))
        path = "/" + path;

    Serial.println("Listing directory: " + path);

    fs::File root = SD.open(path);
    if (!root)
    {
        server.send(404, "application/json", "{\"error\":\"Directory not found\"}");
        return;
    }

    if (!root.isDirectory())
    {
        server.send(400, "application/json", "{\"error\":\"Not a directory\"}");
        root.close();
        return;
    }

    String json = "{\"files\":[";
    bool first = true;

    fs::File file = root.openNextFile();
    while (file)
    {
        String name = String(file.name());

        // 处理文件名，移除完整路径，只保留相对路径部分
        if (name.startsWith(path))
            name = name.substring(path.length());
        if (name.startsWith("/"))
            name = name.substring(1);

        // 跳过空名称和系统文件（如索引文件）
        if (name.length() > 0 && !name.equals("SYSTEM~1") && !name.equals("System Volume Information"))
        {
            if (!first)
            {
                json += ",";
            }

            json += "{";
            json += "\"name\":\"" + escapeJsonString(name) + "\",";
            json += "\"path\":\"" + escapeJsonString(path + (path.endsWith("/") ? "" : "/") + name) + "\",";
            json += "\"isDirectory\":" + String(file.isDirectory() ? "true" : "false") + ",";
            json += "\"size\":" + String(file.size()) + ",";
            json += "\"modified\":" + String(file.getLastWrite());
            json += "}";

            first = false;
        }

        file = root.openNextFile();
    }

    json += "]}";
    server.send(200, "application/json", json);
    root.close();
}

// 处理文件下载
void handleFileDownload()
{
    if (!server.hasArg("path"))
    {
        server.send(400, "text/plain", "Path parameter required");
        return;
    }

    String path = server.arg("path");
    if (!path.startsWith("/"))
        path = "/" + path;

    Serial.println("Download request: " + path);

    if (!SD.exists(path))
    {
        server.send(404, "text/plain", "File not found");
        return;
    }

    fs::File file = SD.open(path);
    if (!file || file.isDirectory())
    {
        server.send(400, "text/plain", "Cannot download directory or file open failed");
        if (file)
            file.close();
        return;
    }

    // 获取文件名用于下载
    String filename = path;
    int lastSlash = filename.lastIndexOf('/');
    if (lastSlash != -1)
    {
        filename = filename.substring(lastSlash + 1);
    }

    server.sendHeader("Content-Type", "application/octet-stream");
    server.sendHeader("Content-Disposition", "attachment; filename=\"" + filename + "\"");
    server.sendHeader("Connection", "close");

    // 流式发送文件内容
    server.setContentLength(file.size());
    server.send(200, "application/octet-stream", "");

    uint8_t buf[1024];
    size_t len = 0;
    while ((len = file.read(buf, sizeof(buf))) > 0)
    {
        server.client().write(buf, len);
    }

    file.close();
}

// 处理文件删除
void handleFileDelete()
{
    if (!server.hasArg("path"))
    {
        server.send(400, "application/json", "{\"error\":\"Path parameter required\"}");
        return;
    }

    String path = server.arg("path");
    bool isDirectory = server.hasArg("isDirectory") && server.arg("isDirectory") == "true";

    if (!path.startsWith("/"))
        path = "/" + path;

    Serial.println("Delete request: " + path + (isDirectory ? " (directory)" : " (file)"));

    if (!SD.exists(path))
    {
        server.send(404, "application/json", "{\"error\":\"File or directory not found\"}");
        return;
    }

    bool success = isDirectory ? SD.rmdir(path) : SD.remove(path);

    if (success)
    {
        server.send(200, "application/json", "{\"message\":\"Delete successful\"}");
    }
    else
    {
        server.send(500, "application/json", "{\"error\":\"Delete failed\"}");
    }
}

// 处理创建目录
void handleMakeDir()
{
    if (!server.hasArg("path"))
    {
        server.send(400, "application/json", "{\"error\":\"Path parameter required\"}");
        return;
    }

    String path = server.arg("path");
    if (!path.startsWith("/"))
        path = "/" + path;

    Serial.println("Create directory: " + path);

    if (SD.exists(path))
    {
        server.send(400, "application/json", "{\"error\":\"File or directory already exists\"}");
        return;
    }

    if (SD.mkdir(path))
        server.send(200, "application/json", "{\"message\":\"Created\"}");
    else
        server.send(500, "application/json", "{\"error\":\"Failed\"}");
}

// 处理存储信息请求
void handleStorageInfo()
{
    uint64_t totalBytes = SD.totalBytes();
    uint64_t usedBytes = SD.usedBytes();

    String json = "{";
    json += "\"totalBytes\":" + String(totalBytes) + ",";
    json += "\"usedBytes\":" + String(usedBytes);
    json += "}";

    server.send(200, "application/json", json);
}

// 路由注册（全程序只执行一次）
// server 为全局对象，且 WebServer::on() 分配的 handler 仅在 WebServer 析构时释放；
// 若每次进入 App 都注册一遍，handler 链表会持续增长、堆内存只增不减，故加守卫。
static bool routesRegistered = false;

void setupRoutes()
{
    if (routesRegistered)
        return;
    routesRegistered = true;

    // 网页静态资源路由（从 LittleFS 流式发送）
    registerStaticRoute("/");
    registerStaticRoute("/index.html");
    registerStaticRoute("/style.css");
    registerStaticRoute("/app.js");

    // 文件管理 API 路由（SD 卡）
    server.on("/list", HTTP_GET, handleFileList);
    server.on("/upload", HTTP_POST, []()
              { server.send(200, "application/json", "{\"status\":\"success\"}"); }, handleFileUpload);
    server.on("/download", HTTP_GET, handleFileDownload);
    server.on("/delete", HTTP_DELETE, handleFileDelete);
    server.on("/storage", HTTP_GET, handleStorageInfo);
    server.on("/mkdir", HTTP_POST, handleMakeDir);

    // 未命中路由的 GET 请求尝试作为 LittleFS 静态资源返回（如 favicon.ico、图片等）
    server.onNotFound([]()
                      {
        if (server.method() == HTTP_GET && serveStaticFile(server.uri()))
            return;
        server.send(404, "text/plain", "Not found"); });
}

void FileManager::Init()
{
    // 挂载 LittleFS（网页资源分区，分区表见 platformio.ini 的 board_build.partitions）
    if (!LittleFS.begin(false, "/littlefs", 10, "littlefs"))
    {
        Serial.println("LittleFS mount failed, retry with format...");
        if (!LittleFS.begin(true, "/littlefs", 10, "littlefs"))
        {
            Serial.println("LittleFS mount/format failed! Web UI unavailable.");
            Serial.println("Check board_build.partitions / board_build.filesystem, then run: pio run -t uploadfs");
        }
    }
    else
    {
        Serial.printf("LittleFS mounted. Total: %u KB, Used: %u KB\r\n",
                      (unsigned)(LittleFS.totalBytes() / 1024),
                      (unsigned)(LittleFS.usedBytes() / 1024));
    }

    const char *ssid = "ESP32-S3-SD-Manager";
    const char *password = "12345678";

    // 设置为热点模式
    WiFi.mode(WIFI_AP);
    bool result = WiFi.softAP(ssid, password);

    if (result)
    {
        Serial.println("Hotspot started successfully!");
        Serial.print("Hotspot SSID: ");
        Serial.println(ssid);
        Serial.print("Hotspot Password: ");
        Serial.println(password);
        Serial.print("IP address: ");
        Serial.println(WiFi.softAPIP());
    }
    else
    {
        Serial.println("Failed to start hotspot!");
        return;
    }

    // 路由注册（守卫保证全程序只执行一次，避免 handler 链表重复累积）
    setupRoutes();

    server.begin();
    Serial.println("HTTP server started");
    Serial.println("Connect to hotspot and visit: http://" + WiFi.softAPIP().toString());

    HAL::GetCanvas()->setFont(&fonts::Font0);
    HAL_LOG_INFO("HTTP server started");
    HAL_LOG_INFO("Hotspot: %s", ssid);
    HAL_LOG_INFO("Password: %s", password);
    HAL_LOG_INFO("Visit: http://%s", WiFi.softAPIP().toString());
}

// 退出 App 时释放资源：停止 HTTP 服务、关闭 AP 热点、卸载 LittleFS
void FileManager::Deinit()
{
    // 停止 HTTP 服务器（关闭监听 socket 与活动连接）
    server.stop();

    // 防御：关闭可能残留的上传文件
    if (currentUploadFile)
        currentUploadFile.close();
    currentUploadFilename = "";

    // 关闭 AP 热点，避免退出后射频常开
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);

    // 卸载 LittleFS（若后续 App 仍需访问文件系统，可去掉此调用）
    LittleFS.end();

    HAL_LOG_INFO("HTTP server stopped");
}

void FileManager::Main()
{
    HAL::GetDisplay()->fillScreen(TFT_BLACK);
    HAL::GetCanvas()->createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
    HAL::GetCanvas()->setTextDatum(TC_DATUM);
    HAL::GetCanvas()->setTextColor(TFT_WHITE);
    HAL::GetCanvas()->setTextSize(1);
    HAL::GetCanvas()->setFont(&FreeSansBold9pt7b);
    HAL::GetCanvas()->drawString("HTTP File Manager", SCREEN_WIDTH / 2, 15);
    HAL::GetCanvas()->setTextDatum(0);
    HAL::GetCanvas()->pushSprite(0, 0);
    HAL::GetCanvas()->setCursor(0, 64);

    Init();

    while (1)
    {
        server.handleClient();
        if (HAL::isReturn())
            break;
    }

    Deinit();
}
