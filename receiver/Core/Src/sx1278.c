#include "sx1278.h"

extern SPI_HandleTypeDef hspi1;

volatile bool sx1278_tx_done = false;
volatile bool sx1278_rx_done = false;

static void SX1278_CS_LOW(void)
{
    HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_RESET);
}

static void SX1278_CS_HIGH(void)
{
    HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_SET);
}

static void spi_wait_ready(void)
{
    while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY);
}

static uint8_t SX1278_ReadReg(uint8_t reg)
{
    uint8_t addr = reg & 0x7F;
    uint8_t val;
    SX1278_CS_LOW();
    HAL_SPI_Transmit(&hspi1, &addr, 1, 100);
    spi_wait_ready();
    HAL_SPI_Receive(&hspi1, &val, 1, 100);
    spi_wait_ready();
    SX1278_CS_HIGH();
    return val;
}

static void SX1278_WriteReg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2];
    buf[0] = reg | 0x80;
    buf[1] = val;
    SX1278_CS_LOW();
    HAL_SPI_Transmit(&hspi1, buf, 2, 100);
    spi_wait_ready();
    SX1278_CS_HIGH();
}

static void SX1278_WriteBurst(uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t addr = reg | 0x80;
    SX1278_CS_LOW();
    HAL_SPI_Transmit(&hspi1, &addr, 1, 100);
    spi_wait_ready();
    HAL_SPI_Transmit(&hspi1, data, len, 100);
    spi_wait_ready();
    SX1278_CS_HIGH();
}

void SX1278_HardReset(void)
{
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(LORA_RST_GPIO_Port, LORA_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(100);
}

static bool SX1278_CheckVersion(void)
{
    uint8_t ver = SX1278_ReadReg(SX1278_REG_VERSION);
    return (ver == 0x12);
}

static bool SX1278_WaitMode(uint8_t expected_mode)
{
    uint32_t timeout = HAL_GetTick() + 100;
    while (1) {
        uint8_t current = SX1278_ReadReg(SX1278_REG_OP_MODE);
        if ((current & 0x07) == expected_mode) return true;
        if (HAL_GetTick() >= timeout) return false;
    }
}

bool SX1278_Init(void)
{
    SX1278_HardReset();

    bool ok = SX1278_CheckVersion();
    if (!ok) return false;

    SX1278_WriteReg(SX1278_REG_OP_MODE, SX1278_MODE_LONG_RANGE | SX1278_MODE_SLEEP);
    HAL_Delay(100);

    SX1278_WriteReg(SX1278_REG_OP_MODE, SX1278_MODE_LONG_RANGE | SX1278_MODE_STDBY);
    SX1278_WaitMode(SX1278_MODE_STDBY);

    uint64_t frf = ((uint64_t)433000000 << 19) / SX1278_FXOSC;
    SX1278_WriteReg(SX1278_REG_FRF_MSB, (frf >> 16) & 0xFF);
    SX1278_WriteReg(SX1278_REG_FRF_MID, (frf >> 8) & 0xFF);
    SX1278_WriteReg(SX1278_REG_FRF_LSB, frf & 0xFF);

    SX1278_WriteReg(SX1278_REG_PA_CONFIG, 0x8F);
    SX1278_WriteReg(SX1278_REG_PA_RAMP, 0x09);
    SX1278_WriteReg(SX1278_REG_OCP, 0x0B);
    SX1278_WriteReg(SX1278_REG_LNA, 0x23);
    SX1278_WriteReg(SX1278_REG_PA_DAC, 0x84);

    SX1278_WriteReg(SX1278_REG_MODEM_CONFIG_1, 0x72);
    SX1278_WriteReg(SX1278_REG_MODEM_CONFIG_2, 0x94);
    SX1278_WriteReg(SX1278_REG_MODEM_CONFIG_3, 0x04);

    SX1278_WriteReg(SX1278_REG_SYMB_TIMEOUT_LSB, 0xFF);
    SX1278_WriteReg(SX1278_REG_PREAMBLE_MSB, 0x00);
    SX1278_WriteReg(SX1278_REG_PREAMBLE_LSB, 0x08);
    SX1278_WriteReg(SX1278_REG_SYNC_WORD, 0x12);

    SX1278_WriteReg(SX1278_REG_FIFO_TX_BASE_ADDR, 0x00);
    SX1278_WriteReg(SX1278_REG_FIFO_RX_BASE_ADDR, 0x00);

    SX1278_WriteReg(SX1278_REG_DIO_MAPPING_1, SX1278_DIO0_RX_DONE);

    SX1278_WriteReg(SX1278_REG_IRQ_FLAGS_MASK, 0xF7);

    return true;
}

bool SX1278_SendPacket(uint8_t *data, uint8_t len)
{
    if (len > SX1278_MAX_PACKET_SIZE) return false;

    SX1278_WriteReg(SX1278_REG_OP_MODE, SX1278_MODE_LONG_RANGE | SX1278_MODE_STDBY);
    SX1278_WaitMode(SX1278_MODE_STDBY);

    SX1278_WriteReg(SX1278_REG_FIFO_ADDR_PTR, 0x00);
    SX1278_WriteReg(SX1278_REG_PAYLOAD_LENGTH, len);
    SX1278_WriteBurst(SX1278_REG_FIFO, data, len);

    sx1278_tx_done = false;

    SX1278_WriteReg(SX1278_REG_IRQ_FLAGS, 0xFF);
    SX1278_WriteReg(SX1278_REG_OP_MODE, SX1278_MODE_LONG_RANGE | SX1278_MODE_TX);

    uint32_t timeout = HAL_GetTick() + 5000;
    while (!sx1278_tx_done)
    {
        if (HAL_GetTick() > timeout)
        {
            SX1278_HardReset();
            SX1278_Init();
            return false;
        }
    }

    SX1278_WriteReg(SX1278_REG_IRQ_FLAGS, 0xFF);
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

void SX1278_StartRX(void)
{
    SX1278_WriteReg(SX1278_REG_OP_MODE, SX1278_MODE_LONG_RANGE | SX1278_MODE_STDBY);
    SX1278_WaitMode(SX1278_MODE_STDBY);

    SX1278_WriteReg(SX1278_REG_DIO_MAPPING_1, SX1278_DIO0_RX_DONE);

    SX1278_WriteReg(SX1278_REG_IRQ_FLAGS, 0xFF);
    SX1278_WriteReg(SX1278_REG_FIFO_RX_BASE_ADDR, 0x00);

    sx1278_rx_done = false;

    SX1278_WriteReg(SX1278_REG_OP_MODE, SX1278_MODE_LONG_RANGE | SX1278_MODE_RX_CONTINUOUS);
}

bool SX1278_ReadPacket(uint8_t *data, uint8_t *len)
{
    uint8_t irq = SX1278_ReadReg(SX1278_REG_IRQ_FLAGS);

    if (!(irq & SX1278_IRQ_RX_DONE_MASK))
    {
        return false;
    }

    SX1278_WriteReg(SX1278_REG_IRQ_FLAGS, 0xFF);

    if (irq & SX1278_IRQ_PAYLOAD_CRC_ERROR_MASK)
    {
        return false;
    }

    uint8_t nb_bytes = SX1278_ReadReg(SX1278_REG_RX_NB_BYTES);
    if (nb_bytes == 0 || nb_bytes > SX1278_MAX_PACKET_SIZE)
    {
        return false;
    }

    SX1278_WriteReg(SX1278_REG_FIFO_ADDR_PTR,
                    SX1278_ReadReg(SX1278_REG_FIFO_RX_CURRENT_ADDR));

    uint8_t addr = SX1278_REG_FIFO;
    SX1278_CS_LOW();
    HAL_SPI_Transmit(&hspi1, &addr, 1, 100);
    spi_wait_ready();
    HAL_SPI_Receive(&hspi1, data, nb_bytes, 100);
    spi_wait_ready();
    SX1278_CS_HIGH();

    *len = nb_bytes;
    return true;
}

int SX1278_GetLastRSSI(void)
{
    int rssi = (int)SX1278_ReadReg(SX1278_REG_PKT_RSSI_VALUE);
    return rssi - 157;
}

int SX1278_GetLastSNR(void)
{
    int snr = (int8_t)SX1278_ReadReg(SX1278_REG_PKT_SNR_VALUE);
    return snr / 4;
}

uint8_t SX1278_ReadIrqFlags(void)
{
    return SX1278_ReadReg(SX1278_REG_IRQ_FLAGS);
}

void SX1278_ClearIRQ(void)
{
    SX1278_WriteReg(SX1278_REG_IRQ_FLAGS, 0xFF);
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == LORA_DIO0_Pin)
    {
        uint8_t irq = SX1278_ReadReg(SX1278_REG_IRQ_FLAGS);
        if (irq & SX1278_IRQ_TX_DONE_MASK)
        {
            sx1278_tx_done = true;
        }
        if (irq & SX1278_IRQ_RX_DONE_MASK)
        {
            sx1278_rx_done = true;
        }
    }
}
