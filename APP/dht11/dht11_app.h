/**
 * @file    dht11_app.h
 * @brief   DHT11 温湿度传感器，数据线接 PB11（与 DS18B20 共用）
 *
 * 接线：
 *   DATA -> PB11，VCC -> 3V3，GND -> GND
 *   DATA 需要 4.7k~10k 上拉到 3.3V；多数 3 针 DHT11 模块板上已经带了上拉。
 *
 * 注意：
 *   1. DHT11 和 DS18B20 都接 PB11，同一时间只能接一个。主程序用
 *      TEMP_SENSOR_DHT11 宏选择，两个驱动代码都保留。
 *   2. DHT11 的"单总线"不是 DS18B20 的 1-Wire，时序完全不同，
 *      不能复用 ds18b20_app.c 里的复位/读写函数。
 *   3. 上电后 DHT11 需要约 1s 稳定；主程序把第一次读取安排在 1s 之后。
 *   4. 调用 dht11_read() 前必须先 delay_init()（main 已保证）。
 */

#ifndef __DHT11_APP_H /* 头文件保护宏 */
#define __DHT11_APP_H

#include "stm32f10x.h" /* 标准外设库：uint8_t / GPIO */

/* ------------------------------------------------------------------
 * 接线：换引脚只改这里（端口和引脚要一起改）
 *   本工程按你的要求接 PB11（和 DS18B20 共用同一根线，二选一）。
 * ------------------------------------------------------------------ */
#define DHT11_PORT GPIOB
#define DHT11_PIN  GPIO_Pin_11

/* 初始化 PB11：推挽输出并拉高总线；读数据时会自动切成上拉输入。 */
void dht11_init(void);

/* 读一次温湿度，阻塞约 20ms。
 *   humidity_rh     输出湿度整数部分，单位 %RH（DHT11 的小数位固定为 0）；
 *   temperature_x10 输出温度，单位 0.1°C；DHT11 分辨率是 1°C，所以末位恒为 0。
 * 返回 1 = 读取成功，0 = 无响应或校验和错误。 */
uint8_t dht11_read(uint8_t *humidity_rh, int16_t *temperature_x10);

#endif /* __DHT11_APP_H */
