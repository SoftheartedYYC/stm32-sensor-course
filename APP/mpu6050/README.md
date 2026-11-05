# mpu6050 —— 六轴传感器（加速度 + 陀螺仪 + 温度）

MPU6050 走**软件 I2C**，和 OLED / BH1750 共用 PB6/PB7；INT 接 PB8，AD0 接 PB9。

| 项目 | 内容 |
|---|---|
| 头文件 | `mpu6050.h`（显示用 `mpu6050_ui.h`） |
| 引脚 | SDA = PB6；SCL = PB7；INT = PB8；AD0 = PB9 |
| 从机地址 | 7 位 `0x68`（驱动把 PB9 拉低得到），拉高改 `0x69` |
| 依赖 | `delay_us()` / `delay_ms()`（先 `delay_init()`）；显示依赖 `APP/oled` |
| 资源 | 2 个开漏 GPIO + 2 个普通 GPIO，1KB RAM（显示用的显存镜像） |

## 接线

| 模块引脚 | MCU | 说明 |
|---|---|---|
| VCC / GND | 3V3 / GND | 不要接 5V |
| SDA | PB6 | 和 OLED 的 SDA 拧在一起，模块 4.7k 上拉 |
| SCL | PB7 | 和 OLED 的 SCL 拧在一起 |
| INT | PB8 | 数据就绪输出，**不接也能用**（驱动只把它当"有新数据"的提示） |
| AD0 | PB9 | 地址选择，驱动里配成推挽输出并拉低 → 地址 0x68 |

> AD0 接 MCU 而不是直接接地，好处是同一块板子上想挂两片 MPU6050 时，
> 把另一片的 `MPU6050_AD0_LEVEL` 改成 1（地址 0x69）就行；只有一片时保持默认 0。

## 函数

| 函数 | 说明 |
|---|---|
| `uint8_t mpu6050_init(void)` | 配引脚 → 复位 → 唤醒 → 采样率/带宽/量程 → 开数据就绪中断。1 = 成功，0 = 无应答 |
| `uint8_t mpu6050_read(mpu6050_data_t *data)` | 一次突发读 14 字节并换算成 g / °/s / °C |
| `uint8_t mpu6050_who_am_i(uint8_t *id)` | 读 0x75，正常返回 0x68 |
| `uint8_t mpu6050_int_ready(void)` | PB8 电平，1 = 有新数据 |
| `uint8_t mpu6050_calibrate_gyro(uint16_t samples)` | 静止标定陀螺零偏（0 表示按 100 次算） |
| `void mpu6050_get_angles(const mpu6050_data_t *, float *pitch, float *roll, float *tilt)` | 由加速度算三个角度（度），传 `NULL` 表示那个值不要 |
| `void mpu6050_get_tilt(const mpu6050_data_t *, float *pitch, float *roll)` | 同上，只算 pitch / roll（老接口，保留兼容） |
| `mpu6050_write_reg / mpu6050_read_reg / mpu6050_read_regs` | 底层寄存器读写，留给扩展 |

数据结构：

```c
typedef struct
{
    mpu6050_vec3_t accel_g;   /* g，±2g 量程时 1.00 = 1 个重力 */
    mpu6050_vec3_t gyro_dps;  /* °/s，已扣零偏 */
    float          temp_c;    /* 芯片温度 */
    int16_t raw_accel[3];     /* 原始值，X/Y/Z */
    int16_t raw_gyro[3];
    int16_t raw_temp;
} mpu6050_data_t;
```

## 示例

```c
#include "mpu6050.h"
#include "mpu6050_ui.h"

mpu6050_data_t d;

if (mpu6050_init() == 0) { /* 没接：查 SDA/SCL/AD0 */ }
mpu6050_calibrate_gyro(100);      /* 上电静止时做一次 */

while (1)
{
    if (mpu6050_read(&d))
    {
        mpu6050_ui_show(&d);      /* 刷到 OLED */
    }
    delay_ms(200);
}
```

## 屏幕内容

前三行是角度（度），每行右边跟着该轴的 g 值，第 4 行是芯片温度：

```
P   -12.3deg 0.02      ← 前后倾（绕 Y 轴）  + 该轴加速度 ax
R     5.8deg 0.01      ← 左右倾（绕 X 轴）  + ay
T    13.6deg 1.00      ← 合成倾角（板子与水平面夹角）+ az
TEMP   25.3C
```

| 列 | 内容 |
|---|---|
| 1 | `P` / `R` / `T`（pitch / roll / tilt） |
| 2 | 空格 |
| 3~8 | 角度，6 字符、1 位小数（`-180.0` ~ `180.0` 放得下） |
| 9~11 | `deg` —— 字库里没有 `°`（0xB0），只能用字母写单位 |
| 12~16 | 对应轴的加速度，5 字符、2 位小数（`-2.00` ~ `2.00`） |

角度定义（都用 `atan2f`，板子竖起来也不会像 `asin` 那样发散）：

| 值 | 算法 | 范围 | 平放 | 正方向 |
|---|---|---|---|---|
| `pitch` | `atan2(-ax, √(ay²+az²))` | -90 ~ +90 | 0° | 前倾为正 |
| `roll` | `atan2(ay, az)` | -180 ~ +180 | 0° | 右侧抬起为正 |
| `tilt` | `atan2(√(ax²+ay²), az)` | 0 ~ 180 | 0° | 立起来 90°、翻过来 180° |

**自检**：板子平放时三行角度都该在 `0.0` 附近（±2° 内），`T` 行右边 `az≈1.00`、`ax/ay≈0.00`。
把板子抬起 30°，`T` 行立刻跳到 `30.0` 左右；再抬到竖直，`T` 就是 `90.0` 上下。
读出来全是 0 或乱跳，说明量程宏和实际芯片对不上，或者 SDA/SCL 没通。

> 为什么以前看 g 值"变化不大"：加速度计测的是**比力**，不是角度，`ax²+ay²+az²` 恒等于 1g。
> 倾斜时重力只是在三个轴之间重新分配，而且是 `cos` 关系——倾斜 30° 时 `az` 只从 `1.00` 掉到 `0.87`，
> 所以看着像"没反应"。角度必须用反三角函数反算出来，也就是上面这三个值。

角速度（`gyro_dps`）没上屏，要看得自己改 `mpu6050_ui.c` 里第 4 行（温度那行）；
4 行 16 列已经写满，加内容得先决定砍掉哪一项。

## 配置

改 `mpu6050.h` 顶部的宏，重新编译即可：

| 宏 | 默认 | 说明 |
|---|---|---|
| `MPU6050_AD0_LEVEL` | 0 | 0 → 地址 0x68，1 → 0x69 |
| `MPU6050_I2C_DELAY_US` | 5 | 半周期，5 ≈ 100kHz；改成 2 约 250kHz 更快但不稳 |
| `MPU6050_SMPLRT_DIV` | 4 | 采样率 = 1kHz/(1+值) = 200Hz |
| `MPU6050_DLPF_CFG` | 3 | 数字低通，3 = 加速度 44Hz / 陀螺 42Hz |
| `MPU6050_GYRO_FS_SEL` | 1 | 1 = ±500°/s（0=±250，2=±1000，3=±2000） |
| `MPU6050_ACCEL_FS_SEL` | 0 | 0 = ±2g（1=±4g，2=±8g，3=±16g） |

改量程不用改换算代码：`MPU6050_ACCEL_LSB_PER_G` / `MPU6050_GYRO_LSB_PER_DPS`
会跟着量程宏自动选对值。

刷新节拍在 `USER/main.h`：`MPU6050_SAMPLE_PERIOD_MS`（采样 50ms）、`MPU6050_UI_REFRESH_DIV`（4 次刷一屏）。

## 注意

- **没找到片子不会卡死**：`mpu6050_init()` 返回 0 时屏幕显示 `MPU6050 MISSING / check SDA/SCL`，
  主循环每秒自动重试一次初始化，插好线不用复位板子。常见原因：模块接了 5V、SDA/SCL 接反、AD0 悬空。
- **上电标定要静止**：`mpu6050_calibrate_gyro()` 采样期间板子在动，零偏反而变成"动的时候的读数"，静止时 G 会一直偏。
- **温度不是环境温度**：芯片自热，会比室温高 3~8℃，只能看趋势。
- **倾角只用加速度算**：静态准，一晃动就跟着抖；要平滑得做互补滤波（陀螺积分 + 加速度校正）。
- **和 OLED / BH1750 共用 PB6/PB7**：三者初始化顺序无所谓（都是开漏 + 放开总线），但同一时刻只能有一个在通信，别在中断里读传感器。
- **刷屏耗时**：一屏 1024 字节 ≈ 25~30ms，`MPU6050_UI_REFRESH_DIV` 别设成 1（50ms 刷一次就跑不动别的了）。
- **INT 只用来判"有没有新数据"**：没接也不会影响读数（`mpu6050_read()` 不管 INT）。要真用中断，在 `stm32f10x_it.c` 里加 `EXTI9_5_IRQHandler`，判断 `EXTI_GetITStatus(EXTI_Line8)` 后置个标志，主循环里读数据——注意 PB8 的中断线是 EXTI8（和 PC8 等共用 `EXTI9_5`）。
- **中文没法直接显示**：`oled_font.h` 里没有"加速度/角度/陀螺仪"这些字，而重生成会改到 `APP/oled`，
  所以屏幕标签只用 `P`/`R`/`T`/`TEMP`/`deg` 这类 ASCII。真要中文，用 `tools/gen_oled_font.py`
  把字加进 `CN_CHARS` 重新生成，或者单独生成一份放在本模块里自己画。
