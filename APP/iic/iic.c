/**
 * @file    iic.c
 * @brief   软件 I2C 总线工具：PB6/PB7，开漏输出 + 外部上拉
 *
 * 这些函数只做"总线层面"的事：配脚、看总线是否空闲、探测地址、把总线救回来。
 * 具体的字节读写时序在 OLED / BH1750 驱动里（两边速率不同，各自优化过）。
 */

#include "iic.h"
#include "delay.h"

/**
 * @brief  把 PB6/PB7 配成开漏输出并放开总线
 * @note   开漏 + 外部上拉 = 空闲高；换引脚只改 iic.h 里的宏
 */
void IIC_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(IIC_RCC, ENABLE);

    gpio.GPIO_Pin = IIC_SCL_PIN | IIC_SDA_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(IIC_PORT, &gpio);

    GPIO_SetBits(IIC_PORT, IIC_SCL_PIN | IIC_SDA_PIN); /* 放开，靠上拉拉高 */
}

/**
 * @brief  判断 I2C 总线是不是"正常空闲"（两根线都能被上拉拉高）
 * @retval 1 = 正常；0 = 没有上拉 / 被某个器件拉死
 *
 * 为什么要查：
 *   PB6/PB7 是开漏脚，"高电平"完全靠外部上拉电阻。如果 OLED 和 BH1750
 *   都没接（或者都没电），两根线就是浮空的：这时软件 I2C 照样能"写"，
 *   第 9 个时钟读回来的 ACK 却可能读到低电平，把不存在的器件当成在线，
 *   于是 BH1750 读出 0 lx，RGB 就一直红、怎么看都不变色。
 *
 * 做法：先主动把两根线拉低一下，放掉线电容上的电，再放开；
 *   线上有 4.7k 上拉时会很快回到高电平，没有上拉就会一直浮在低电平。
 */
uint8_t IIC_BusIdle(void)
{
    IIC_Init();

    GPIO_ResetBits(IIC_PORT, IIC_SCL_PIN | IIC_SDA_PIN); /* 主动拉低，放电 */
    delay_us(50);
    GPIO_SetBits(IIC_PORT, IIC_SCL_PIN | IIC_SDA_PIN); /* 放开，靠上拉拉高 */
    delay_us(50);

    if ((GPIO_ReadInputDataBit(IIC_PORT, IIC_SDA_PIN) == Bit_RESET) ||
        (GPIO_ReadInputDataBit(IIC_PORT, IIC_SCL_PIN) == Bit_RESET))
    {
        return 0; /* 还是低：没有上拉，或者被器件拉死了 */
    }

    return 1;
}

/**
 * @brief  探测总线上某个地址有没有从机应答
 * @param  addr8 8 位写地址（OLED 是 0x78 或 0x7A，BH1750 是 0x46 / 0xB8）
 * @retval 1 = 有 ACK；0 = 没人应答
 *
 * 流程就是一条最小的写事务：START -> 8 位地址 -> 第 9 个时钟放开 SDA 读 ACK -> STOP。
 * 有应答说明"线接对了、模块有电、地址也对"，没应答就是硬件层面的问题。
 */
uint8_t IIC_Probe(uint8_t addr8)
{
    uint8_t i;
    uint8_t ack;

    IIC_Init();

    /* START：SCL 高电平期间 SDA 由高变低 */
    GPIO_SetBits(IIC_PORT, IIC_SCL_PIN | IIC_SDA_PIN);
    delay_us(IIC_DELAY_US);
    GPIO_ResetBits(IIC_PORT, IIC_SDA_PIN);
    delay_us(IIC_DELAY_US);
    GPIO_ResetBits(IIC_PORT, IIC_SCL_PIN);

    for (i = 0; i < 8; i++) /* 高位在前发 8 位地址 */
    {
        if ((addr8 & (uint8_t)(0x80 >> i)) != 0)
        {
            GPIO_SetBits(IIC_PORT, IIC_SDA_PIN);
        }
        else
        {
            GPIO_ResetBits(IIC_PORT, IIC_SDA_PIN);
        }
        delay_us(IIC_DELAY_US);
        GPIO_SetBits(IIC_PORT, IIC_SCL_PIN);
        delay_us(IIC_DELAY_US);
        GPIO_ResetBits(IIC_PORT, IIC_SCL_PIN);
        delay_us(IIC_DELAY_US);
    }

    /* 第 9 个时钟：放开 SDA，从机会把它拉低表示 ACK */
    GPIO_SetBits(IIC_PORT, IIC_SDA_PIN);
    delay_us(IIC_DELAY_US);
    GPIO_SetBits(IIC_PORT, IIC_SCL_PIN);
    delay_us(IIC_DELAY_US);
    ack = (GPIO_ReadInputDataBit(IIC_PORT, IIC_SDA_PIN) == Bit_RESET) ? 1 : 0;
    GPIO_ResetBits(IIC_PORT, IIC_SCL_PIN);

    /* STOP */
    GPIO_ResetBits(IIC_PORT, IIC_SDA_PIN);
    delay_us(IIC_DELAY_US);
    GPIO_SetBits(IIC_PORT, IIC_SCL_PIN);
    delay_us(IIC_DELAY_US);
    GPIO_SetBits(IIC_PORT, IIC_SDA_PIN);
    delay_us(IIC_DELAY_US);

    return ack;
}

/**
 * @brief  I2C 总线恢复：把可能卡在半截字节里的从机"放"出来
 *
 * 什么时候需要：
 *   从机（BH1750）如果在一次传输中间被打断（比如上电顺序、电源抖动、
 *   线松了一下），它可能一直把 SDA 拉低，总线就死了。这时候再发 START
 *   也没用，OLED 自然一片黑。
 *   标准做法：主机先放 9 个 SCL 时钟，把从机的移位寄存器移空，再手打一个
 *   STOP，之后总线就回到空闲（SDA/SCL 都被上拉拉高）。
 */
void IIC_BusRecover(void)
{
    uint8_t i;

    IIC_Init(); /* 开漏 + 先放开总线（靠外部上拉为高） */
    delay_us(IIC_DELAY_US);

    for (i = 0; i < 9; i++) /* 9 个时钟：从机最多剩 8 位 + 1 个 ACK 位 */
    {
        GPIO_ResetBits(IIC_PORT, IIC_SCL_PIN);
        delay_us(IIC_DELAY_US);
        GPIO_SetBits(IIC_PORT, IIC_SCL_PIN);
        delay_us(IIC_DELAY_US);
    }

    /* 手动 STOP：SCL 高电平期间，SDA 由低变高 */
    GPIO_ResetBits(IIC_PORT, IIC_SDA_PIN);
    delay_us(IIC_DELAY_US);
    GPIO_SetBits(IIC_PORT, IIC_SCL_PIN);
    delay_us(IIC_DELAY_US);
    GPIO_SetBits(IIC_PORT, IIC_SDA_PIN);
    delay_us(IIC_DELAY_US);
}
