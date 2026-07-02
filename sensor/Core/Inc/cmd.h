#ifndef __CMD_H
#define __CMD_H

#include "main.h"
#include "sx1278.h"
#include <stdint.h>

extern SX1278_Config g_lora_cfg;
extern uint32_t g_interval_ms;
extern float g_offset_temp;
extern float g_offset_hum;
extern float g_offset_pres;
extern bool g_config_dirty;

void CMD_Init(UART_HandleTypeDef *huart);
void CMD_Poll(void);
bool CMD_ConsumeDirty(void);

#endif
