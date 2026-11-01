# led_app —— 板载 LED

两颗板载 LED：D0 做状态指示，D1 做心跳灯，都是**低电平点亮**。

| 项目 | 内容 |
|---|---|
| 头文件 | `led_app.h` |
| 引脚 | D0 = PB5；D1 = PE5 |
| 依赖 | 无（不调 `delay_init()` 也能用） |
| 资源 | 普通推挽 GPIO |

## 接线

| 信号 | 引脚 | 说明 |
|---|---|---|
| D0 | PB5 | 阳极经 510Ω 接 3V3，阴极进 MCU |
| D1 | PE5 | 同上 |

## 函数

| 函数 | 说明 |
|---|---|
| `void led_init(void)` | 两颗灯配推挽输出并熄灭 |
| `void led_on(uint8_t id)` | id = 0 → D0(PB5)；id = 1 → D1(PE5) |
| `void led_off(uint8_t id)` | 熄灭指定灯 |
| `void led_blink(uint8_t id)` | 翻转一次（配合 `delay_ms()` 就是闪） |
| `void led_all(uint8_t on)` | 非 0 = 两颗都亮，0 = 都灭 |
| `led0_on()` / `led0_off()` / `led0_turn()` | 无参数兼容接口 |
| `led1_on()` / `led1_off()` / `led1_turn()` | 同上 |

## 示例

```c
#include "stm32f10x.h"
#include "delay.h"
#include "led_app.h"

led_init();

led0_on();       /* D0 亮 */
delay_ms(500);
led0_off();      /* D0 灭 */

while (1)        /* D1 心跳：500ms 翻转一次 */
{
    led1_turn();
    delay_ms(500);
}
```

## 注意

- 低电平点亮：`led_on()` 内部是 `GPIO_ResetBits`。
- `id` 只认 0 和 1，传别的值什么也不做。
- 引脚宏在 `led_app.c` 顶部：`LED0_PORT`、`LED0_PIN`、`LED1_PORT`、`LED1_PIN`。
