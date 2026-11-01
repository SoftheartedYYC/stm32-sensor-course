# oled_app —— OLED 显示屏

0.96 寸 SSD1306（128×64），**软件 I2C**：SDA = PB6、SCL = PB7。
每行 16 个 ASCII 列、共 4 行；中文 16×16，占 2 列。

| 项目 | 内容 |
|---|---|
| 头文件 | `oled_app.h`（字库在 `oled_font.h`） |
| 引脚 | SDA = PB6；SCL = PB7 |
| 依赖 | 无（`oled_init()` 自带软件延时，不依赖 `delay_init()`） |
| 资源 | 2 个开漏 GPIO |

## 接线

| 模块引脚 | MCU | 说明 |
|---|---|---|
| SDA | PB6 | 开漏 + 模块 4.7k 上拉 |
| SCL | PB7 | 开漏 + 模块 4.7k 上拉 |
| VCC / GND | 3V3 / GND | 从机地址默认 0x78 |

## 函数

| 函数 | 说明 |
|---|---|
| `void oled_init(void)` | 配引脚 + 写 SSD1306 配置序列 + 清屏 |
| `void oled_set_addr(uint8_t addr8)` | 改从机地址（0x78 / 0x7A），要在 `oled_init()` 之前调 |
| `void oled_clear(void)` | 整屏清空 |
| `void oled_show_char(uint8_t line, uint8_t col, char ch)` | 显示一个 ASCII 字符（0x20~0x7E） |
| `void oled_show_string(uint8_t line, uint8_t col, const char *str)` | 显示以 `\0` 结尾的字符串 |
| `void oled_show_num(uint8_t line, uint8_t col, uint32_t num, uint8_t len)` | 定长十进制数，高位补 0，只取低 `len` 位 |
| `void oled_show_cn(uint8_t line, uint8_t col, uint8_t index)` | 显示 16×16 中文，`index` 取 `OLED_CN_xxx` |
| `void oled_write_run(uint8_t page, uint8_t col, const uint8_t *data, uint8_t len)` | 整段写显存：页 0~7、列 0~127、`len` ≤ 128 |

坐标：`line` 1~4，`col` 1~16，原点在左上角。

## 示例

```c
#include "oled_app.h"

oled_init();

oled_show_string(1, 1, "Hello");
oled_show_num(2, 1, 1234, 4);      /* 显示 1234 */
oled_show_cn(3, 1, OLED_CN_WEN);   /* 温 */
oled_show_cn(3, 3, OLED_CN_DU);    /* 度 */
```

## 字库

`oled_font.h` 里现有的中文（`OLED_CN_xxx`）：
光 敏 电 阻 值 亮 暗 温 度 模 拟 量 开 关 伏 压 照 摄 氏 ℃ 叶 雨 晨 分 阈 超 标 湿

要加字：改 `tools/gen_oled_font.py` 的 `CN_CHARS` 重新生成，不要手改 `oled_font.c`。

## 注意

- 从机地址绝大多数是 0x78；模块背面 SA0 接 VCC 的是 0x7A，用 `oled_set_addr(0x7A)` 切换。
- 和 BH1750 共用 PB6/PB7：先 `oled_init()` 再 `BH1750_Init()`，屏先出来更安全。
- 从机地址探测、总线空闲检查、总线恢复用 `APP/iic`（`IIC_Probe()` / `IIC_BusIdle()` / `IIC_BusRecover()`）。
- 花屏/乱码时把 `oled_app.c` 里的 `OLED_I2C_DELAY_LOOPS` 调大（时序更慢更稳）。
- 整屏清屏、整行刷新都走 `oled_write_run()` 的整页写入，别改成逐字节写，会慢一个数量级。
