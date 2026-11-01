# bh1750 —— 照度传感器

BH1750 数字照度传感器，I2C 接口，和 OLED 共用 PB6/PB7 这条软件 I2C 总线。
地址 0x23（ADDR 接 GND）或 0x5C（ADDR 接 VCC），`BH1750_Init()` 会自动依次探测。

| 项目 | 内容 |
|---|---|
| 头文件 | `bh1750.h` |
| 引脚 | SDA = PB6；SCL = PB7（端口固定 GPIOB） |
| 依赖 | `delay_init()` |
| 资源 | 2 个开漏 GPIO，与 OLED 共用 |

## 接线

| 模块引脚 | MCU | 说明 |
|---|---|---|
| SDA | PB6 | 与 OLED 的 SDA 并接 |
| SCL | PB7 | 与 OLED 的 SCL 并接 |
| ADDR | GND 或 VCC | 接地 0x23，接 VCC 0x5C |
| VCC / GND | 3V3 / GND | 共地 |

## 函数

| 函数 | 返回 | 说明 |
|---|---|---|
| `u8 BH1750_Init(void)` | 1 = 成功，0 = 没找到 | 先试 0x23 再试 0x5C；内部 `delay_ms(10)` + `delay_ms(180)`，阻塞约 190ms |
| `u8 BH1750_ReadLux_x10(u16 *lux)` | 1 = 成功，0 = 失败 | 读一次照度 |

## 示例

```c
#include "delay.h"
#include "bh1750.h"

u16 lux;

delay_init();

if (BH1750_Init())
{
    if (BH1750_ReadLux_x10(&lux))
    {
        /* lux 单位是 lx，例如 300 */
    }
}
```

## 注意

- 函数名里的 `_x10` 是历史名字，**输出单位是 lx**（内部按 `raw*10/12` 换算并四舍五入），不是 0.1 lx。
- 连续测量模式（H 分辨率），约 120ms 更新一次；读得比这快拿到的还是同一个值。
- 引脚宏是 `bh1750.c` 里的 `BH1750_SDA_PIN` / `BH1750_SCL_PIN`，端口写死 GPIOB。
- 和 OLED 共用总线：上拉由模块提供，不要再并电阻；初始化顺序先 OLED 后 BH1750。
- 上电探测、总线空闲检查、总线被拉死后的恢复用 `APP/iic`（`IIC_BusIdle()` / `IIC_BusRecover()`）。
