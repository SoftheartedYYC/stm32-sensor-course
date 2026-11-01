# rgb_app —— RGB 三色灯

R/G/B = PC6/PC7/PC8，用 TIM3 全重映射的三个通道输出 10kHz PWM，占空比 0~100 可调。
本工程模块是**共阳（低电平点亮）**。

| 项目 | 内容 |
|---|---|
| 头文件 | `rgb_app.h` |
| 引脚 | R = PC6（TIM3_CH1）、G = PC7（CH2）、B = PC8（CH3） |
| 依赖 | 无 |
| 资源 | TIM3 + 3 个复用推挽 GPIO |

## 接线

| 模块引脚 | MCU | 说明 |
|---|---|---|
| R | PC6 | TIM3_CH1（全重映射） |
| G | PC7 | TIM3_CH2（全重映射） |
| B | PC8 | TIM3_CH3（全重映射） |
| 公共端 | 3V3（共阳） | 低电平点亮；共阴模块公共端接 GND |

## 可改的宏

| 宏 | 默认 | 说明 |
|---|---|---|
| `RGB_USE_PWM` | 1 | 1 = TIM3 PWM 可调色；0 = 普通推挽 GPIO（只能亮/灭） |
| `RGB_COMMON_ANODE` | 1 | 1 = 共阳（低电平点亮）；0 = 共阴 |
| `RGB_R_PIN` / `RGB_G_PIN` / `RGB_B_PIN` | PC6 / PC7 / PC8 | 换引脚 |
| `RGB_MAX` | 100 | 亮度满量程 |

## 函数

| 函数 / 类型 | 说明 |
|---|---|
| `void rgb_init(void)` | 配三路 PWM，先熄灭 |
| `void rgb_set(uint8_t r, uint8_t g, uint8_t b)` | 三路亮度 0~100，超出按 100 算 |
| `void rgb_show(rgb_color_t color)` | 用预定义颜色 |
| `void rgb_off(void)` | 三色全灭 |
| `void rgb_self_test(void)` | 自检：红 → 绿 → 蓝 → 白 各 500ms，最后熄灭（阻塞约 2s） |

预定义颜色（`rgb_color_t`）：`RGB_BLACK`、`RGB_RED`、`RGB_GREEN`、`RGB_BLUE`、
`RGB_YELLOW`、`RGB_CYAN`、`RGB_MAGENTA`、`RGB_WHITE`。

## 示例

```c
#include "delay.h"
#include "rgb_app.h"

rgb_init();

rgb_show(RGB_RED);
delay_ms(500);

rgb_set(0, 100, 0);   /* 纯绿，亮度 100 */
delay_ms(500);

rgb_off();
```

## 注意

- `RGB_COMMON_ANODE` 设反会表现为"全亮 / 颜色相反"。
- 把 `RGB_INIT_SELFTEST` 改成 1，`rgb_init()` 会自动跑一遍红→绿→蓝→白自检（约 2s），
  用来确认模块、共阴共阳和三根颜色线有没有接对；也可以手动调 `rgb_self_test()`。
- 只有 `rgb_init()` 会配置 TIM3，别在主程序里再初始化 TIM3。
- `RGB_USE_PWM = 0` 时 `rgb_set()` 只认 0 / 非 0。
- PC8 兼 SDIO_D0，接 SD 卡时不能同时用 RGB。
