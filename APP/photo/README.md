# photo_app —— 光敏电阻

AO 接 PA5，用 ADC1 采 0~3.3V 连续电压，换算成 0~100 的光照分值；
DO 接 PB0（上拉输入），只保留做接线调试，不参与控制。

| 项目 | 内容 |
|---|---|
| 头文件 | `photo_app.h` |
| 引脚 | AO = PA5（ADC1_IN5）；DO = PB0 |
| 依赖 | 无 |
| 资源 | ADC1 + 2 个 GPIO |

## 接线

| 模块引脚 | MCU | 说明 |
|---|---|---|
| AO | PA5 | 模拟量，0~3.3V |
| DO | PB0 | 比较器输出，仅调试 |
| VCC / GND | 3V3 / GND | **必须 3.3V**，5V 会打坏 PA5 |

## 可改的宏

| 宏 | 默认 | 说明 |
|---|---|---|
| `PHOTO_SCORE_INVERT` | 1 | 1 = 越亮分值越大（本模块）；模块方向相反时改 0 |
| `PHOTO_DARK_SCORE` | 30 | 默认判暗阈值（分值） |
| `PHOTO_DARK_HYST` | 3 | 判暗回差 |
| `PHOTO_DO_DARK_LEVEL` | 1 | DO 等于该电平时算"暗" |
| `PHOTO_AO_PORT/PIN/RCC/CHANNEL` | PA5 / ADC 通道 5 | 换 AO 引脚时 4 个一起改 |
| `PHOTO_DO_PORT/PIN/RCC` | PB0 | 换 DO 引脚 |

## 函数

| 函数 | 返回 | 说明 |
|---|---|---|
| `void photo_init(void)` | — | 配 PA5 模拟输入、PB0 上拉输入，并校准 ADC1 |
| `uint16_t photo_ao_raw(void)` | 0~4095 | 采一次 AO |
| `uint16_t photo_ao_avg(uint8_t times)` | 0~4095 | 连采 `times` 次求平均（0 按 1 次算） |
| `uint16_t photo_raw_to_mv(uint16_t raw)` | 0~3300 | 原始值换算成毫伏 |
| `uint8_t photo_score(uint16_t raw)` | 0~100 | 换算成光照分值 |
| `uint8_t photo_score_is_dark(uint8_t score, uint8_t dark_score)` | 1=暗 / 0=亮 | 低于阈值判暗，回到 阈值+3 以上才判亮 |
| `uint8_t photo_do_raw(void)` | 0/1 | 直接读 DO 电平（无滤波） |
| `uint8_t photo_do_get(void)` | 0/1 | 读 DO 并做 5 取 3 表决 |
| `uint8_t photo_is_dark(void)` | 1=暗 / 0=亮 | 用 DO 判明暗（保留接口） |

## 示例

```c
#include "photo_app.h"
#include "led_app.h"

uint16_t ao;
uint8_t  score;
uint16_t mv;

photo_init();

ao    = photo_ao_avg(8);   /* 0~4095 */
score = photo_score(ao);    /* 0~100，越亮越大 */
mv    = photo_raw_to_mv(ao);

if (photo_score_is_dark(score, PHOTO_DARK_SCORE))
{
    led0_on();              /* 偏暗：点亮 D0 */
}
```

## 注意

- 3 针光敏模块**没有 AO**（只有 VCC/GND/DO），接上去 PA5 悬空，分值是恒定的 50 左右。
- 模块**必须 3.3V 供电**：5V 时 AO 满幅 5V，会先把 PA5 读数顶到 4095 再打坏引脚。
- DO 的门槛由模块上的蓝色电位器决定，代码改不了；要代码调阈值就用 AO + `photo_score_is_dark()`。
- `photo_score_is_dark()` 内部用 `static` 记上次结果实现回差，同一套阈值只在一个地方调用，别混用。
