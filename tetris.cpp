#include <Windows.h>
#include <algorithm>
#include <array>
#include <conio.h>
#include <fstream>
#include <iostream>
#include <random>

using namespace std;

namespace tetris
{
constexpr int kRows = 29, kColumns = 20, kPieceSize = 4, kShapeCount = 7, kRotationCount = 4;
constexpr char kScoreFile[] = "high_score.txt";
constexpr char kRankFile[] = "leaderboard.txt";
constexpr int kRankSize = 5;
using Grid = array<array<unsigned char, 4>, 4>;
enum class Shape : unsigned char
{
    T,
    L,
    J,
    Z,
    S,
    O,
    I
};
enum class Rotation : unsigned char
{
    R0,
    R90,
    R180,
    R270
};
struct Piece
{
    Shape shape;
    Rotation rotation;
};
struct Cell
{
    bool occupied = false;
    Shape shape = Shape::T;
};
struct Difficulty
{
    const char *name;
    int initialInterval;
    int intervalStep;
    int minimumInterval;
    int linesPerLevel;
};
constexpr int index(Shape s)
{
    return static_cast<int>(s);
}
constexpr int index(Rotation r)
{
    return static_cast<int>(r);
}

class PieceSet
{
    array<array<Grid, 4>, 7> cells_{};
    static Grid rotate(const Grid &source)
    {
        Grid result{};
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                result[r][c] = source[3 - c][r];
        return result;
    }

  public:
    PieceSet()
    {
        auto &t = cells_[index(Shape::T)][0];
        t[1][0] = t[1][1] = t[1][2] = t[2][1] = 1;
        auto &l = cells_[index(Shape::L)][0];
        for (int r = 1; r <= 3; ++r)
            l[r][1] = 1;
        l[3][2] = 1;
        auto &j = cells_[index(Shape::J)][0];
        for (int r = 1; r <= 3; ++r)
            j[r][2] = 1;
        j[3][1] = 1;
        auto &z = cells_[index(Shape::Z)][0];
        z[1][0] = z[1][1] = z[2][1] = z[2][2] = 1;
        auto &s = cells_[index(Shape::S)][0];
        s[1][1] = s[1][2] = s[2][0] = s[2][1] = 1;
        auto &o = cells_[index(Shape::O)][0];
        o[1][1] = o[1][2] = o[2][1] = o[2][2] = 1;
        auto &i = cells_[index(Shape::I)][0];
        for (int r = 0; r < 4; ++r)
            i[r][1] = 1;
        for (int sh = 0; sh < 7; ++sh)
            for (int ro = 1; ro < 4; ++ro)
                cells_[sh][ro] = rotate(cells_[sh][ro - 1]);
    }
    const Grid &cells(Piece p) const
    {
        return cells_[index(p.shape)][index(p.rotation)];
    }
    int color(Shape s) const
    {
        switch (s)
        {
        case Shape::T:
            return 13;
        case Shape::L:
        case Shape::J:
            return 12;
        case Shape::Z:
        case Shape::S:
            return 10;
        case Shape::O:
            return 14;
        case Shape::I:
            return 11;
        }
        return 7;
    }
};

class Board
{
    array<array<Cell, kColumns>, kRows> cells_{};

  public:
    Board()
    {
        reset();
    }
    void reset()
    {
        for (auto &row : cells_)
            for (auto &cell : row)
                cell = {};
        for (int r = 0; r < kRows; ++r)
        {
            cells_[r][0].occupied = true;
            cells_[r][kColumns - 1].occupied = true;
        }
        for (auto &cell : cells_[kRows - 1])
            cell.occupied = true;
    }
    bool occupied(int r, int c) const
    {
        return r < 0 || r >= kRows || c < 0 || c >= kColumns || cells_[r][c].occupied;
    }
    bool canPlace(const PieceSet &ps, Piece p, int x, int y) const
    {
        const auto &g = ps.cells(p);
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                if (g[r][c] && occupied(y + r, x + c))
                    return false;
        return true;
    }
    void lock(const PieceSet &ps, Piece p, int x, int y)
    {
        const auto &g = ps.cells(p);
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                if (g[r][c] && y + r >= 0 && y + r < kRows && x + c >= 0 && x + c < kColumns)
                    cells_[y + r][x + c] = {true, p.shape};
    }
    int clearFullRows()
    {
        int cleared = 0;
        for (int r = kRows - 2; r > 0; --r)
        {
            bool full =
                all_of(cells_[r].begin() + 1, cells_[r].end() - 1, [](const Cell &c) { return c.occupied; });
            if (!full)
                continue;
            ++cleared;
            for (int m = r; m > 1; --m)
                cells_[m] = cells_[m - 1];
            cells_[1].fill({});
            ++r;
        }
        return cleared;
    }
    bool topOccupied() const
    {
        for (int c = 1; c < kColumns - 1; ++c)
            if (cells_[1][c].occupied)
                return true;
        return false;
    }
    const Cell &at(int r, int c) const
    {
        return cells_[r][c];
    }
    int shadowY(const PieceSet &ps, Piece p, int x, int y) const
    {
        int result = y;
        while (canPlace(ps, p, x, result + 1))
            ++result;
        return result;
    }
};

class ScoreBoard
{
    int current_ = 0;
    int high_ = 0;

  public:
    int current() const
    {
        return current_;
    }
    int high() const
    {
        return high_;
    }
    void load(int highScore)
    {
        high_ = max(0, highScore);
    }
    void reset()
    {
        current_ = 0;
    }
    void addLines(int lines)
    {
        current_ += lines * 10;
        high_ = max(high_, current_);
    }
};

class Leaderboard
{
    array<int, kRankSize> scores_{};

  public:
    int best() const
    {
        return scores_[0];
    }
    int at(int place) const
    {
        return scores_[place];
    }
    void add(int score)
    {
        scores_[kRankSize - 1] = score;
        for (int i = 0; i < kRankSize; ++i)
            for (int j = i + 1; j < kRankSize; ++j)
                if (scores_[j] > scores_[i])
                    swap(scores_[i], scores_[j]);
    }
    void load()
    {
        ifstream input(kRankFile);
        for (int &score : scores_)
            if (!(input >> score))
                score = 0;
    }
    void save() const
    {
        ofstream output(kRankFile);
        for (int score : scores_)
            output << score << '\n';
    }
};

struct GameState
{
    Piece current{Shape::T, Rotation::R0}, next{Shape::T, Rotation::R0};
    ScoreBoard score;
    Difficulty difficulty{"普通", 140, 10, 40, 10};
    int level = 1;
    int totalLines = 0;
    void reset(Piece a, Piece b, Difficulty selectedDifficulty)
    {
        score.reset();
        current = a;
        next = b;
        difficulty = selectedDifficulty;
        level = 1;
        totalLines = 0;
    }
    void addLines(int lines)
    {
        if (lines <= 0)
            return;
        score.addLines(lines);
        totalLines += lines;
        level = totalLines / difficulty.linesPerLevel + 1;
    }
    int dropInterval() const
    {
        int interval = difficulty.initialInterval - (level - 1) * difficulty.intervalStep;
        return max(interval, difficulty.minimumInterval);
    }
};
} // namespace tetris
using namespace tetris;
Board board;
PieceSet pieces;
GameState game;
Leaderboard leaderboard;
mt19937 rng{random_device{}()};
void ConfigureConsoleEncoding()
{
    // 源文件和界面文本使用 UTF-8；让 Windows 控制台用同一编码解释输出字节。
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}
void HideCursor()
{
    CONSOLE_CURSOR_INFO i{1, FALSE};
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &i);
}
void CursorJump(int x, int y)
{
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), COORD{(SHORT)x, (SHORT)y});
}
void SetColor(int c)
{
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), (WORD)c);
}
void DrawPiece(Piece p, int x, int y, int colorOverride = -1)
{
    const auto &g = pieces.cells(p);
    SetColor(colorOverride < 0 ? pieces.color(p.shape) : colorOverride);
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            if (g[r][c])
            {
                CursorJump(2 * (x + c), y + r);
                cout << "■";
            }
}
void ErasePiece(Piece p, int x, int y)
{
    const auto &g = pieces.cells(p);
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            if (g[r][c])
            {
                CursorJump(2 * (x + c), y + r);
                cout << "  ";
            }
}
void ClearPreviewArea(int x, int y)
{
    SetColor(7);
    for (int r = 0; r < kPieceSize; ++r)
        for (int c = 0; c < kPieceSize; ++c)
        {
            CursorJump(2 * (x + c), y + r);
            cout << "  ";
        }
}
void DrawBoard()
{
    for (int r = 0; r < kRows; ++r)
        for (int c = 0; c < kColumns; ++c)
            if (board.at(r, c).occupied)
            {
                CursorJump(2 * c, r);
                SetColor(r == kRows - 1 || c == 0 || c == kColumns - 1 ? 7 : pieces.color(board.at(r, c).shape));
                cout << "■";
            }
}
Piece RandomPiece()
{
    return {static_cast<Shape>(rng() % kShapeCount), static_cast<Rotation>(rng() % kRotationCount)};
}
void ReadGrade()
{
    ifstream in(kScoreFile);
    int highScore = 0;
    if (in >> highScore)
        game.score.load(highScore);
    else
    {
        game.score.load(0);
        ofstream(kScoreFile) << 0;
    }
}
void WriteGrade()
{
    ofstream out(kScoreFile);
    if (out)
        out << game.score.high();
}
int ReadMenuKey()
{
    int key = getch();
    if (key == 0 || key == 224)
        key = getch();
    return key;
}
Difficulty SelectDifficulty()
{
    Difficulty options[] = {
        {"简单", 280, 20, 80, 15},
        {"普通", 140, 10, 40, 10},
        {"困难", 93, 7, 27, 8}};
    int selected = 1;
    while (true)
    {
        system("cls");
        SetColor(14);
        CursorJump(24, 7);
        cout << "选择难度";
        for (int i = 0; i < 3; ++i)
        {
            CursorJump(24, 11 + i * 2);
            SetColor(i == selected ? 11 : 7);
            cout << (i == selected ? "> " : "  ") << options[i].name << "模式";
        }
        SetColor(8);
        CursorJump(17, 20);
        cout << "上下键选择，回车确认，Esc返回";
        int key = ReadMenuKey();
        if (key == 72 && selected > 0)
            --selected;
        else if (key == 80 && selected < 2)
            ++selected;
        else if (key == 13)
            return options[selected];
        else if (key == 27)
            return options[1];
    }
}
void ShowLeaderboard()
{
    system("cls");
    SetColor(14);
    CursorJump(24, 6);
    cout << "排行榜";
    SetColor(7);
    for (int i = 0; i < kRankSize; ++i)
    {
        CursorJump(22, 10 + i * 2);
        if (leaderboard.at(i) > 0)
            cout << "第" << i + 1 << "名：" << leaderboard.at(i) << " 分";
        else
            cout << "第" << i + 1 << "名：---";
    }
    SetColor(8);
    CursorJump(19, 22);
    cout << "按任意键返回主菜单";
    getch();
}
int MainMenu()
{
    int selected = 0;
    while (true)
    {
        system("cls");
        SetColor(14);
        CursorJump(22, 7);
        cout << "俄罗斯方块";
        const char *items[] = {"开始游戏", "排行榜", "退出游戏"};
        for (int i = 0; i < 3; ++i)
        {
            CursorJump(24, 12 + i * 2);
            SetColor(i == selected ? 11 : 7);
            cout << (i == selected ? "> " : "  ") << items[i];
        }
        SetColor(8);
        CursorJump(18, 20);
        cout << "上下键选择，回车确认";
        int key = ReadMenuKey();
        if (key == 72 && selected > 0)
            --selected;
        else if (key == 80 && selected < 2)
            ++selected;
        else if (key == 13)
            return selected;
        else if (key == 27)
            return 2;
    }
}
void InitInterface()
{
    system("cls");
    SetColor(7);
    DrawBoard();
    CursorJump(2 * kColumns, 1);
    cout << "下一个方块：";
    CursorJump(2 * kColumns + 4, kRows - 19);
    cout << "左移：←";
    CursorJump(2 * kColumns + 4, kRows - 17);
    cout << "右移：→";
    CursorJump(2 * kColumns + 4, kRows - 15);
    cout << "加速：↓";
    CursorJump(2 * kColumns + 4, kRows - 13);
    cout << "旋转：空格";
    CursorJump(2 * kColumns + 4, kRows - 11);
    cout << "暂停: S";
    CursorJump(2 * kColumns + 4, kRows - 9);
    cout << "退出: Esc";
    CursorJump(2 * kColumns + 4, kRows - 7);
    cout << "重新开始:R";
    CursorJump(2 * kColumns + 4, kRows - 6);
    cout << "难度：" << game.difficulty.name;
    CursorJump(2 * kColumns + 4, kRows - 5);
    cout << "最高纪录:" << game.score.high();
    CursorJump(2 * kColumns + 4, kRows - 4);
    cout << "当前等级:" << game.level;
    CursorJump(2 * kColumns + 4, kRows - 3);
    cout << "当前分数：" << game.score.current();
}
void StartGame()
{
    game.reset(RandomPiece(), RandomPiece(), game.difficulty);
    while (true)
    {
        Piece p = game.current, n = game.next;
        int x = kColumns / 2 - 2, y = 0;
        bool restarted = false;
        DWORD lastDrop = GetTickCount();
        ClearPreviewArea(kColumns + 3, 3);
        DrawPiece(n, kColumns + 3, 3);
        while (true)
        {
            int shadowY = board.shadowY(pieces, p, x, y);
            DrawPiece(p, x, shadowY, 8);
            DrawPiece(p, x, y);
            if (GetTickCount() - lastDrop >= static_cast<DWORD>(game.dropInterval()))
            {
                lastDrop = GetTickCount();
                if (!board.canPlace(pieces, p, x, y + 1))
                {
                    board.lock(pieces, p, x, y);
                    int lines = board.clearFullRows();
                    game.addLines(lines);
                    DrawBoard();
                    if (lines)
                        InitInterface();
                    break;
                }
                ErasePiece(p, x, shadowY);
                ErasePiece(p, x, y);
                DrawBoard();
                ++y;
            }
            else if (kbhit())
            {
                int key = ReadMenuKey();
                if (key == 80 && board.canPlace(pieces, p, x, y + 1))
                {
                    ErasePiece(p, x, shadowY);
                    ErasePiece(p, x, y);
                    DrawBoard();
                    ++y;
                }
                else if (key == 75 && board.canPlace(pieces, p, x - 1, y))
                {
                    ErasePiece(p, x, shadowY);
                    ErasePiece(p, x, y);
                    DrawBoard();
                    --x;
                }
                else if (key == 77 && board.canPlace(pieces, p, x + 1, y))
                {
                    ErasePiece(p, x, shadowY);
                    ErasePiece(p, x, y);
                    DrawBoard();
                    ++x;
                }
                else if (key == 32)
                {
                    Piece q{p.shape, (Rotation)((index(p.rotation) + 1) % 4)};
                    if (board.canPlace(pieces, q, x, y))
                    {
                        ErasePiece(p, x, shadowY);
                        ErasePiece(p, x, y);
                        DrawBoard();
                        p = q;
                    }
                }
                else if (key == 's' || key == 'S')
                    system("pause>nul");
                else if (key == 27)
                {
                    WriteGrade();
                    return;
                }
                else if (key == 'r' || key == 'R')
                {
                    board.reset();
                    game.reset(RandomPiece(), RandomPiece(), game.difficulty);
                    InitInterface();
                    restarted = true;
                    break;
                }
            }
        }
        if (restarted)
            continue;
        if (board.topOccupied())
        {
            leaderboard.add(game.score.current());
            leaderboard.save();
            WriteGrade();
            return;
        }
        game.current = n;
        game.next = RandomPiece();
    }
}
int main()
{
    ConfigureConsoleEncoding();
    system("title 俄罗斯方块");
    system("mode con lines=29 cols=60");
    HideCursor();
    ReadGrade();
    leaderboard.load();
    while (true)
    {
        int choice = MainMenu();
        if (choice == 0)
        {
            game.difficulty = SelectDifficulty();
            board.reset();
            game.reset(RandomPiece(), RandomPiece(), game.difficulty);
            InitInterface();
            StartGame();
        }
        else if (choice == 1)
            ShowLeaderboard();
        else
            break;
    }
    return 0;
}
