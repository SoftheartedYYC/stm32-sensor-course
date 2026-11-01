# matrixkey —— 4×4 矩阵键盘

行线 PF0~PF3（推挽输出，扫描时逐行拉低），列线 PF4~PF7（上拉输入，按下读到低电平）。

> 这是**轮询版**：主循环调 `MATRIXKEY_Scan()`，按下时还会在里面 `delay_ms(10)` 消抖。
> 想把这个阻塞也去掉，用中断版：[APP/interrupt](../interrupt/README.md)
>（本工程 `main.c` 初始化的是中断版）。

| 项目 | 内容 |
|---|---|
| 头文件 | `matrixkey.h` |
| 引脚 | 行 PF0~PF3；列 PF4~PF7 |
| 依赖 | `delay_init()`（扫描里用 `delay_us` / `delay_ms`） |
| 资源 | 8 个 GPIO |

## 接线

| 线 | MCU | 方向 |
|---|---|---|
| 行 L1~L4 | PF0 / PF1 / PF2 / PF3 | 推挽输出 |
| 列 C1~C4 | PF4 / PF5 / PF6 / PF7 | 上拉输入 |

## 键值

键值 1~16，行优先：

| 行 | 键值 |
|---|---|
| 第 1 行（PF0 拉低） | 1 2 3 4 |
| 第 2 行（PF1 拉低） | 5 6 7 8 |
| 第 3 行（PF2 拉低） | 9 10 11 12 |
| 第 4 行（PF3 拉低） | 13 14 15 16 |

## 函数

| 函数 | 说明 |
|---|---|
| `void MATRIXKEY_Init(void)` | 配 4 行推挽输出、4 列上拉输入 |
| `uint8_t MATRIXKEY_Scan(void)` | 返回 1~16，没有按键返回 0；按住不放不重复返回 |

## 示例

```c
#include "delay.h"
#include "matrixkey.h"

uint8_t k;

delay_init();
MATRIXKEY_Init();

/* 和 key_scan() 一起放在 10ms 拍里 */
k = MATRIXKEY_Scan();
if (k != 0)
{
    /* k = 1~16 */
}
```

## 注意

- 只有检测到按键时才 `delay_ms(10)` 消抖，没按键时只花几十微秒。
- 按住不放只返回一次（内部 `busy` 标志），松手再按才会再返回。
- 行列接反（行接 PF4~PF7、列接 PF0~PF3）：把 `MATRIX_ROW_SHIFT` 改成 4、`MATRIX_COL_SHIFT` 改成 0，扫描代码不用动。
- 不支持多键同时按；键值是行优先，不是标准计算器布局。
