/**
 * @file    dht11_app.c
 * @brief   DHT11 温湿度传感器驱动，DATA = PB11（与 DS18B20 共用）
 *
 * DHT11 的"单总线"和 DS18B20 的 1-Wire 不是一回事：
 *   1. 主机把数据线拉低至少 18ms 作为起始信号，然后释放；
 *   2. DHT11 应答：拉低约 80us，再拉高约 80us；
 *   3. 随后发 40 位数据，高位先出。每一位都是"50us 低 + 高电平"，
 *      高电平持续 26~28us 表示 0，持续约 70us 表示 1；
 *   4. 5 个字节依次是：湿度整数、湿度小数、温度整数、温度小数、校验和，
 *      校验和 = 前 4 个字节相加的低 8 位。
 *
 * 所以它不能复用 DS18B20 的复位/读写时序，必须单独写一套。
 * 读取一次约 20ms（20ms 起始 + 4ms 数据），阻塞式；主程序每秒调用一次。
 */

#include "dht11_app.h"
#include "delay.h"

/* 等待电平翻转的超时计数。delay_us(1) 每次至少 1us，300 次约 300~450us，
 * 足够覆盖 DHT11 最长的 80us 应答脉冲和 70us 数据高电平，又不会死等。 */
#define DHT11_TIMEOUT 300

/* 推挽输出：起始信号和释放阶段主动驱动总线，不依赖外部上拉也能拉高。
 * 读数据时再切成上拉输入，让 DHT11 拉低/释放。 */
static void dht11_pin_output(void)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin = DHT11_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT11_PORT, &gpio);
}

/* 上拉输入：读 DHT11 拉高/拉低；内部上拉只是兜底，外部 4.7k~10k 才是主上拉 */
static void dht11_pin_input(void)
{
    GPIO_InitTypeDef gpio;

    gpio.GPIO_Pin = DHT11_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT11_PORT, &gpio);
}

static void dht11_low(void)
{
    GPIO_ResetBits(DHT11_PORT, DHT11_PIN);
}

static void dht11_release(void)
{
    GPIO_SetBits(DHT11_PORT, DHT11_PIN);
}

static uint8_t dht11_read_pin(void)
{
    return GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN);
}

/**
 * @brief  等数据线变成指定电平
 * @param  level     要等的电平（0 或 1）
 * @param  timeout_us 超时计数，每个计数至少 1us
 * @retval 1 = 等到了，0 = 超时
 * @note   用 delay_us(1) 而不是空循环，是因为 72MHz 下空循环的周期
 *         依赖编译器和优化等级，换优化选项就可能超时；delay_us 由 SysTick
 *         计时，时间稳定。
 */
static uint8_t dht11_wait_level(uint8_t level, uint16_t timeout_us)
{
    while (dht11_read_pin() != level)
    {
        if (timeout_us == 0)
        {
            return 0;
        }
        timeout_us--;
        delay_us(1);
    }
    return 1;
}

/* 初始化 PB11，输出高电平。上电后第一次读之前要等约 1s（主程序负责）。 */
void dht11_init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    dht11_pin_output();
    dht11_release();
}

/**
 * @brief  读一次 DHT11 温湿度
 * @param  humidity_rh     输出湿度整数部分，单位 %RH
 * @param  temperature_x10 输出温度，单位 0.1°C
 * @retval 1 = 成功，0 = 无响应 / 校验和错误
 */
uint8_t dht11_read(uint8_t *humidity_rh, int16_t *temperature_x10)
{
    uint8_t data[5] = {0, 0, 0, 0, 0}; /* 湿度整数、湿度小数、温度整数、温度小数、校验和 */
    uint8_t i;
    uint8_t j;
    uint8_t checksum;

    if ((humidity_rh == 0) || (temperature_x10 == 0))
    {
        return 0; /* 防止调用者传空指针 */
    }

    /* ---- 1. 起始信号：拉低至少 18ms，这里给 20ms 留余量 ---- */
    dht11_pin_output();
    dht11_low();
    delay_ms(20);
    dht11_release();     /* 推挽输出高，主动把总线拉高 */
    delay_us(30);        /* 保持高电平 30us，给 DHT11 准备应答的时间 */
    dht11_pin_input();   /* 切成输入，准备读 DHT11 的应答 */

    /* ---- 2. 应答：80us 低 + 80us 高 ---- */
    if (!dht11_wait_level(0, DHT11_TIMEOUT)) /* 等 DHT11 把线拉低 */
    {
        return 0;
    }
    if (!dht11_wait_level(1, DHT11_TIMEOUT)) /* 80us 低电平结束 */
    {
        return 0;
    }
    if (!dht11_wait_level(0, DHT11_TIMEOUT)) /* 80us 高电平结束，第一位开始 */
    {
        return 0;
    }

    /* ---- 3. 收 40 位，高位先出 ---- */
    for (i = 0; i < 5; i++)
    {
        for (j = 0; j < 8; j++)
        {
            if (!dht11_wait_level(1, DHT11_TIMEOUT)) /* 等这一位的 50us 低电平结束 */
            {
                return 0;
            }

            /* 40us 后采样：0 的高电平只有 26~28us，已经落下；
             * 1 的高电平约 70us，仍然是高。 */
            delay_us(40);
            data[i] <<= 1;
            if (dht11_read_pin())
            {
                data[i] |= 0x01;

                /* 是 1：等 70us 高电平结束，好让下一位从低电平开始 */
                if (!dht11_wait_level(0, DHT11_TIMEOUT))
                {
                    return 0;
                }
            }
        }
    }

    /* ---- 4. 校验：前 4 字节之和的低 8 位应等于第 5 字节 ---- */
    checksum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (checksum != data[4])
    {
        return 0;
    }

    /* DHT11 的湿度、温度小数位固定为 0，只取整数部分。
     * 温度按 0.1°C 输出，和 DS18B20 的接口保持一致，显示代码不用改。 */
    *humidity_rh = data[0];
    *temperature_x10 = (int16_t)data[2] * 10;

    /* 读完把总线交还给主机并拉高，让 DHT11 进入空闲 */
    dht11_pin_output();
    dht11_release();
    return 1;
}
