#include <Windows.h>
#include <algorithm>
#include <array>
#include <conio.h>
#include <fstream>
#include <iostream>
#include <random>

namespace tetris
{
constexpr int kRows = 29, kColumns = 20, kPieceSize = 4, kShapeCount = 7, kRotationCount = 4;
constexpr char kScoreFile[] = "high_score.txt";
using Grid = std::array<std::array<unsigned char, 4>, 4>;
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
    std::array<std::array<Grid, 4>, 7> cells_{};
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
    std::array<std::array<Cell, kColumns>, kRows> cells_{};

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
                std::all_of(cells_[r].begin() + 1, cells_[r].end() - 1, [](const Cell &c) { return c.occupied; });
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
};

struct GameState
{
    int score = 0, highScore = 0;
    Piece current{Shape::T, Rotation::R0}, next{Shape::T, Rotation::R0};
    void reset(Piece a, Piece b)
    {
        score = 0;
        current = a;
        next = b;
    }
};
} // namespace tetris
using namespace tetris;
Board board;
PieceSet pieces;
GameState game;
std::mt19937 rng{std::random_device{}()};
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
void DrawPiece(Piece p, int x, int y)
{
    const auto &g = pieces.cells(p);
    SetColor(pieces.color(p.shape));
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            if (g[r][c])
            {
                CursorJump(2 * (x + c), y + r);
                std::cout << "■";
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
                std::cout << "  ";
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
                std::cout << "■";
            }
}
Piece RandomPiece()
{
    return {(Shape)(rng() % 7), (Rotation)(rng() % 4)};
}
void ReadGrade()
{
    std::ifstream in(kScoreFile);
    if (!(in >> game.highScore))
    {
        game.highScore = 0;
        std::ofstream(kScoreFile) << 0;
    }
}
void WriteGrade()
{
    std::ofstream out(kScoreFile);
    if (out)
        out << game.highScore;
}
void InitInterface()
{
    system("cls");
    SetColor(7);
    DrawBoard();
    CursorJump(2 * kColumns, 1);
    std::cout << "下一个方块：";
    CursorJump(2 * kColumns + 4, kRows - 19);
    std::cout << "左移：←";
    CursorJump(2 * kColumns + 4, kRows - 17);
    std::cout << "右移：→";
    CursorJump(2 * kColumns + 4, kRows - 15);
    std::cout << "加速：↓";
    CursorJump(2 * kColumns + 4, kRows - 13);
    std::cout << "旋转：空格";
    CursorJump(2 * kColumns + 4, kRows - 11);
    std::cout << "暂停: S";
    CursorJump(2 * kColumns + 4, kRows - 9);
    std::cout << "退出: Esc";
    CursorJump(2 * kColumns + 4, kRows - 7);
    std::cout << "重新开始:R";
    CursorJump(2 * kColumns + 4, kRows - 5);
    std::cout << "最高纪录:" << game.highScore;
    CursorJump(2 * kColumns + 4, kRows - 3);
    std::cout << "当前分数：" << game.score;
}
void StartGame()
{
    game.reset(RandomPiece(), RandomPiece());
    while (true)
    {
        Piece p = game.current, n = game.next;
        int x = kColumns / 2 - 2, y = 0, ticks = 10000;
        ErasePiece(n, kColumns + 3, 3);
        DrawPiece(n, kColumns + 3, 3);
        while (true)
        {
            DrawPiece(p, x, y);
            if (ticks > 0)
                --ticks;
            if (ticks == 0)
            {
                if (!board.canPlace(pieces, p, x, y + 1))
                {
                    board.lock(pieces, p, x, y);
                    int lines = board.clearFullRows();
                    game.score += lines * 10;
                    if (lines)
                        InitInterface();
                    break;
                }
                ErasePiece(p, x, y);
                ++y;
                ticks = 10000;
            }
            else if (kbhit())
            {
                int key = getch();
                if (key == 80 && board.canPlace(pieces, p, x, y + 1))
                {
                    ErasePiece(p, x, y);
                    ++y;
                }
                else if (key == 75 && board.canPlace(pieces, p, x - 1, y))
                {
                    ErasePiece(p, x, y);
                    --x;
                }
                else if (key == 77 && board.canPlace(pieces, p, x + 1, y))
                {
                    ErasePiece(p, x, y);
                    ++x;
                }
                else if (key == 32)
                {
                    Piece q{p.shape, (Rotation)((index(p.rotation) + 1) % 4)};
                    if (board.canPlace(pieces, q, x, y))
                    {
                        ErasePiece(p, x, y);
                        p = q;
                    }
                }
                else if (key == 's' || key == 'S')
                    system("pause>nul");
                else if (key == 27)
                    return;
                else if (key == 'r' || key == 'R')
                {
                    board.reset();
                    InitInterface();
                    break;
                }
            }
        }
        if (board.topOccupied())
        {
            game.highScore = std::max(game.highScore, game.score);
            WriteGrade();
            return;
        }
        game.current = n;
        game.next = RandomPiece();
    }
}
int main()
{
    system("title 俄罗斯方块");
    system("mode con lines=29 cols=60");
    HideCursor();
    ReadGrade();
    InitInterface();
    StartGame();
    return 0;
}
