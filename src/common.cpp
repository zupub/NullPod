#include "hal/hal.h"
#include "hal/hal_config.h"
#include "common.h"
#include <esp_partition.h>
#include <esp_flash.h>

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

// RGB565 变暗
uint16_t dim565(uint16_t c, float factor)
{
    if (factor < 0.0f)
        factor = 0.0f;
    if (factor > 1.0f)
        factor = 1.0f;

    uint8_t r5 = (uint8_t)(((c >> 11) & 0x1F) * factor);
    uint8_t g6 = (uint8_t)(((c >> 5) & 0x3F) * factor);
    uint8_t b5 = (uint8_t)((c & 0x1F) * factor);

    return (uint16_t)((r5 << 11) | (g6 << 5) | b5);
}

int getRamPercent()
{
    uint32_t free = ESP.getFreeHeap();
    uint32_t total = ESP.getHeapSize();
    if (total == 0)
        return 0;
    return ((total - free) * 100) / total;
}

int getPsramPercent()
{
    if (!psramFound())
        return 0;
    uint32_t total = ESP.getPsramSize();
    if (total == 0)
        return 0;
    uint32_t free = ESP.getFreePsram();
    return ((total - free) * 100) / total;
}

int getFlashPercent()
{
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    if (!part)
    {
        part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    }
    if (!part)
        return 0;

    uint32_t total = ESP.getFlashChipSize();
    if (total == 0)
        return 0;

    return (part->size * 100) / total;
}

void drawScaledIcon(LGFX_Sprite *canvas, int16_t x, int16_t y, float scale, const uint16_t *src, bool clicked)
{
    if (scale <= 0)
        return;

    int newSize = (int)(ICON_SRC_SIZE * scale);
    if (newSize <= 0)
        return;

    if (x + newSize <= 0 || x >= SCREEN_WIDTH ||
        y + newSize <= 0 || y >= SCREEN_HEIGHT)
        return;

    uint16_t tintColor = rgb565(0, 0, 0);
    uint8_t tr = (tintColor >> 11) & 0x1F;
    uint8_t tg = (tintColor >> 5) & 0x3F;
    uint8_t tb = tintColor & 0x1F;

    for (int dy = 0; dy < newSize; dy++)
    {
        for (int dx = 0; dx < newSize; dx++)
        {
            int sx = (dx * ICON_SRC_SIZE) / newSize;
            int sy = (dy * ICON_SRC_SIZE) / newSize;

            if (sx >= ICON_SRC_SIZE)
                sx = ICON_SRC_SIZE - 1;
            if (sy >= ICON_SRC_SIZE)
                sy = ICON_SRC_SIZE - 1;

            uint16_t pixel = pgm_read_word(&src[sy * ICON_SRC_SIZE + sx]);
            if (pixel == 0x0000)
                continue;

            uint8_t sr = (pixel >> 11) & 0x1F;
            uint8_t sg = (pixel >> 5) & 0x3F;
            uint8_t sb = pixel & 0x1F;

            if (clicked)
            {
                sr = (sr * 3 + tr * 1) >> 2;
                sg = (sg * 3 + tg * 1) >> 2;
                sb = (sb * 3 + tb * 1) >> 2;
            }

            canvas->drawPixel(x + dx, y + dy, (sr << 11) | (sg << 5) | sb);
        }
    }
}

// 模拟 TFT_eSPI 的 drawWedgeLine
void drawWedgeLine(LGFX_Sprite *s, float x0, float y0, float x1, float y1, float w0, float w1, uint32_t color, uint32_t bg)
{
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len == 0)
        return;

    float ex = -dy / len;
    float ey = dx / len;

    // 中心轴线
    float cx0 = x0, cy0 = y0;
    float cx1 = x1, cy1 = y1;

    int steps = 6; // 平滑度

    for (int i = -steps; i <= steps; i++)
    {
        float t = (float)i / (float)steps;
        float w = (w0 * (1 - t * 0.5f) + w1 * (1 + t * 0.5f)) * 0.5f;

        float ox = ex * w * t;
        float oy = ey * w * t;

        uint8_t alpha = 255 - abs(i) * (255 / (steps + 1));

        s->drawLine(
            cx0 + ox, cy0 + oy,
            cx1 + ox, cy1 + oy,
            s->color565(
                ((color >> 11) & 0x1F) * alpha / 255,
                ((color >> 5) & 0x3F) * alpha / 255,
                (color & 0x1F) * alpha / 255));
    }
}
