# 传感器程序集合（STM32F103ZET6）

课程实验工程：`APP/` 下每个模块一个文件夹、一份 README（`interrupt/` 放中断版驱动）；
`USER/main.c` 按顺序初始化各模块，后续要用的功能直接写在 `main.c` 的 `while(1)` 里。

## 模块一览

| 模块 | 文件夹 | 头文件 | 引脚 | 说明 |
|---|---|---|---|---|
| 板载 LED | [APP/led](APP/led/README.md) | `led_app.h` | PB5 / PE5 | D0 状态灯、D1 心跳灯，低电平点亮 |
| 光敏电阻 | [APP/photo](APP/photo/README.md) | `photo_app.h` | PA5 / PB0 | AO 走 ADC1_IN5，DO 仅调试 |
| 独立按键 | [APP/key](APP/key/README.md) | `key_app.h` | PE0~PE3 | 50ms 消抖 + 长按连发（轮询版） |
| 独立按键（中断版） | [APP/interrupt](APP/interrupt/README.md) | `key_irq.h` | PE0~PE3 | 短按 / 长按 / 双击 / 连发，EXTI + 1ms 时基 |
| 矩阵键盘 | [APP/matrixkey](APP/matrixkey/README.md) | `matrixkey.h` | PF0~PF7 | 4×4，键值 1~16（轮询版） |
| 矩阵键盘（中断版） | [APP/interrupt](APP/interrupt/README.md) | `matrix_irq.h` | PF0~PF7 | TIM6 每 1ms 扫一行 |
| 蜂鸣器 | [APP/buzzer](APP/buzzer/README.md) | `buzzer_app.h` | PB10 | 有源/无源靠宏切换，软件翻转方波（轮询版） |
| 蜂鸣器（中断版） | [APP/interrupt](APP/interrupt/README.md) | `buzzer_irq.h` | PB10 | TIM2 更新中断，连续发声、不占主循环 |

> 蜂鸣器有两种驱动（轮询版 `APP/buzzer`、中断版 `APP/interrupt`），共用一份配置
> [`APP/buzzer/buzzer_cfg.h`](APP/buzzer/buzzer_cfg.h)：改里面的 `BUZZER_TYPE` 宏就能在
> **有源低电平触发 / 有源高电平触发 / 无源（要方波）** 之间切换，两个驱动和业务代码都不用动。
| 继电器 | [APP/relay](APP/relay/README.md) | `relay_app.h` | PC0 / PC1 | 高电平吸合 |
| RGB 灯 | [APP/rgb](APP/rgb/README.md) | `rgb_app.h` | PC6~PC8 | TIM3 PWM，共阳 |
| 数码管 | [APP/smg](APP/smg/README.md) | `smg.h` | PD0~PD7 / PG9~PG12 | TIM4 中断扫描 |
| OLED | [APP/oled](APP/oled/README.md) | `oled_app.h` | PB6 / PB7 | 软件 I2C，128×64 |
| 照度 BH1750 | [APP/bh1750](APP/bh1750/README.md) | `bh1750.h` | PB6 / PB7 | 与 OLED 共用总线 |
| 软件 I2C | [APP/iic](APP/iic/README.md) | `iic.h` | PB6 / PB7 | 总线空闲检测、地址探测、总线恢复 |
| DHT11 | [APP/dht11](APP/dht11/README.md) | `dht11_app.h` | PB11 | 温湿度 |
| DS18B20 | [APP/ds18b20](APP/ds18b20/README.md) | `ds18b20_app.h` | PB11 | 温度，与 DHT11 二选一 |
| 超声波 HC-SR04 | [APP/hcsr04](APP/hcsr04/README.md) | `hcsr04.h` | PA8 / PB12 | TIM1 输入捕获测距（main.c 暂未使用） |

函数、宏、示例和注意事项都在各自文件夹的 `README.md` 里；
引脚全表见 [IO分配表.md](IO分配表.md)，各模块的初始化都在 [USER/main.c](USER/main.c)；
配置宏和后续自己的定义写在 [USER/main.h](USER/main.h)。

## 目录结构

```
传感器程序集合/
├── APP/              各模块驱动（每个子目录一份 README）
├── USER/             main.c（初始化 + 主循环）、main.h（自己的定义）、Keil 工程
├── SYSTEM/           delay / sys / usart
├── CORE/             CMSIS + startup
├── STM32F10x_FWLib/  标准外设库 V3.5.0
├── OBJ/              编译输出
└── IO分配表.md        引脚分配表
```

## 快速开始

```c
#include "stm32f10x.h"
#include "delay.h"
#include "led_app.h"
#include "smg.h"

int main(void)
{
    delay_init();   /* 第一件事：SysTick 延时基准 */
    led_init();
    SMG_Init();

    while (1)
    {
        led1_turn();
        SMG_ShowNum(1234);
        delay_ms(500);
    }
}
```

顺序：先 `delay_init()`，再各模块 `_init()`，最后进主循环。
用到 `delay_us/delay_ms` 的驱动：`smg`、`buzzer`、`bh1750`、`dht11`、`ds18b20`、`matrixkey`。

## 共用资源

| 资源 | 谁用 | 注意 |
|---|---|---|
| PB6 / PB7 | OLED + BH1750 软件 I2C | 先 `oled_init()` 再 `BH1750_Init()`；总线探测/恢复用 `APP/iic` |
| PB11 | DHT11 / DS18B20 | 物理上二选一，只调一个 `_init()` |
| TIM3 | RGB PWM | 别在别处初始化 TIM3 |
| TIM4 | 数码管扫描 | 别在别处初始化 TIM4；`TIM4_IRQHandler` 在 `smg.c` |
| TIM2 | 中断版蜂鸣器 | 无源：每 185µs 更新中断翻转 PB10；有源：1ms 一拍只数发声时长；`TIM2_IRQHandler` 在 `buzzer_irq.c` |
| TIM1 | 超声波输入捕获 | CH1（PA8）；`TIM1_CC_IRQHandler` 在 `hcsr04.c` |
| TIM6 | 1ms 时基 | 按键消抖、矩阵扫描共用；`TIM6_IRQHandler` 在 `tick_irq.c` |
| EXTI0~3 | 独立按键 | PE0~PE3 双边沿；`EXTI0~3_IRQHandler` 在 `key_irq.c` |
| PB10 | 蜂鸣器 | 类型在 `buzzer_cfg.h` 选；有源只给电平，无源才发方波 |

## 移植到别的工程

`APP/` 下每个文件夹都是独立驱动，只依赖标准外设库和 `SYSTEM/`（`delay`、`sys`），
不引用 `USER/` 里的任何文件。

> 例外：蜂鸣器的两个驱动（`APP/buzzer`、`APP/interrupt` 里的 `buzzer_irq.*`）共用
> `APP/buzzer/buzzer_cfg.h`。只搬其中一个也要把 `buzzer_cfg.h` 带上（或把 `APP/buzzer`
> 整个拷过去），Include Paths 里要有 `..\APP\buzzer`。

1. **只搬驱动**：把需要的 `APP/xxx` 文件夹拷过去，加 Include Paths（`..\APP\xxx`）和对应的 `.c`，
   在 `main.c` 里 `#include` 头文件、调 `xxx_init()` 和功能函数。
2. **整个工程一起搬**：把 `APP/` 整个拷过去，再带上 `USER/main.c`、`USER/main.h`。
   `main.c` 已经按顺序初始化好各模块，`main.h` 里放着常用阈值和开关，接着往下写功能即可。

## 编译

Keil 打开 `USER/Framework.uvprojx`，Rebuild（当前 0 Error 0 Warning）。
