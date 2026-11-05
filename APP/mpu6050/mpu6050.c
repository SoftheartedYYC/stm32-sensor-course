/**
 * @file    mpu6050.c
 * @brief   MPU6050 驱动：软件 I2C（PB6/PB7）+ AD0 地址选择（PB9）+ INT 数据就绪（PB8）
 *
 * 为什么自己搓一份软件 I2C：
 *   PB6/PB7 上已经挂着 OLED 和 BH1750，总线是"多主多从"共用的一条线。
 *   OLED 驱动里那套时序是给"只写不读 + 不看 ACK"优化的，MPU6050 要读寄存器、
 *   必须判 ACK，所以这里独立一份（和 APP/bh1750 的做法一致），互不影响。
 *
 * 读数据为什么用一次突发读：
 *   0x3B 起的 14 个字节是连续排列的，MPU6050 支持地址自增。
 *   一次事务读完，既快又保证 6 个轴是"同一时刻"采样出来的；
 *   分成 6 次读会读到不同时刻的数据，算倾角时会有偏差。
 */

#include "mpu6050.h"
#include "delay.h"
#include <math.h>

/* ------------------------------------------------------------------
 * 寄存器地址（只列本驱动用到的）
 * ------------------------------------------------------------------ */
#define MPU6050_REG_SMPLRT_DIV   0x19 /* 采样率分频 */
#define MPU6050_REG_CONFIG       0x1A /* 数字低通 + 外部同步 */
#define MPU6050_REG_GYRO_CONFIG  0x1B /* 陀螺量程 */
#define MPU6050_REG_ACCEL_CONFIG 0x1C /* 加速度量程 */
#define MPU6050_REG_INT_PIN_CFG  0x37 /* INT 引脚电平/驱动方式/锁存 */
#define MPU6050_REG_INT_ENABLE   0x38 /* 中断使能 */
#define MPU6050_REG_INT_STATUS   0x3A /* 中断标志 */
#define MPU6050_REG_ACCEL_XOUT_H 0x3B /* 加速度 6 字节 + 温度 2 字节 + 角速度 6 字节 */
#define MPU6050_REG_GYRO_XOUT_H  0x43 /* 角速度 6 字节 */
#define MPU6050_REG_PWR_MGMT_1   0x6B /* 电源管理 1：复位 / 睡眠 / 时钟源 */
#define MPU6050_REG_WHO_AM_I     0x75 /* 器件 ID */

#define MPU6050_RAD_TO_DEG 57.2957795f /* 弧度 -> 度 */

/* 陀螺仪零偏（原始 LSB），mpu6050_calibrate_gyro() 里标定，mpu6050_read() 里扣掉 */
static float s_gyro_offset[3] = {0.0f, 0.0f, 0.0f};

/* 内部函数声明 */
static void mpu6050_gpio_init(void);
static void i2c_start(void);
static void i2c_stop(void);
static uint8_t i2c_write_byte(uint8_t byte);
static uint8_t i2c_read_byte(uint8_t ack);
static uint8_t mpu_reg_read(uint8_t reg, uint8_t *buf, uint8_t len);
static int16_t mpu_be16(const uint8_t *p);

/* ------------------------------------------------------------------
 * 软件 I2C 时序层
 *   规则：数据只能在 SCL 低电平期间改变，SCL 高电平期间从机采样。
 * ------------------------------------------------------------------ */

/* 拉高/拉低：开漏输出，写 1 只是"放开"，真正拉高靠模块上的 4.7k 上拉 */
static void i2c_sda(uint8_t level)
{
    if (level != 0)
    {
        GPIO_SetBits(MPU6050_I2C_PORT, MPU6050_SDA_PIN);
    }
    else
    {
        GPIO_ResetBits(MPU6050_I2C_PORT, MPU6050_SDA_PIN);
    }
}

static void i2c_scl(uint8_t level)
{
    if (level != 0)
    {
        GPIO_SetBits(MPU6050_I2C_PORT, MPU6050_SCL_PIN);
    }
    else
    {
        GPIO_ResetBits(MPU6050_I2C_PORT, MPU6050_SCL_PIN);
    }
}

/* 读 SDA：开漏输出模式下 IDR 照样能读到引脚真实电平，不用来回切输入模式 */
static uint8_t i2c_sda_read(void)
{
    return (GPIO_ReadInputDataBit(MPU6050_I2C_PORT, MPU6050_SDA_PIN) == Bit_RESET) ? 0 : 1;
}

#define I2C_DELAY() delay_us(MPU6050_I2C_DELAY_US)

/* 起始条件：SCL 高电平期间，SDA 由高变低 */
static void i2c_start(void)
{
    i2c_sda(1);
    i2c_scl(1);
    I2C_DELAY();

    i2c_sda(0);
    I2C_DELAY();

    i2c_scl(0);
    I2C_DELAY();
}

/* 停止条件：SCL 高电平期间，SDA 由低变高 */
static void i2c_stop(void)
{
    i2c_sda(0);
    I2C_DELAY();

    i2c_scl(1);
    I2C_DELAY();

    i2c_sda(1);
    I2C_DELAY();
}

/**
 * @brief  发一个字节（高位在前），并把第 9 个时钟的应答读回来
 * @retval 1 = 从机给了 ACK；0 = 没人应答（器件不在/地址错）
 */
static uint8_t i2c_write_byte(uint8_t byte)
{
    uint8_t i;
    uint8_t ack;

    for (i = 0; i < 8; i++)
    {
        if ((byte & 0x80) != 0)
        {
            i2c_sda(1);
        }
        else
        {
            i2c_sda(0);
        }
        byte = (uint8_t)(byte << 1);

        I2C_DELAY();
        i2c_scl(1);
        I2C_DELAY();
        i2c_scl(0);
        I2C_DELAY();
    }

    /* 第 9 个时钟：主机放开 SDA，从机拉低表示 ACK */
    i2c_sda(1);
    I2C_DELAY();
    i2c_scl(1);
    I2C_DELAY();
    ack = (uint8_t)(i2c_sda_read() == 0 ? 1 : 0);
    i2c_scl(0);
    I2C_DELAY();

    return ack;
}

/**
 * @brief  读一个字节（高位在前）
 * @param  ack 1 = 读完给从机 ACK（还要继续读）；0 = 给 NACK（最后一个字节）
 */
static uint8_t i2c_read_byte(uint8_t ack)
{
    uint8_t i;
    uint8_t byte = 0;

    i2c_sda(1); /* 主机放开 SDA，交给从机拉 */

    for (i = 0; i < 8; i++)
    {
        byte = (uint8_t)(byte << 1);

        I2C_DELAY();
        i2c_scl(1);
        I2C_DELAY();
        if (i2c_sda_read() != 0)
        {
            byte |= 0x01;
        }
        i2c_scl(0);
        I2C_DELAY();
    }

    /* 第 9 个时钟：主机给应答 */
    i2c_sda((ack != 0) ? 0 : 1);
    I2C_DELAY();
    i2c_scl(1);
    I2C_DELAY();
    i2c_scl(0);
    I2C_DELAY();
    i2c_sda(1);

    return byte;
}

/* ------------------------------------------------------------------
 * 寄存器读写
 * ------------------------------------------------------------------ */

/**
 * @brief  连续读寄存器：START -> 写地址 -> 写寄存器号 -> 重复START -> 读地址 -> 连读 -> STOP
 * @note   靠 MPU6050 的寄存器地址自增，一次事务读多字节
 */
static uint8_t mpu_reg_read(uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;
    uint8_t ok;

    if (buf == 0 || len == 0)
    {
        return 0;
    }

    i2c_start();
    ok = i2c_write_byte((uint8_t)(MPU6050_ADDR7 << 1)); /* 写方向 */
    if (ok != 0)
    {
        ok = i2c_write_byte(reg); /* 告诉从机从哪个寄存器开始 */
    }
    if (ok == 0)
    {
        i2c_stop();
        return 0;
    }

    i2c_start();                                        /* 重复起始，不改方向就是写，所以要重来 */
    ok = i2c_write_byte((uint8_t)((MPU6050_ADDR7 << 1) | 0x01)); /* 读方向 */
    if (ok == 0)
    {
        i2c_stop();
        return 0;
    }

    for (i = 0; i < len; i++)
    {
        buf[i] = i2c_read_byte((uint8_t)((i + 1 < len) ? 1 : 0)); /* 最后一个字节给 NACK */
    }

    i2c_stop();
    return 1;
}

uint8_t mpu6050_read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    return mpu_reg_read(reg, buf, len);
}

uint8_t mpu6050_read_reg(uint8_t reg, uint8_t *value)
{
    return mpu_reg_read(reg, value, 1);
}

uint8_t mpu6050_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t ok;

    i2c_start();
    ok = i2c_write_byte((uint8_t)(MPU6050_ADDR7 << 1));
    if (ok != 0)
    {
        ok = i2c_write_byte(reg);
    }
    if (ok != 0)
    {
        ok = i2c_write_byte(value);
    }
    i2c_stop();

    return ok;
}

/* 大端两字节拼成有符号 16 位（MPU6050 的高低位是分开的两个寄存器） */
static int16_t mpu_be16(const uint8_t *p)
{
    return (int16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

/* ------------------------------------------------------------------
 * 引脚初始化
 * ------------------------------------------------------------------ */
static void mpu6050_gpio_init(void)
{
    GPIO_InitTypeDef gpio;

    /* PB6/PB7/PB8/PB9 都在 GPIOB，一次开时钟就够 */
    RCC_APB2PeriphClockCmd(MPU6050_I2C_RCC, ENABLE);

    /* SDA / SCL：开漏输出。F1 的内部上拉只在输入模式生效，所以这里靠模块上的 4.7k */
    gpio.GPIO_Pin = MPU6050_SDA_PIN | MPU6050_SCL_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(MPU6050_I2C_PORT, &gpio);
    GPIO_SetBits(MPU6050_I2C_PORT, MPU6050_SDA_PIN | MPU6050_SCL_PIN); /* 总线空闲：两根线都放开 */

    /* AD0：推挽输出，电平决定从机地址（0x68 / 0x69） */
    gpio.GPIO_Pin = MPU6050_AD0_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(MPU6050_AD0_PORT, &gpio);
    if (MPU6050_AD0_LEVEL != 0)
    {
        GPIO_SetBits(MPU6050_AD0_PORT, MPU6050_AD0_PIN);
    }
    else
    {
        GPIO_ResetBits(MPU6050_AD0_PORT, MPU6050_AD0_PIN);
    }

    /* INT：模块推挽输出、高有效，所以配下拉输入：没接时读到稳定的低电平，不会乱触发 */
    gpio.GPIO_Pin = MPU6050_INT_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(MPU6050_INT_PORT, &gpio);
}

/* ------------------------------------------------------------------
 * 对外接口
 * ------------------------------------------------------------------ */

uint8_t mpu6050_init(void)
{
    uint8_t id = 0;
    uint8_t status = 0;
    uint8_t ok;

    mpu6050_gpio_init();
    delay_ms(50); /* 等模块内部稳压和振荡器稳定 */

    /* 先确认总线上真的有片子：WHO_AM_I 是只读的固定 ID */
    if (mpu_reg_read(MPU6050_REG_WHO_AM_I, &id, 1) == 0)
    {
        return 0;
    }
    if (id != 0x68 && id != 0x70 && id != 0x72 && id != 0x98)
    {
        return 0; /* 0x68 是正品；其余是 MPU6500 系克隆片，寄存器兼容 */
    }

    /* 软复位：让所有寄存器回到默认值，避免上一次运行留下的配置 */
    ok = mpu6050_write_reg(MPU6050_REG_PWR_MGMT_1, 0x80);
    if (ok == 0)
    {
        return 0;
    }
    delay_ms(100); /* 数据手册要求复位后等 100ms */

    /* 唤醒 + 时钟源选 X 轴陀螺 PLL（比内部 8MHz RC 稳得多，零偏漂移小） */
    ok = mpu6050_write_reg(MPU6050_REG_PWR_MGMT_1, 0x01);
    if (ok == 0)
    {
        return 0;
    }
    delay_ms(30);

    mpu6050_write_reg(MPU6050_REG_SMPLRT_DIV, MPU6050_SMPLRT_DIV);
    mpu6050_write_reg(MPU6050_REG_CONFIG, (uint8_t)(MPU6050_DLPF_CFG & 0x07));
    mpu6050_write_reg(MPU6050_REG_GYRO_CONFIG, (uint8_t)((MPU6050_GYRO_FS_SEL & 0x03) << 3));
    mpu6050_write_reg(MPU6050_REG_ACCEL_CONFIG, (uint8_t)((MPU6050_ACCEL_FS_SEL & 0x03) << 3));

    /* INT 引脚：推挽、高有效、锁存（一直保持到被读走）+ 任意寄存器读清零
     * 这样主循环轮询 PB8 也不会漏掉"新数据"这个事件 */
    mpu6050_write_reg(MPU6050_REG_INT_PIN_CFG, 0x30);

    /* 只开"数据就绪"这一个中断源 */
    mpu6050_write_reg(MPU6050_REG_INT_ENABLE, 0x01);

    /* 读一次中断标志，把复位期间可能挂起的标志清掉 */
    mpu_reg_read(MPU6050_REG_INT_STATUS, &status, 1);

    return 1;
}

uint8_t mpu6050_who_am_i(uint8_t *id)
{
    return mpu_reg_read(MPU6050_REG_WHO_AM_I, id, 1);
}

uint8_t mpu6050_read(mpu6050_data_t *data)
{
    uint8_t buf[14];
    uint8_t i;

    if (data == 0)
    {
        return 0;
    }

    /* 0x3B 起连续 14 字节：AX AY AZ TEMP GX GY GZ（每个都是高字节在前） */
    if (mpu_reg_read(MPU6050_REG_ACCEL_XOUT_H, buf, 14) == 0)
    {
        return 0;
    }

    for (i = 0; i < 3; i++)
    {
        data->raw_accel[i] = mpu_be16(&buf[i * 2]);
        data->raw_gyro[i] = mpu_be16(&buf[8 + i * 2]);
    }
    data->raw_temp = mpu_be16(&buf[6]);

    /* 原始值 -> 物理量 */
    data->accel_g.x = (float)data->raw_accel[0] / MPU6050_ACCEL_LSB_PER_G;
    data->accel_g.y = (float)data->raw_accel[1] / MPU6050_ACCEL_LSB_PER_G;
    data->accel_g.z = (float)data->raw_accel[2] / MPU6050_ACCEL_LSB_PER_G;

    data->gyro_dps.x = ((float)data->raw_gyro[0] - s_gyro_offset[0]) / MPU6050_GYRO_LSB_PER_DPS;
    data->gyro_dps.y = ((float)data->raw_gyro[1] - s_gyro_offset[1]) / MPU6050_GYRO_LSB_PER_DPS;
    data->gyro_dps.z = ((float)data->raw_gyro[2] - s_gyro_offset[2]) / MPU6050_GYRO_LSB_PER_DPS;

    /* 温度公式来自数据手册：T(°C) = 原始值 / 340 + 36.53 */
    data->temp_c = (float)data->raw_temp / 340.0f + 36.53f;

    return 1;
}

uint8_t mpu6050_int_ready(void)
{
    uint8_t level;

    level = (GPIO_ReadInputDataBit(MPU6050_INT_PORT, MPU6050_INT_PIN) == Bit_RESET) ? 0 : 1;

#if (MPU6050_INT_ACTIVE_HIGH != 0)
    return level;
#else
    return (uint8_t)(level == 0 ? 1 : 0);
#endif
}

uint8_t mpu6050_calibrate_gyro(uint16_t samples)
{
    uint8_t buf[6];
    uint16_t i;
    uint8_t k;
    int32_t sum[3];

    if (samples == 0)
    {
        samples = 100;
    }

    sum[0] = 0;
    sum[1] = 0;
    sum[2] = 0;

    for (i = 0; i < samples; i++)
    {
        if (mpu_reg_read(MPU6050_REG_GYRO_XOUT_H, buf, 6) == 0)
        {
            return 0;
        }
        sum[0] += (int32_t)mpu_be16(&buf[0]);
        sum[1] += (int32_t)mpu_be16(&buf[2]);
        sum[2] += (int32_t)mpu_be16(&buf[4]);

        delay_ms(1); /* 让两次采样落在不同时刻，平均值才有意义 */
    }

    for (k = 0; k < 3; k++)
    {
        s_gyro_offset[k] = (float)sum[k] / (float)samples;
    }

    return 1;
}

void mpu6050_get_angles(const mpu6050_data_t *data, float *pitch, float *roll, float *tilt)
{
    float ax;
    float ay;
    float az;

    if (data == 0)
    {
        return;
    }

    ax = data->accel_g.x;
    ay = data->accel_g.y;
    az = data->accel_g.z;

    /* pitch：前后倾（绕 Y 轴），roll：左右倾（绕 X 轴）
     * 用 atan2 而不是 asin，好处是 az 接近 0（板子竖起来）时也不会炸 */
    if (pitch != 0)
    {
        *pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * MPU6050_RAD_TO_DEG;
    }
    if (roll != 0)
    {
        *roll = atan2f(ay, az) * MPU6050_RAD_TO_DEG;
    }
    if (tilt != 0)
    {
        /* 合成倾角：板子和水平面的夹角。Z 轴朝上（平放）= 0°，竖起来 = 90°，翻过来 = 180°
         * 用 atan2 合成就够了，不用 acosf（少链一个库函数） */
        *tilt = atan2f(sqrtf(ax * ax + ay * ay), az) * MPU6050_RAD_TO_DEG;
    }
}

void mpu6050_get_tilt(const mpu6050_data_t *data, float *pitch, float *roll)
{
    mpu6050_get_angles(data, pitch, roll, 0);
}
