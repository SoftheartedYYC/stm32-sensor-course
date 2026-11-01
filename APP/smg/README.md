# smg —— 四位数码管

段选 A~DP = PD0~PD7（推挽输出），位选 D1~D4 = PG9~PG12（低电平选通，PNP 驱动）。
扫描交给 TIM4 的 1ms 中断，主循环只写显示缓存。

| 项目 | 内容 |
|---|---|
| 头文件 | `smg.h` |
| 引脚 | 段选 PD0~PD7；位选 PG9~PG12 |
| 依赖 | `delay_init()` |
| 资源 | TIM4 + 12 个 GPIO，占 TIM4 更新中断 |

## 接线

| 线 | MCU | 说明 |
|---|---|---|
| 段选 A~DP | PD0~PD7 | A→PD0 … DP→PD7，共阳、段码低电平点亮 |
| 位选 D1~D4 | PG9~PG12 | 低电平选通，D1→PG9 … D4→PG12 |

## 函数

| 函数 | 说明 |
|---|---|
| `void SMG_Init(void)` | 配 GPIO + TIM4 1ms 扫描中断（调用前先 `delay_init()`） |
| `void SMG_ShowNum(uint16_t value)` | 显示 0~9999，右对齐、前导位不亮；超范围显示 `----` |
| `void SMG_ShowTemp(int16_t temp_x10)` | 温度（0.1°C 单位）：253 → `25.3`，-50 → `-5.0` |
| `void SMG_ShowDashes(void)` | 显示 `----` |
| `void SMG_Clear(void)` | 四位全灭 |
| `void SMG_ShowOne(uint8_t pos, uint8_t num)` | 单独改一位：`pos` 0~3（左到右），`num` 0~9 / `SMG_MINUS` / `SMG_BLANK` |
| `void SMG_Display(const uint8_t *buf)` | 一次写 4 位：`buf[0]~buf[3]` 从左到右 |
| `void SMG_SelfTest(void)` | 逐位点亮自检，阻塞约 1.5s |

## 可改的宏

| 宏 | 默认 | 说明 |
|---|---|---|
| `SMG_COMMON_ANODE` | 1 | 1 = 共阳（段码低电平点亮）；0 = 共阴 |
| `SMG_DIG_ACTIVE_HIGH` | 0 | 0 = 位选低电平选通（PNP）；1 = 高电平选通 |
| `SMG_DIG_REVERSE` | 0 | 0 = PG9 最左；1 = PG12 最左 |
| `SMG_DIG_OUT_OD` | 0 | PNP 基极上拉到 5V 时改成 1 |
| `SMG_GHOST_US` | 10 | 换位前消影等待（µs），有串影加到 20~50 |
| `SMG_INIT_SELFTEST` | 0 | 改成 1 上电逐位自检 |
| `SMG_SEG_PORT/SHIFT/MASK` | GPIOD / 0 / 0x00FF | 换段选端口 |
| `SMG_DIG_PORT/SHIFT/MASK` | GPIOG / 9 / 0x000F | 换位选端口 |

## 示例

```c
#include "delay.h"
#include "smg.h"

delay_init();
SMG_Init();

SMG_ShowNum(1234);    /* 1234 */
SMG_ShowTemp(253);    /* 25.3 */
SMG_ShowDashes();     /* ---- */
```

## 注意

- 扫描在 TIM4 中断里做，OLED 刷新、DHT11 读取不会让数码管只亮一位。
- `TIM4_IRQHandler` 只允许在 `smg.c` 里定义一次，别在 `stm32f10x_it.c` 里重复写。
- PD0/PD1 在 144 脚封装上兼 OSC_IN/OSC_OUT，核心板焊了 HSE 晶振时这两脚不能当普通 IO 用。
