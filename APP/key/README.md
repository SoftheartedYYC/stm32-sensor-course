# key_app —— 四个独立按键

PE0~PE3 四个按键，上拉输入、按下读到低电平。驱动里做了 50ms 消抖和 500ms 长按连发。

> 这是**轮询版**：主循环每 10ms 调一次 `key_scan()`，只有"按下"和"连发"两种事件，
> 没有短按/长按/双击。要这些手势就用中断版：[APP/interrupt](../interrupt/README.md)
>（本工程 `main.c` 初始化的是中断版）。

| 项目 | 内容 |
|---|---|
| 头文件 | `key_app.h` |
| 引脚 | PE0 ~ PE3 |
| 依赖 | 无（但要求每 10ms 调一次 `key_scan()`） |
| 资源 | 4 个上拉输入 |

## 接线

| 按键 | 引脚 | 功能（本工程用法，驱动本身不关心） |
|---|---|---|
| KEY0 | PE0 | 选择阈值项 |
| KEY1 | PE1 | 加 |
| KEY2 | PE2 | 减 |
| KEY3 | PE3 | 静音 |

按键一端接引脚，另一端接 GND，不需要外部上拉。

## 函数

| 函数 | 说明 |
|---|---|
| `void key_init(void)` | PE0~PE3 配成上拉输入 |
| `uint8_t key_scan(void)` | 每 10ms 调一次；返回 `KEY_NONE` 或一个按键事件 |

## 事件编码

- 低 4 位是键号：`KEY_ID_0` = 1 … `KEY_ID_3` = 4（对应 PE0~PE3）。
- bit7（`KEY_EVENT_REPEAT` = 0x80）表示这是长按连发，不是第一次按下。
- 消抖 50ms 后报第一次；按住 500ms 起，每 100ms 再报一次（带连发标志）。

## 示例

```c
#include "key_app.h"

uint8_t ev;
uint8_t id;
uint8_t rep;

key_init();

/* 放在 10ms 周期的循环里 */
ev = key_scan();
if (ev != KEY_NONE)
{
    id  = ev & 0x0F;                        /* 1~4 = KEY0~KEY3 */
    rep = (ev & KEY_EVENT_REPEAT) ? 1 : 0;  /* 1 = 长按连发 */

    if ((id == KEY_ID_0) && (rep == 0))
    {
        /* 只在"按下 PE0 那一下"执行一次 */
    }
}
```

## 注意

- 必须持续每 10ms 调用，调用太慢消抖会失效。
- 不想响应连发就判断 `rep == 0`。
- 一次调用最多返回一个事件；同时按两个键，另一个会在下一拍返回。
- 换引脚改 `KEY_PORT` 和 `KEY0_PIN ~ KEY3_PIN`。
