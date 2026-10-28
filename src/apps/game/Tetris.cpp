//========================================================================
// TETRIS APP
//   原始程序 : TETRIS with M5STACK (Transplant by macsbug, 2018.01.20)
//   硬件抽象 : HAL (GetCanvas / getEncoder / beep / isReturn)
//   显示     : LGFX_Sprite canvas，左侧棋盘 + 右侧信息面板
//   操作     : 左/右键 平移，旋钮与 OK 键 旋转，上键 开始·暂停·继续，
//              下键 点按落一格 / 按住持续快速落到底，OK 长按 重开
//========================================================================
#include "Tetris.h"
#include "common.h"

Tetris g_tetris;

static const lgfx::IFont *font_small = &fonts::efontCN_16;
static const lgfx::IFont *font_medium = &fonts::efontCN_24;

//========================================================================
// 颜色表：索引 0 = 空，1~7 对应 7 种方块（RGB565）
//========================================================================
static const uint16_t kBlockColors[8] = {
    0x0000, // 0 空
    0xF800, // 1 I  红
    0x780F, // 2 O  紫
    0x001F, // 3 J  蓝
    0x07E0, // 4 S  绿
    0xFFE0, // 5 L  黄
    0x07FF, // 6 Z  青
    0xF81F  // 7 T  粉
};

// 棋盘底色 / 网格线 / 外框
static const uint16_t C_BOARD_BG = 0x0841;
static const uint16_t C_BOARD_GRID = 0x18C3;
static const uint16_t C_BOARD_EDGE = 0x8410;

//========================================================================
// 7 种方块的 4 个旋转态坐标（沿用原始程序数据）
//========================================================================
static const TetrisBlock kBlocks[7] = {
    // 1 I : ----
    {{{{-1, 0}, {0, 0}, {1, 0}, {2, 0}},
      {{0, -1}, {0, 0}, {0, 1}, {0, 2}},
      {{0, 0}, {0, 0}, {0, 0}, {0, 0}},
      {{0, 0}, {0, 0}, {0, 0}, {0, 0}}},
     2,
     1},
    // 2 O : 口
    {{{{0, -1}, {1, -1}, {0, 0}, {1, 0}},
      {{0, 0}, {0, 0}, {0, 0}, {0, 0}},
      {{0, 0}, {0, 0}, {0, 0}, {0, 0}},
      {{0, 0}, {0, 0}, {0, 0}, {0, 0}}},
     1,
     2},
    // 3 J
    {{{{-1, -1}, {-1, 0}, {0, 0}, {1, 0}},
      {{-1, 1}, {0, 1}, {0, 0}, {0, -1}},
      {{-1, 0}, {0, 0}, {1, 0}, {1, 1}},
      {{1, -1}, {0, -1}, {0, 0}, {0, 1}}},
     4,
     3},
    // 4 S
    {{{{-1, 0}, {0, 0}, {0, 1}, {1, 1}},
      {{0, -1}, {0, 0}, {-1, 0}, {-1, 1}},
      {{0, 0}, {0, 0}, {0, 0}, {0, 0}},
      {{0, 0}, {0, 0}, {0, 0}, {0, 0}}},
     2,
     4},
    // 5 L
    {{{{-1, 0}, {0, 0}, {1, 0}, {1, -1}},
      {{-1, -1}, {0, -1}, {0, 0}, {0, 1}},
      {{-1, 1}, {-1, 0}, {0, 0}, {1, 0}},
      {{0, -1}, {0, 0}, {0, 1}, {1, 1}}},
     4,
     5},
    // 6 Z
    {{{{-1, 1}, {0, 1}, {0, 0}, {1, 0}},
      {{0, -1}, {0, 0}, {1, 0}, {1, 1}},
      {{0, 0}, {0, 0}, {0, 0}, {0, 0}},
      {{0, 0}, {0, 0}, {0, 0}, {0, 0}}},
     2,
     6},
    // 7 T
    {{{{-1, 0}, {0, 0}, {1, 0}, {0, -1}},
      {{0, -1}, {0, 0}, {0, 1}, {-1, 0}},
      {{-1, 0}, {0, 0}, {1, 0}, {0, 1}},
      {{0, -1}, {0, 0}, {0, 1}, {1, 0}}},
     4,
     7}};

// 消行得分：1/2/3/4 行
static const int kLineScore[5] = {0, 100, 300, 500, 800};

//========================================================================
// 颜色工具：向目标色混合，用于方块高光 / 阴影
//========================================================================
static uint16_t mixColor(uint16_t a, uint16_t b, uint8_t ratio)
{
    uint8_t r1 = (a >> 11) & 0x1F, g1 = (a >> 5) & 0x3F, b1 = a & 0x1F;
    uint8_t r2 = (b >> 11) & 0x1F, g2 = (b >> 5) & 0x3F, b2 = b & 0x1F;
    uint8_t r = (uint8_t)((r1 * (255 - ratio) + r2 * ratio) / 255);
    uint8_t g = (uint8_t)((g1 * (255 - ratio) + g2 * ratio) / 255);
    uint8_t bl = (uint8_t)((b1 * (255 - ratio) + b2 * ratio) / 255);
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

//========================================================================
// 初始化
//========================================================================
void Tetris::Init()
{
    canvas = HAL::GetCanvas();
    layout();
    // 随机种子（如需更高质量随机，可换成 randomSeed(esp_random())）
    randomSeed(micros());
    loadHighScore(); // App 进入时从 Flash 调出最高分
    resetGame();
    HAL::logMemory();
    Serial.println("[Tetris] Init done");
}

//========================================================================
// 最高分持久化（Preferences / NVS，命名空间 "tetris"）
//   与 Compass::saveLevelOffset() 同一套机制：begin(ns, 只读/可写) + get/put + end。
//========================================================================
void Tetris::loadHighScore()
{
    Preferences prefs;
    if (prefs.begin(TETRIS_NVS_NS, true)) // true = 只读
    {
        g_highScore = prefs.getInt(TETRIS_NVS_KEY_HIGH, 0);
        prefs.end();
    }
    if (g_highScore < 0)
        g_highScore = 0; // 异常值兜底
    g_highDirty = false;
    Serial.printf("[Tetris] 载入最高分: %d\n", g_highScore);
}

void Tetris::saveHighScore()
{
    Preferences prefs;
    if (prefs.begin(TETRIS_NVS_NS, false)) // false = 可写
    {
        prefs.putInt(TETRIS_NVS_KEY_HIGH, g_highScore);
        prefs.end();
        Serial.printf("[Tetris] 保存最高分: %d\n", g_highScore);
    }
}

// 仅更新内存：每次固化后调用，面板卡片即时刷新，不写 Flash
void Tetris::updateHighScore()
{
    if (g_score > g_highScore)
    {
        g_highScore = g_score;
        g_highDirty = true;
    }
}

// 落盘：仅在破纪录时写一次，避免频繁擦写 Flash（开局 / 结束 / 退出时调用）
void Tetris::commitHighScore()
{
    updateHighScore();
    if (g_highDirty)
    {
        saveHighScore();
        g_highDirty = false;
    }
}

//========================================================================
// 布局：左侧游戏区尽量占画布宽的 2/3，右侧面板占 1/3
//   格子边长 = min(2/3宽 / COLS, 画布高 / ROWS)，保证 10x20 完整可见且格子为正方形。
//   当画布高度不足（如 240x280）时，高度成为约束，游戏区会略窄于 2/3，
//   剩余空间自动让给右侧面板。
//========================================================================
void Tetris::layout()
{
    int cw = canvas->width();
    int ch = canvas->height();

    int targetBoardW = (int)(cw * TETRIS_BOARD_RATIO); // 2/3 宽
    int cellByW = targetBoardW / TETRIS_COLS;
    int cellByH = ch / TETRIS_ROWS; // 20 行必须放得下

    g_cell = cellByW < cellByH ? cellByW : cellByH;
    if (g_cell > TETRIS_CELL_MAX)
        g_cell = TETRIS_CELL_MAX;
    if (g_cell < TETRIS_CELL_MIN)
        g_cell = TETRIS_CELL_MIN;

    g_boardW = g_cell * TETRIS_COLS;
    g_boardH = g_cell * TETRIS_ROWS;
    g_boardX = 2;
    g_boardY = (ch - g_boardH) / 2; // 垂直居中
    if (g_boardY < 0)
        g_boardY = 0;

    g_panelX = g_boardX + g_boardW + TETRIS_GAP;
    g_panelW = cw - g_panelX - 2;
    if (g_panelW < 0)
        g_panelW = 0;
}

//========================================================================
// 清空棋盘与计分
//========================================================================
void Tetris::resetGame()
{
    for (int x = 0; x < TETRIS_COLS; ++x)
        for (int y = 0; y < TETRIS_ROWS; ++y)
            g_grid[x][y] = 0;

    g_score = 0;
    g_lines = 0;
    g_level = 1;
    g_state = TETRIS_READY;
    g_next = kBlocks[random(7)];
    g_block = g_next;
    g_bx = TETRIS_COLS / 2 - 1;
    g_by = 1;
    g_rot = 0;
    g_dirty = true;
}

//========================================================================
// 开新一局
//========================================================================
void Tetris::startGame()
{
    commitHighScore(); // 结算上一局（长按重开时也能保住纪录）
    resetGame();
    spawn();
    if (g_state != TETRIS_GAMEOVER)
        g_state = TETRIS_PLAYING;
    HAL::beep(880, 60);
    Serial.println("[Tetris] Game start");
}

//========================================================================
// 生成方块：next -> current，并补一个新的 next
//========================================================================
void Tetris::spawn()
{
    g_block = g_next;
    g_next = kBlocks[random(7)];

    g_bx = TETRIS_COLS / 2 - 1;
    g_by = 1;
    g_rot = random(g_block.numRotate);

    g_lastFall = millis();
    g_touchBottomAt = 0; // 新方块：清除触底计时

    if (!canPlace(g_block, g_bx, g_by, g_rot))
    {
        g_state = TETRIS_GAMEOVER;
        commitHighScore(); // 结束时落盘
        HAL::beep(200, 300);
        Serial.println("[Tetris] Game Over");
    }
}

//========================================================================
// 放置判定：越界或与已有方块重叠则不合法
//========================================================================
bool Tetris::canPlace(const TetrisBlock &b, int px, int py, int r) const
{
    for (int i = 0; i < 4; ++i)
    {
        int x = px + b.square[r][i].x;
        int y = py + b.square[r][i].y;
        if (x < 0 || x >= TETRIS_COLS || y < 0 || y >= TETRIS_ROWS)
            return false;
        if (g_grid[x][y] != 0)
            return false;
    }
    return true;
}

//========================================================================
// 尝试移动到新位置 / 新旋转态
//========================================================================
bool Tetris::tryPlace(int nx, int ny, int nrot)
{
    if (!canPlace(g_block, nx, ny, nrot))
        return false;
    g_bx = nx;
    g_by = ny;
    g_rot = nrot;
    g_dirty = true;
    return true;
}

void Tetris::moveHorizontal(int dir)
{
    if (g_state != TETRIS_PLAYING)
        return;
    tryPlace(g_bx + dir, g_by, g_rot);
}

//========================================================================
// 旋转：先原地转，失败则左右墙踢（wall kick）
//========================================================================
void Tetris::rotateBlock(int dir)
{
    if (g_state != TETRIS_PLAYING)
        return;

    int nrot = (g_rot + dir + g_block.numRotate) % g_block.numRotate;
    if (nrot == g_rot)
        return;

    static const int kickX[5] = {0, -1, 1, -2, 2};
    for (int i = 0; i < 5; ++i)
    {
        if (tryPlace(g_bx + kickX[i], g_by, nrot))
        {
            HAL::beep(600, 15);
            return;
        }
    }
}

//========================================================================
// 下键点按：立刻下移一格并计分；已到底则立即固化出块
//========================================================================
void Tetris::softDrop()
{
    if (g_state != TETRIS_PLAYING)
        return;

    if (tryPlace(g_bx, g_by + 1, g_rot))
    {
        g_score += TETRIS_SOFT_DROP_SCORE;
        g_lastFall = millis(); // 重置自然下落计时，避免叠加导致双跳
        g_dirty = true;
    }
    else
    {
        lockBlock(); // 已经贴底/贴块：直接锁定，手感更干脆
    }
}

//========================================================================
// 下键按住：持续快速下落到底
//   HAL::GetButton() 在按住期间持续返回 true（内部已做按下/松开边沿蜂鸣），
//   因此直接轮询即可，不再需要"按下后若干毫秒内加速"的计时窗口。
//   落到底后固化出块，并置 g_holdLocked，必须松手才能再次触发，
//   否则按住不放会把后续方块一个个全部砸到底。
//========================================================================
void Tetris::pollHoldDrop()
{
    bool held = HAL::GetButton(ENCODER::BTN_DOWN);

    // 松手：清除按住相关状态
    if (!held)
    {
        g_holdActive = false;
        g_holdLocked = false;
        return;
    }

    if (g_state != TETRIS_PLAYING || g_holdLocked)
        return;

    uint32_t now = millis();

    // 按下边沿：初始化计时，避免沿用旧时间戳导致瞬间连跳
    if (!g_holdActive)
    {
        g_holdActive = true;
        g_lastDropStep = now;
        g_touchBottomAt = 0;
        return; // 首帧先不下落，下一帧再开始
    }

    if (now - g_lastDropStep < TETRIS_HOLD_DROP_MS)
        return;

    g_lastDropStep = now;

    if (tryPlace(g_bx, g_by + 1, g_rot))
    {
        // 还能往下：继续加速下落
        g_score += TETRIS_SOFT_DROP_SCORE;
        g_lastFall = now; // 压制自然下落，避免与加速叠加导致跳两格
        g_touchBottomAt = 0;
        g_dirty = true;
    }
    else
    {
        // 已经到底：可选延迟一小段，留出最后的微调机会
        const uint32_t delayMs = TETRIS_HOLD_LOCK_DELAY_MS;
        if (g_touchBottomAt == 0)
        {
            g_touchBottomAt = now;
            if (delayMs > 0)
                return;
        }
        else if (now - g_touchBottomAt < delayMs)
        {
            return;
        }

        lockBlock();         // 固化并生成下一块
        g_holdLocked = true; // 本轮按住结束，需松手后才能再触发
    }
}

//========================================================================
// 固化和消行
//========================================================================
void Tetris::lockBlock()
{
    for (int i = 0; i < 4; ++i)
    {
        int x = g_bx + g_block.square[g_rot][i].x;
        int y = g_by + g_block.square[g_rot][i].y;
        if (x >= 0 && x < TETRIS_COLS && y >= 0 && y < TETRIS_ROWS)
            g_grid[x][y] = g_block.color;
    }

    int cleared = clearLines();
    if (cleared > 0)
    {
        g_lines += cleared;
        g_score += kLineScore[cleared] * g_level;
        g_level = g_lines / TETRIS_LINES_PER_LEVEL + 1;
        HAL::beep(1000, 60);
    }
    else
    {
        HAL::beep(320, 20);
    }

    updateHighScore(); // 破纪录时立即刷新面板上的最高分
    spawn();
    g_dirty = true;
}

//========================================================================
// 消行：从下往上扫描，消除后上方整体下移并重新检查当前行
//========================================================================
int Tetris::clearLines()
{
    int cleared = 0;
    for (int y = TETRIS_ROWS - 1; y >= 0; --y)
    {
        bool full = true;
        for (int x = 0; x < TETRIS_COLS; ++x)
        {
            if (g_grid[x][y] == 0)
            {
                full = false;
                break;
            }
        }
        if (!full)
            continue;

        for (int k = y; k > 0; --k)
            for (int x = 0; x < TETRIS_COLS; ++x)
                g_grid[x][k] = g_grid[x][k - 1];
        for (int x = 0; x < TETRIS_COLS; ++x)
            g_grid[x][0] = 0;

        ++cleared;
        ++y; // 上方内容已下移到此行，需重新检查
    }
    return cleared;
}

//========================================================================
// 落点预测
//========================================================================
int Tetris::ghostY() const
{
    int y = g_by;
    while (canPlace(g_block, g_bx, y + 1, g_rot))
        ++y;
    return y;
}

uint16_t Tetris::fallInterval() const
{
    int iv = TETRIS_FALL_BASE_MS - (g_level - 1) * TETRIS_FALL_STEP_MS;
    if (iv < TETRIS_FALL_MIN_MS)
        iv = TETRIS_FALL_MIN_MS;
    return (uint16_t)iv;
}

//========================================================================
// 每帧逻辑：到点下落一格，落不动则固化
//========================================================================
void Tetris::tick(uint32_t now)
{
    if (g_state != TETRIS_PLAYING)
        return;

    uint16_t iv = fallInterval();

    if (now - g_lastFall < iv)
        return;

    g_lastFall = now;
    if (!tryPlace(g_bx, g_by + 1, g_rot))
        lockBlock();
}

//========================================================================
// 输入
//========================================================================
void Tetris::handleKey(uint8_t key)
{
    HAL::beep(500, 20);
    switch (key)
    {
    case BTN_LEFT_SP:
        moveHorizontal(-1);
        break;
    case BTN_RIGHT_SP:
        moveHorizontal(+1);
        break;
    case BTN_ENC_CW:
        rotateBlock(-1); // 编码器右转：顺时针
        break;
    case BTN_ENC_CC:
        rotateBlock(+1); // 编码器左转：逆时针
        break;
    case BTN_OK_SP:
        handleShortPress(); // OK 短按同样旋转
        break;
    case BTN_OK_LP:
        handleLongPress();
        break;
    case BTN_UP_SP:
        handlePauseResume(); // 编码器上键：开始 / 暂停 / 继续
        break;
    case BTN_DOWN_SP:
        // 点按下键：下落一格。按住期间由 pollHoldDrop() 轮询处理，
        // 避免事件重复上报与轮询叠加导致一帧掉两格。
        if (!HAL::GetButton(ENCODER::BTN_DOWN))
            softDrop();
        break;
    default:
        break;
    }
}

// OK 短按：游戏中旋转（与编码器等效）；未开局时兼作开始
void Tetris::handleShortPress()
{
    if (g_state == TETRIS_PLAYING)
    {
        rotateBlock(-1); // OK 与旋转键一致，默认顺时针
    }
    else if (g_state == TETRIS_READY || g_state == TETRIS_GAMEOVER)
    {
        startGame(); // 未开局时的兜底，不改变"上键开始"的主路径
    }
    else
    {
        HAL::beep(300, 30); // 暂停中：旋转无效，给个轻提示
    }
}

// 上键：开始 / 暂停 / 继续
void Tetris::handlePauseResume()
{
    switch (g_state)
    {
    case TETRIS_READY:
    case TETRIS_GAMEOVER:
        startGame();
        break;
    case TETRIS_PLAYING:
        g_state = TETRIS_PAUSED;
        g_dirty = true;
        HAL::beep(400, 60);
        Serial.println("[Tetris] Paused");
        break;
    case TETRIS_PAUSED:
        g_state = TETRIS_PLAYING;
        g_lastFall = millis(); // 重置下落计时，避免恢复瞬间连掉
        g_dirty = true;
        HAL::beep(660, 60);
        Serial.println("[Tetris] Resumed");
        break;
    default:
        break;
    }
}

// OK 长按：任意时刻重开一局
void Tetris::handleLongPress()
{
    HAL::beep(300, 80);
    startGame();
}

//========================================================================
// Update：读键 -> 逻辑 -> 绘制
//========================================================================
void Tetris::Update()
{
    uint8_t key = 0;
    if (HAL::getEncoder(&key))
        handleKey(key);

    pollHoldDrop(); // 轮询下键：按住期间持续快速下落到底

    uint32_t now = millis();
    tick(now);

    // 游戏中按帧刷新，其余状态只在变化时重绘
    if (g_dirty || (g_state == TETRIS_PLAYING && now - g_lastDraw >= TETRIS_FRAME_MS))
    {
        g_lastDraw = now;
        draw();
        g_dirty = false;
    }
}

//========================================================================
// 绘制：带立体高光的方块单元
//========================================================================
void Tetris::drawUnit(LGFX_Sprite *s, int x, int y, int size, uint16_t color)
{
    int inner = size - 1;
    if (inner < 1)
        inner = 1;
    s->fillRect(x, y, inner, inner, color);
    s->drawFastHLine(x, y, inner, mixColor(color, 0xFFFF, 110));             // 上高光
    s->drawFastVLine(x, y, inner, mixColor(color, 0xFFFF, 110));             // 左高光
    s->drawFastHLine(x, y + inner - 1, inner, mixColor(color, 0x0000, 140)); // 下阴影
    s->drawFastVLine(x + inner - 1, y, inner, mixColor(color, 0x0000, 140)); // 右阴影
}

// 按格坐标画一个方块
void Tetris::drawCell(int gx, int gy, uint8_t colorIdx)
{
    int x = g_boardX + gx * g_cell + 1;
    int y = g_boardY + gy * g_cell + 1;
    drawUnit(canvas, x, y, g_cell - 1, kBlockColors[colorIdx]);
}

//========================================================================
// 棋盘：底色 + 网格 + 已固化方块 + 落点虚影 + 当前方块
//========================================================================
void Tetris::drawBoard()
{
    canvas->fillRect(g_boardX, g_boardY, g_boardW, g_boardH, C_BOARD_BG);

    // 网格线
    for (int x = 1; x < TETRIS_COLS; ++x)
        canvas->drawFastVLine(g_boardX + x * g_cell, g_boardY, g_boardH, C_BOARD_GRID);
    for (int y = 1; y < TETRIS_ROWS; ++y)
        canvas->drawFastHLine(g_boardX, g_boardY + y * g_cell, g_boardW, C_BOARD_GRID);
    canvas->drawRect(g_boardX, g_boardY, g_boardW, g_boardH, C_BOARD_EDGE);

    // 已固化
    for (int x = 0; x < TETRIS_COLS; ++x)
        for (int y = 0; y < TETRIS_ROWS; ++y)
            if (g_grid[x][y] != 0)
                drawCell(x, y, g_grid[x][y]);

    if (g_state == TETRIS_PLAYING || g_state == TETRIS_PAUSED)
    {
        // 落点虚影
        int gy = ghostY();
        uint16_t gc = mixColor(kBlockColors[g_block.color], 0x0000, 120);
        for (int i = 0; i < 4; ++i)
        {
            int x = g_bx + g_block.square[g_rot][i].x;
            int y = gy + g_block.square[g_rot][i].y;
            if (x < 0 || x >= TETRIS_COLS || y < 0 || y >= TETRIS_ROWS)
                continue;
            int px = g_boardX + x * g_cell + 1;
            int py = g_boardY + y * g_cell + 1;
            canvas->drawRect(px, py, g_cell - 2, g_cell - 2, gc);
        }

        // 当前方块
        for (int i = 0; i < 4; ++i)
        {
            int x = g_bx + g_block.square[g_rot][i].x;
            int y = g_by + g_block.square[g_rot][i].y;
            if (x < 0 || x >= TETRIS_COLS || y < 0 || y >= TETRIS_ROWS)
                continue;
            drawCell(x, y, g_block.color);
        }
    }
}

//========================================================================
// 右侧信息面板
//========================================================================
void Tetris::drawPanel()
{
    if (g_panelW < 40)
        return;

    int px = g_panelX;
    int pw = g_panelW;
    int y = g_boardY;

    // ---- 标题（面板仅 1/3 宽，用英文标题避免中文溢出）----
    canvas->setTextColor(MENU_TITLE_COLOR);
    canvas->setFont(&FreeSans9pt7b);
    canvas->drawString("TETRIS", px + 4, y + 10);
    canvas->drawFastHLine(px, y + 30, pw, C_BOARD_EDGE);
    y += 36;

    // ---- NEXT 预览 ----
    int boxH = g_cell * 3;
    canvas->setTextColor(TFT_LIGHTGREY);
    canvas->drawString("NEXT", px + 4, y, font_small);
    y += 18;
    drawNextPreview(px, y, pw, boxH);
    y += boxH;

    // ---- 计分卡片：标签一行、数值一行（窄面板下不会相互挤压）----
    auto drawStat = [&](const char *label, int value, uint16_t accent)
    {
        if (y + TETRIS_STAT_H > canvas->height()) // 空间不足则不再绘制
            return;
        canvas->fillRoundRect(px, y, pw, TETRIS_STAT_H, 6, MENU_STATUS_COLOR);
        canvas->drawRoundRect(px, y, pw, TETRIS_STAT_H, 6, accent);
        canvas->setTextColor(TFT_LIGHTGREY);
        canvas->drawString(label, px + 6, y + 2, font_small);
        canvas->setTextColor(accent);
        canvas->setTextDatum(MR_DATUM);
        char buf[24];
        snprintf(buf, sizeof(buf), "%d", value);
        canvas->drawString(buf, px + pw - 6, y + 26, font_medium);
        canvas->setTextDatum(0);
        y += TETRIS_STAT_H + TETRIS_STAT_GAP;
    };

    drawStat("分数", g_score, TFT_YELLOW);
    drawStat("最高分", g_highScore, TFT_ORANGE);
    drawStat("消行", g_lines, TFT_CYAN);
    drawStat("等级", g_level, TFT_GREEN);

    // ---- 操作提示（贴面板底部，空间不足时自动省略）----
    static const char *tips[5] = {"左右移动", "旋钮旋转", "上键暂停", "按住落底", "长按重开"};
    const int tipCount = 5;
    int tipH = tipCount * 16;
    int tipY = g_boardY + g_boardH - tipH;
    if (tipY > y)
    {
        canvas->setTextColor(TFT_DARKGREY);
        for (int i = 0; i < tipCount; ++i)
            canvas->drawString(tips[i], px, tipY + i * 16, font_small);
    }
}

//========================================================================
// NEXT 预览框
//========================================================================
void Tetris::drawNextPreview(int px, int py, int pw, int ph)
{
    canvas->drawRect(px, py, pw, ph, C_BOARD_EDGE);

    const TetrisBlock &b = g_next;
    int minX = 9, maxX = -9, minY = 9, maxY = -9;
    for (int i = 0; i < 4; ++i)
    {
        const TetrisPoint &p = b.square[0][i];
        if (p.x < minX)
            minX = p.x;
        if (p.x > maxX)
            maxX = p.x;
        if (p.y < minY)
            minY = p.y;
        if (p.y > maxY)
            maxY = p.y;
    }

    int w = maxX - minX + 1;
    int h = maxY - minY + 1;
    int size = g_cell;
    if (size > (pw - 8) / w)
        size = (pw - 8) / w;
    if (size > (ph - 8) / h)
        size = (ph - 8) / h;
    if (size < 4)
        size = 4;

    int ox = px + (pw - w * size) / 2 - minX * size;
    int oy = py + (ph - h * size) / 2 - minY * size;

    for (int i = 0; i < 4; ++i)
    {
        int x = ox + b.square[0][i].x * size;
        int y = oy + b.square[0][i].y * size;
        drawUnit(canvas, x, y, size - 1, kBlockColors[b.color]);
    }
}

//========================================================================
// 状态提示层：开始 / 暂停 / 结束
//========================================================================
void Tetris::drawOverlay()
{
    const char *title = nullptr;
    const char *sub = nullptr;
    char buf[32];

    switch (g_state)
    {
    case TETRIS_READY:
        title = "俄罗斯方块";
        sub = "按上键开始";
        break;
    case TETRIS_PAUSED:
        title = "已暂停";
        sub = "上键继续";
        break;
    case TETRIS_GAMEOVER:
        title = "游戏结束";
        snprintf(buf, sizeof(buf), "得分 %d", g_score);
        sub = buf;
        break;
    default:
        return;
    }

    int w = g_boardW - 12;
    int h = 62;
    int x = g_boardX + (g_boardW - w) / 2;
    int y = g_boardY + (g_boardH - h) / 2;

    canvas->fillRoundRect(x, y, w, h, 8, 0x0000);
    canvas->drawRoundRect(x, y, w, h, 8, MENU_TITLE_COLOR);

    canvas->setTextDatum(MC_DATUM);
    canvas->setTextColor(MENU_TITLE_COLOR);
    canvas->drawString(title, x + w / 2, y + 20, font_small);
    canvas->setTextColor(TFT_LIGHTGREY);
    canvas->drawString(sub, x + w / 2, y + 44, font_small);
    canvas->setTextDatum(0);
}

//========================================================================
// 整页绘制
//========================================================================
void Tetris::draw()
{
    canvas->fillSprite(TFT_BLACK);
    drawBoard();
    drawPanel();
    drawOverlay();
    canvas->pushSprite(0, 0);
}

//========================================================================
// 清理
//========================================================================
void Tetris::Exit()
{
    commitHighScore(); // 中途退出也保住本局纪录
    // canvas 由 HAL 统一管理，此处不释放
    HAL::logMemory();
    Serial.println("[Tetris] Exit");
}

//========================================================================
// App 主入口
//========================================================================
void Tetris::Main()
{
    Init();
    while (1)
    {
        Update();
        if (HAL::isReturn())
            break;
    }
    Exit();
}
