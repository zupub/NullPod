#ifndef _FILEBROWSER_H_
#define _FILEBROWSER_H_

#include "hal/hal.h"
#include "hal/hal_config.h"
#include <vector>
#include <algorithm>

//==================== 配置宏 ====================
#define FB_LIST_START_X 10
#define FB_LIST_START_Y 44
#define FB_ITEM_HEIGHT 20
#define FB_LIST_WIDTH 220
#define FB_ITEMS_PER_PAGE 9
#define FB_INDENT_WIDTH 16 // 子项缩进像素

//==================== 列表项类型 ====================
enum FbItemType
{
    FB_TYPE_FILE = 0,   // 普通文件
    FB_TYPE_FOLDER = 1, // 文件夹
    FB_TYPE_PARENT = 2, // ".." 返回上级
};

//==================== 单个列表项 ====================
struct FbItem
{
    String path;     // 完整路径，如 "/games/nes"
    String name;     // 显示名称，如 "nes"
    FbItemType type; // 类型
    bool isNes;      // 是否是 .nes 文件
    bool expanded;   // 文件夹是否已展开
    int depth;       // 嵌套深度（缩进层级），0=根目录
    int childCount;  // 子项数量（展开后填入）
    int childStart;  // 子项在 flatList 中的起始索引
};

//==================== FileBrowser 类 ====================
class FileBrowser
{
public:
    void Main();

protected:
    void Init();
    void Update();
    void Exit();

private:
    LGFX *disp = nullptr;
    LGFX_Sprite *canvas = nullptr;

    //-------- 数据 --------
    std::vector<FbItem> flatList;  // 展平后的列表（含展开的子项）
    std::vector<String> pathStack; // 目录栈（方便后续加"返回上级"）

    int selectedIndex;    // 当前选中项在 flatList 中的索引
    int scrollOffset;     // 滚动偏移
    bool confirmed;       // 是否确认选中
    String confirmedPath; // 确认选中的文件路径

    //-------- 绘制 --------
    void drawMain();
    void drawList();
    void drawStatusBar();

    //-------- 文件操作 --------
    void scanDirectory(const String &path, int depth);
    void toggleFolder(int index);
    void refreshRoot();

    //-------- 输入处理 --------
    void handleInput();

    //-------- 工具 --------
    String getDisplayName(const FbItem &item) const;
    String trimTextToWidth(const String &text, int maxWidth) const;
    int calcIndent(int depth) const;
    void ensureVisible();
    void drawItem(int screenRow, const FbItem &item, bool selected, int maxTextW);
};

extern FileBrowser g_fileBrowser;

#endif
