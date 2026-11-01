# relay_app —— 两路继电器

IN1 = PC0、IN2 = PC1，推挽输出，**高电平吸合**。上电先把两路写成"释放"再切输出，不会"啪"一下。

| 项目 | 内容 |
|---|---|
| 头文件 | `relay_app.h` |
| 引脚 | IN1 = PC0；IN2 = PC1 |
| 依赖 | 无 |
| 资源 | 2 个推挽 GPIO |

## 接线

| 模块引脚 | MCU | 说明 |
|---|---|---|
| IN1 | PC0 | 高电平吸合 |
| IN2 | PC1 | 高电平吸合 |
| VCC / GND | 5V / GND | 按模块要求供电，**信号地必须和 MCU 共地** |
| COM / NO / NC | 被控设备 | 吸合时 COM-NO 导通 |

## 函数

| 函数 | 说明 |
|---|---|
| `void relay_init(void)` | PC0/PC1 推挽输出，并让两路都释放 |
| `void relay_set(uint8_t ch, uint8_t on)` | `ch` = `RELAY_CH1` / `RELAY_CH2`；`on` 非 0 吸合 |
| `void relay_on(uint8_t ch)` | 单路吸合 |
| `void relay_off(uint8_t ch)` | 单路释放 |
| `void relay_toggle(uint8_t ch)` | 翻转一路 |
| `void relay_all(uint8_t on)` | 两路一起吸合 / 释放 |
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
```

## 注意

- 触发极性由 `RELAY_ON_LEVEL` 决定：1 = 高电平吸合（本模块），0 = 低电平吸合。
- `relay_get()` 读的是 MCU 输出锁存器，不是触点反馈。
- 3.3V 高电平驱动 5V 模块可能不够可靠，必要时换 3.3V 兼容模块或加三极管。
- 强电侧接线和调试务必断电。
