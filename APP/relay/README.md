# relay_app —— 三路继电器

IN1 = PC0、IN2 = PC1、IN3 = PC2，推挽输出，**高电平吸合**。上电先把三路写成"释放"再切输出，不会"啪"一下。

| 项目 | 内容 |
|---|---|
| 头文件 | `relay_app.h` |
| 引脚 | IN1 = PC0；IN2 = PC1；IN3 = PC2 |
| 依赖 | 无 |
| 资源 | 3 个推挽 GPIO |

## 接线

| 模块引脚 | MCU | 说明 |
|---|---|---|
| IN1 | PC0 | 高电平吸合 |
| IN2 | PC1 | 高电平吸合 |
| IN3 | PC2 | 高电平吸合 |
| VCC / GND | 5V / GND | 按模块要求供电，**信号地必须和 MCU 共地** |
| COM / NO / NC | 被控设备 | 吸合时 COM-NO 导通 |

## 函数

| 函数 | 说明 |
|---|---|
| `void relay_init(void)` | PC0/PC1/PC2 推挽输出，并让三路都释放 |
| `void relay_set(uint8_t ch, uint8_t on)` | `ch` = `RELAY_CH1` / `RELAY_CH2` / `RELAY_CH3`；`on` 非 0 吸合 |
| `void relay_on(uint8_t ch)` | 单路吸合 |
| `void relay_off(uint8_t ch)` | 单路释放 |
| `void relay_toggle(uint8_t ch)` | 翻转一路 |
| `void relay_all(uint8_t on)` | 三路一起吸合 / 释放（按 `RELAY_CH_COUNT` 循环） |
| `uint8_t relay_get(uint8_t ch)` | 读输出锁存器：1 = 吸合，0 = 释放 |

## 示例

```c
#include "delay.h"
#include "relay_app.h"

relay_init();

relay_set(RELAY_CH1, 1);   /* 继电器 1 吸合 */
delay_ms(1000);
relay_off(RELAY_CH1);      /* 继电器 1 释放 */

relay_on(RELAY_CH2);
relay_toggle(RELAY_CH3);   /* 继电器 3（PC2）翻转 */
```

## 再加一路要改哪里

三路都是同一套代码，加第四路只要三步：

1. `relay_app.h`：加 `#define RELAY4_PIN GPIO_Pin_3`、`#define RELAY_CH4 4`，把 `RELAY_CH_COUNT` 改成 4；
2. `relay_app.c`：引脚表补一行 `RELAY4_PIN`，`RELAY_ALL_PINS` 加上它；
3. 主循环里照常 `relay_set(RELAY_CH4, 1)`。

## 注意

- 触发极性由 `RELAY_ON_LEVEL` 决定：1 = 高电平吸合（本模块），0 = 低电平吸合；三路共用。
- `relay_get()` 读的是 MCU 输出锁存器，不是触点反馈。
- 3.3V 高电平驱动 5V 模块可能不够可靠，必要时换 3.3V 兼容模块或加三极管。
- 强电侧接线和调试务必断电。
