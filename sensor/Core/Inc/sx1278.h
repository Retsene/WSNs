#ifndef __SX1278_H
#define __SX1278_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define SX1278_REG_FIFO         0x00
#define SX1278_REG_OP_MODE      0x01
#define SX1278_REG_FRF_MSB      0x06
#define SX1278_REG_FRF_MID      0x07
#define SX1278_REG_FRF_LSB      0x08
#define SX1278_REG_PA_CONFIG    0x09
#define SX1278_REG_PA_RAMP      0x0A
#define SX1278_REG_OCP          0x0B
#define SX1278_REG_LNA          0x0C
#define SX1278_REG_FIFO_ADDR_PTR 0x0D
#define SX1278_REG_FIFO_TX_BASE_ADDR 0x0E
#define SX1278_REG_FIFO_RX_BASE_ADDR 0x0F
#define SX1278_REG_FIFO_RX_CURRENT_ADDR 0x10
#define SX1278_REG_IRQ_FLAGS_MASK 0x11
#define SX1278_REG_IRQ_FLAGS    0x12
#define SX1278_REG_RX_NB_BYTES  0x13
#define SX1278_REG_PKT_SNR_VALUE 0x19
#define SX1278_REG_PKT_RSSI_VALUE 0x1A
#define SX1278_REG_MODEM_CONFIG_1 0x1D
#define SX1278_REG_MODEM_CONFIG_2 0x1E
#define SX1278_REG_MODEM_CONFIG_3 0x26
#define SX1278_REG_PAYLOAD_LENGTH 0x22
#define SX1278_REG_FIFO_RX_BYTE_ADDR 0x25
#define SX1278_REG_DIO_MAPPING_1 0x40
#define SX1278_REG_DIO_MAPPING_2 0x41
#define SX1278_REG_VERSION       0x42
#define SX1278_REG_PA_DAC        0x4D

#define SX1278_MODE_LONG_RANGE   0x80
#define SX1278_MODE_SLEEP        0x00
#define SX1278_MODE_STDBY        0x01
#define SX1278_MODE_TX           0x03
#define SX1278_MODE_RX_CONTINUOUS 0x05
#define SX1278_MODE_RX_SINGLE    0x06

#define SX1278_FXOSC             32000000ULL

#define SX1278_IRQ_TX_DONE_MASK  0x08
#define SX1278_IRQ_RX_DONE_MASK  0x40
#define SX1278_IRQ_RX_TIMEOUT_MASK 0x80

#define SX1278_DIO0_TX_DONE      0x01
#define SX1278_DIO0_RX_DONE      0x00

#define SX1278_MAX_PACKET_SIZE   255

typedef enum {
    SX1278_CFG_POWER = 0,
    SX1278_CFG_BALANCED,
    SX1278_CFG_PERFORMANCE,
    SX1278_CFG_COUNT
} SX1278_Config;

extern volatile bool sx1278_tx_done;

bool SX1278_Init(SX1278_Config cfg);
bool SX1278_SendPacket(uint8_t *data, uint8_t len);
void SX1278_HardReset(void);
void SX1278_EnterSleep(void);
void SX1278_SetStandby(void);
uint8_t SX1278_ReadVersion(void);
uint8_t SX1278_ReadIrqFlags(void);

#endif
