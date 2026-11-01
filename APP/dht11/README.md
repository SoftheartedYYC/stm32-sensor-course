# dht11_app —— DHT11 温湿度

单总线温湿度传感器，DATA 接 PB11（和 DS18B20 共用，二选一）。
一次事务阻塞约 20ms，两次读取间隔要 ≥ 1s。

| 项目 | 内容 |
|---|---|
| 头文件 | `dht11_app.h` |
| 引脚 | DATA = PB11 |
| 依赖 | `delay_init()`（读函数里用 `delay_us` / `delay_ms`） |
| 资源 | 1 个 GPIO（读时在推挽输出 / 上拉输入间切换） |

## 接线

| 模块引脚 | MCU | 说明 |
|---|---|---|
| DATA | PB11 | 需 4.7k~10k 上拉到 3.3V |
| VCC / GND | 3V3 / GND | 上电后约 1s 才稳定 |

## 函数

| 函数 | 说明 |
|---|---|
| `void dht11_init(void)` | PB11 配推挽输出并拉高总线；读的时候自动切成上拉输入 |
| `uint8_t dht11_read(uint8_t *humidity_rh, int16_t *temperature_x10)` | 读一次，阻塞约 20ms；1 = 成功，0 = 无响应或校验错 |

输出：`humidity_rh` 是湿度整数 %RH；`temperature_x10` 是 0.1°C 单位，但 DHT11 分辨率 1°C，末位恒为 0。

## 示例

```c
#include "delay.h"
#include "dht11_app.h"

uint8_t humi;
int16_t temp_x10;

delay_init();
dht11_init();
delay_ms(1000);          /* 上电后约 1s 才稳定 */

if (dht11_read(&humi, &temp_x10))
{
    /* humi = 45 (%RH)，temp_x10 = 253 (25.3°C) */
}
```

## 注意

- 两次读取间隔 **≥ 1s**，否则可能读到旧值或校验失败。
- DATA 需要 4.7k~10k 上拉；多数 3 针模块板上自带。
- PB11 和 DS18B20 共用，同一时间只能接一颗。
- DHT11 的"单总线"不是 DS18B20 的 1-Wire，时序不同，两个驱动不能混用。
