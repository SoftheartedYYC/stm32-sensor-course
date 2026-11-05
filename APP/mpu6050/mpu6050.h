/**
 * @file    mpu6050.h
 * @brief   MPU6050 六轴传感器（三轴加速度 + 三轴角速度 + 温度）驱动
 *
 * 接线（和 OLED / BH1750 共用一条软件 I2C 总线）：
 *   SDA = PB6      开漏，靠模块上的 4.7k 上拉拉高
 *   SCL = PB7      开漏，同上
 *   INT = PB8      数据就绪中断输出（不接也能用，只用来看"有没有新数据"）
 *   AD0 = PB9      地址选择，MCU 推挽输出：拉低 -> 7 位地址 0x68，拉高 -> 0x69
 *
 * 依赖：delay_us() / delay_ms()（Board_Init() 里 delay_init() 已经最先跑过）
 *
 * 量程和带宽都做成下面的宏，改宏重新编译即可，不用动 .c 文件。
 */
#ifndef __MPU6050_H
#define __MPU6050_H

#include "stm32f10x.h"

/* ------------------------------------------------------------------
 * 接线：换引脚只改这几行
 * ------------------------------------------------------------------ */
#define MPU6050_I2C_PORT  GPIOB      /* SDA/SCL 所在端口 */
#define MPU6050_I2C_RCC   RCC_APB2Periph_GPIOB
#define MPU6050_SDA_PIN   GPIO_Pin_6 /* PB6 */
#define MPU6050_SCL_PIN   GPIO_Pin_7 /* PB7 */

#define MPU6050_INT_PORT  GPIOB      /* INT 所在端口 */
#define MPU6050_INT_PIN   GPIO_Pin_8 /* PB8 */
#define MPU6050_INT_ACTIVE_HIGH 1    /* 模块默认高电平有效（INT_PIN_CFG 里配成推挽高有效） */

#define MPU6050_AD0_PORT  GPIOB      /* AD0 所在端口 */
#define MPU6050_AD0_PIN   GPIO_Pin_9 /* PB9 */
#define MPU6050_AD0_LEVEL 0          /* 0 = AD0 拉低 -> 地址 0x68；1 = 拉高 -> 0x69 */

/* 7 位从机地址：由 AD0 电平直接算出来，改 MPU6050_AD0_LEVEL 就自动跟着变 */
#define MPU6050_ADDR7     (MPU6050_AD0_LEVEL ? 0x69 : 0x68)

/* ------------------------------------------------------------------
 * 总线速率
 *   半周期 5us ≈ 100kHz。模块支持 400kHz，但这条总线上还挂着 OLED，
 *   100kHz 最稳；嫌慢就往下调（2 最激进，约 250kHz）。
 * ------------------------------------------------------------------ */
#define MPU6050_I2C_DELAY_US 5

/* ------------------------------------------------------------------
 * 采样配置（改这里就能换量程/带宽）
 *   SMPLRT_DIV  ：采样率 = 1kHz / (1 + 该值)，4 -> 200Hz
 *   DLPF_CFG    ：数字低通 0~6，3 = 加速度 44Hz / 陀螺 42Hz（抖动和延迟的平衡点）
 *   GYRO_FS_SEL ：0=±250 1=±500 2=±1000 3=±2000 °/s
 *   ACCEL_FS_SEL：0=±2g  1=±4g   2=±8g    3=±16g
 * ------------------------------------------------------------------ */
#define MPU6050_SMPLRT_DIV   4
#define MPU6050_DLPF_CFG     3
#define MPU6050_GYRO_FS_SEL  1
#define MPU6050_ACCEL_FS_SEL 0

/* 灵敏度（LSB 每单位）：由上面两个量程宏自动选，量程与灵敏度的对应关系见数据手册 */
#if   (MPU6050_ACCEL_FS_SEL == 0)
#define MPU6050_ACCEL_LSB_PER_G 16384.0f
#elif (MPU6050_ACCEL_FS_SEL == 1)
#define MPU6050_ACCEL_LSB_PER_G 8192.0f
#elif (MPU6050_ACCEL_FS_SEL == 2)
#define MPU6050_ACCEL_LSB_PER_G 4096.0f
#else
#define MPU6050_ACCEL_LSB_PER_G 2048.0f
#endif

#if   (MPU6050_GYRO_FS_SEL == 0)
#define MPU6050_GYRO_LSB_PER_DPS 131.0f
#elif (MPU6050_GYRO_FS_SEL == 1)
#define MPU6050_GYRO_LSB_PER_DPS 65.5f
#elif (MPU6050_GYRO_FS_SEL == 2)
#define MPU6050_GYRO_LSB_PER_DPS 32.8f
#else
#define MPU6050_GYRO_LSB_PER_DPS 16.4f
#endif

/* WHO_AM_I 的期望值：正品 MPU6050 是 0x68；MPU6500/6500 系克隆片是 0x70/0x72/0x98 */
#define MPU6050_WHO_AM_I_DEFAULT 0x68

/* ------------------------------------------------------------------
 * 数据类型
 * ------------------------------------------------------------------ */

/* 三轴矢量：加速度用 g，角速度用 °/s */
typedef struct
{
    float x;
    float y;
    float z;
} mpu6050_vec3_t;

/* 一次读回来的全部数据 */
typedef struct
{
    mpu6050_vec3_t accel_g;   /* 加速度，单位 g（±2g 量程时 1.00 = 1g 重力） */
    mpu6050_vec3_t gyro_dps;  /* 角速度，单位 °/s（已经扣掉标定出来的零偏） */
    float temp_c;             /* 芯片温度，°C（不是环境温度，会比环境高几度） */
    int16_t raw_accel[3];     /* 原始值，顺序 X/Y/Z，方便自己换算 */
    int16_t raw_gyro[3];      /* 原始值，未扣零偏 */
    int16_t raw_temp;         /* 原始温度值 */
} mpu6050_data_t;

/* ------------------------------------------------------------------
 * 接口
 * ------------------------------------------------------------------ */

/* 初始化：配 PB6/PB7/PB8/PB9 -> 复位 -> 唤醒 -> 配采样率/带宽/量程 -> 开数据就绪中断
 * 返回 1 = 找到芯片且配置完成；0 = 总线上没有应答（没接/没电/地址不对） */
uint8_t mpu6050_init(void);

/* 一次性突发读 14 个字节（加速度 6 + 温度 2 + 角速度 6），同时完成单位换算
 * 返回 1 = 本次读成功；0 = I2C 无应答（读失败时 data 里的内容不动） */
uint8_t mpu6050_read(mpu6050_data_t *data);

/* 读 WHO_AM_I（0x75），正常是 0x68；用来判断"线接对没有" */
uint8_t mpu6050_who_am_i(uint8_t *id);

/* INT 引脚当前电平：1 = 有新数据可读（配置成锁存 + 任意读清零，读一次数据后自动变低） */
uint8_t mpu6050_int_ready(void);

/* 陀螺仪零偏标定：静止状态下采 samples 次取平均当零偏，之后 mpu6050_read() 自动扣掉
 * samples = 0 时按 100 次算（约 0.3s）。标定时板子必须静止不动，否则零偏反而是错的 */
uint8_t mpu6050_calibrate_gyro(uint16_t samples);

/* 由加速度算三个角度（度）。传 NULL 表示那个值不要：
 *   pitch 前后倾（绕 Y 轴），-90 ~ +90
 *   roll  左右倾（绕 X 轴），-180 ~ +180
 *   tilt  合成倾角（板子与水平面夹角），0 ~ 180  平放 0°、立起来 90°、翻过来 180°
 * 注意：只用加速度算，静态下准；板子一晃就跟着抖，要平滑得自己做互补滤波。 */
void mpu6050_get_angles(const mpu6050_data_t *data, float *pitch, float *roll, float *tilt);

/* 同上，但只算 pitch / roll（老接口，保留兼容） */
void mpu6050_get_tilt(const mpu6050_data_t *data, float *pitch, float *roll);

/* 底层寄存器读写：留给调试/扩展用（比如自己配 FIFO、中断、低功耗）
 * 返回 1 = 成功，0 = 无应答 */
uint8_t mpu6050_write_reg(uint8_t reg, uint8_t value);
uint8_t mpu6050_read_reg(uint8_t reg, uint8_t *value);
uint8_t mpu6050_read_regs(uint8_t reg, uint8_t *buf, uint8_t len);

#endif /* __MPU6050_H */
