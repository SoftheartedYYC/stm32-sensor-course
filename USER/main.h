/**
 * @file    main.h
 * @brief   自己的定义和变量放这里
 *
 * 现在只保留 main.c 初始化时用到的开关和常用阈值；
 * 后续写功能时，把宏、全局变量、函数声明继续加在这个文件里。
 */
#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f10x.h"

/* ------------------------------------------------------------------
 * 初始化开关
 * ------------------------------------------------------------------ */

/* 温度传感器二选一：1 = DHT11（默认），0 = DS18B20 */
#define TEMP_SENSOR_DHT11 1

/* BH1750 照度传感器：1 = 上电初始化（没接也不影响，BH1750_Init() 返回 0），0 = 完全不用 */
#define USE_BH1750 1

/* ------------------------------------------------------------------
 * 常用阈值（后续写阈值判断时直接用）
 *   温度 0~50°C，步进 1；湿度 20~90%RH，步进 5；光敏 0~95，步进 5
 * ------------------------------------------------------------------ */
#define TEMP_THRESHOLD_DEFAULT 27
#define TEMP_THRESHOLD_MIN     0
#define TEMP_THRESHOLD_MAX     50
#define TEMP_THRESHOLD_STEP    1

#define HUMI_THRESHOLD_DEFAULT 50
#define HUMI_THRESHOLD_MIN     20
#define HUMI_THRESHOLD_MAX     90
#define HUMI_THRESHOLD_STEP    5

#define LIGHT_THRESHOLD_MIN    0
#define LIGHT_THRESHOLD_MAX    95
#define LIGHT_THRESHOLD_STEP   5

/* ------------------------------------------------------------------
 * RGB 光照判暗门限（写光照/RGB 逻辑时用）
 *   AO：亮度 = 4095 - 原始值，低于 AO_BRIGHT_LEVEL 判暗
 *   BH1750：lux 低于 BH_DARK_LEVEL 判暗
 * ------------------------------------------------------------------ */
#define AO_BRIGHT_LEVEL    1500
#define BH_DARK_LEVEL       300
#define LIGHT_LEVEL_HYST     20
#define LIGHT_LEVEL_RAW_MAX 4095

/* ------------------------------------------------------------------
 * 下面写你自己的定义 / 变量声明 / 函数声明
 * ------------------------------------------------------------------ */

#endif /* __MAIN_H */
