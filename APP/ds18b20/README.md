# ds18b20_app —— DS18B20 温度

单总线温度传感器，DQ 接 PB11（和 DHT11 共用，二选一）。
驱动采用"启动转换 → 轮询完成 → 读温度"的非阻塞流程，读回时做 CRC-8 校验。

| 项目 | 内容 |
|---|---|
| 头文件 | `ds18b20_app.h` |
| 引脚 | DQ = PB11 |
| 依赖 | `delay_init()`（读写时隙用 `delay_us`） |
| 资源 | 1 个 GPIO（开漏输出 / 上拉输入切换） |

## 接线

| 模块引脚 | MCU | 说明 |
|---|---|---|
| DQ | PB11 | 需约 4.7k 上拉到 3.3V |
| VDD / GND | 3V3 / GND | 三线供电，不要用寄生供电 |

## 函数

| 函数 | 返回 | 说明 |
|---|---|---|
| `void ds18b20_init(void)` | — | PB11 单总线引脚初始化，释放总线 |
| `uint8_t ds18b20_start_conversion(void)` | 1 = 检测到传感器 | 启动一次温度转换，不阻塞 |
| `uint8_t ds18b20_conversion_done(void)` | 1 = 转换完成 | 轮询用，12 位转换最长约 750ms |
| `uint8_t ds18b20_read_temperature(int16_t *temperature_x10)` | 1 = 成功 | 读温度（0.1°C 单位），带 CRC-8 校验 |

## 示例

```c
#include "delay.h"
#include "ds18b20_app.h"

int16_t temp_x10;

delay_init();
ds18b20_init();

if (ds18b20_start_conversion())
{
    while (!ds18b20_conversion_done())
    {
        /* 等待期间可以干别的 */
    }

    if (ds18b20_read_temperature(&temp_x10))
    {
        /* 253 = 25.3°C */
    }
}
```

## 注意

- 别死等：主循环里"启动转换 → 每 100ms 查一次 `conversion_done()` → 读到就再启动下一次"即可。
- 读失败（CRC 错 / 没接）返回 0，不要把脏数据显示出去。
- 和 DHT11 共用 PB11，二选一。
- 数据线没有上拉时会一直检测不到传感器。
