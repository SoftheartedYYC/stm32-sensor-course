/**
 * @file ds18b20_app.c
 * @brief DS18B20 单总线驱动，PB11，按位读写、带 CRC 校验
 */
#include "ds18b20_app.h"
#include "delay.h"

/* DS18B20 数据线接 PB11 */
#define DS18B20_PORT GPIOB
#define DS18B20_PIN  GPIO_Pin_11

/* 单总线需要上拉。使用 GPIO 开漏：输出 0 拉低，写 1 时由外部电阻拉高。 */
static void ds18b20_pin_output(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = DS18B20_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DS18B20_PORT, &gpio);
}

static void ds18b20_pin_input(void)
{
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = DS18B20_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DS18B20_PORT, &gpio);
}

static void ds18b20_low(void)
{
    GPIO_ResetBits(DS18B20_PORT, DS18B20_PIN);
}

static void ds18b20_release(void)
{
    GPIO_SetBits(DS18B20_PORT, DS18B20_PIN);
}

static uint8_t ds18b20_read_pin(void)
{
    return GPIO_ReadInputDataBit(DS18B20_PORT, DS18B20_PIN);
}

/* 复位总线并检查是否有传感器应答：检测到返回 1。 */
static uint8_t ds18b20_reset(void)
{
    uint8_t present;

    ds18b20_pin_output();
    ds18b20_low();
    delay_us(500);                 /* 主机拉低至少 480us */
    ds18b20_release();
    ds18b20_pin_input();
    delay_us(70);                  /* 等待传感器应答脉冲 */
    present = (uint8_t)(ds18b20_read_pin() == 0);
    delay_us(410);                 /* 等应答时隙结束，总复位时隙大于 480us */
    return present;
}

/* 向 DS18B20 写入 1 字节，最低位先发。 */
static void ds18b20_write_byte(uint8_t data)
{
    uint8_t i;

    ds18b20_pin_output();
    for (i = 0; i < 8; i++)
    {
        ds18b20_low();
        if (data & 0x01)
        {
            delay_us(2);           /* 写 1：短暂拉低后释放 */
            ds18b20_release();
            delay_us(60);
        }
        else
        {
            delay_us(60);          /* 写 0：长时间拉低 */
            ds18b20_release();
            delay_us(2);
        }
        data >>= 1;
    }
}

/* 从 DS18B20 读取 1 字节，最低位先收。 */
static uint8_t ds18b20_read_byte(void)
{
    uint8_t i;
    uint8_t data = 0;

    for (i = 0; i < 8; i++)
    {
        ds18b20_pin_output();
        ds18b20_low();
        delay_us(2);
        ds18b20_release();
        ds18b20_pin_input();
        delay_us(10);              /* 在读时隙前段采样 */
        if (ds18b20_read_pin())
        {
            data |= (uint8_t)(1U << i);
        }
        delay_us(50);              /* 补足一个读时隙 */
    }
    return data;
}

/* Dallas/Maxim CRC-8：检查暂存器前 8 字节是否正确。 */
static uint8_t ds18b20_crc8(const uint8_t *data, uint8_t length)
{
    uint8_t i;
    uint8_t bit;
    uint8_t crc = 0;

    for (i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (bit = 0; bit < 8; bit++)
        {
            if (crc & 0x01)
            {
                crc = (uint8_t)((crc >> 1) ^ 0x8C);
            }
            else
            {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/* 初始化 PB11，并让总线处于释放状态。 */
void ds18b20_init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    ds18b20_pin_output();
    ds18b20_release();
}

/* 发起一次温度转换，不等待转换完成。 */
uint8_t ds18b20_start_conversion(void)
{
    if (!ds18b20_reset())
    {
        return 0;
    }

    ds18b20_write_byte(0xCC);       /* 跳过 ROM：总线上只有一颗 DS18B20 */
    ds18b20_write_byte(0x44);       /* 开始温度转换 */
    ds18b20_release();
    return 1;
}

/* 读取转换忙闲位：1=已完成，0=仍在转换。 */
uint8_t ds18b20_conversion_done(void)
{
    uint8_t done;

    ds18b20_pin_output();
    ds18b20_low();
    delay_us(2);
    ds18b20_release();
    ds18b20_pin_input();
    delay_us(10);
    done = ds18b20_read_pin();
    delay_us(50);
    return done;
}

/* 读取温度；成功时输出 0.1°C 单位的有符号整数。 */
uint8_t ds18b20_read_temperature(int16_t *temperature_x10)
{
    uint8_t i;
    uint8_t scratchpad[9];
    int16_t raw;

    if ((temperature_x10 == 0) || !ds18b20_reset())
    {
        return 0;
    }

    ds18b20_write_byte(0xCC);       /* 跳过 ROM */
    ds18b20_write_byte(0xBE);       /* 读取暂存器 */
    for (i = 0; i < 9; i++)
    {
        scratchpad[i] = ds18b20_read_byte();
    }

    if (ds18b20_crc8(scratchpad, 8) != scratchpad[8])
    {
        return 0;                   /* CRC 不符，丢弃错误读数 */
    }

    raw = (int16_t)(((uint16_t)scratchpad[1] << 8) | scratchpad[0]);
    *temperature_x10 = (int16_t)(((int32_t)raw * 10) / 16); /* 12 位温度分辨率 */
    return 1;
}
