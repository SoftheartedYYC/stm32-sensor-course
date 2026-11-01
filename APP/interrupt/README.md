# interrupt —— 中断驱动合集

这里放"用中断实现更合适"的驱动；轮询版保留在原模块目录，方便对照。

| 模块 | 文件 | 中断源 | 一句话 |
|---|---|---|---|
| 1ms 时基 | `tick_irq.c/.h` | TIM6 更新中断 | 毫秒计数 + 顺序调用钩子，按键/矩阵共用 |
| 蜂鸣器 | `buzzer_irq.c/.h` | TIM2 更新中断 | 无源：每 185µs 翻转 PB10 出方波；有源：1ms 计时 |
| 独立按键 | `key_irq.c/.h` | EXTI0~EXTI3 | 短按 / 长按 / 双击 / 连发，1ms 时基消抖 |
| 4×4 矩阵键盘 | `matrix_irq.c/.h` | TIM6 的 1ms 钩子 | 每 1ms 扫一行，4ms 扫完一轮 |

## 初始化顺序

```c
TICK_Init();        /* 先配好 TIM6 时基，但还不启动 */
KeyIRQ_Init();      /* 配 EXTI，并把按键状态机挂到 1ms 钩子 */
MatrixIRQ_Init();   /* 配矩阵行列，并把扫描函数挂到 1ms 钩子 */
/* ... 其它模块初始化 ... */
TICK_Start();       /* 钩子都挂好了，最后启动 1ms 心跳 */
```

顺序不能反：`TICK_Start()` 之前要把钩子挂完，否则前几毫秒的按键/扫描会漏掉。

---

## tick_irq —— 1ms 时基（TIM6）

| 项目 | 内容 |
|---|---|
| 头文件 | `tick_irq.h` |
| 资源 | TIM6（基本定时器，不占引脚）+ 更新中断 |
| 依赖 | 无 |

| 函数 | 说明 |
|---|---|
| `void TICK_Init(void)` | 72MHz/(71+1)=1MHz，ARR=999 → 1ms 中断；先不启动 |
| `void TICK_Start(void)` | 启动心跳 |
| `uint32_t TICK_GetMs(void)` | 开机以来的毫秒数 |
| `uint8_t TICK_AddHook(void (*fn)(void))` | 挂一个每 1ms 调用的函数（最多 4 个） |

注意：

- 钩子在中断里执行，必须短小（读几个 IO、跑个小状态机），**不能 delay、printf、等标志位**。
- `TICK_GetMs()` 的 32 位计数会溢出，但用 `(now - t) >= n` 这种减法比较，溢出后仍然正确。
- 优先级 3，低于数码管（TIM4）、蜂鸣器（TIM2）和按键 EXTI。

---

## key_irq —— 独立按键（EXTI，短按 / 长按 / 双击）

| 项目 | 内容 |
|---|---|
| 头文件 | `key_irq.h` |
| 引脚 | PE0~PE3（上拉输入，按下 = 低） |
| 依赖 | 先 `TICK_Init()`，最后 `TICK_Start()` |
| 资源 | EXTI0~EXTI3 + 1ms 钩子 + 8 字节事件队列 |

### 接线

| 按键 | 引脚 | 说明 |
|---|---|---|
| KEY0 | PE0 | 一端接引脚，另一端接 GND |
| KEY1 | PE1 | 同上 |
| KEY2 | PE2 | 同上 |
| KEY3 | PE3 | 同上 |

### 参数

| 宏 | 默认 | 说明 |
|---|---|---|
| `KEYIRQ_DEBOUNCE_MS` | 20 | 边沿后稳定 20ms 才认 |
| `KEYIRQ_LONG_MS` | 500 | 按住满 500ms 算长按 |
| `KEYIRQ_DOUBLE_MS` | 300 | 两次短按间隔小于 300ms 算双击 |
| `KEYIRQ_REPEAT_MS` | 100 | 长按后每 100ms 连发一次 |

### 事件

| 事件 | 什么时候报 | 说明 |
|---|---|---|
| `KEY_EVENT_SHORT` | 松开后约 300ms | 等双击窗口过去、没有第二下才报（区分双击的代价） |
| `KEY_EVENT_LONG` | 按住满 500ms | 立刻报，不用等松开 |
| `KEY_EVENT_DOUBLE` | 第二次松开时 | 两次短按间隔小于 300ms |
| `KEY_EVENT_REPEAT` | 长按后每 100ms | 连发，方便快速加减阈值 |

编码：低 4 位是键号，高 4 位是事件类型，例 `KEY_ID_1 | KEY_EVENT_DOUBLE` 表示 KEY1 双击；
取键号用 `ev & KEY_ID_MASK`，取事件用 `ev & KEY_EVENT_MASK`。

### 函数

| 函数 | 说明 |
|---|---|
| `void KeyIRQ_Init(void)` | PE0~PE3 上拉输入 + 4 条 EXTI（双边沿）+ NVIC，并挂 1ms 钩子 |
| `uint8_t KeyIRQ_Scan(void)` | 从队列取一个事件；没有返回 `KEY_NONE` |

### 示例

```c
#include "tick_irq.h"
#include "key_irq.h"

uint8_t ev;
uint8_t id;
uint8_t e;

TICK_Init();
KeyIRQ_Init();
TICK_Start();

while (1)
{
    ev = KeyIRQ_Scan();
    if (ev != KEY_NONE)
    {
        id = ev & KEY_ID_MASK;
        e  = ev & KEY_EVENT_MASK;

        if ((id == KEY_ID_0) && (e == KEY_EVENT_SHORT))  { /* PE0 短按 */ }
        if ((id == KEY_ID_0) && (e == KEY_EVENT_LONG))   { /* PE0 长按 */ }
        if ((id == KEY_ID_0) && (e == KEY_EVENT_DOUBLE)) { /* PE0 双击 */ }
        if ((id == KEY_ID_1) && (e == KEY_EVENT_REPEAT)) { /* PE1 连发 */ }
    }

    delay_ms(10);   /* 取事件的节奏，取慢了也不会丢（队列 8 个） */
}
```

### 注意

- EXTI 配的是**双边沿**：按下和松开都要知道，否则没法判断长按什么时候结束。
- 中断里只记"有边沿 + 时间"，20ms 后再读引脚确认，抖动不会误报。
- **短按要等 300ms 才报**（要先确认不是双击）；长按是满 500ms 立刻报。
- 双击的第二下如果按住超过 500ms，按长按算，不会再报双击。
- 事件队列只有 8 个：如果主循环很久不取（比如 1 秒），超过 8 次事件会被丢弃。
- `EXTI0_IRQHandler` ~ `EXTI3_IRQHandler` 只在这里定义，别在 `stm32f10x_it.c` 里重复写。
- 和轮询版 `APP/key` 不能同时初始化（引脚和 EXTI 会打架），二选一。
- 矩阵键盘没有这三类事件，仍然只返回键值 1~16。

### 按键功能由 main.c 决定

`KeyIRQ_Scan()` 只负责给出"哪个键、什么事件"，具体做什么在 `USER/main.c` 里写。
后续要设置阈值、切换界面、静音，直接在这个事件上挂自己的逻辑即可。

---

## matrix_irq —— 4×4 矩阵键盘（定时扫描）

| 项目 | 内容 |
|---|---|
| 头文件 | `matrix_irq.h` |
| 引脚 | 行 PF0~PF3（推挽输出）、列 PF4~PF7（上拉输入） |
| 依赖 | 先 `TICK_Init()`，最后 `TICK_Start()` |
| 资源 | 8 个 GPIO + 1ms 钩子 + 8 字节键值队列 |

### 接线

| 线 | MCU | 方向 |
|---|---|---|
| 行 L1~L4 | PF0 / PF1 / PF2 / PF3 | 推挽输出 |
| 列 C1~C4 | PF4 / PF5 / PF6 / PF7 | 上拉输入 |

### 键值

1~16，行优先：第 1 行 1~4、第 2 行 5~8、第 3 行 9~12、第 4 行 13~16。

### 函数

| 函数 | 说明 |
|---|---|
| `void MatrixIRQ_Init(void)` | 配 4 行输出、4 列输入，先选中第 0 行，并挂 1ms 钩子 |
| `uint8_t MatrixIRQ_Scan(void)` | 取一个键值；没有返回 0。按住不放只返回一次 |

### 示例

```c
#include "tick_irq.h"
#include "matrix_irq.h"

uint8_t k;

TICK_Init();
MatrixIRQ_Init();
TICK_Start();

while (1)
{
    k = MatrixIRQ_Scan();
    if (k != 0)
    {
        /* k = 1~16 */
    }

    delay_ms(10);
}
```

### 注意

- 扫描节奏：每 1ms 中断里"读上一行、驱动下一行"，4ms 扫完 4 行；连续 3 轮（约 12~16ms）一致才认按下。
- 读的是**上一拍**拉低的那一行，已经稳定 1ms，所以不需要消抖延时。
- **中断里绝对不能调 `delay_us()`**：它会重配 SysTick，把主循环正在进行的延时搞乱。
- 只支持单键：同时按两个键时，一轮里最后扫到的那个生效。
- `MKEY_STABLE_CYCLES` 调大更稳但更迟钝；调小响应快但可能被抖动骗到。
- 行列接反时把 `MKEY_ROW_SHIFT` / `MKEY_COL_SHIFT` 对调（4 / 0）。
- 和轮询版 `APP/matrixkey` 不能同时初始化，二选一。

---

## buzzer_irq —— 蜂鸣器（TIM2，类型靠宏切换）

轮询版（`APP/buzzer`）要主循环不停调 `BEEP_Poll()`，报警时只能借用 10ms 的按键等待窗口，
声音一段一段的，发声期间 CPU 也被占着。中断版让硬件定时器打拍子，主循环只调
`BuzzerIRQ_Start()` / `BuzzerIRQ_Stop()` / `BuzzerIRQ_Beep()`。

和轮询版共用同一份配置：[`APP/buzzer/buzzer_cfg.h`](../buzzer/buzzer_cfg.h)。
改里面的 `BUZZER_TYPE` 一个宏，就能在三种蜂鸣器之间切换，本模块和业务代码都不用改：

| `BUZZER_TYPE` | 蜂鸣器 | 中断里做的事 |
|---|---|---|
| `BUZZER_TYPE_ACTIVE_LOW` | 有源，低电平触发（最常见） | **不翻转**，只数 Beep 还剩多少 ms |
| `BUZZER_TYPE_ACTIVE_HIGH` | 有源，高电平触发 | 同上 |
| `BUZZER_TYPE_PASSIVE` | 无源，要方波才响 | 每半个周期翻转一次引脚，出 2.7kHz 方波 |

| 项目 | 内容 |
|---|---|
| 头文件 | `buzzer_irq.h`（内部已包含 `buzzer_cfg.h`） |
| 引脚 | PB10（推挽输出），换引脚改 `buzzer_cfg.h` |
| 依赖 | 无（不调 `delay_init()` 也能用） |
| 资源 | TIM2 时基 + 更新中断；有源连续鸣叫时连定时器都不用开 |

### 接线

| 模块引脚 | MCU | 说明 |
|---|---|---|
| IN | PB10 | 推挽输出 |
| VCC / GND | 3V3 / GND | 共地 |

### 定时器参数

| 项目 | 无源 `BUZZER_TYPE_PASSIVE` | 有源 `ACTIVE_LOW` / `ACTIVE_HIGH` |
|---|---|---|
| 定时器时钟 | 72MHz（APB1 定时器时钟） | 同左 |
| 预分频 | 71 → 1MHz，1 个计数 = 1µs | 同左 |
| 自动重装 | 184 → 每 185µs 一次中断 | 999 → 每 1ms 一次中断 |
| 中断里做的事 | 翻转 PB10，高低各 185µs → 约 2.7kHz | 只把剩余 ms 减 1，引脚电平不动 |
| 静音 | 关定时器 + 拉高 | 关定时器 + 回到静音电平 |

改 `BUZZER_FREQ_HZ`（默认 2700）后，重装值会自动跟着算，常见 2700 / 4000。

### 函数

| 函数 | 说明 |
|---|---|
| `void BuzzerIRQ_Init(void)` | 引脚推挽输出 + TIM2 时基 + 更新中断 + NVIC；初始化完静音 |
| `void BuzzerIRQ_Start(void)` | 连续鸣叫，直到 `Stop`；有源只给电平，无源启动定时器翻转 |
| `void BuzzerIRQ_Stop(void)` | 停中断、引脚回到静音电平，静音时零中断开销 |
| `void BuzzerIRQ_Beep(uint16_t ms)` | 响 ms 毫秒后自动停，**非阻塞** |
| `void BuzzerIRQ_SelfTest(void)` | 上电自检：响 200ms 后自动停 |
| `uint8_t BuzzerIRQ_IsOn(void)` | 1 = 正在响，0 = 静音 |

### 示例

```c
#include "stm32f10x.h"
#include "delay.h"
#include "buzzer_irq.h"

delay_init();
BuzzerIRQ_Init();      /* 上电自检会响 200ms */

BuzzerIRQ_Beep(200);   /* 响 200ms，函数立刻返回 */

BuzzerIRQ_Start();     /* 一直响 */
delay_ms(500);
BuzzerIRQ_Stop();
```

### 注意

- `TIM2_IRQHandler` 只能在工程里定义一次，本文件已经定义；别在 `stm32f10x_it.c` 里再写一个。
- TIM2 被这个模块占用了，别的模块不要再用 TIM2（PB10 不需要重映射，仍当普通 GPIO）。
- 中断优先级设为 2，比数码管的 TIM4（抢占 1）低一档，屏幕扫描优先。
- 静音时定时器是关掉的，不会白白进中断；`Beep` 结束后中断里会自动关。
- `BUZZER_INIT_SELFTEST`（默认 1，在 `buzzer_irq.h` 里）为 1 时，`BuzzerIRQ_Init()` 里会自动响 200ms 做上电自检；
  不想每次上电都响就改成 0。
- 有源蜂鸣器的上电自检同样有效：只输出电平，顺便确认类型和触发极性选对了。
- 无源才有"方波 / 频率"的概念；有源给方波只会发出"嗒嗒"声，所以驱动不会翻转它的引脚。
- 换引脚、改类型都去改 `APP/buzzer/buzzer_cfg.h`，不要改 `buzzer_irq.h`。