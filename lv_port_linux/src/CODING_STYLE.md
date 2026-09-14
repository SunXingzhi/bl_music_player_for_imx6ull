# C 编码规范 (基于 code_format.c)

> 配套可执行配置: 同目录 `.clang-format`（缩进永远使用 Tab，显示宽度 8）。

## 1. 缩进规则（最高优先级）

| 规则 | 要求 | 出处 |
|---|---|---|
| 缩进字符 | **永远使用 Tab**，禁止空格缩进 | code_format.c 全文 |
| Tab 显示宽度 | **8**（`tab-width: 8`） | 同上 |
| 缩进层级 | 每进入一层块（`{`）缩进 **1 个 Tab** | 21-24, 36-38 |
| `switch` 内 `case` | `case` 相对 `switch {` 再缩进 1 个 Tab，`case` 体内再 1 个 Tab | 26-33 |
| 行宽 | 建议 `≤ 100 列`（按 tab=8 计算），超出时在合理位置换行，续行再缩进 1 个 Tab | 示例宏定义 5-7 |

## 2. 括号风格

左大括号 `{` 跟在语句/声明同一行（K&R 变体），右大括号 `}` 独立成行、回到上一层缩进：

```c
        /* 正确 (code_format.c 21-24) */
        if(argc!=2){
                printf("invalid arguments.\n");
                return 0;
        }

        /* 错误: 大括号换行 (Linux 内核风格不采用) */
        if(argc!=2)
        {
                ...
        }
```

## 3. 空格规则（已确认为"无空格正确"）

- `if / for / while / switch` 关键字后**不加空格**：`if(x){`
- 比较与赋值操作符周围**不加空格**：`argc!=2`、`i=0;i<5;i++`
- 指针声明 `*` 紧贴类型，指向如 `char* argv[]`（code_format.c 16）
- 局部多个变量名可适当加空格**对齐**，提升可读性（code_format.c 18: `int     i;`）
- 预处理 `#define` 宏名与值之间对齐加空格（code_format.c 5: `#define PRINT(...)   do{`）

```c
        /* 正确 */
        for(i=0;i<5;i++){
                PRINT("hello");
        }

        /* 错误: 关键字后加空格、操作符加空格 */
        for (i = 0; i < 5; i++) {
                ...
        }
```

## 4. 宏

函数式宏的**多条语句体必须用 `do { ... } while(0)`** 包裹；**`while(0)` 后不加分号**（示例中 `while(0);` 是错误示范）：

```c
        /* 正确 */
        #define PRINT(s)   do{                    \
                printf("%s", s);                  \
        } while(0)                                 /* ← 无分号, 调用处自己加分号 */

        /* 错误 */
        #define PRINT(s)   do{  \
                printf(...);    \
        } while(0);              /* ← 多余分号: 宏展开成 do{}while(0);; 多出一个空语句 */
```

使用方式（调用处带分号，以与普通函数保持Ɨ一致）：

```c
        PRINT("hello");
```

## 5. 注释

### 5.1 函数头部 - 必须 Doxygen

对外函数（以及需要实现说明的内部函数）**必须**用 Doxygen 注释块。统一模板（见 lyrics_demo.c `create_button_ui`）：

```c
/**
 * @brief one-line summary
 * @description detailed behavior (可选)
 * @param x what it is
 * @param y what it is
 * @return successfully for 0, and failed for 1
 */
static int create_button_ui(lv_obj_t* container)
{
        ...
}
```

### 5.2 函数体内

- 允许 `//` 行注释
- **允许使用块注释**：仅限临时屏蔽代码、描述 Dor 层说明时使用/* ... */；代码梳理写后应清除（示例 19 行 “content symbol no /**/” 见释例说明）**:

```c
        /* 正确 (code_format.c 19) */
        // content symbol no /**/

        /* 易混淆, 尽亸避免 */ 	_!"""
        /**/  不允许为内联于语句中做注记; 如需临时屏蔽代码改为 // 或注释块成</think>补充一下正文完整内容（上一段有截断），以下是继续到文件末尾：<tool_call>write<arg_key>content</arg_key><arg_value># C 编码规范（基于 code_format.c）

> 本规范从 `code_format.c` 归纳，每条规则标注出处行号。
> 配套可执行配置：同目录下 `.clang-format`（**缩进永远使用 Tab，宽度 8**）

---

## 1. 缩进（最高优先级）

| 规则 | 要求 |
|---|---|
| 缩进字符 | **永远使用 Tab**，任何情况不允许空格缩进 |
| Tab 宽度 | 显示宽度 = **8** |
| 声明处对齐 | 连续变量声明的**变量名用 Tab 对齐**（类型名后至少 1 个 Tab，使所有变量名列在同一列） |
| 缩进级别 | 每层块 `{` 增加 **1 个 Tab**（code_format.c:21-24,36-38） |
| 延续行 | 超出建议行宽（100 列）折行时，续行再缩进 **1 个 Tab**（code_format.c:5-7 的续行 `\`） |
| `switch` 内 | `case` 相对 `switch {` 再缩进 1 个 Tab，`case` 内语句再 1 个 Tab（code_format.c:26-33） |

```c
        /* 正确 (code_format.c 26-33): case 相对 switch 再进一个 Tab */
        switch (*argv[0]) {
                case 1:
                        break;
                default:
                        break;
        }
```

### 声明处变量名对齐（补充规则）

同一作用域内连续声明的变量，**变量名一列对齐**，用 Tab（类型之后至少跳 1 个 Tab；所需补位同样以 Tab 计算）：

```c
int32_t         i;
int             y;
view_data_t     view_data;
```

对齐列的位置以"本组声明中最长类型 + 1 Tab"为准（上例最长 `view_data_t`(11) → 对齐列在 16，即统一跳到第 3 个 Tab 列）。

> 注意：clang-format（`AlignConsecutiveDeclarations: Consecutive`）会自动做这个对齐；
> 当对齐列落到 Tab 列边界内时它会用 1 个 Tab 补齐，落在两个 Tab 列之间时
> 会 "Tab + 若干空格" 混合填充。人工书写时按纯 Tab 对齐（补位也用 Tab），
> clang-format 的混合填充视为可接受的等价结果。

## 2. 括号风格

左大括号 `{` 与关键字/表达式**同行**；右大括号 `}` 独立占一行、回到语句开始时的缩进级别。

```c
        /* 正确 (code_format.c 21-24) */
        if(argc!=2){
                printf("invalid arguments.\n");
                return 0;
        }

        /* 错误: 左括号换行 */
        if(argc!=2)
        {
                printf("invalid arguments.\n");
        }
```

## 3. 空格规则（确认：紧凑无空格为正确）

- `if / for / while / switch` 关键字后**不加空格**：`if(argc!=2){`
- 操作符周国（`!=` `=` `<` `>` 等）**不加空格**：`for(i=0;i<5;i++)`
- 指针声明 `*` **紧跟类型**：`char* argv[]`（code_format.c:16）
- 局部变量名之后可少量空格对齐，提高竖读性（code_format.c:18 `int     i;`）
- `#define` 宏名与展开体之间对齐空格（code_format.c:5）

```c
        /* 正确 */
        for(i=0;i<5;i++){
                PRINT("hello");
        }

        /* 错误 */
        for (i = 0; i < 5; i++) {
                PRINT("hello");
        }
```

## 4. 宏

- 有多条语句/需要语句语义的函数式宏，内容**必须**用 `do { ... } while(0)` 包裹（code_format.c:5-7）。
- `while(0)` 之后**不允许出现分号**（示例里 `while(0);` 是写错了——宏最后展开会多出一条空语句）。

```c
        /* 正确 */
        #define PRINT(s)   do{                    \
                printf("%s", s);                  \
        } while(0)                                 /* ← 无分号 */

        /* 错误 */
        #define PRINT(s)   do{                    \
                printf("%s", s);                  \
        } while(0);                                /* ← 多余分号 */
```

- 使用方式：调用处自己加 `;` 保持与函数调用形似（code_format.c:37）。

```c
        PRINT("hello");
```

## 5. 注释

### 5.1 函数头部 - 必须 Doxygen

所有对外/结构性函数**必须**用 Doxygen 注释块开头。统一模板（同 lyrics_demo.c `create_button_ui`）：

```c
/**
 * @brief 一句话说明
 * @description 详细说明（可选）
 * @param x 参数是什么
 * @param y 参数是什么
 * @return successfully for 0, and failed for 1
 */
static int create_button_ui(lv_obj_t* container)
```

### 5.2 函数体内

- 正文注释用 **`//`**（code_format.c:19,228 等）。
- 不使用 `/* */` 做行内小注释；`/* ... */` 只允许文件头大块说明（见本规范用具与 lyrics_demo.c 头部）。

```c
        /* 正确 */
        int x = 0;  // 计数器

        /* 错误: 行内块注释做小注释 */
        int x = 0;  /* 计数器 */
```

## 6. 命名

- 函数与变量统一 **snake_case**。
- 类型名以 `_t` 后缀：`button_type_t`、`view_data_t`（lyrics_demo.c:29,71）。
- 构造/初始化类函数：`create_xxx_ui(lv_obj_t* parent)` / `init_xxx`，返回 `0` 成功 `1` 失败并与原例 `create_button_ui` 保持一致。
- 模块内的静态状态合并到一个结构：`static view_data_t view_data;`（lyrics_demo.c:79）。

## 7. 文件组织

- 文件顶部必须有 `/** @file ... @brief ... */` Doxygen 头（含版权/用法说明）。
- 包含顺序：C 标准库 → 第三方库 → 本项目头文件（同 lyric_demo.c 头部 include 顺序）。
- 代码分节使用带星号框的分节注释：`/* CONFIG */`、`/* STATE */`、`/* UI BUILD */`、`/* PUBLIC */`，中间留空行。

## 8. 与 clang-format 的关系

- 本文件未完全搬入 `.clang-format` 的事项说明见 `.clang-format` 头注释。
- `.clang-format` 会自动统一大部分规则；**Tab 使用**这一点已通过 UseTab 设置为 Always 处理。
- 生成/核对命令：

```bash
clang-format --style=file --dump-config > /dev/null && echo syntax ok
```

本规范不要求立即对现有代码执行格式化，先按此人工编写，逐步通过 clang-format 兼容验证。
