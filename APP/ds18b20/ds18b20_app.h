/**
 * @file ds18b20_app.h
 * @brief DS18B20 温度传感器，数据线接 PB11
 */
#ifndef __DS18B20_APP_H
#define __DS18B20_APP_H

#include "stm32f10x.h"

/* 初始化 PB11 单总线引脚。数据线上需要外接约 4.7k 上拉到 3.3V。 */
void ds18b20_init(void);

/* 启动一次温度转换。返回 1 表示检测到传感器，0 表示没有检测到。 */
uint8_t ds18b20_start_conversion(void);

/* 查询转换是否完成：1=完成，0=还在转换。转换期间不阻塞等待。 */
uint8_t ds18b20_conversion_done(void);

/* 读取温度，单位为 0.1 摄氏度；例如 253 表示 25.3°C。
 * 返回 1=读取成功，0=通信或 CRC 校验失败。 */
uint8_t ds18b20_read_temperature(int16_t *temperature_x10);

#endif /* __DS18B20_APP_H */
