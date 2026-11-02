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

## 步距角 + 驱动器细分（两个宏）

```c
#define STEPPER_STEP_ANGLE STEPPER_ANGLE_18   /* 1.8°/步；0.9° 电机改成 STEPPER_ANGLE_09 */
#define STEPPER_MICROSTEP  1u                 /* 驱动器屏幕上的"细分"值，1 = 整步 */
```

一圈需要的脉冲数 = 整步步数 × 细分：

| | 1.8° 整步 | 0.9° 整步 |
|---|---|---|
| 整步 200 步/圈 | 200 | 400 |
| 细分 16（屏幕设 16） | 3200 | 6400 |
| `stepper_move_deg(90)`（细分 16） | 800 步 | 1600 步 |
| 400 步/秒、细分 16 时转速 | 0.125 转/秒 | 0.0625 转/秒 |

**这是最容易踩的坑**：驱动器屏幕上细分不是 1，程序里不改 `STEPPER_MICROSTEP`，
`stepper_move_deg(360)` 就只会转 360/细分 度。

同理，细分越大、同样步频下转得越慢。想要同样的转速，步频要乘细分倍数：
整步时 400 步/秒，细分 16 就要 6400 步/秒（仍在 20000 的上限内）。

`stepper_steps_per_rev()` 返回"一圈脉冲数"（含细分），`stepper_move_deg()` 按它换算。

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

## 配 Emm42 / 张大头这类闭环驱动器（本文电机）

资料：<https://blog.csdn.net/zhangdatou666/article/details/128320222>（Emm42_V4.2 说明书）

| 驱动器参数 | 值 | 本驱动对应 |
|---|---|---|
| 供电 | **7~28V**（别用 5V），和 MCU 共地 | 外部电源 |
| 信号电平 | 3.3~24V，支持共阳/共阴 | 3.3V 直接接 |
| 最大脉冲频率 | 约 120kHz | `STEPPER_SPS_MAX` 20000 步/秒，远低于上限 |
| 细分 | 1~256 任意 | `STEPPER_MICROSTEP` |
| 电机类型 | 0.9° / 1.8° 可选（默认 1.8°） | `STEPPER_STEP_ANGLE` |

接线方式决定极性，二选一：

| 接法 | 怎么接 | EN 极性 |
|---|---|---|
| **共阳**（和默认一致） | PUL+/DIR+/ENA+ 接 +5V，PUL-/DIR-/ENA- 接单片机 | 低电平使能：`STEPPER_EN_ACTIVE_LEVEL 0` |
| **共阴** | PUL+/DIR+/ENA+ 接单片机，PUL-/DIR-/ENA- 接 GND | 高电平使能：改成 `1` |

方向（DIR）和接法/电机安装方向有关：上电转的方向不对，就把 `STEPPER_DIR_CW_LEVEL` 取反（0/1 互换），不用改接线。

屏幕上还要确认（驱动板自带屏幕，按 SET 改）：

- 控制模式 = **脉冲 / Dir-Step**（不是串口/CAN）；
- 细分 = 1~16（和 `STEPPER_MICROSTEP` 一致）；
- 电机类型 = 0.9° 或 1.8°（和 `STEPPER_STEP_ANGLE` 一致）；
- 电流按电机额定值设（常见 1.5~2A），太大电机发烫、太小力矩不够。

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
