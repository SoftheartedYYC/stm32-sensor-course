# buzzer_app —— 蜂鸣器（轮询版，三种类型一个宏切换）

PB10 推挽输出。**有源**蜂鸣器给对电平就响，**无源**蜂鸣器必须给方波（脉冲）才响——
本驱动把这两种情况都做进去了，改 `buzzer_cfg.h` 里的 `BUZZER_TYPE` 一个宏就能切换，
`BEEP_On()` / `BEEP_Off()` / `BEEP_Beep()` 这些调用一行都不用改。

> 这是**轮询版**：无源蜂鸣器要靠主循环反复调 `BEEP_Poll()` 才出方波，发声期间占 CPU。
> 想让蜂鸣器自己响、主循环不用管，用中断版：[APP/interrupt](../interrupt/README.md)。

## 第一步：选类型（只改这一行）

打开 [buzzer_cfg.h](buzzer_cfg.h)，把 `BUZZER_TYPE` 改成下面三个之一：

```c
#define BUZZER_TYPE  BUZZER_TYPE_ACTIVE_LOW
```

| 宏 | 什么样的蜂鸣器 | 怎么响 | 静音电平 |
|---|---|---|---|
| `BUZZER_TYPE_ACTIVE_LOW` | 有源模块，丝印"低电平触发"（最常见） | 引脚拉低就响 | 高 |
| `BUZZER_TYPE_ACTIVE_HIGH` | 有源模块，丝印"高电平触发" | 引脚拉高就响 | 低 |
| `BUZZER_TYPE_PASSIVE` | 无源（裸蜂鸣器 / 没有驱动板） | 必须给 2.7kHz 方波 | 高 |

判断方法：

- 模块有三个引脚（VCC / GND / IN）或背面丝印写了"低电平触发 / 高电平触发" → **有源**，按丝印选前两个；
- 只有两个引脚、板上没有三极管、丝印只写 `BUZZER` → **无源**，选第三个；
- 拿不准就挨个试，反正只改宏不改代码。选错的表现：有源选成无源会一直"嗒嗒"轻响或干脆不响。

同一个文件里还能改接线和频率：

| 宏 | 默认值 | 说明 |
|---|---|---|
| `BUZZER_PORT` / `BUZZER_PIN` | `GPIOB` / `GPIO_Pin_10` | 换引脚改这两行 |
| `BUZZER_GPIO_RCC` | `RCC_APB2Periph_GPIOB` | 跟着端口改（GPIOA~G 都在 APB2） |
| `BUZZER_FREQ_HZ` | `2700u` | 只有无源才用；常见谐振频率 2700 / 4000，选最接近的声音最大 |

改完宏之后，驱动内部会自动得到 `BUZZER_NEED_SQUARE`（要不要方波）和
`BUZZER_LEVEL_ON` / `BUZZER_LEVEL_OFF`（响 / 静音分别是什么电平），不需要手动管。

## 文件

| 文件 | 内容 |
|---|---|
| `buzzer_cfg.h` | **只用改这里**：类型、引脚、频率 |
| `buzzer_app.h/.c` | 驱动本体，三种类型共用同一套接口 |
| `README.md` | 本文件 |

| 项目 | 内容 |
|---|---|
| 头文件 | `buzzer_app.h`（内部已包含 `buzzer_cfg.h`） |
| 引脚 | PB10，推挽输出 |
| 依赖 | `delay_init()`（`BEEP_Poll()` / `BEEP_Beep()` 里用 `delay_us` / `delay_ms`） |
| 资源 | 普通 GPIO，不占定时器 |

## 接线

| 模块引脚 | MCU | 说明 |
|---|---|---|
| IN | PB10 | 推挽输出 |
| VCC / GND | 3V3 / GND | 信号地要和 MCU 共地 |

## 函数与宏

| 名称 | 说明 |
|---|---|
| `void BEEP_Init(void)` | 引脚配推挽输出，初始静音（静音电平按类型自动选） |
| `void BEEP_On(void)` | 开始响：有源直接给电平；无源只置标志，还要调 `BEEP_Poll()` |
| `void BEEP_Off(void)` | 静音：引脚回到静音电平（无源拉高，不会一直通直流） |
| `void BEEP_Poll(void)` | 无源：翻转一次 + 延时半个周期（约 185µs）；有源：空函数，什么也不做 |
| `void BEEP_Square(void)` | 旧接口：无源 = 翻转换相 + 延时；有源 = 等于 `BEEP_On()` |
| `void BEEP_Beep(uint16_t ms)` | 阻塞式响 `ms` 毫秒，返回时已静音（有源走 `delay_ms`，无源走半周期循环） |
| `BEEP_ON()` / `BEEP_OFF()` | 直接给"响 / 静音"电平（宏，不改变无源的轮询标志） |
| `BEEP_FREQ_HZ` / `BEEP_HALF_US` | 方波频率 / 半周期微秒数，值来自 `buzzer_cfg.h` |
| `BEEP_PORT` / `BEEP_PIN` | 等价于 `BUZZER_PORT` / `BUZZER_PIN` |

## 示例

### 1. 一句话响 200ms（三种类型都适用）

```c
#include "delay.h"
#include "buzzer_app.h"

delay_init();
BEEP_Init();

BEEP_Beep(200);      /* 响 200ms，期间阻塞 */
```

### 2. 非阻塞：主循环里边响边干别的

```c
BEEP_Init();
BEEP_On();

while (1)
{
    BEEP_Poll();     /* 无源：必须调，否则不响；有源：空函数，调了也没影响 */

    /* 这里照常做别的事：读传感器、刷显示…… */
}
```

### 3. 报警：响 10 次短音

```c
uint8_t i;

for (i = 0; i < 10; i++)
{
    BEEP_Beep(50);
    delay_ms(50);
}
```

## 注意

- 无源蜂鸣器**只给恒定电平不会响**，必须靠 `BEEP_Poll()`（或 `BEEP_Square()`）不停翻转；
  有源蜂鸣器反过来，给方波只会"嗒嗒"响，所以驱动会给恒定电平。
- `BEEP_ON()` / `BEEP_OFF()` 是宏，直接动引脚，不改变无源版的"正在响"标志。
  无源要停止请用函数 `BEEP_Off()`；只想临时把引脚拉高不发声，用 `BEEP_OFF()` 也行。
- 方波靠 CPU 循环产生，响的时候不要再夹长延时，否则频率会偏、声音变调。
- 要"响 N 毫秒"且不想占 CPU，直接用中断版的 `BuzzerIRQ_Beep(ms)`（见
  [APP/interrupt](../interrupt/README.md)），接口和这里的 `BEEP_Beep(ms)` 一样。
- 不响时引脚保持在静音电平，不会一直给"响"的电平：有源模块的三极管关断，无源线圈也不会一直通直流。
