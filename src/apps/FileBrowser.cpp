#include "FileBrowser.h"
#include "common.h"
#include "SD.h"
#include "FS.h"

//==================== 全局实例 ====================
FileBrowser g_fileBrowser;

//==================== 字体与颜色 ====================
static const lgfx::IFont *FB_FONT = &fonts::efontCN_16;
static const int FB_NAME_MAX = 220;

//==================== 初始化 / 主循环 / 退出 ====================

void FileBrowser::Init()
{
    disp = HAL::GetDisplay();
    canvas = HAL::GetCanvas();

    selectedIndex = 0;
    scrollOffset = 0;
    confirmed = false;
    confirmedPath = "";

    pathStack.clear();
    pathStack.push_back("/");

    refreshRoot();
    drawMain();
}

void FileBrowser::Update()
{
    handleInput();

    if (confirmed)
    {
        confirmed = false;
        if (confirmedPath.length() > 0)
        {
            Serial.printf(">>> Selected file: %s\n", confirmedPath.c_str());
        }
    }
}

void FileBrowser::Exit()
{
    flatList.clear();
    pathStack.clear();
}

void FileBrowser::Main()
{
    Init();
    while (1)
    {
        Update();
        if (HAL::isReturn())
            break;
        delay(10);
    }
    Exit();
}

//==================== 目录扫描 ====================

/*
 * scanDirectory
 *  递归扫描指定目录，把结果追加到 flatList
 *  depth=0 表示根目录
 *
 *  文件夹条目后面紧跟它的子项（如果已展开），
 *  形成一棵"展平树"，方便用 selectedIndex 直接定位。
 */
void FileBrowser::scanDirectory(const String &path, int depth)
{
    File dir = SD.open(path);
    if (!dir || !dir.isDirectory())
    {
        if (dir)
            dir.close();
        return;
    }

    // 收集子项
    std::vector<String> names;
    std::vector<bool> isDirs;

    File entry = dir.openNextFile();
    while (entry)
    {
        String name = entry.name();

        // 跳过 macOS 元数据
        String base = name;
        int ls = name.lastIndexOf('/');
        if (ls >= 0)
            base = name.substring(ls + 1);
        if (base.startsWith("._"))
        {
            entry.close();
            entry = dir.openNextFile();
            continue;
        }

        names.push_back(name);
        isDirs.push_back(entry.isDirectory());
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();

    // 排序：文件夹在前，然后按名称
    // 简单做法：分别收集再合并
    std::vector<std::pair<String, bool>> all;
    for (size_t i = 0; i < names.size(); i++)
        all.push_back({names[i], isDirs[i]});
    std::sort(all.begin(), all.end(),
              [](const std::pair<String, bool> &a, const std::pair<String, bool> &b)
              {
                  if (a.second != b.second)
                      return a.second > b.second; // 文件夹优先
                  return a.first < b.first;
              });

    // 写入 flatList
    for (auto &p : all)
    {
        String fullPath = p.first;
        if (!fullPath.startsWith("/"))
            fullPath = "/" + fullPath;

        String displayName = fullPath;
        int ls = displayName.lastIndexOf('/');
        if (ls >= 0)
            displayName = displayName.substring(ls + 1);

        bool isNes = (!p.second) &&
                     (displayName.endsWith(".nes") ||
                      displayName.endsWith(".NES") ||
                      displayName.endsWith(".Nes"));

        FbItem item;
        item.path = fullPath;
        item.name = displayName;
        item.type = p.second ? FB_TYPE_FOLDER : FB_TYPE_FILE;
        item.isNes = isNes;
        item.expanded = false;
        item.depth = depth;
        item.childCount = 0;
        item.childStart = 0;

        flatList.push_back(item);
    }
}

/*
 * refreshRoot
 *  重新扫描根目录，构建初始 flatList
 */
void FileBrowser::refreshRoot()
{
    flatList.clear();
    scanDirectory("/", 0);

    // 顺便打印调试
    Serial.printf("[FB] Total items: %d\n", (int)flatList.size());
    for (int i = 0; i < (int)flatList.size(); i++)
    {
        const char *t = flatList[i].type == FB_TYPE_FOLDER ? "DIR " : "FILE";
        Serial.printf("  [%d] %s %s\n", i, t, flatList[i].path.c_str());
    }

    if (flatList.empty())
    {
        selectedIndex = -1;
    }
    else
    {
        selectedIndex = 0;
        scrollOffset = 0;
    }
}

//==================== 文件夹展开 / 折叠 ====================

/*
 * toggleFolder
 *  在 flatList[index] 处展开或折叠文件夹
 *
 *  展开：扫描该文件夹，把子项插入到 index+1 的位置
 *  折叠：从 flatList 中移除该文件夹下面的所有子项
 */
void FileBrowser::toggleFolder(int index)
{
    if (index < 0 || index >= (int)flatList.size())
        return;
    if (flatList[index].type != FB_TYPE_FOLDER)
        return;

    if (flatList[index].expanded)
    {
        //---- 折叠：移除子项 ----
        int removeCount = flatList[index].childCount;
        flatList.erase(flatList.begin() + index + 1,
                       flatList.begin() + index + 1 + removeCount);
        flatList[index].expanded = false;
        flatList[index].childCount = 0;
        flatList[index].childStart = 0;

        Serial.printf("[FB] Collapsed: %s (removed %d items)\n",
                      flatList[index].path.c_str(), removeCount);
    }
    else
    {
        //---- 展开：扫描并插入子项 ----
        int depth = flatList[index].depth + 1;
        std::vector<FbItem> children;

        File dir = SD.open(flatList[index].path);
        if (!dir || !dir.isDirectory())
        {
            if (dir)
                dir.close();
            Serial.printf("[FB] Cannot open folder: %s\n",
                          flatList[index].path.c_str());
            return;
        }

        File entry = dir.openNextFile();
        while (entry)
        {
            String name = entry.name();
            String base = name;
            int ls = name.lastIndexOf('/');
            if (ls >= 0)
                base = name.substring(ls + 1);
            if (base.startsWith("._"))
            {
                entry.close();
                entry = dir.openNextFile();
                continue;
            }

            String fullPath = name;
            if (!fullPath.startsWith("/"))
                fullPath = "/" + fullPath;

            String displayName = base;
            bool isNes = (!entry.isDirectory()) &&
                         (displayName.endsWith(".nes") ||
                          displayName.endsWith(".NES") ||
                          displayName.endsWith(".Nes"));

            FbItem child;
            child.path = fullPath;
            child.name = displayName;
            child.type = entry.isDirectory() ? FB_TYPE_FOLDER : FB_TYPE_FILE;
            child.isNes = isNes;
            child.expanded = false;
            child.depth = depth;
            child.childCount = 0;
            child.childStart = 0;

            children.push_back(child);
            entry.close();
            entry = dir.openNextFile();
        }
        dir.close();

        // 排序：文件夹优先
        std::sort(children.begin(), children.end(),
                  [](const FbItem &a, const FbItem &b)
                  {
                      if (a.type != b.type)
                          return a.type == FB_TYPE_FOLDER;
                      return a.path < b.path;
                  });

        // 插入到 index+1
        flatList[index].childStart = index + 1;
        flatList[index].childCount = (int)children.size();
        flatList[index].expanded = true;

        flatList.insert(flatList.begin() + index + 1,
                        children.begin(), children.end());

        Serial.printf("[FB] Expanded: %s (+%d items)\n",
                      flatList[index].path.c_str(), (int)children.size());
    }

    // 修正选中索引不越界
    if (selectedIndex >= (int)flatList.size())
        selectedIndex = (int)flatList.size() - 1;
}

//==================== 输入处理 ====================

void FileBrowser::handleInput()
{
    uint8_t key = 0;
    bool event = HAL::getEncoder(&key);
    if (!event)
        return;

    if (flatList.empty())
    {
        // 无内容时，OK 键重试
        if (key == BTN_OK_SP)
        {
            Serial.println("[FB] Retry scan...");
            SD.end();
            delay(100);
            if (HAL::tryInitSD())
            {
                refreshRoot();
                drawMain();
            }
        }
        return;
    }

    switch (key)
    {
    case BTN_ENC_CW: // 逆时针 → 上移
        HAL::beep(500, 15);
        if (selectedIndex > 0)
        {
            selectedIndex--;
            ensureVisible();
            drawList();
        }
        break;

    case BTN_ENC_CC: // 顺时针 → 下移
        HAL::beep(700, 15);
        if (selectedIndex < (int)flatList.size() - 1)
        {
            selectedIndex++;
            ensureVisible();
            drawList();
        }
        break;

    case BTN_OK_SP: // 短按确认
        HAL::beep(1200, 30);
        {
            const FbItem &item = flatList[selectedIndex];
            if (item.type == FB_TYPE_FOLDER)
            {
                // 文件夹：切换展开/折叠
                toggleFolder(selectedIndex);
                drawList();
            }
            else
            {
                // 文件：确认选中
                confirmed = true;
                confirmedPath = item.path;
                Serial.printf(">>> File confirmed: %s\n",
                              confirmedPath.c_str());

                // 视觉反馈：反色闪烁
                int row = selectedIndex - scrollOffset;
                int y = FB_LIST_START_Y + row * FB_ITEM_HEIGHT;

                // 反色画一行
                canvas->fillRect(FB_LIST_START_X, y,
                                 FB_LIST_WIDTH - 6, FB_ITEM_HEIGHT - 1,
                                 MENU_TITLE_COLOR);
                canvas->setTextColor(MENU_BG_COLOR);
                canvas->drawString(getDisplayName(item),
                                   FB_LIST_START_X + 18 + calcIndent(item.depth),
                                   y + 2, FB_FONT);
                canvas->pushSprite(0, 0);
                delay(80);

                drawList(); // 恢复正常
            }
        }
        break;

    case BTN_OK_LP: // 长按：回到顶部
        HAL::beep(900, 40);
        selectedIndex = 0;
        scrollOffset = 0;
        Serial.println("[FB] Jump to top");
        drawList();
        break;
    }
}

//==================== 滚动辅助 ====================

void FileBrowser::ensureVisible()
{
    if (selectedIndex < scrollOffset)
    {
        scrollOffset = selectedIndex;
    }
    else if (selectedIndex >= scrollOffset + FB_ITEMS_PER_PAGE)
    {
        scrollOffset = selectedIndex - FB_ITEMS_PER_PAGE + 1;
    }
}

//==================== 绘制 ====================

void FileBrowser::drawMain()
{
    canvas->fillSprite(MENU_BG_COLOR);

    // ===== 标题栏 =====
    canvas->fillRect(0, 0, SCREEN_WIDTH, MENU_HEADER_HEIGHT, MENU_HEADER_COLOR);
    canvas->setTextColor(MENU_TITLE_COLOR);
    canvas->setFont(&fonts::Font0);
    canvas->setTextSize(2);
    canvas->setCursor(50, 10);
    canvas->print("File Browser");

    // ===== 列表区域 =====
    canvas->drawRect(FB_LIST_START_X - 2, FB_LIST_START_Y - 2,
                     FB_LIST_WIDTH + 4, FB_ITEMS_PER_PAGE * FB_ITEM_HEIGHT + 4,
                     MENU_BORDER_COLOR);

    if (flatList.empty())
    {
        canvas->setTextColor(MENU_HINT_COLOR);
        canvas->setTextSize(1);
        if (!HAL::SD_isReady())
        {
            canvas->setCursor(60, FB_LIST_START_Y + 40);
            canvas->print("No SD card detected");
            canvas->setCursor(40, FB_LIST_START_Y + 60);
            canvas->print("Please insert SD card");
            canvas->setCursor(50, FB_LIST_START_Y + 100);
            canvas->setTextColor(MENU_ARROW_COLOR);
            canvas->print("Press OK to retry");
        }
        else
        {
            canvas->setCursor(80, FB_LIST_START_Y + 60);
            canvas->print("Empty directory");
        }
    }
    else
    {
        drawList();
    }

    // ===== 底部状态栏 =====
    drawStatusBar();

    canvas->pushSprite(0, 0);
}

void FileBrowser::drawStatusBar()
{
    int hintY = SCREEN_HEIGHT - MENU_STATUS_HEIGHT;

    canvas->fillRect(0, hintY, SCREEN_WIDTH, MENU_STATUS_HEIGHT, MENU_HEADER_COLOR);
    canvas->setTextColor(MENU_HINT_COLOR);
    canvas->setFont(&fonts::Font0);
    canvas->setTextSize(1);

    // 左：操作提示
    canvas->setCursor(18, hintY + 10);
    canvas->print("SP: Open/Select  LP: Top");

    // 右：位置信息
    int total = (int)flatList.size();
    if (total > 0)
    {
        char info[32];
        snprintf(info, sizeof(info), "[%d/%d]", selectedIndex + 1, total);
        canvas->setCursor(SCREEN_WIDTH - 60, hintY + 10);
        canvas->print(info);
    }
}

void FileBrowser::drawList()
{
    canvas->setTextSize(1);
    canvas->setFont(FB_FONT);

    int x0 = FB_LIST_START_X;
    int y0 = FB_LIST_START_Y;
    int w = FB_LIST_WIDTH;

    // 计算文本可用宽度（减去左侧边距和右侧滚动条空间）
    int textAreaW = w - 6;         // 右侧留 6px 给滚动条
    int maxTextW = textAreaW - 14; // 再减去三角箭头占的 14px

    // 清除列表区域
    canvas->fillRect(x0, y0, w, FB_ITEMS_PER_PAGE * FB_ITEM_HEIGHT, MENU_BG_COLOR);

    for (int i = 0; i < FB_ITEMS_PER_PAGE; i++)
    {
        int idx = scrollOffset + i;
        if (idx >= (int)flatList.size())
            break;

        drawItem(i, flatList[idx], (idx == selectedIndex), maxTextW);
    }

    // ===== 滚动条 =====
    int total = (int)flatList.size();
    if (total > FB_ITEMS_PER_PAGE)
    {
        int bar_x = SCREEN_WIDTH - 14;
        int bar_h = FB_ITEMS_PER_PAGE * FB_ITEM_HEIGHT;
        int thumb_h = (FB_ITEMS_PER_PAGE * bar_h) / total;
        int thumb_y = (scrollOffset * (bar_h - thumb_h)) / (total - FB_ITEMS_PER_PAGE);

        canvas->fillRect(bar_x, y0, 3, bar_h, 0x2104);
        canvas->fillRect(bar_x, y0 + thumb_y, 3, thumb_h, 0x8410);
    }

    canvas->pushSprite(0, 0);
}

void FileBrowser::drawItem(int screenRow, const FbItem &item, bool selected, int maxTextW)
{
    int x0 = FB_LIST_START_X;
    int y = FB_LIST_START_Y + screenRow * FB_ITEM_HEIGHT;
    int w = FB_LIST_WIDTH;
    int indent = calcIndent(item.depth);

    if (selected)
    {
        // 选中行高亮
        canvas->fillRect(x0, y, w - 6, FB_ITEM_HEIGHT - 1, MENU_HIGHLIGHT_BG);

        // 左侧三角箭头
        int16_t tri_y = y + (FB_ITEM_HEIGHT - 1) / 2;
        canvas->fillTriangle(x0 + 4, tri_y - 4,
                             x0 + 4, tri_y + 4,
                             x0 + 9, tri_y, MENU_ARROW_COLOR);

        canvas->setTextColor(MENU_TITLE_COLOR);
    }
    else
    {
        canvas->setTextColor(item.type == FB_TYPE_FOLDER ? 0xCE79 : MENU_TEXT_COLOR);
    }

    // 图标 + 名称
    int textX = x0 + 14 + indent;
    String display;

    if (item.type == FB_TYPE_FOLDER)
    {
        // 文件夹图标 [+] / [-]
        display = String(item.expanded ? "[-] " : "[+] ") + getDisplayName(item);
    }
    else
    {
        display = String(" ") + getDisplayName(item);
    }

    // 用传入的 maxTextW 减去缩进，作为截断宽度上限
    int availW = maxTextW - indent;
    if (availW < 20)
        availW = 20; // 安全下限
    display = trimTextToWidth(display, availW);

    canvas->drawString(display, textX, y + 2, FB_FONT);
}

//==================== 工具函数 ====================

String FileBrowser::getDisplayName(const FbItem &item) const
{
#if 0
    String name = item.name;

    // 去掉扩展名（仅对文件）
    if (item.type == FB_TYPE_FILE)
    {
        int dot = name.lastIndexOf('.');
        if (dot > 0)
            name = name.substring(0, dot);
    }
    return name;
#endif

    // 直接返回原始名称，不做任何裁剪
    return item.name;
}

int FileBrowser::calcIndent(int depth) const
{
    if (depth <= 0)
        return 0;
    return depth * FB_INDENT_WIDTH;
}

/*
 * trimTextToWidth
 *  按像素宽度截断字符串，支持 UTF-8 中文
 */
String FileBrowser::trimTextToWidth(const String &text, int maxWidth) const
{
    if (canvas->textWidth(text, FB_FONT) <= maxWidth)
        return text;

    const String ellipsis = "...";
    int ellipsisW = canvas->textWidth(ellipsis, FB_FONT);
    if (ellipsisW >= maxWidth)
        return "";

    String result;
    for (int i = 0; i < text.length();)
    {
        // 计算当前 UTF-8 字符长度
        uint8_t lead = (uint8_t)text[i];
        int charBytes = 1;
        if ((lead & 0xE0) == 0xC0)
            charBytes = 2;
        else if ((lead & 0xF0) == 0xE0)
            charBytes = 3;
        else if ((lead & 0xF8) == 0xF0)
            charBytes = 4;

        if (i + charBytes > text.length())
            break;

        String ch = text.substring(i, i + charBytes);
        String candidate = result + ch + ellipsis;
        if (canvas->textWidth(candidate, FB_FONT) > maxWidth)
            break;

        result += ch;
        i += charBytes;
    }
    return result + ellipsis;
}
