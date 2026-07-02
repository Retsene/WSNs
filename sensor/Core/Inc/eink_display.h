#ifndef __EINK_DISPLAY_H
#define __EINK_DISPLAY_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define EINK_WIDTH   200
#define EINK_HEIGHT  200

#define EINK_COLOR_WHITE 0xFF
#define EINK_COLOR_BLACK 0x00

#define EINK_W_BUFF_SIZE ((EINK_WIDTH % 8 == 0) ? (EINK_WIDTH / 8) : (EINK_WIDTH / 8 + 1))
#define EINK_BUFF_SIZE   (EINK_W_BUFF_SIZE * EINK_HEIGHT)

typedef struct {
    uint16_t width;
    uint16_t height;
} EINK_Config;

bool EINK_Init(EINK_Config *cfg);
void EINK_Clear(void);
void EINK_Update(void);
void EINK_Sleep(void);
void EINK_DrawString(int x, int y, const char *str, uint8_t size);
void EINK_DrawStringScaled(int x, int y, const char *str, uint8_t scale);
void EINK_LoadImage(const uint8_t *image);

#endif
