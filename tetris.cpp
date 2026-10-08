#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <string>
#include <vector>

// ============================================================
// 布局与尺寸
// ============================================================
static constexpr int BOARD_W = 10;   // 棋盘列数
static constexpr int BOARD_H = 20;   // 棋盘行数
static constexpr int CELL = 30;      // 每格像素
static constexpr int BOARD_X = 25;
static constexpr int BOARD_Y = 25;
static constexpr int PANEL_X = 345;  // 右侧信息栏起点
static constexpr int WIN_W = 570;
static constexpr int WIN_H = 650;

static constexpr int RANK_SIZE = 5;
static constexpr const char* SCORE_FILE = "high_score.txt";
static constexpr const char* RANK_FILE = "leaderboard.txt";

// ============================================================
// 颜色
// ============================================================
static const Color kBg = {18, 18, 24, 255};
static const Color kBoardBg = {11, 11, 16, 255};
static const Color kGrid = {30, 30, 40, 255};
static const Color kText = {226, 226, 232, 255};
static const Color kTextDim = {128, 128, 144, 255};
static const Color kGold = {240, 200, 70, 255};
static const Color kSelect = {52, 84, 158, 255};

// 形状编号沿用原控制台版：0=T 1=J 2=L 3=S 4=Z 5=O 6=I
static const Color kShapeColor[7] = {
    {176, 80, 225, 255},   // T
    {52, 92, 232, 255},    // J
    {238, 142, 38, 255},   // L
    {60, 185, 80, 255},    // S
    {228, 68, 72, 255},    // Z
    {232, 205, 55, 255},   // O
    {45, 198, 218, 255}    // I
};

static Color Shade(Color c, float f)
{
    return Color{
        (unsigned char)std::clamp((int)(c.r * f), 0, 255),
        (unsigned char)std::clamp((int)(c.g * f), 0, 255),
        (unsigned char)std::clamp((int)(c.b * f), 0, 255),
        c.a};
}

// 带高光和暗边的立体方块
static void DrawCell(int x, int y, int sz, Color c)
{
    DrawRectangle(x, y, sz, sz, c);
    int b = sz >= 25 ? 3 : 2;
    DrawRectangle(x + 1, y + 1, sz - 2, b, Shade(c, 1.35f));
    DrawRectangle(x + 1, y + 1, b, sz - 2, Shade(c, 1.35f));
    DrawRectangle(x + 1, y + sz - 1 - b, sz - 2, b, Shade(c, 0.55f));
    DrawRectangle(x + sz - 1 - b, y + 1, b, sz - 2, Shade(c, 0.55f));
}

// 落点影子（半透明填充 + 描边）
static void DrawGhostCell(int x, int y, int sz, Color c)
{
    DrawRectangle(x + 1, y + 1, sz - 2, sz - 2, Color{c.r, c.g, c.b, 40});
    DrawRectangleLinesEx(Rectangle{x + 1.0f, y + 1.0f, sz - 2.0f, sz - 2.0f},
                         1.5f, Color{c.r, c.g, c.b, 150});
}

// ============================================================
// 中文字体
// ============================================================
// 所有界面静态文案，码点收集器会从中提取全部汉字
static const char* kUiTexts[] = {
    "俄罗斯方块",
    "开始游戏", "排行榜", "退出游戏",
    "↑↓ 选择，回车确认，也可点击鼠标",
    "选择难度", "模式", "简单", "普通", "困难",
    "适合新手，下落较慢", "标准速度", "高速挑战",
    "↑↓ 选择，回车确认，Esc 返回",
    "第 ", " 名    ", " 分", "---",
    "按任意键或点击鼠标返回",
    "下一个", "当前分数", "最高纪录", "当前等级", "消除行数",
    "← → 移动", "↓ 加速", "↑/空格 旋转", "P 暂停", "R 重开", "Esc 菜单",
    "难度：",
    "游戏暂停", "按 P 继续", "Esc 返回主菜单",
    "游戏结束", "本局得分：", "成绩已记入排行榜",
    "R 重新开始", "Esc 返回主菜单"};

static std::vector<int> CollectCodePoints()
{
    std::set<int> s;
    for (int c = 32; c <= 126; ++c) s.insert(c);
    static const int extra[] = {
        0x2190, 0x2191, 0x2192, 0x2193,   // ← ↑ → ↓
        0xFF0C, 0xFF1A,                    // ， ：
        0x2014};                           // —
    for (int c : extra) s.insert(c);

    for (const char* t : kUiTexts)
    {
        const unsigned char* p = (const unsigned char*)t;
        while (*p)
        {
            int cp = 0;
            if (*p < 0x80) { cp = *p; ++p; }
            else if ((*p & 0xE0) == 0xC0)
            {
                cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
                p += 2;
            }
            else if ((*p & 0xF0) == 0xE0)
            {
                cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
                p += 3;
            }
            else if ((*p & 0xF8) == 0xF0)
            {
                cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) |
                     ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
                p += 4;
            }
            else ++p;
            s.insert(cp);
        }
    }

    return std::vector<int>(s.begin(), s.end());
}

static Font fontUI;
static Font fontBig;

static void TextCenter(Font f, const char* text, float cx, float y, float size, Color c)
{
    Vector2 m = MeasureTextEx(f, text, size, 1.0f);
    DrawTextEx(f, text, Vector2{cx - m.x / 2, y}, size, 1.0f, c);
}

// ============================================================
// 方块形状表（由 0 号形态旋转生成，与原控制台版一致）
// ============================================================
struct Piece { int s[4][4]; };
static Piece g_piece[7][4];

static void InitPieces()
{
    for (int sh = 0; sh < 7; ++sh)
        for (int f = 0; f < 4; ++f)
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    g_piece[sh][f].s[i][j] = 0;

    for (int i = 0; i <= 2; ++i) g_piece[0][0].s[1][i] = 1;
    g_piece[0][0].s[2][1] = 1;

    for (int i = 1; i <= 3; ++i)
    {
        g_piece[1][0].s[i][1] = 1;
        g_piece[2][0].s[i][2] = 1;
    }
    g_piece[1][0].s[3][2] = 1;
    g_piece[2][0].s[3][1] = 1;

    for (int i = 0; i <= 1; ++i)
    {
        g_piece[3][0].s[1][i] = 1;
        g_piece[3][0].s[2][i + 1] = 1;
        g_piece[4][0].s[1][i + 1] = 1;
        g_piece[4][0].s[2][i] = 1;
        g_piece[5][0].s[1][i + 1] = 1;
        g_piece[5][0].s[2][i + 1] = 1;
    }
    for (int i = 0; i <= 3; ++i) g_piece[6][0].s[1][i] = 1;

    for (int sh = 0; sh < 7; ++sh)
        for (int f = 0; f < 3; ++f)
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                    g_piece[sh][f + 1].s[i][j] = g_piece[sh][f].s[3 - j][i];
}

// ============================================================
// 棋盘（纯数据，不涉及任何绘制）
// ============================================================
class Board
{
public:
    int cell[BOARD_H][BOARD_W];

    Board() { reset(); }

    void reset()
    {
        for (int i = 0; i < BOARD_H; ++i)
            for (int j = 0; j < BOARD_W; ++j)
                cell[i][j] = -1;
    }

    bool legal(int shape, int form, int bx, int by) const
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                if (g_piece[shape][form].s[i][j] != 1) continue;
                int x = bx + j;
                int y = by + i;
                if (x < 0 || x >= BOARD_W || y >= BOARD_H) return false;
                if (y < 0) continue;
                if (cell[y][x] != -1) return false;
            }
        }
        return true;
    }

    void lockPiece(int shape, int form, int bx, int by)
    {
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                if (g_piece[shape][form].s[i][j] == 1)
                {
                    int y = by + i, x = bx + j;
                    if (y >= 0 && y < BOARD_H && x >= 0 && x < BOARD_W)
                        cell[y][x] = shape;
                }
    }

    std::vector<int> fullRows() const
    {
        std::vector<int> rows;
        for (int y = 0; y < BOARD_H; ++y)
        {
            bool full = true;
            for (int x = 0; x < BOARD_W; ++x)
                if (cell[y][x] == -1) { full = false; break; }
            if (full) rows.push_back(y);
        }
        return rows;
    }

    // 按行号从大到小删除，保证多行同时消除正确
    void removeRows(const std::vector<int>& rows)
    {
        for (int k = (int)rows.size() - 1; k >= 0; --k)
        {
            int r = rows[k];
            for (int y = r; y > 0; --y)
                for (int x = 0; x < BOARD_W; ++x)
                    cell[y][x] = cell[y - 1][x];
            for (int x = 0; x < BOARD_W; ++x) cell[0][x] = -1;
        }
    }
};

// ============================================================
// 难度
// ============================================================
struct Difficulty
{
    const char* name;
    const char* desc;
    float initial;   // 初始下落间隔（秒）
    float step;      // 每级减少量
    float minimum;   // 最小间隔
    int linesPerLevel;
};

static const Difficulty kDiff[3] = {
    {"简单", "适合新手，下落较慢", 0.280f, 0.020f, 0.080f, 15},
    {"普通", "标准速度", 0.140f, 0.010f, 0.040f, 10},
    {"困难", "高速挑战", 0.093f, 0.007f, 0.027f, 8}};

// ============================================================
// 排行榜（与原控制台版逻辑一致）
// ============================================================
class Leaderboard
{
    int scores_[RANK_SIZE] = {};

public:
    int at(int i) const { return scores_[i]; }

    void add(int score)
    {
        std::vector<int> all;
        for (int i = 0; i < RANK_SIZE; ++i) all.push_back(scores_[i]);
        all.push_back(score);
        std::sort(all.begin(), all.end(), std::greater<int>());
        for (int i = 0; i < RANK_SIZE; ++i) scores_[i] = all[i];
    }

    void load()
    {
        std::ifstream in(RANK_FILE);
        for (int i = 0; i < RANK_SIZE; ++i)
            if (!(in >> scores_[i])) scores_[i] = 0;
    }

    void save() const
    {
        std::ofstream out(RANK_FILE);
        for (int i = 0; i < RANK_SIZE; ++i) out << scores_[i] << '\n';
    }
};

// ============================================================
// 游戏（状态机 + 各界面更新与绘制）
// ============================================================
enum Phase
{
    PH_MENU,
    PH_DIFFICULTY,
    PH_RANK,
    PH_PLAYING,
    PH_PAUSED,
    PH_CLEARING,
    PH_GAMEOVER
};

class Game
{
public:
    Board board;
    Leaderboard ranks;
    Phase phase = PH_MENU;
    bool wantExit = false;

    int menuSel = 0;
    int diffSel = 1;

    int shape = 0, form = 0, px = 3, py = -1;
    int nextShape = 0, nextForm = 0;
    int diffIdx = 1;

    int level = 1;
    int totalLines = 0;
    int score = 0;
    int high = 0;

    float dropTimer = 0.0f;
    float repL = 0.0f, repR = 0.0f, repD = 0.0f;
    bool holdL = false, holdR = false, holdD = false;

    std::vector<int> clearing;
    float clearTimer = 0.0f;

    Rectangle menuBounds[3];
    Rectangle diffBounds[3];

    float dropInterval() const
    {
        const Difficulty& d = kDiff[diffIdx];
        return std::max(d.minimum, d.initial - (level - 1) * d.step);
    }

    void saveHigh()
    {
        std::ofstream out(SCORE_FILE);
        if (out) out << high;
    }

    void startGame(int idx)
    {
        diffIdx = idx;
        board.reset();
        score = 0;
        level = 1;
        totalLines = 0;
        dropTimer = 0.0f;
        holdL = holdR = holdD = false;
        repL = repR = repD = 0.0f;
        clearing.clear();
        nextShape = GetRandomValue(0, 6);
        nextForm = GetRandomValue(0, 3);
        spawn();
        phase = PH_PLAYING;
    }

    void spawn()
    {
        shape = nextShape;
        form = nextForm;
        nextShape = GetRandomValue(0, 6);
        nextForm = GetRandomValue(0, 3);
        px = 3;
        py = -1;
        dropTimer = 0.0f;
        if (!board.legal(shape, form, px, py)) endGame();
    }

    void endGame()
    {
        ranks.add(score);
        ranks.save();
        high = std::max(high, score);
        saveHigh();
        phase = PH_GAMEOVER;
    }

    bool moveBy(int dx, int dy)
    {
        if (!board.legal(shape, form, px + dx, py + dy)) return false;
        px += dx;
        py += dy;
        return true;
    }

    // 带简单踢墙的旋转
    void rotate()
    {
        int nf = (form + 1) % 4;
        static const int ox[] = {0, -1, 1, -2, 2};
        for (int oy = 0; oy >= -1; --oy)
            for (int k = 0; k < 5; ++k)
                if (board.legal(shape, nf, px + ox[k], py + oy))
                {
                    px += ox[k];
                    py += oy;
                    form = nf;
                    return;
                }
    }

    void beginLock()
    {
        board.lockPiece(shape, form, px, py);
        std::vector<int> rows = board.fullRows();
        if (!rows.empty())
        {
            clearing = rows;
            clearTimer = 0.0f;
            phase = PH_CLEARING;
        }
        else spawn();
    }

    void stepDown()
    {
        if (!moveBy(0, 1)) beginLock();
    }

    int shadowY() const
    {
        int sy = py;
        while (board.legal(shape, form, px, sy + 1)) ++sy;
        return sy;
    }

    void addLines(int n)
    {
        if (n <= 0) return;
        score += n * 10;
        totalLines += n;
        level = totalLines / kDiff[diffIdx].linesPerLevel + 1;
        high = std::max(high, score);
    }

    // --------------------------------------------------------
    // 更新
    // --------------------------------------------------------
    void update(float dt)
    {
        switch (phase)
        {
        case PH_MENU: updateMenu(); break;
        case PH_DIFFICULTY: updateDifficulty(); break;
        case PH_RANK: updateRank(); break;
        case PH_PLAYING: updatePlaying(dt); break;
        case PH_PAUSED: updatePaused(); break;
        case PH_CLEARING: updateClearing(dt); break;
        case PH_GAMEOVER: updateOver(); break;
        }
    }

    void updateMenu()
    {
        Vector2 m = GetMousePosition();
        for (int i = 0; i < 3; ++i)
            if (CheckCollisionPointRec(m, menuBounds[i])) menuSel = i;
        if (IsKeyPressed(KEY_UP) && menuSel > 0) --menuSel;
        if (IsKeyPressed(KEY_DOWN) && menuSel < 2) ++menuSel;
        if (IsKeyPressed(KEY_ENTER)) chooseMenu(menuSel);
        if (IsKeyPressed(KEY_ESCAPE)) wantExit = true;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            for (int i = 0; i < 3; ++i)
                if (CheckCollisionPointRec(m, menuBounds[i])) chooseMenu(i);
    }

    void chooseMenu(int i)
    {
        if (i == 0) phase = PH_DIFFICULTY;
        else if (i == 1) phase = PH_RANK;
        else wantExit = true;
    }

    void updateDifficulty()
    {
        Vector2 m = GetMousePosition();
        for (int i = 0; i < 3; ++i)
            if (CheckCollisionPointRec(m, diffBounds[i])) diffSel = i;
        if (IsKeyPressed(KEY_UP) && diffSel > 0) --diffSel;
        if (IsKeyPressed(KEY_DOWN) && diffSel < 2) ++diffSel;
        if (IsKeyPressed(KEY_ENTER)) startGame(diffSel);
        if (IsKeyPressed(KEY_ESCAPE)) phase = PH_MENU;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            for (int i = 0; i < 3; ++i)
                if (CheckCollisionPointRec(m, diffBounds[i])) startGame(i);
    }

    void updateRank()
    {
        if (GetKeyPressed() != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            phase = PH_MENU;
    }

    void updatePaused()
    {
        if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_S)) phase = PH_PLAYING;
        if (IsKeyPressed(KEY_ESCAPE))
        {
            saveHigh();
            phase = PH_MENU;
        }
    }

    void updateOver()
    {
        if (IsKeyPressed(KEY_R)) startGame(diffIdx);
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_ESCAPE)) phase = PH_MENU;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) phase = PH_MENU;
    }

    void updatePlaying(float dt)
    {
        if (IsKeyPressed(KEY_ESCAPE))
        {
            saveHigh();
            phase = PH_MENU;
            return;
        }
        if (IsKeyPressed(KEY_P) || IsKeyPressed(KEY_S))
        {
            phase = PH_PAUSED;
            return;
        }
        if (IsKeyPressed(KEY_R))
        {
            startGame(diffIdx);
            return;
        }

        // 左右移动（带长按重复）
        if (IsKeyPressed(KEY_LEFT)) { moveBy(-1, 0); holdL = true; repL = 0.0f; }
        if (IsKeyReleased(KEY_LEFT)) holdL = false;
        if (holdL && IsKeyDown(KEY_LEFT))
        {
            repL += dt;
            if (repL >= 0.16f) { moveBy(-1, 0); repL -= 0.05f; }
        }
        if (IsKeyPressed(KEY_RIGHT)) { moveBy(1, 0); holdR = true; repR = 0.0f; }
        if (IsKeyReleased(KEY_RIGHT)) holdR = false;
        if (holdR && IsKeyDown(KEY_RIGHT))
        {
            repR += dt;
            if (repR >= 0.16f) { moveBy(1, 0); repR -= 0.05f; }
        }

        // 软降
        if (IsKeyPressed(KEY_DOWN)) { stepDown(); holdD = true; repD = 0.0f; }
        if (IsKeyReleased(KEY_DOWN)) holdD = false;
        if (holdD && IsKeyDown(KEY_DOWN))
        {
            repD += dt;
            if (repD >= 0.05f) { stepDown(); repD -= 0.05f; }
        }

        // 旋转（↑ 或空格，保留原空格习惯）
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_SPACE)) rotate();

        if (phase != PH_PLAYING) return;

        // 自然下落
        dropTimer += dt;
        if (dropTimer >= dropInterval())
        {
            dropTimer = 0.0f;
            stepDown();
        }
    }

    void updateClearing(float dt)
    {
        clearTimer += dt;
        if (clearTimer >= 0.42f)
        {
            board.removeRows(clearing);
            addLines((int)clearing.size());
            clearing.clear();
            spawn();
            if (phase == PH_CLEARING) phase = PH_PLAYING;
        }
    }

    // --------------------------------------------------------
    // 绘制
    // --------------------------------------------------------
    void draw()
    {
        DrawRectangle(0, 0, WIN_W, WIN_H, kBg);
        switch (phase)
        {
        case PH_MENU: drawMenu(); break;
        case PH_DIFFICULTY: drawDifficulty(); break;
        case PH_RANK: drawRank(); break;
        default:
            drawGame();
            if (phase == PH_PAUSED) drawPause();
            if (phase == PH_GAMEOVER) drawGameOverScreen();
            break;
        }
    }

    void drawMenu()
    {
        TextCenter(fontBig, "俄罗斯方块", WIN_W / 2.0f, 110, 60, kGold);
        const char* items[3] = {"开始游戏", "排行榜", "退出游戏"};
        for (int i = 0; i < 3; ++i)
        {
            Rectangle b = Rectangle{155, 260 + i * 70.0f, 260, 56};
            menuBounds[i] = b;
            bool on = i == menuSel;
            if (on)
            {
                DrawRectangleRounded(b, 0.3f, 8, kSelect);
                DrawRectangleRoundedLinesEx(b, 0.3f, 8, 2.0f, Shade(kSelect, 1.4f));
            }
            TextCenter(fontUI, items[i], WIN_W / 2.0f, b.y + 12, 28, on ? WHITE : kTextDim);
        }
        TextCenter(fontUI, "↑↓ 选择，回车确认，也可点击鼠标", WIN_W / 2.0f, 590, 18, kTextDim);
    }

    void drawDifficulty()
    {
        TextCenter(fontBig, "选择难度", WIN_W / 2.0f, 80, 46, kGold);
        for (int i = 0; i < 3; ++i)
        {
            Rectangle b = Rectangle{95, 200 + i * 100.0f, 380, 78};
            diffBounds[i] = b;
            bool on = i == diffSel;
            if (on)
            {
                DrawRectangleRounded(b, 0.18f, 8, kSelect);
                DrawRectangleRoundedLinesEx(b, 0.18f, 8, 2.0f, Shade(kSelect, 1.4f));
            }
            std::string name = std::string(kDiff[i].name) + "模式";
            TextCenter(fontUI, name.c_str(), WIN_W / 2.0f, b.y + 10, 26, on ? WHITE : kText);
            TextCenter(fontUI, kDiff[i].desc, WIN_W / 2.0f, b.y + 44, 17, on ? kText : kTextDim);
        }
        TextCenter(fontUI, "↑↓ 选择，回车确认，Esc 返回", WIN_W / 2.0f, 600, 18, kTextDim);
    }

    void drawRank()
    {
        TextCenter(fontBig, "排行榜", WIN_W / 2.0f, 65, 46, kGold);
        for (int i = 0; i < RANK_SIZE; ++i)
        {
            float y = 155 + i * 80.0f;
            if (ranks.at(i) > 0)
            {
                std::string s = "第 " + std::to_string(i + 1) + " 名    " +
                                std::to_string(ranks.at(i)) + " 分";
                TextCenter(fontUI, s.c_str(), WIN_W / 2.0f, y, 28, i == 0 ? kGold : kText);
            }
            else
            {
                std::string s = "第 " + std::to_string(i + 1) + " 名    ---";
                TextCenter(fontUI, s.c_str(), WIN_W / 2.0f, y, 28, kTextDim);
            }
        }
        TextCenter(fontUI, "按任意键或点击鼠标返回", WIN_W / 2.0f, 600, 18, kTextDim);
    }

    void drawGame()
    {
        // 棋盘外框与背景
        DrawRectangle(BOARD_X - 4, BOARD_Y - 4, BOARD_W * CELL + 8, BOARD_H * CELL + 8,
                      Color{42, 42, 56, 255});
        DrawRectangle(BOARD_X, BOARD_Y, BOARD_W * CELL, BOARD_H * CELL, kBoardBg);
        for (int x = 1; x < BOARD_W; ++x)
            DrawRectangle(BOARD_X + x * CELL, BOARD_Y, 1, BOARD_H * CELL, kGrid);
        for (int y = 1; y < BOARD_H; ++y)
            DrawRectangle(BOARD_X, BOARD_Y + y * CELL, BOARD_W * CELL, 1, kGrid);

        // 已锁定方块
        for (int y = 0; y < BOARD_H; ++y)
        {
            for (int x = 0; x < BOARD_W; ++x)
            {
                if (board.cell[y][x] == -1) continue;
                Color c = kShapeColor[board.cell[y][x]];
                if (phase == PH_CLEARING &&
                    std::find(clearing.begin(), clearing.end(), y) != clearing.end())
                {
                    if (std::fmod(clearTimer, 0.14f) < 0.07f) c = WHITE;
                }
                DrawCell(BOARD_X + x * CELL, BOARD_Y + y * CELL, CELL, c);
            }
        }

        // 活动方块与影子
        if (phase == PH_PLAYING || phase == PH_PAUSED)
        {
            int sy = shadowY();
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                {
                    if (g_piece[shape][form].s[i][j] != 1) continue;
                    DrawGhostCell(BOARD_X + (px + j) * CELL,
                                  BOARD_Y + (sy + i) * CELL, CELL, kShapeColor[shape]);
                }
            for (int i = 0; i < 4; ++i)
                for (int j = 0; j < 4; ++j)
                {
                    if (g_piece[shape][form].s[i][j] != 1) continue;
                    int by = py + i;
                    if (by < 0) continue;
                    DrawCell(BOARD_X + (px + j) * CELL,
                             BOARD_Y + by * CELL, CELL, kShapeColor[shape]);
                }
        }

        drawPanel();
    }

    void drawPanel()
    {
        float x = (float)PANEL_X;

        DrawTextEx(fontUI, "下一个", Vector2{x, 18}, 22, 1.0f, kText);

        Rectangle pv = Rectangle{x + 5, 48, 190, 100};
        DrawRectangleRounded(pv, 0.08f, 8, Color{26, 26, 36, 255});
        DrawRectangleRoundedLinesEx(pv, 0.08f, 8, 2.0f, Color{52, 52, 70, 255});
        drawNext(pv);

        statBlock(x, 165, "当前分数", score);
        statBlock(x, 245, "最高纪录", high);
        statBlock(x, 325, "当前等级", level);
        statBlock(x, 405, "消除行数", totalLines);

        const char* help[] = {"← → 移动", "↓ 加速", "↑/空格 旋转",
                              "P 暂停", "R 重开", "Esc 菜单"};
        for (int i = 0; i < 6; ++i)
            DrawTextEx(fontUI, help[i], Vector2{x, 475.0f + i * 23.0f}, 17, 1.0f, kTextDim);

        std::string d = "难度：" + std::string(kDiff[diffIdx].name);
        DrawTextEx(fontUI, d.c_str(), Vector2{x, 618}, 19, 1.0f, kGold);
    }

    void statBlock(float x, int y, const char* label, int value)
    {
        DrawTextEx(fontUI, label, Vector2{x, (float)y}, 18, 1.0f, kTextDim);
        DrawTextEx(fontUI, std::to_string(value).c_str(),
                   Vector2{x, (float)y + 24}, 26, 1.0f, kText);
    }

    void drawNext(Rectangle pv)
    {
        int minR = 4, maxR = -1, minC = 4, maxC = -1;
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                if (g_piece[nextShape][nextForm].s[i][j] == 1)
                {
                    minR = std::min(minR, i);
                    maxR = std::max(maxR, i);
                    minC = std::min(minC, j);
                    maxC = std::max(maxC, j);
                }
        const int gs = 22;
        float w = (maxC - minC + 1) * gs;
        float h = (maxR - minR + 1) * gs;
        float ox = pv.x + (pv.width - w) / 2;
        float oy = pv.y + (pv.height - h) / 2;
        for (int i = minR; i <= maxR; ++i)
            for (int j = minC; j <= maxC; ++j)
                if (g_piece[nextShape][nextForm].s[i][j] == 1)
                    DrawCell((int)(ox + (j - minC) * gs),
                             (int)(oy + (i - minR) * gs), gs, kShapeColor[nextShape]);
    }

    void drawPause()
    {
        DrawRectangle(0, 0, WIN_W, WIN_H, Color{0, 0, 0, 160});
        TextCenter(fontBig, "游戏暂停", WIN_W / 2.0f, 250, 52, kText);
        TextCenter(fontUI, "按 P 继续", WIN_W / 2.0f, 330, 22, kText);
        TextCenter(fontUI, "Esc 返回主菜单", WIN_W / 2.0f, 375, 20, kTextDim);
    }

    void drawGameOverScreen()
    {
        DrawRectangle(0, 0, WIN_W, WIN_H, Color{0, 0, 0, 170});
        TextCenter(fontBig, "游戏结束", WIN_W / 2.0f, 170, 52, Color{235, 95, 95, 255});
        std::string s = "本局得分：" + std::to_string(score);
        TextCenter(fontUI, s.c_str(), WIN_W / 2.0f, 260, 28, kText);
        TextCenter(fontUI, "成绩已记入排行榜", WIN_W / 2.0f, 310, 19, kTextDim);
        TextCenter(fontUI, "R 重新开始", WIN_W / 2.0f, 400, 22, kText);
        TextCenter(fontUI, "Esc 返回主菜单", WIN_W / 2.0f, 445, 20, kTextDim);
    }
};

// ============================================================
// 主程序
// ============================================================
int main()
{
    InitWindow(WIN_W, WIN_H, "俄罗斯方块");
    SetTargetFPS(60);

    InitPieces();

    std::vector<int> cp = CollectCodePoints();
    // 注意：raylib 6.0 的 LoadFontEx 对 msyh.ttc 等 TrueType Collection 支持异常，
    // 会回退成默认 ASCII 字形，中文显示为问号，所以这里使用单一 TTF 的黑体。
    const char* fontPath = "C:\\Windows\\Fonts\\simhei.ttf";
    fontUI = LoadFontEx(fontPath, 26, cp.data(), (int)cp.size());
    fontBig = LoadFontEx(fontPath, 56, cp.data(), (int)cp.size());
    if (fontUI.texture.id == 0 || fontBig.texture.id == 0)
    {
        fontUI = GetFontDefault();
        fontBig = GetFontDefault();
    }

    Game game;
    std::ifstream in(SCORE_FILE);
    int h = 0;
    if (in >> h) game.high = std::max(0, h);
    game.ranks.load();

    while (!WindowShouldClose() && !game.wantExit)
    {
        float dt = GetFrameTime();
        game.update(dt);
        BeginDrawing();
        game.draw();
        EndDrawing();
    }

    game.saveHigh();
    UnloadFont(fontUI);
    UnloadFont(fontBig);
    CloseWindow();
    return 0;
}
