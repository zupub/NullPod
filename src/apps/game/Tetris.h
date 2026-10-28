#ifndef _TETRIS_H_
#define _TETRIS_H_

#include "hal/hal.h"
#include "hal/hal_config.h"

// ==================== 棋盘（逻辑格数）====================
#define TETRIS_COLS 10 // 横向格数
#define TETRIS_ROWS 20 // 纵向格数

// ==================== 布局比例 ====================
// 左侧游戏区目标宽度 = 画布宽 * 2/3（右侧面板占 1/3）
#define TETRIS_BOARD_RATIO 0.667f
#define TETRIS_GAP 4 // 游戏区与面板之间的间隙(px)

// ==================== 单格像素边长 ====================
// 优先按 2/3 宽度取格子边长，再受画布高度约束（10x20 必须完整显示）
#define TETRIS_CELL_MAX 20
#define TETRIS_CELL_MIN 6

// ==================== 节奏参数 ====================
#define TETRIS_FALL_BASE_MS 500   // 1 级时每格下落间隔
#define TETRIS_FALL_STEP_MS 45    // 每升 1 级减少的间隔
#define TETRIS_FALL_MIN_MS 90     // 最快下落间隔
#define TETRIS_LINES_PER_LEVEL 10 // 每消 10 行升 1 级
#define TETRIS_FRAME_MS 33        // 绘制帧间隔（约 30fps）

// ==================== 下键参数 ====================
#define TETRIS_SOFT_DROP_SCORE 1    // 手动下落每格得分
#define TETRIS_HOLD_DROP_MS 30      // 按住下键时每格的下落间隔(ms)，越小越快
                                    // （20 行约 600ms 落到底，肉眼可见的连续下落）
#define TETRIS_HOLD_LOCK_DELAY_MS 0 // 触底后的固化延迟(ms)：0=立即锁定；
                                    // 设 100~150 可在触底后留一点微调时间

// ==================== 最高分持久化（Preferences / NVS）====================
#define TETRIS_NVS_NS "tetris"
#define TETRIS_NVS_KEY_HIGH "high"

// ==================== 信息面板卡片高度 ====================
// 面板共 4 张卡片（分数 / 最高分 / 消行 / 等级），压缩高度以适配 240x280 等矮屏
#define TETRIS_STAT_H 36
#define TETRIS_STAT_GAP 4

// ==================== 方块数据结构 ====================
struct TetrisPoint
{
    int x, y;
};

struct TetrisBlock
{
    TetrisPoint square[4][4]; // [旋转态][第几个格子]
    int numRotate;            // 旋转态数量
    uint8_t color;            // 颜色索引（1~7，0 = 空）
};

// ==================== 运行状态 ====================
enum TetrisState
{
    TETRIS_READY = 0, // 等待开始
    TETRIS_PLAYING,   // 游戏中
    TETRIS_PAUSED,    // 暂停
    TETRIS_GAMEOVER   // 结束
};

class Tetris
{
public:
    void Main();   // App 主入口
    void Init();   // 初始化
    void Update(); // 每帧更新
    void Exit();   // 清理

    // ---- 状态查询 ----
    TetrisState getState() const { return g_state; }
    int getScore() const { return g_score; }
    int getLines() const { return g_lines; }
    int getLevel() const { return g_level; }
    int getHighScore() const { return g_highScore; }

    // ---- 最高分持久化 ----
    void loadHighScore();   // App 进入时从 Flash 读取
    void saveHighScore();   // 写入 Flash
    void updateHighScore(); // 仅更新内存（每次固化后调用，卡片即时刷新）
    void commitHighScore(); // 更新内存并在破纪录时落盘（开局/结束/退出时调用）

protected:
    // ==================== 布局 ====================
    void layout(); // 依据画布尺寸计算格子边长与棋盘 / 面板位置

    // ==================== 游戏流程 ====================
    void resetGame(); // 清空棋盘与计分
    void startGame(); // 开新一局
    void spawn();     // 取出 next 生成当前方块
    void tick(uint32_t now);
    uint16_t fallInterval() const; // 当前等级对应的下落间隔

    // ==================== 方块操作 ====================
    bool canPlace(const TetrisBlock &b, int px, int py, int r) const;
    bool tryPlace(int nx, int ny, int nrot);
    void moveHorizontal(int dir); // -1 左 / +1 右
    void rotateBlock(int dir);    // -1 顺时针 / +1 逆时针
    void softDrop();              // 下键点按：下落一格（落不动则立即固化）
    void pollHoldDrop();          // 轮询下键：按住时持续快速下落到底
    void lockBlock();             // 固化到底并消行、生成下一个
    int clearLines();             // 返回本次消除的行数
    int ghostY() const;           // 落点预测（虚影）的 Y

    // ==================== 输入 ====================
    void handleKey(uint8_t key);
    void handleShortPress();  // OK 短按：游戏中旋转（等同编码器）
    void handleLongPress();   // OK 长按：重开一局
    void handlePauseResume(); // 上键：开始 / 暂停 / 继续

    // ==================== 绘制 ====================
    void draw();
    void drawBoard();                                                             // 棋盘 + 已固化方块 + 当前方块 + 虚影
    void drawPanel();                                                             // 右侧信息面板
    void drawNextPreview(int px, int py, int pw, int ph);                         // NEXT 预览框
    void drawOverlay();                                                           // 开始 / 暂停 / 结束提示层
    void drawCell(int gx, int gy, uint8_t colorIdx);                              // 按格坐标画一个方块
    static void drawUnit(LGFX_Sprite *s, int x, int y, int size, uint16_t color); // 带立体边的方块

private:
    LGFX_Sprite *canvas = nullptr;

    // ---- 布局缓存 ----
    int g_cell = TETRIS_CELL_MAX;
    int g_boardX = 0, g_boardY = 0, g_boardW = 0, g_boardH = 0;
    int g_panelX = 0, g_panelW = 0;

    // ---- 棋盘：0 = 空，1~7 = 颜色索引 ----
    uint8_t g_grid[TETRIS_COLS][TETRIS_ROWS];

    // ---- 当前 / 下一个方块 ----
    TetrisBlock g_block;
    TetrisBlock g_next;
    int g_bx = 0, g_by = 0, g_rot = 0;

    // ---- 运行状态 ----
    TetrisState g_state = TETRIS_READY;
    int g_score = 0, g_lines = 0, g_level = 1;
    uint32_t g_lastFall = 0, g_lastDraw = 0;
    bool g_dirty = true; // 需要重绘

    // ---- 最高分 ----
    int g_highScore = 0;      // 历史最高分（存于 Flash，App 进入时载入）
    bool g_highDirty = false; // 已破纪录但尚未落盘

    // ---- 按住下键持续下落 ----
    bool g_holdActive = false;    // 下键是否处于按住状态（边沿检测用）
    bool g_holdLocked = false;    // 本块已因按住而落定，需松手后才能继续触发
    uint32_t g_lastDropStep = 0;  // 上一次加速下落的时刻
    uint32_t g_touchBottomAt = 0; // 触底时刻，用于 TETRIS_HOLD_LOCK_DELAY_MS 延迟锁定
};

extern Tetris g_tetris;

#endif // _TETRIS_H_
