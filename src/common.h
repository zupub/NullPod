#ifndef _COMMON_H_
#define _COMMON_H_

// ================ 菜单颜色配置 (高级灰色调) ================
#define MENU_BG_COLOR 0x2104       // 深灰背景 (RGB: 32, 32, 32)
#define MENU_HEADER_COLOR 0x4A69   // 中灰标题背景 (RGB: 72, 77, 72)
#define MENU_STATUS_COLOR 0x4A69   // 中灰状态栏背景 (RGB: 72, 77, 72)
#define MENU_TEXT_COLOR 0xC618     // 浅灰文字 (RGB: 192, 192, 192)
#define MENU_HIGHLIGHT_BG 0xFDE0   // 选中项背景 (RGB: 255, 255, 0)
#define MENU_ARROW_COLOR 0xAD75    // 箭头颜色 (RGB: 168, 174, 168)
#define MENU_HINT_COLOR 0x7BCF     // 提示文字颜色 (RGB: 120, 120, 120)
#define MENU_TITLE_COLOR 0xE71C    // 标题文字 (RGB: 224, 224, 224)
#define MENU_BORDER_COLOR 0x52AA   // 边框颜色 (RGB: 80, 85, 80)
#define PAUSE_OVERLAY_COLOR 0x18C3 // 暂停遮罩 (深色半透明效果)

#define MENU_HEADER_HEIGHT 36
#define MENU_STATUS_Y 250
#define MENU_STATUS_HEIGHT 30
#define ICON_SRC_SIZE 64

#define LIGHT_BLUE 0x1DDC
#define LIGHT_YELLOW 0xBD41
#define LIGHT_PURPLE 0xD0B0
#define LIGHT_GRAY 0x1902
#define JOYSTICK_GREEN 0x07E0

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
uint16_t dim565(uint16_t c, float factor);
int getRamPercent();
int getPsramPercent();
int getFlashPercent();
void drawScaledIcon(LGFX_Sprite *canvas, int16_t x, int16_t y, float scale, const uint16_t *src, bool clicked);
void drawWedgeLine(LGFX_Sprite *s, float x0, float y0, float x1, float y1, float w0, float w1, uint32_t color, uint32_t bg);

#endif
