#include "bme280.h"

static HAL_StatusTypeDef i2c_status;

static uint8_t BME280_ReadReg(BME280_HandleTypeDef *bme, uint8_t reg)
{
    uint8_t val = 0;
    i2c_status = HAL_I2C_Mem_Read(bme->hi2c, bme->addr << 1, reg,
                     I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
    return val;
}

static HAL_StatusTypeDef BME280_WriteReg(BME280_HandleTypeDef *bme, uint8_t reg, uint8_t val)
{
    i2c_status = HAL_I2C_Mem_Write(bme->hi2c, bme->addr << 1, reg,
                      I2C_MEMADD_SIZE_8BIT, &val, 1, 100);
    return i2c_status;
}

static void BME280_ReadBurst(BME280_HandleTypeDef *bme, uint8_t reg,
                             uint8_t *buf, uint8_t len)
{
    i2c_status = HAL_I2C_Mem_Read(bme->hi2c, bme->addr << 1, reg,
                     I2C_MEMADD_SIZE_8BIT, buf, len, 100);
}

static void BME280_ReadCalib(BME280_HandleTypeDef *bme)
{
    uint8_t calib[26];
    BME280_ReadBurst(bme, 0x88, calib, 24);

    bme->dig_T1 = (uint16_t)(calib[1] << 8 | calib[0]);
    bme->dig_T2 = (int16_t)(calib[3] << 8 | calib[2]);
    bme->dig_T3 = (int16_t)(calib[5] << 8 | calib[4]);
    bme->dig_P1 = (uint16_t)(calib[7] << 8 | calib[6]);
    bme->dig_P2 = (int16_t)(calib[9] << 8 | calib[8]);
    bme->dig_P3 = (int16_t)(calib[11] << 8 | calib[10]);
    bme->dig_P4 = (int16_t)(calib[13] << 8 | calib[12]);
    bme->dig_P5 = (int16_t)(calib[15] << 8 | calib[14]);
    bme->dig_P6 = (int16_t)(calib[17] << 8 | calib[16]);
    bme->dig_P7 = (int16_t)(calib[19] << 8 | calib[18]);
    bme->dig_P8 = (int16_t)(calib[21] << 8 | calib[20]);
    bme->dig_P9 = (int16_t)(calib[23] << 8 | calib[22]);

    bme->dig_H1 = BME280_ReadReg(bme, 0xA1);

    BME280_ReadBurst(bme, 0xE1, calib, 8);
    bme->dig_H2 = (int16_t)((uint16_t)calib[1] << 8 | calib[0]);
    bme->dig_H3 = calib[2];
    bme->dig_H4 = (int16_t)(((uint16_t)calib[3] << 4) | (calib[4] & 0x0F));
    bme->dig_H5 = (int16_t)(((uint16_t)calib[5] << 4) | (calib[4] >> 4));
    bme->dig_H6 = (int8_t)calib[6];
}

bool BME280_Init(BME280_HandleTypeDef *bme, I2C_HandleTypeDef *hi2c, uint8_t addr)
{
    bme->hi2c = hi2c;
    bme->addr = addr;
    bme->t_fine = 0;

    uint8_t id = BME280_ReadReg(bme, BME280_REG_CHIP_ID);
    if (id != BME280_CHIP_ID) return false;

    BME280_WriteReg(bme, BME280_REG_RESET, BME280_RESET_CMD);
    HAL_Delay(2);
    {
        uint8_t status;
        int tries = 5;
        do {
            HAL_Delay(2);
            BME280_ReadBurst(bme, BME280_REG_STATUS, &status, 1);
        } while (--tries && (status & 0x01));
    }

    BME280_ReadCalib(bme);

    BME280_WriteReg(bme, BME280_REG_CTRL_HUM, BME280_OVERSAMPLING_1);
    BME280_WriteReg(bme, BME280_REG_CONFIG,
                    (BME280_STANDBY_0_5 << 5) | (BME280_FILTER_OFF << 2));
    BME280_WriteReg(bme, BME280_REG_CTRL_MEAS,
                    (BME280_OVERSAMPLING_1 << 5) |
                    (BME280_OVERSAMPLING_1 << 2) |
                    BME280_MODE_SLEEP);

    return true;
}

static int32_t BME280_Compensate_T(BME280_HandleTypeDef *bme, int32_t adc_T)
{
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)bme->dig_T1 << 1)))
                   * ((int32_t)bme->dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)bme->dig_T1))
                   * ((adc_T >> 4) - ((int32_t)bme->dig_T1))) >> 12)
                   * ((int32_t)bme->dig_T3)) >> 14;
    bme->t_fine = var1 + var2;
    return (bme->t_fine * 5 + 128) >> 8;
}

static uint32_t BME280_Compensate_P(BME280_HandleTypeDef *bme, int32_t adc_P)
{
    int64_t var1 = ((int64_t)bme->t_fine) - 128000;
    int64_t var2 = var1 * var1 * (int64_t)bme->dig_P6;
    var2 = var2 + ((var1 * (int64_t)bme->dig_P5) << 17);
    var2 = var2 + (((int64_t)bme->dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)bme->dig_P3) >> 8) +
           ((var1 * (int64_t)bme->dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)bme->dig_P1) >> 33;
    if (var1 == 0) return 0;
    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)bme->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)bme->dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)bme->dig_P7) << 4);
    return (uint32_t)p;
}

static uint32_t BME280_Compensate_H(BME280_HandleTypeDef *bme, int32_t adc_H)
{
    int32_t v_x1_u32r = bme->t_fine - ((int32_t)76800);
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)bme->dig_H4) << 20)
                  - (((int32_t)bme->dig_H5) * v_x1_u32r))
                  + ((int32_t)16384)) >> 15)
                  * (((((((v_x1_u32r * ((int32_t)bme->dig_H6)) >> 10)
                  * (((v_x1_u32r * ((int32_t)bme->dig_H3)) >> 11)
                  + ((int32_t)32768))) >> 10) + ((int32_t)2097152))
                  * ((int32_t)bme->dig_H2) + 8192) >> 14));
    v_x1_u32r = v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7)
                  * ((int32_t)bme->dig_H1)) >> 4);
    v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
    v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;
    return (uint32_t)(v_x1_u32r >> 12);
}

bool BME280_ReadAll(BME280_HandleTypeDef *bme, BME280_Data *data)
{
    BME280_WriteReg(bme, BME280_REG_CTRL_HUM, BME280_OVERSAMPLING_1);
    if (i2c_status != HAL_OK) goto err;

    BME280_WriteReg(bme, BME280_REG_CTRL_MEAS,
                    (BME280_OVERSAMPLING_1 << 5) |
                    (BME280_OVERSAMPLING_1 << 2) |
                    BME280_MODE_FORCED);
    if (i2c_status != HAL_OK) goto err;

    HAL_Delay(10);

    {
        uint8_t press_msb  = BME280_ReadReg(bme, 0xF7);
        uint8_t press_lsb  = BME280_ReadReg(bme, 0xF8);
        uint8_t press_xlsb = BME280_ReadReg(bme, 0xF9);
        uint8_t temp_msb   = BME280_ReadReg(bme, 0xFA);
        uint8_t temp_lsb   = BME280_ReadReg(bme, 0xFB);
        uint8_t temp_xlsb  = BME280_ReadReg(bme, 0xFC);
        uint8_t hum_msb    = BME280_ReadReg(bme, 0xFD);
        uint8_t hum_lsb    = BME280_ReadReg(bme, 0xFE);
        if (i2c_status != HAL_OK) goto err;

        int32_t adc_p = ((int32_t)press_msb << 12) | ((int32_t)press_lsb << 4) | (press_xlsb >> 4);
        int32_t adc_t = ((int32_t)temp_msb << 12) | ((int32_t)temp_lsb << 4) | (temp_xlsb >> 4);
        int32_t adc_h = ((int32_t)hum_msb << 8) | hum_lsb;

        int32_t t_comp = BME280_Compensate_T(bme, adc_t);
        data->temperature = t_comp / 100.0f;

        uint32_t p_var4 = BME280_Compensate_P(bme, adc_p);
        data->pressure = (float)p_var4 / 256.0f;

        uint32_t h_comp = BME280_Compensate_H(bme, adc_h);
        data->humidity = h_comp / 1024.0f;
    }
    return true;

err:
    data->temperature = -(float)i2c_status;
    data->humidity = 0.0f;
    data->pressure = 0.0f;
    HAL_I2C_DeInit(bme->hi2c);
    HAL_I2C_Init(bme->hi2c);
    return false;
}
