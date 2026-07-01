#ifndef __BME280_H
#define __BME280_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define BME280_I2C_ADDR_PRIM   0x76
#define BME280_I2C_ADDR_SEC    0x77

#define BME280_REG_CHIP_ID     0xD0
#define BME280_REG_RESET       0xE0
#define BME280_REG_CTRL_HUM    0xF2
#define BME280_REG_STATUS      0xF3
#define BME280_REG_CTRL_MEAS   0xF4
#define BME280_REG_CONFIG      0xF5
#define BME280_REG_PRESS_MSB   0xF7
#define BME280_REG_TEMP_MSB    0xFA
#define BME280_REG_HUM_MSB     0xFD

#define BME280_CHIP_ID         0x60

#define BME280_RESET_CMD       0xB6

#define BME280_OVERSAMPLING_SKIP 0
#define BME280_OVERSAMPLING_1    1
#define BME280_OVERSAMPLING_2    2
#define BME280_OVERSAMPLING_4    3
#define BME280_OVERSAMPLING_8    4
#define BME280_OVERSAMPLING_16   5

#define BME280_MODE_SLEEP   0
#define BME280_MODE_FORCED  1
#define BME280_MODE_NORMAL  3

#define BME280_STANDBY_0_5   0
#define BME280_STANDBY_62_5  1
#define BME280_STANDBY_125   2
#define BME280_STANDBY_250   3
#define BME280_STANDBY_500   4
#define BME280_STANDBY_1000  5
#define BME280_STANDBY_10    6
#define BME280_STANDBY_20    7

#define BME280_FILTER_OFF  0
#define BME280_FILTER_2    1
#define BME280_FILTER_4    2
#define BME280_FILTER_8    3
#define BME280_FILTER_16   4

typedef struct {
    uint8_t  addr;
    I2C_HandleTypeDef *hi2c;

    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4;
    int16_t  dig_H5;
    int8_t   dig_H6;

    int32_t  t_fine;
} BME280_HandleTypeDef;

typedef struct {
    float temperature;
    float humidity;
    float pressure;
} BME280_Data;

bool BME280_Init(BME280_HandleTypeDef *bme, I2C_HandleTypeDef *hi2c, uint8_t addr);
bool BME280_ReadAll(BME280_HandleTypeDef *bme, BME280_Data *data);

#endif
