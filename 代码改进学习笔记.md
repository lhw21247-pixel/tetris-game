# 俄罗斯方块代码改进学习笔记

## 1. 先确认当前版本

当前版本文件是 `tetris.cpp`，使用 MinGW 编译器验证通过：

```text
g++ -std=c++17 -Wall -Wextra -g tetris.cpp -o tetris.exe
```

程序运行需要 Windows 控制台，因为代码使用了 `Windows.h`、`conio.h` 和控制台光标 API。

最高分保存在同目录的 `high_score.txt` 中。它是普通文本文件，例如内容为 `120`，不是二进制文件。

---

## 2. 原始版和改进版的总体区别

### 原始版

原始代码大约 494 行，主要特点是：

- `Face face` 和 `Block block[7][4]` 是全局裸结构体数组；
- `grade`、`max` 是全局整数；
- 游戏规则、键盘输入、控制台绘图、文件读写全部放在一个文件中；
- 形状通过 `0、1、2...6` 表示；
- 方块状态通过 `block[shape][form].space[i][j]` 直接访问；
- 棋盘的占用状态和颜色分开保存在两个二维数组中；
- `main()` 被重新开始逻辑递归调用；
- 最高分使用二进制 `fwrite/fread`，但文件以文本方式打开，容易出问题。

### 改进版

改进版保留了原来的控制台玩法，但把数据和规则整理成了几个清楚的类型：

```text
PieceSet    管理七种方块和四种旋转形态
Board       管理棋盘、碰撞、落块、消行
ScoreBoard  管理当前分数和最高分
GameState   管理当前方块、下一个方块和分数
```

主要收益是：

1. 调用方不再直接修改棋盘内部数组；
2. 边界检查集中在 `Board::occupied()`；
3. 方块形状和旋转规则集中在 `PieceSet`；
4. 分数的重置、加分、读取和最高分更新集中在 `ScoreBoard`；
5. 文件名、可执行文件名和调试路径全部使用英文，避免 MinGW/GDB 的中文路径问题；
6. 控制台启动时设置 UTF-8，解决中文和方块字符乱码；
7. 下一个方块预览区每次完整清空，解决残影叠加。

---

## 3. 数据结构变化

### 3.1 原始的 `Face`

原始版使用两个并行数组：

```cpp
struct Face
{
    int data[ROW][COL + 10];
    int color[ROW][COL + 10];
} face;
```

一个数组表示“有没有方块”，另一个数组表示“方块是什么颜色”。这种方式有一个风险：两个数组可能不一致。例如 `data[row][col]` 已经清空，但 `color[row][col]` 还留着旧值。

### 3.2 改进后的 `Cell`

```cpp
struct Cell
{
    bool occupied = false;
    Shape shape = Shape::T;
};
```

一个 `Cell` 就是棋盘上的一个格子：

- `occupied == false`：空格；
- `occupied == true`：有固定方块；
- `shape`：记录这个格子属于哪种方块，用来决定颜色。

相关数据被放进 `Board`：

```cpp
std::array<std::array<Cell, kColumns>, kRows> cells_{};
```

这就是标准数据结构的第一个重要思想：**把同一个概念有关的数据放在一起。**

### 3.3 原始的 `Block`

原始版用整数索引表达形状：

```cpp
block[shape][form].space[i][j]
```

读代码的人必须记住：0 是 T，1 是 L，6 是 I。

改进版使用枚举：

```cpp
enum class Shape : unsigned char
{
    T, L, J, Z, S, O, I
};

enum class Rotation : unsigned char
{
    R0, R90, R180, R270
};
```

所以：

```cpp
Piece{Shape::T, Rotation::R90}
```

比 `shape = 0, form = 1` 更容易理解，也不容易传错参数。

### 3.4 `Piece`

```cpp
struct Piece
{
    Shape shape;
    Rotation rotation;
};
```

`Piece` 表示“一个什么形状、处于什么旋转状态的方块”。它描述的是方块身份，不包含方块在棋盘上的坐标。

方块坐标仍由游戏循环中的 `x`、`y` 表示，这是为了让初学者容易理解：

```text
Piece = 方块是什么
x/y   = 方块在哪里
```

---

## 4. `PieceSet`：方块形状和旋转

`PieceSet` 内部保存七种方块、四种旋转和每种形态的 4×4 网格。

```cpp
std::array<std::array<Grid, 4>, 7> cells_{};
```

逻辑上可以理解为：

```text
cells_[形状][旋转][行][列]
```

`Grid` 是一个 4×4 的小网格，值为 1 表示有小方块，值为 0 表示空白。

例如 T 方块初始形态大致是：

```text
0 0 0 0
1 1 1 0
0 1 0 0
0 0 0 0
```

### 4.1 为什么只手写一个方向？

构造函数先写每种形状的初始方向，然后使用：

```cpp
cells_[sh][ro] = rotate(cells_[sh][ro - 1]);
```

自动计算下一个旋转方向。这样旋转公式只有一处，减少了重复代码。

### 4.2 `rotate()` 做什么？

```cpp
result[r][c] = source[3 - c][r];
```

这是一种 4×4 矩阵顺时针旋转公式。每个新位置都从旧网格中找到对应位置。

### 4.3 `cells()` 和 `color()`

```cpp
const Grid &cells(Piece p) const;
```

根据方块的形状和旋转状态返回对应的 4×4 网格。

```cpp
int color(Shape s) const;
```

根据方块类型返回 Windows 控制台颜色编号。颜色属于方块目录，因此绘图函数不需要自己写一堆形状判断。

---

## 5. `Board`：棋盘规则的核心

### 5.1 `reset()`

`reset()` 做三件事：

1. 清空所有格子；
2. 把第 0 列和最后一列设为墙；
3. 把最后一行设为地面。

这样碰撞检测只要判断“目标格是否被占用”，不需要到处单独判断墙和地面。

### 5.2 `occupied()`

```cpp
bool occupied(int r, int c) const
{
    return r < 0 || r >= kRows ||
           c < 0 || c >= kColumns ||
           cells_[r][c].occupied;
}
```

越界直接视为“占用”。这是一个很实用的设计：

```text
越过左边界 = 撞墙
越过右边界 = 撞墙
越过底部   = 撞地
碰到已有方块 = 碰撞
```

所有情况都可以用同一个函数判断。

### 5.3 `canPlace()`

这个函数判断一个方块能不能放在 `(x, y)`：

```cpp
if (g[r][c] && occupied(y + r, x + c))
    return false;
```

只有当方块自身该位置有小方块时，才去检查棋盘对应位置。如果所有小方块都没有碰撞，就返回 `true`。

它同时服务于：

- 自动下落；
- 左移；
- 右移；
- 旋转。

### 5.4 `lock()`

当方块不能继续下落时，`lock()` 把活动方块写入棋盘：

```cpp
cells_[y + r][x + c] = {true, p.shape};
```

从这一刻开始，方块不再是临时图像，而是棋盘的一部分。

### 5.5 `clearFullRows()`

步骤如下：

1. 从底部可用行向上检查；
2. 判断中间区域是否全部占用；
3. 如果满行，记录 `cleared++`；
4. 将上面的行整体向下复制一行；
5. 清空最上面一行；
6. 重新检查当前位置，因为新的行已经落下来了。

函数返回消除的行数，游戏逻辑再根据行数加分。

### 5.6 `topOccupied()`

它检查最上方的有效区域是否已经有固定方块。如果有，说明棋盘堆到顶部，游戏结束。

---

## 6. `ScoreBoard`：最高分是否会记录？

现在分数结构是：

```cpp
class ScoreBoard
{
    int current_ = 0;
    int high_ = 0;
};
```

### `addLines()`

```cpp
current_ += lines * 10;
high_ = std::max(high_, current_);
```

每消除一行加 10 分，并立即在内存中更新最高分。

### `ReadGrade()`

程序启动时从 `high_score.txt` 读取一个整数。如果文件不存在或内容不是数字，就初始化为 0。

### `WriteGrade()`

程序在以下两个时机保存最高分：

- 棋盘堆满、游戏结束；
- 玩家按 `Esc` 退出。

因此，正常退出时最高分会被保留下来。强制关闭窗口或系统断电无法保证保存，这是普通文件保存程序的正常限制。

### `R` 重开

按 `R` 时现在会同时执行：

```cpp
board.reset();
game.reset(RandomPiece(), RandomPiece());
```

也就是：

- 清空棋盘；
- 当前分数归零；
- 重新生成当前方块；
- 重新生成下一个方块。

历史最高分不会清零。

---

## 7. 绘图部分

### `DrawPiece()`

绘制当前活动方块。它根据 `PieceSet::cells()` 取得 4×4 网格，把值为 1 的位置输出为 `■`。

### `ErasePiece()`

用两个空格覆盖活动方块原来的位置。它只负责擦除活动方块，不负责擦除棋盘中的固定方块。

### `ClearPreviewArea()`

下一个方块预览区必须清除完整的 4×4 区域：

```cpp
ClearPreviewArea(kColumns + 3, 3);
DrawPiece(n, kColumns + 3, 3);
```

之前只擦除旧方块占用的位置，会导致旧方块和新方块叠加，形成残影。现在每次先清空固定区域，再画新方块。

### UTF-8 设置

```cpp
SetConsoleOutputCP(CP_UTF8);
SetConsoleCP(CP_UTF8);
```

源码使用 UTF-8，所以程序启动时也让 Windows 控制台使用 UTF-8，避免中文和 `■` 显示乱码。

---

## 8. `StartGame()` 的运行流程

可以把它看成两层循环。

### 外层循环

外层循环负责“一块方块从出现到落地”：

```text
取出 current
显示 next
循环等待当前方块落地
落地后消行和加分
current = next
next = 随机生成
```

### 内层循环

内层循环负责当前方块的实时移动：

1. 绘制当前方块；
2. 等待一段时间；
3. 没有按键时自动下落；
4. 有按键时尝试移动或旋转；
5. 如果不能继续下落，就锁定方块。

移动前都先调用 `board.canPlace()`，验证成功后才修改 `x`、`y` 或旋转状态。这就是“先检查，后更新”。

---

## 9. 配置文件的作用

`.vscode/tasks.json` 负责构建：

```text
g++ -std=c++17 tetris.cpp -o tetris.exe
```

`.vscode/launch.json` 负责调试：

- `program` 指向 `tetris.exe`；
- `cwd` 使用 `C:\tetris`，避免 GDB 直接处理中文路径；
- `preLaunchTask` 表示按 F5 前先自动构建。

`.gitignore` 忽略 `tetris.exe`，因为它是本地编译生成物，不需要提交到 GitHub。

---

## 10. 这次改进解决了原始版的哪些问题

| 原始问题 | 改进方式 |
| --- | --- |
| `Face` 直接暴露两个数组 | `Board` 隐藏 `Cell` 数组 |
| `data` 和 `color` 可能不一致 | 合并为 `Cell` |
| shape/form 使用魔数 | `Shape` 和 `Rotation` 枚举 |
| 碰撞代码可能越界 | `Board::occupied()` 集中处理边界 |
| 形状旋转代码分散 | `PieceSet::rotate()` 统一处理 |
| `grade`、`max` 是全局整数 | `ScoreBoard` 管理分数 |
| `R` 重开不完整 | 同时重置棋盘、分数和方块 |
| Esc 退出不保存最高分 | 退出前调用 `WriteGrade()` |
| 预览方块残影 | 完整清空 4×4 预览区 |
| 中文路径导致构建/GDB失败 | 源文件、输出文件和调试路径使用英文 |
| 中文控制台乱码 | 启动时设置 UTF-8 |
| 二进制读写和文本模式混用 | 使用文本流读写 `high_score.txt` |

---

## 11. 目前仍保留的简单化设计

这些不是错误，而是为了让项目适合学习而暂时保留的设计：

1. 所有代码仍在一个 `tetris.cpp` 中，便于你一次性阅读；
2. `board`、`pieces`、`game` 仍是全局对象；
3. Windows 控制台绘图和游戏规则还没有拆成不同文件；
4. 分数规则暂时是“一行 10 分”，没有引入等级、连击等复杂规则；
5. 没有引入模板元编程、继承体系或复杂设计模式。

下一步如果要继续学习，最自然的顺序是：

```text
先理解当前单文件版本
→ 把 GameState 和 Board 收进 Game 类
→ 再把控制台绘图单独放到 ConsoleRenderer
→ 最后为 Board::canPlace 和 clearFullRows 写小测试
```

不要一次拆太多文件。每次只移动一个职责，并确保编译和运行正常。

---

## 12. 建议的阅读顺序

第一次阅读时建议按下面顺序，而不是从第一行一直读到最后一行：

1. 先看 `main()`，理解程序入口；
2. 看 `StartGame()`，理解游戏循环；
3. 看 `GameState` 和 `ScoreBoard`，理解状态保存；
4. 看 `Board::canPlace()`、`lock()`、`clearFullRows()`，理解规则；
5. 看 `PieceSet`，理解形状和旋转；
6. 最后看 `DrawPiece()`、`DrawBoard()` 和 Windows 控制台函数。

你可以先回答三个问题来检查自己是否理解：

1. 一个方块从“正在下落”变成“固定方块”是哪一行代码完成的？
2. 为什么 `occupied()` 把越界当成占用？
3. 为什么预览区不能只调用 `ErasePiece()`？

如果这三个问题能回答清楚，就已经理解了这版程序的主要结构。
