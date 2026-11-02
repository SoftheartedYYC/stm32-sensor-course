# stepper_app —— 步进电机（TIM7 中断发脉冲）

EN = PE12、STP = PE13、DIR = PE14，推挽输出。**用 TIM7 更新中断翻转 STP**，一个完整脉冲走一步；
定时器发脉冲，主循环不参与，刷 OLED / 读传感器都不会影响步频。

| 项目 | 内容 |
|---|---|
| 头文件 | `stepper_app.h` |
| 引脚 | EN = PE12；STP = PE13；DIR = PE14 |
| 资源 | TIM7（APB1，72MHz）+ 3 个推挽 GPIO |
| 依赖 | 无（不调 `delay_init()` 也能用） |
| 中断 | `TIM7_IRQHandler` 在本文件里定义，全工程只能有一份 |

## 接线

| 驱动器引脚 | MCU | 说明 |
|---|---|---|
| EN | PE12 | 使能，默认**低电平有效**（`STEPPER_EN_ACTIVE_LEVEL` 可改） |
| STP | PE13 | 脉冲，每来一个脉冲走一步 |
| DIR | PE14 | 方向，默认**高电平 = 顺时针**（`STEPPER_DIR_CW_LEVEL` 可改） |
| VCC / GND | 按模块要求 | **必须和 STM32 共地**，否则 STP 的电平没有参考 |
| A+ / A- / B+ / B- | 电机线圈 | 接驱动器功率侧，不要直接接单片机 |
| V+ / GND（功率） | 电机电源 | 按电机额定电压/电流，驱动器上设好限流 |

> 常见模块（TB6600、A4988、DRV8825）的 EN 都是低电平使能；如果手上的板子是“高电平使能”，
> 把 `STEPPER_EN_ACTIVE_LEVEL` 改成 1。

## 步距角：只改一个宏

```c
#define STEPPER_STEP_ANGLE STEPPER_ANGLE_18   /* 1.8°/步，200 步/圈 */
// #define STEPPER_STEP_ANGLE STEPPER_ANGLE_09  /* 0.9°/步，400 步/圈 */
```

改成 `STEPPER_ANGLE_09` 后：

| | 1.8° | 0.9° |
|---|---|---|
| 步/圈 | 200 | 400 |
| `stepper_move_deg(90)` | 50 步 | 100 步 |
| 400 步/秒时转速 | 2 转/秒 | 1 转/秒 |

`stepper_steps_per_rev()` 返回这个值，`stepper_move_deg()` 按它换算。

## 定时器参数

| 项目 | 值 |
|---|---|
| 定时器 | TIM7（APB1，72MHz） |
| 预分频 | 71 → 1MHz，1 个计数 = 1µs |
| 自动重装 | 半个脉冲周期 − 1 |
| 中断里 | 翻转 STP；定长模式下数剩余步数，数完自动停 |
| 步频范围 | 10 ~ 20000 步/秒（脉冲宽度被限制在 ≥ 5µs） |

例：400 步/秒 → 半个周期 1250µs → STP 高 1250µs、低 1250µs，一个周期正好一步。

## 函数

| 函数 | 说明 |
|---|---|
| `void stepper_init(void)` | PE12/PE13/PE14 推挽输出 + TIM7 时基/中断/NVIC；初始化完**失能、不转** |
| `void stepper_enable(uint8_t on)` | 使能 / 失能电机；失能会立刻停脉冲 |
| `void stepper_set_dir(uint8_t dir)` | `STEPPER_DIR_CW` / `STEPPER_DIR_CCW` |
| `void stepper_set_speed(uint16_t sps)` | 步频（步/秒），运行中也能改 |
| `uint16_t stepper_get_speed(void)` | 读当前步频 |
| `void stepper_move_steps(uint32_t steps)` | 按当前方向走 N 步，**非阻塞** |
| `void stepper_move_deg(int32_t deg)` | 走指定角度，正数顺时针、负数逆时针，**非阻塞** |
| `void stepper_run(uint8_t dir)` | 连续转，直到 `stepper_stop()` |
| `void stepper_stop(void)` | 立刻停止发脉冲 |
| `uint8_t stepper_is_busy(void)` | 1 = 还在走 |
| `uint16_t stepper_steps_per_rev(void)` | 一圈多少步（200 / 400） |

## 示例

```c
#include "stepper_app.h"

stepper_init();
stepper_set_speed(400);        /* 400 步/秒：1.8°电机约 2 转/秒 */
stepper_enable(1);

stepper_move_deg(90);          /* 转 90°，立刻返回 */
while (stepper_is_busy())      /* 等它转完（这里可以顺便刷显示） */
{
}

stepper_move_steps(200);       /* 1.8°/步 -> 正好一圈 */
while (stepper_is_busy())
{
}

stepper_run(STEPPER_DIR_CCW);  /* 一直反转 */
delay_ms(2000);
stepper_stop();
stepper_enable(0);             /* 松开电机（失能后线圈没电流，可以手转） */
```

## 注意

- **TIM7 被这个模块独占**，别在别处再初始化 TIM7，也别再写 `TIM7_IRQHandler`。
- 中断优先级设为 2，比数码管扫描（TIM4，抢占 1）低一档，屏幕扫描优先。
- 步频不要一上来就给很高：步进电机有启动频率，直接 5kHz 起步多半会堵转/丢步。
  要快就分几段加速（例如 400 → 800 → 1200），或者先用默认 400 试。
- 脉冲低电平时间被限制在 ≥ 5µs，兼容常见驱动器的最小脉宽要求。
- `stepper_stop()` 是“急停”，电机停在当前步；高速时会有一点惯性滑行，属正常现象。
- `stepper_enable(0)` 后驱动器不再给线圈电流，电机可以自由转动（也保持不住位置）。
- 3.3V 的 STP 电平对 5V 驱动器一般够用（多数驱动器输入是光耦，3.3V 也能触发）；
  如果驱动不了，加一级三极管/电平转换。
- 电机电源和 MCU 电源最好分开供电、共地，避免电机电流把 MCU 拉复位。
