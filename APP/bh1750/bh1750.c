#include "bh1750.h"
#include "delay.h"

#define BH1750_ADDR_LOW  0x23
#define BH1750_ADDR_HIGH 0x5C
#define BH1750_POWER_ON  0x01
#define BH1750_CONT_HRES 0x10

#define BH1750_SDA_PIN GPIO_Pin_6
#define BH1750_SCL_PIN GPIO_Pin_7
static uint8_t bh1750_address = BH1750_ADDR_LOW;

static void BH1750_SDA(uint8_t level)
{
    if (level) GPIO_SetBits(GPIOB, BH1750_SDA_PIN);
    else GPIO_ResetBits(GPIOB, BH1750_SDA_PIN);
    delay_us(5);
}

static void BH1750_SCL(uint8_t level)
{
    if (level) GPIO_SetBits(GPIOB, BH1750_SCL_PIN);
    else GPIO_ResetBits(GPIOB, BH1750_SCL_PIN);
    delay_us(5);
}

static uint8_t BH1750_SDA_Read(void)
{
    return GPIO_ReadInputDataBit(GPIOB, BH1750_SDA_PIN) != Bit_RESET;
}

static void BH1750_Start(void)
{
    BH1750_SDA(1); BH1750_SCL(1); BH1750_SDA(0); BH1750_SCL(0);
}

static void BH1750_Stop(void)
{
    BH1750_SDA(0); BH1750_SCL(1); BH1750_SDA(1);
}

static uint8_t BH1750_WaitAck(void)
{
    uint16_t timeout = 0;
    BH1750_SDA(1);
    BH1750_SCL(1);
    while (BH1750_SDA_Read())
    {
        if (++timeout > 1000)
        {
            BH1750_SCL(0);
            return 0;
        }
        delay_us(1);
    }
    BH1750_SCL(0);
    return 1;
}

static uint8_t BH1750_WriteByte(uint8_t data)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        BH1750_SDA((data & 0x80) != 0);
        BH1750_SCL(1);
        BH1750_SCL(0);
        data <<= 1;
    }
    return BH1750_WaitAck();
}

static uint8_t BH1750_ReadByte(uint8_t ack)
{
    uint8_t i;
    uint8_t data = 0;
    BH1750_SDA(1);
    for (i = 0; i < 8; i++)
    {
        data <<= 1;
        BH1750_SCL(1);
        if (BH1750_SDA_Read()) data |= 1;
        BH1750_SCL(0);
    }
    BH1750_SDA(ack ? 0 : 1);
    BH1750_SCL(1);
    BH1750_SCL(0);
    BH1750_SDA(1);
    return data;
}

static uint8_t BH1750_WriteCommand(uint8_t command)
{
    uint8_t ok;
    BH1750_Start();
    ok = BH1750_WriteByte((bh1750_address << 1) | 0);
    if (ok) ok = BH1750_WriteByte(command);
    BH1750_Stop();
    return ok;
}

uint8_t BH1750_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    gpio.GPIO_Pin = BH1750_SDA_PIN | BH1750_SCL_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);
    GPIO_SetBits(GPIOB, BH1750_SDA_PIN | BH1750_SCL_PIN);

    delay_ms(10);

    /* ADDR接地时为0x23，ADDR接VCC时为0x5C。 */
    bh1750_address = BH1750_ADDR_LOW;
    if (!BH1750_WriteCommand(BH1750_POWER_ON) ||
        !BH1750_WriteCommand(BH1750_CONT_HRES))
    {
        bh1750_address = BH1750_ADDR_HIGH;
        if (!BH1750_WriteCommand(BH1750_POWER_ON) ||
            !BH1750_WriteCommand(BH1750_CONT_HRES))
            return 0;
    }

    delay_ms(180);
    return 1;
}

uint8_t BH1750_ReadLux_x10(uint16_t *lux_x10)
{
    uint8_t high;
    uint8_t low;
    uint16_t raw;

    BH1750_Start();
    if (!BH1750_WriteByte((bh1750_address << 1) | 1))
    {
        BH1750_Stop();
        return 0;
    }
    high = BH1750_ReadByte(1);
    low = BH1750_ReadByte(0);
    BH1750_Stop();

    raw = ((uint16_t)high << 8) | low;
    *lux_x10 = (uint16_t)(((uint32_t)raw * 10 + 6) / 12);
    return 1;
}
