#include <stdio.h>
#include <Windows.h>
#include <stdlib.h>
#include <time.h>
#include <conio.h>
#include <fstream>
#include <iostream>
#include <algorithm>

using namespace std;

#define ROW 29
#define COL 20
#define DOWN 80
#define LEFT 75
#define RIGHT 77
#define UP 72
#define SPACE 32
#define ESC 27
#define ENTER 13

#define FLASH_TIMES 3
#define FLASH_DELAY 80
#define GRAY_COLOR 8
#define RANK_SIZE 5

constexpr char SCORE_FILE[] = "high_score.txt";
constexpr char RANK_FILE[] = "leaderboard.txt";

struct Face
{
    int data[ROW][COL + 10];
    int color[ROW][COL + 10];
} face;

struct Block
{
    int space[4][4];
} block[7][4];

struct Difficulty
{
    const char *name;
    int initialInterval;
    int intervalStep;
    int minimumInterval;
    int linesPerLevel;
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
    int scores_[RANK_SIZE] = {};

public:
    int at(int place) const
    {
        return scores_[place];
    }

    void add(int score)
    {
        int allScores[RANK_SIZE + 1];

        for (int i = 0; i < RANK_SIZE; i++)
        {
            allScores[i] = scores_[i];
        }
        allScores[RANK_SIZE] = score;

        for (int i = 0; i < RANK_SIZE; i++)
        {
            for (int j = i + 1; j <= RANK_SIZE; j++)
            {
                if (allScores[j] > allScores[i])
                {
                    swap(allScores[i], allScores[j]);
                }
            }
        }

        for (int i = 0; i < RANK_SIZE; i++)
        {
            scores_[i] = allScores[i];
        }
    }

    void load()
    {
        ifstream input(RANK_FILE);

        for (int i = 0; i < RANK_SIZE; i++)
        {
            if (!(input >> scores_[i]))
            {
                scores_[i] = 0;
            }
        }
    }

    void save() const
    {
        ofstream output(RANK_FILE);

        for (int i = 0; i < RANK_SIZE; i++)
        {
            output << scores_[i] << '\n';
        }
    }
};

struct GameState
{
    int shape = 0;
    int form = 0;
    int nextShape = 0;
    int nextForm = 0;
    Difficulty difficulty{"普通", 140, 10, 40, 10};
    int level = 1;
    int totalLines = 0;
    ScoreBoard score;

    void reset(Difficulty selectedDifficulty)
    {
        shape = rand() % 7;
        form = rand() % 4;
        nextShape = rand() % 7;
        nextForm = rand() % 4;
        difficulty = selectedDifficulty;
        level = 1;
        totalLines = 0;
        score.reset();
    }

    void addLines(int lines)
    {
        if (lines <= 0)
        {
            return;
        }

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

GameState game;
Leaderboard leaderboard;

void ConfigureConsoleEncoding();
void HideCursor();
void CursorJump(int x, int y);
void color(int num);
int ReadMenuKey();

void ResetGameData();
void InitInterface();
void InitBlockInfo();
void DrawBoard();
void DrawBlock(int shape, int form, int x, int y, int colorOverride = -1);
void DrawSpace(int shape, int form, int x, int y);
void ClearPreviewArea(int x, int y);

int IsLegal(int shape, int form, int x, int y);
int CalcShadowY(int shape, int form, int x, int y);
int IsGameOver();
void LockBlock(int shape, int form, int x, int y);
int ClearFullRows();

Difficulty SelectDifficulty();
int MainMenu();
void ShowLeaderboard();

void ReadGrade();
void WriteGrade();
void PauseGame();
void StartGame();

int main()
{
    ConfigureConsoleEncoding();
    system("title 俄罗斯方块");
    system("mode con lines=29 cols=60");
    HideCursor();
    InitBlockInfo();
    srand((unsigned int)time(NULL));
    ReadGrade();
    leaderboard.load();

    while (true)
    {
        int choice = MainMenu();

        if (choice == 0)
        {
            Difficulty difficulty = SelectDifficulty();
            ResetGameData();
            game.reset(difficulty);
            InitInterface();
            StartGame();
        }
        else if (choice == 1)
        {
            ShowLeaderboard();
        }
        else
        {
            break;
        }
    }

    return 0;
}

void ConfigureConsoleEncoding()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(output, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(output, mode);
}

void HideCursor()
{
    CONSOLE_CURSOR_INFO info;
    info.dwSize = 1;
    info.bVisible = FALSE;
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
}

void CursorJump(int x, int y)
{
    COORD position;
    position.X = x;
    position.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), position);
}

void color(int num)
{
    int consoleColor = num;

    if (num >= 0 && num <= 6)
    {
        switch (num)
        {
        case 0:
            consoleColor = 13;
            break;
        case 1:
        case 2:
            consoleColor = 12;
            break;
        case 3:
        case 4:
            consoleColor = 10;
            break;
        case 5:
            consoleColor = 14;
            break;
        case 6:
            consoleColor = 11;
            break;
        }
    }

    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), consoleColor);
}

void ResetGameData()
{
    for (int i = 0; i < ROW; i++)
    {
        for (int j = 0; j < COL + 10; j++)
        {
            face.data[i][j] = 0;
            face.color[i][j] = 0;
        }
    }
}

void InitInterface()
{
    system("cls");
    color(7);

    for (int i = 0; i < ROW; i++)
    {
        for (int j = 0; j < COL + 10; j++)
        {
            if (j == 0 || j == COL - 1 || j == COL + 9)
            {
                face.data[i][j] = 1;
                face.color[i][j] = 7;
                CursorJump(2 * j, i);
                cout << "■";
            }
            else if (i == ROW - 1)
            {
                face.data[i][j] = 1;
                face.color[i][j] = 7;
                CursorJump(2 * j, i);
                cout << "■";
            }
        }
    }

    for (int i = COL; i < COL + 10; i++)
    {
        face.data[8][i] = 1;
        face.color[8][i] = 7;
        CursorJump(2 * i, 8);
        cout << "■";
    }

    CursorJump(2 * COL, 1);
    cout << "下一个方块：";

    CursorJump(2 * COL + 4, ROW - 19);
    cout << "左移：←";
    CursorJump(2 * COL + 4, ROW - 17);
    cout << "右移：→";
    CursorJump(2 * COL + 4, ROW - 15);
    cout << "加速：↓";
    CursorJump(2 * COL + 4, ROW - 13);
    cout << "旋转：空格";
    CursorJump(2 * COL + 4, ROW - 11);
    cout << "暂停：S";
    CursorJump(2 * COL + 4, ROW - 9);
    cout << "退出：Esc";
    CursorJump(2 * COL + 4, ROW - 7);
    cout << "重新开始：R";
    CursorJump(2 * COL + 4, ROW - 6);
    cout << "难度：" << game.difficulty.name;
    CursorJump(2 * COL + 4, ROW - 5);
    cout << "最高纪录：" << game.score.high();
    CursorJump(2 * COL + 4, ROW - 4);
    cout << "当前等级：" << game.level;
    CursorJump(2 * COL + 4, ROW - 3);
    cout << "当前分数：" << game.score.current();
    cout.flush();
}

void InitBlockInfo()
{
    for (int i = 0; i <= 2; i++)
    {
        block[0][0].space[1][i] = 1;
    }
    block[0][0].space[2][1] = 1;

    for (int i = 1; i <= 3; i++)
    {
        block[1][0].space[i][1] = 1;
        block[2][0].space[i][2] = 1;
    }
    block[1][0].space[3][2] = 1;
    block[2][0].space[3][1] = 1;

    for (int i = 0; i <= 1; i++)
    {
        block[3][0].space[1][i] = 1;
        block[3][0].space[2][i + 1] = 1;

        block[4][0].space[1][i + 1] = 1;
        block[4][0].space[2][i] = 1;

        block[5][0].space[1][i + 1] = 1;
        block[5][0].space[2][i + 1] = 1;
    }

    for (int i = 0; i <= 3; i++)
    {
        block[6][0].space[i][1] = 1;
    }

    int temp[4][4];

    for (int shape = 0; shape < 7; shape++)
    {
        for (int form = 0; form < 3; form++)
        {
            for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 4; j++)
                {
                    temp[i][j] = block[shape][form].space[i][j];
                }
            }

            for (int i = 0; i < 4; i++)
            {
                for (int j = 0; j < 4; j++)
                {
                    block[shape][form + 1].space[i][j] = temp[3 - j][i];
                }
            }
        }
    }
    cout.flush();
}

void DrawBlock(int shape, int form, int x, int y, int colorOverride)
{
    const int drawColor = colorOverride >= 0 ? colorOverride : shape;
    color(drawColor);

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (block[shape][form].space[i][j] == 1)
            {
                CursorJump(2 * (x + j), y + i);
                cout << "■";
            }
        }
    }
    cout.flush();
}

void DrawSpace(int shape, int form, int x, int y)
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (block[shape][form].space[i][j] == 1)
            {
                CursorJump(2 * (x + j), y + i);
                cout << "  ";
            }
        }
    }
    cout.flush();
}

void ClearPreviewArea(int x, int y)
{
    color(7);

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            CursorJump(2 * (x + j), y + i);
            cout << "  ";
        }
    }
    cout.flush();
}

void DrawBoard()
{
    for (int i = 0; i < ROW; i++)
    {
        for (int j = 0; j < COL + 10; j++)
        {
            if (face.data[i][j] == 1)
            {
                color(face.color[i][j]);
                CursorJump(2 * j, i);
                cout << "■";
            }
        }
    }
    cout.flush();
}

int IsLegal(int shape, int form, int x, int y)
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (block[shape][form].space[i][j] == 1)
            {
                int boardY = y + i;
                int boardX = x + j;

                if (boardY < 0 || boardY >= ROW || boardX < 0 || boardX >= COL)
                {
                    return 0;
                }

                if (face.data[boardY][boardX] == 1)
                {
                    return 0;
                }
            }
        }
    }

    return 1;
}

int CalcShadowY(int shape, int form, int x, int y)
{
    int shadowY = y;

    while (IsLegal(shape, form, x, shadowY + 1))
    {
        shadowY++;
    }

    return shadowY;
}

int IsGameOver()
{
    for (int col = 1; col < COL - 1; col++)
    {
        if (face.data[1][col] == 1)
        {
            return 1;
        }
    }

    return 0;
}

void LockBlock(int shape, int form, int x, int y)
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (block[shape][form].space[i][j] == 1)
            {
                face.data[y + i][x + j] = 1;
                face.color[y + i][x + j] = shape;
            }
        }
    }
}

int ClearFullRows()
{
    int cleared = 0;

    for (int row = ROW - 2; row > 0; row--)
    {
        bool full = true;

        for (int col = 1; col < COL - 1; col++)
        {
            if (face.data[row][col] == 0)
            {
                full = false;
                break;
            }
        }

        if (!full)
        {
            continue;
        }

        cleared++;

        for (int flash = 0; flash < FLASH_TIMES; flash++)
        {
            color(7);

            for (int col = 1; col < COL - 1; col++)
            {
                CursorJump(2 * col, row);
                cout << "■";
            }
            cout.flush();

            Sleep(FLASH_DELAY);

            for (int col = 1; col < COL - 1; col++)
            {
                CursorJump(2 * col, row);
                cout << "  ";
            }
            cout.flush();

            Sleep(FLASH_DELAY);
        }

        for (int moveRow = row; moveRow > 1; moveRow--)
        {
            for (int col = 1; col < COL - 1; col++)
            {
                face.data[moveRow][col] = face.data[moveRow - 1][col];
                face.color[moveRow][col] = face.color[moveRow - 1][col];
            }
        }

        for (int col = 1; col < COL - 1; col++)
        {
            face.data[1][col] = 0;
            face.color[1][col] = 0;
        }

        row++;
    }

    return cleared;
}

int ReadMenuKey()
{
    int key = getch();

    if (key == 0 || key == 224)
    {
        key = getch();
    }

    return key;
}

int MainMenu()
{
    int selected = 0;
    const char *items[] = {"开始游戏", "排行榜", "退出游戏"};

    while (true)
    {
        system("cls");
        color(14);
        CursorJump(22, 7);
        cout << "俄罗斯方块";

        for (int i = 0; i < 3; i++)
        {
            CursorJump(24, 12 + i * 2);
            color(i == selected ? 11 : 7);
            cout << (i == selected ? "> " : "  ") << items[i];
        }

        color(8);
        CursorJump(18, 20);
        cout << "上下键选择，回车确认";
        cout.flush();

        int key = ReadMenuKey();

        if (key == UP && selected > 0)
        {
            selected--;
        }
        else if (key == DOWN && selected < 2)
        {
            selected++;
        }
        else if (key == ENTER)
        {
            return selected;
        }
        else if (key == ESC)
        {
            return 2;
        }
    }
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
        color(14);
        CursorJump(24, 7);
        cout << "选择难度";

        for (int i = 0; i < 3; i++)
        {
            CursorJump(24, 11 + i * 2);
            color(i == selected ? 11 : 7);
            cout << (i == selected ? "> " : "  ") << options[i].name << "模式";
        }

        color(8);
        CursorJump(17, 20);
        cout << "上下键选择，回车确认，Esc返回";
        cout.flush();

        int key = ReadMenuKey();

        if (key == UP && selected > 0)
        {
            selected--;
        }
        else if (key == DOWN && selected < 2)
        {
            selected++;
        }
        else if (key == ENTER)
        {
            return options[selected];
        }
        else if (key == ESC)
        {
            return options[1];
        }
    }
}

void ShowLeaderboard()
{
    system("cls");
    color(14);
    CursorJump(24, 6);
    cout << "排行榜";

    color(7);

    for (int i = 0; i < RANK_SIZE; i++)
    {
        CursorJump(22, 10 + i * 2);

        if (leaderboard.at(i) > 0)
        {
            cout << "第" << i + 1 << "名：" << leaderboard.at(i) << " 分";
        }
        else
        {
            cout << "第" << i + 1 << "名：---";
        }
    }

    color(8);
    CursorJump(19, 22);
    cout << "按任意键返回主菜单";
    cout.flush();
    getch();
}

void PauseGame()
{
    color(7);
    CursorJump(2 * (COL / 3), ROW / 2);
    cout << "  游戏暂停  ";
    CursorJump(2 * (COL / 3) - 2, ROW / 2 + 2);
    cout << "按任意键继续...";
    cout.flush();

    while (!kbhit())
    {
        Sleep(50);
    }

    getch();

    CursorJump(2 * (COL / 3), ROW / 2);
    cout << "            ";
    CursorJump(2 * (COL / 3) - 2, ROW / 2 + 2);
    cout << "                ";
}

void ReadGrade()
{
    ifstream input(SCORE_FILE);
    int highScore = 0;

    if (input >> highScore)
    {
        game.score.load(highScore);
    }
    else
    {
        game.score.load(0);
        ofstream output(SCORE_FILE);
        output << 0;
    }
}

void WriteGrade()
{
    ofstream output(SCORE_FILE);

    if (output)
    {
        output << game.score.high();
    }
}

void StartGame()
{
    while (true)
    {
        int shape = game.shape;
        int form = game.form;
        int x = COL / 2 - 2;
        int y = 0;
        bool restarted = false;
        DWORD lastDrop = GetTickCount();

        ClearPreviewArea(COL + 3, 3);
        DrawBlock(game.nextShape, game.nextForm, COL + 3, 3);

        while (true)
        {
            int shadowY = CalcShadowY(shape, form, x, y);
            DrawBlock(shape, form, x, shadowY, GRAY_COLOR);
            DrawBlock(shape, form, x, y);

            if (GetTickCount() - lastDrop >= (DWORD)game.dropInterval())
            {
                lastDrop = GetTickCount();

                if (!IsLegal(shape, form, x, y + 1))
                {
                    DrawSpace(shape, form, x, shadowY);
                    DrawSpace(shape, form, x, y);
                    LockBlock(shape, form, x, y);

                    int lines = ClearFullRows();
                    game.addLines(lines);
                    DrawBoard();
                    InitInterface();
                    break;
                }

                DrawSpace(shape, form, x, shadowY);
                DrawSpace(shape, form, x, y);
                DrawBoard();
                y++;
            }
            else if (kbhit())
            {
                int key = ReadMenuKey();

                if (key == DOWN && IsLegal(shape, form, x, y + 1))
                {
                    DrawSpace(shape, form, x, shadowY);
                    DrawSpace(shape, form, x, y);
                    DrawBoard();
                    y++;
                }
                else if (key == LEFT && IsLegal(shape, form, x - 1, y))
                {
                    DrawSpace(shape, form, x, shadowY);
                    DrawSpace(shape, form, x, y);
                    DrawBoard();
                    x--;
                }
                else if (key == RIGHT && IsLegal(shape, form, x + 1, y))
                {
                    DrawSpace(shape, form, x, shadowY);
                    DrawSpace(shape, form, x, y);
                    DrawBoard();
                    x++;
                }
                else if (key == SPACE)
                {
                    int nextForm = (form + 1) % 4;

                    if (IsLegal(shape, nextForm, x, y))
                    {
                        DrawSpace(shape, form, x, shadowY);
                        DrawSpace(shape, form, x, y);
                        DrawBoard();
                        form = nextForm;
                    }
                }
                else if (key == 's' || key == 'S')
                {
                    PauseGame();
                }
                else if (key == ESC)
                {
                    WriteGrade();
                    return;
                }
                else if (key == 'r' || key == 'R')
                {
                    ResetGameData();
                    game.reset(game.difficulty);
                    InitInterface();
                    restarted = true;
                    break;
                }
            }

            Sleep(1);
        }

        if (restarted)
        {
            continue;
        }

        if (IsGameOver())
        {
            leaderboard.add(game.score.current());
            leaderboard.save();
            WriteGrade();

            system("cls");
            color(14);
            CursorJump(22, 10);
            cout << "游戏结束";
            color(7);
            CursorJump(20, 13);
            cout << "本局得分：" << game.score.current();
            CursorJump(17, 16);
            cout << "按任意键返回主菜单";
            cout.flush();
            getch();
            return;
        }

        game.shape = game.nextShape;
        game.form = game.nextForm;
        game.nextShape = rand() % 7;
        game.nextForm = rand() % 4;
    }
}
