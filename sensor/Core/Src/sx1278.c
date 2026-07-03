#include "sx1278.h"

extern SPI_HandleTypeDef hspi1;

volatile bool sx1278_tx_done = false;

static void SX1278_CS_LOW(void)
{
    HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_RESET);
}

static void SX1278_CS_HIGH(void)
{
    HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_SET);
}

uint8_t SX1278_ReadReg(uint8_t reg)
{
    uint8_t addr = reg & 0x7F;
    uint8_t val;
    SX1278_CS_LOW();
    HAL_SPI_Transmit(&hspi1, &addr, 1, 100);
    HAL_SPI_Receive(&hspi1, &val, 1, 100);
    SX1278_CS_HIGH();
    return val;
}

void SX1278_WriteReg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2];
    buf[0] = reg | 0x80;
    buf[1] = val;
    SX1278_CS_LOW();
    HAL_SPI_Transmit(&hspi1, buf, 2, 100);
    SX1278_CS_HIGH();
}

static void SX1278_WriteBurst(uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t addr = reg | 0x80;
    SX1278_CS_LOW();
    HAL_SPI_Transmit(&hspi1, &addr, 1, 100);
    HAL_SPI_Transmit(&hspi1, data, len, 100);
    SX1278_CS_HIGH();
}

void SX1278_HardReset(void)
{
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(10);
}

static bool SX1278_CheckVersion(void)
{
    uint8_t ver = SX1278_ReadReg(SX1278_REG_VERSION);
    return (ver == 0x12);
}

typedef struct {
    uint8_t pa_config;
    uint8_t pa_dac;
    uint8_t ocp;
    uint8_t lna;
    uint8_t modem_cfg1;
    uint8_t modem_cfg2;
    uint8_t modem_cfg3;
} SX1278_ConfigTable;

static const SX1278_ConfigTable cfg_table[SX1278_CFG_COUNT] = {
    [SX1278_CFG_POWER]       = { 0x8C, 0x00, 0x0B, 0x23, 0x72, 0x74, 0x04 },
    [SX1278_CFG_BALANCED]    = { 0xCF, 0x00, 0x0B, 0x23, 0x72, 0x94, 0x04 },
    [SX1278_CFG_PERFORMANCE] = { 0xFF, 0x87, 0x1F, 0x23, 0x82, 0xC4, 0x04 },
};

bool SX1278_Init(SX1278_Config cfg)
{
    if (cfg >= SX1278_CFG_COUNT) return false;

    SX1278_HardReset();

    bool ok = SX1278_CheckVersion();
    if (!ok) return false;

    SX1278_WriteReg(SX1278_REG_OP_MODE, SX1278_MODE_LONG_RANGE | SX1278_MODE_SLEEP);
    HAL_Delay(10);

    SX1278_WriteReg(SX1278_REG_OP_MODE, SX1278_MODE_LONG_RANGE | SX1278_MODE_STDBY);

    uint64_t frf = ((uint64_t)433000000 << 19) / SX1278_FXOSC;
    SX1278_WriteReg(SX1278_REG_FRF_MSB, (frf >> 16) & 0xFF);
    SX1278_WriteReg(SX1278_REG_FRF_MID, (frf >> 8) & 0xFF);
    SX1278_WriteReg(SX1278_REG_FRF_LSB, frf & 0xFF);

    const SX1278_ConfigTable *c = &cfg_table[cfg];
    SX1278_WriteReg(SX1278_REG_PA_CONFIG, c->pa_config);
    SX1278_WriteReg(SX1278_REG_PA_RAMP, 0x09);
    SX1278_WriteReg(SX1278_REG_OCP, c->ocp);
    SX1278_WriteReg(SX1278_REG_LNA, c->lna);
    if (c->pa_dac) {
        SX1278_WriteReg(SX1278_REG_PA_DAC, c->pa_dac);
    }
    SX1278_WriteReg(SX1278_REG_MODEM_CONFIG_1, c->modem_cfg1);
    SX1278_WriteReg(SX1278_REG_MODEM_CONFIG_2, c->modem_cfg2);
    SX1278_WriteReg(SX1278_REG_MODEM_CONFIG_3, c->modem_cfg3);

    SX1278_WriteReg(0x39, 0x12);

    SX1278_WriteReg(SX1278_REG_FIFO_TX_BASE_ADDR, 0x00);
    SX1278_WriteReg(SX1278_REG_FIFO_RX_BASE_ADDR, 0x00);

    SX1278_WriteReg(SX1278_REG_DIO_MAPPING_1, (SX1278_DIO0_TX_DONE << 6));

    SX1278_WriteReg(SX1278_REG_IRQ_FLAGS_MASK, 0xF7);
    SX1278_WriteReg(SX1278_REG_IRQ_FLAGS, 0xFF);

    return true;
}

bool SX1278_SendPacket(uint8_t *data, uint8_t len, SX1278_Config cfg)
{
    (void)cfg;

    if (len > SX1278_MAX_PACKET_SIZE) return false;

    SX1278_WriteReg(SX1278_REG_OP_MODE, SX1278_MODE_LONG_RANGE | SX1278_MODE_STDBY);

    SX1278_WriteReg(SX1278_REG_FIFO_ADDR_PTR, 0x00);
    SX1278_WriteReg(SX1278_REG_PAYLOAD_LENGTH, len);
    SX1278_WriteBurst(SX1278_REG_FIFO, data, len);

    sx1278_tx_done = false;

    SX1278_WriteReg(SX1278_REG_OP_MODE, SX1278_MODE_LONG_RANGE | SX1278_MODE_TX);

    uint32_t timeout = HAL_GetTick() + 5000;
    while (1)
    {
        uint8_t flags = SX1278_ReadReg(SX1278_REG_IRQ_FLAGS);
        if (flags & 0x08)
        {
            sx1278_tx_done = true;
            break;
        }
        if (HAL_GetTick() > timeout) break;
    }

    SX1278_WriteReg(SX1278_REG_IRQ_FLAGS, 0xFF);

    if (!sx1278_tx_done) return false;

    return true;
}

void SX1278_EnterSleep(void)
{
    SX1278_WriteReg(SX1278_REG_OP_MODE, SX1278_MODE_LONG_RANGE | SX1278_MODE_SLEEP);
}

void SX1278_SetStandby(void)
{
    SX1278_WriteReg(SX1278_REG_OP_MODE, SX1278_MODE_LONG_RANGE | SX1278_MODE_STDBY);
}

uint8_t SX1278_ReadVersion(void)
{
    return SX1278_ReadReg(SX1278_REG_VERSION);
}

uint8_t SX1278_ReadIrqFlags(void)
{
    return SX1278_ReadReg(SX1278_REG_IRQ_FLAGS);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == LORA_DIO0_Pin)
    {
        sx1278_tx_done = true;
    }
}
