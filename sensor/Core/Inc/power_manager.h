#ifndef __POWER_MANAGER_H
#define __POWER_MANAGER_H

#include "main.h"
#include <stdint.h>

#define WAKEUP_INTERVAL_MINUTES 10

void PM_Init(void);
void PM_EnterStop(uint32_t seconds);

#endif
