# iic —— 软件 I2C 总线工具

PB6/PB7 上的软件 I2C 公共函数：配引脚、判断总线是否空闲、探测从机地址、总线被拉死后的恢复。
原来这些函数写在应用层里，现在独立出来，OLED / BH1750 相关的应用代码只调用本模块。

| 项目 | 内容 |
|---|---|
| 头文件 | `iic.h` |
| 引脚 | SCL = PB7，SDA = PB6（开漏输出，模块 4.7k 上拉） |
| 依赖 | `delay_init()`（`IIC_BusIdle` / `IIC_Probe` / `IIC_BusRecover` 里用 `delay_us`） |
| 资源 | 2 个开漏 GPIO |

## 接线

| 信号 | 引脚 | 说明 |
|---|---|---|
| SCL | PB7 | 开漏，靠外部上拉拉高 |
| SDA | PB6 | 开漏，靠外部上拉拉高 |
| VCC / GND | 3V3 / GND | OLED 和 BH1750 都挂在同一条总线上 |

## 函数

| 函数 | 说明 |
|---|---|
| `void IIC_Init(void)` | 把 SCL/SDA 配成开漏输出并放开总线 |
| `uint8_t IIC_BusIdle(void)` | 1 = 总线正常空闲；0 = 没上拉或被器件拉死 |
| `uint8_t IIC_Probe(uint8_t addr8)` | 探测 8 位写地址：1 = 有 ACK，0 = 没人应答 |
| `void IIC_BusRecover(void)` | 放 9 个 SCL 时钟 + STOP，把卡住的从机放出来 |

## 示例

```c
#include "iic.h"

delay_init();
IIC_Init();

if (IIC_BusIdle() == 0)
{
    /* 线上没有上拉：检查 OLED/BH1750 是否接好、是否供电 */
}

if (IIC_Probe(0x78) != 0)   /* OLED 的写地址 */
{
    /* 屏在线 */
}

IIC_BusRecover();           /* 读失败/总线卡死时救一次 */
```

## 注意

- `IIC_BusIdle()` 会先把两根线拉低放电、再放开，所以它自己会调用 `IIC_Init()`，不需要事先配脚。
- 这是**总线级**的工具；OLED 和 BH1750 驱动内部还有各自优化过的读写时序（OLED 约 300kHz、BH1750 约 100kHz），
  两边的字节传输没有走这里，改的时候别混。
- 反过来说，地址探测、上电检查、总线恢复这类"公共动作"都走本模块，应用层里不再出现 GPIO 位操作。
- 换引脚改 `iic.h` 的 `IIC_PORT / IIC_SCL_PIN / IIC_SDA_PIN / IIC_RCC`；
  注意 OLED 和 BH1750 驱动里也各有一份引脚宏，三处要一起改。
