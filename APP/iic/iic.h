/**
 * @file    iic.h
 * @brief   软件 I2C 总线工具：引脚初始化、空闲检测、地址探测、总线恢复
 *
 * 引脚：SCL = PB7，SDA = PB6（开漏输出，靠模块上的 4.7k 上拉变高）。
 *   这两个脚和 OLED、BH1750 驱动用的是同一对，所以放在这里统一管理。
 *
 * 说明：这是"总线级"的工具，负责上电探测、总线被拉死后的恢复这类公共操作；
 *       OLED 和 BH1750 驱动内部还有各自优化过的时序（速度不同），不在这里。
 */
#ifndef __IIC_H
#define __IIC_H

#include "stm32f10x.h"

/* ------------------------------------------------------------------
 * 接线与速率
 * ------------------------------------------------------------------ */
#define IIC_PORT    GPIOB             /* SCL/SDA 都在 GPIOB */
#define IIC_RCC     RCC_APB2Periph_GPIOB
#define IIC_SCL_PIN GPIO_Pin_7        /* SCL = PB7 */
#define IIC_SDA_PIN GPIO_Pin_6        /* SDA = PB6 */

#define IIC_DELAY_US 5                /* 半周期延时：5us ≈ 100kHz */

/* 把 SCL/SDA 配成开漏输出并放开总线（空闲高） */
void IIC_Init(void);

/* 判断总线是否正常空闲：1 = 两根线都能被上拉拉高；0 = 没上拉/被拉死 */
uint8_t IIC_BusIdle(void);

/* 探测某个 8 位写地址有没有从机应答：1 = 有 ACK，0 = 没人应答 */
uint8_t IIC_Probe(uint8_t addr8);

/* 总线恢复：放 9 个 SCL 时钟 + 手动 STOP，把卡住的从机放出来 */
void IIC_BusRecover(void);

#endif /* __IIC_H */
