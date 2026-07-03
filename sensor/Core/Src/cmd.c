#include "cmd.h"
#include "sx1278.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define CONFIG_FLASH_ADDR  0x0801FC00UL
#define CONFIG_MAGIC       0xBEEFCAFEUL

static UART_HandleTypeDef *cmd_huart;
static char rx_buf[64];
static uint8_t rx_idx;

SX1278_Config g_lora_cfg = SX1278_CFG_PERFORMANCE;
uint32_t g_interval_ms = 5000;
float g_offset_temp = 0.0f;
float g_offset_hum = 0.0f;
float g_offset_pres = 0.0f;
bool g_config_dirty = false;

static void config_save(void)
{
    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef erase = {0};
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = CONFIG_FLASH_ADDR;
    erase.NbPages = 1;
    uint32_t page_err;
    HAL_FLASHEx_Erase(&erase, &page_err);
    uint32_t data[6];
    data[0] = CONFIG_MAGIC;
    data[1] = (uint32_t)g_lora_cfg;
    data[2] = g_interval_ms;
    memcpy(&data[3], &g_offset_temp, 4);
    memcpy(&data[4], &g_offset_hum,  4);
    memcpy(&data[5], &g_offset_pres, 4);
    for (int i = 0; i < 6; i++) {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, CONFIG_FLASH_ADDR + i * 4, data[i]);
    }
    HAL_FLASH_Lock();
}

static bool config_load(void)
{
    uint32_t *p = (uint32_t *)CONFIG_FLASH_ADDR;
    if (p[0] != CONFIG_MAGIC) return false;
    uint32_t cfg     = p[1];
    uint32_t interval = p[2];
    float ot, oh, op;
    memcpy(&ot, &p[3], 4);
    memcpy(&oh, &p[4], 4);
    memcpy(&op, &p[5], 4);
    if (cfg >= SX1278_CFG_COUNT) return false;
    if (interval < 5000 || interval > 3600000) return false;
    if (ot < -100 || ot > 100) return false;
    if (oh < -100 || oh > 100) return false;
    if (op < -1000000 || op > 1000000) return false;
    g_lora_cfg    = (SX1278_Config)cfg;
    g_interval_ms = interval;
    g_offset_temp = ot;
    g_offset_hum  = oh;
    g_offset_pres = op;
    return true;
}

static const char *cfg_name(SX1278_Config cfg)
{
    switch (cfg) {
    case SX1278_CFG_POWER:       return "POWER";
    case SX1278_CFG_BALANCED:    return "BALANCED";
    case SX1278_CFG_PERFORMANCE: return "PERFORMANCE";
    default:                     return "?";
    }
}

static void respond(const char *msg)
{
    HAL_UART_Transmit(cmd_huart, (uint8_t *)msg, strlen(msg), 1000);
}

static int streq_ncase(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++; b++;
    }
    return *a == *b;
}

static void cmd_help(void)
{
    respond("Commands:\r\n"
            "  LORA POWER|BALANCED|PERF <-- switch mode\r\n"
            "  RATE <sec>                 <-- sensor interval (5-3600)\r\n"
            "  OFFSET T|H|P <val>        <-- cal offset\r\n"
            "  STATUS                     <-- show config\r\n"
            "  PING                       <-- PONG\r\n"
            "  HELP                       <-- this\r\n");
}

static void cmd_status(void)
{
    char buf[128];
    snprintf(buf, sizeof(buf),
             "Mode:%s  Rate:%lus  Off T:%.1f H:%.1f P:%.0f\r\n",
             cfg_name(g_lora_cfg), (unsigned long)(g_interval_ms / 1000),
             (double)g_offset_temp, (double)g_offset_hum,
             (double)g_offset_pres);
    respond(buf);
}

static void cmd_parse(const char *line)
{
    char cmd[16] = {0};
    char a1[16] = {0};
    float val = 0.0f;

    int n = sscanf(line, "%15s %15s %f", cmd, a1, &val);

    if (n < 1) return;

    if (streq_ncase(cmd, "PING")) {
        respond("PONG\r\n");
    } else if (streq_ncase(cmd, "HELP")) {
        cmd_help();
    } else if (streq_ncase(cmd, "STATUS")) {
        cmd_status();
    } else if (streq_ncase(cmd, "LORA")) {
        if (n < 2) { respond("ERR: usage LORA POWER|BALANCED|PERF\r\n"); return; }
        SX1278_Config new_cfg = g_lora_cfg;
        if (streq_ncase(a1, "POWER"))       new_cfg = SX1278_CFG_POWER;
        else if (streq_ncase(a1, "BALANCED"))    new_cfg = SX1278_CFG_BALANCED;
        else if (streq_ncase(a1, "PERF") ||
                 streq_ncase(a1, "PERFORMANCE")) new_cfg = SX1278_CFG_PERFORMANCE;
        else { respond("ERR: unknown mode\r\n"); return; }
        if (new_cfg != g_lora_cfg) {
            g_lora_cfg = new_cfg;
            SX1278_Init(g_lora_cfg);
        }
        g_config_dirty = true;
        config_save();
        char r[64];
        snprintf(r, sizeof(r), "OK LoRa=%s\r\n", cfg_name(g_lora_cfg));
        respond(r);
    } else if (streq_ncase(cmd, "RATE")) {
        int sec;
        if (sscanf(line, "%*s %d", &sec) != 1) {
            respond("ERR: usage RATE <seconds>\r\n"); return;
        }
        if (sec < 5 || sec > 3600) { respond("ERR: rate 5-3600s\r\n"); return; }
        g_interval_ms = (uint32_t)sec * 1000;
        g_config_dirty = true;
        config_save();
        char r[64];
        snprintf(r, sizeof(r), "OK rate=%lus\r\n", (unsigned long)(g_interval_ms / 1000));
        respond(r);
    } else if (streq_ncase(cmd, "OFFSET")) {
        if (sscanf(line, "%*s %15s %f", a1, &val) != 2) {
            respond("ERR: usage OFFSET T|H|P <val>\r\n"); return;
        }
        if (streq_ncase(a1, "T"))       { g_offset_temp = val; }
        else if (streq_ncase(a1, "H"))  { g_offset_hum  = val; }
        else if (streq_ncase(a1, "P"))  { g_offset_pres = val; }
        else { respond("ERR: use T, H, or P\r\n"); return; }
        g_config_dirty = true;
        config_save();
        respond("OK\r\n");
    } else {
        respond("ERR: unknown command. Try HELP\r\n");
    }
}

void CMD_Init(UART_HandleTypeDef *huart)
{
    cmd_huart = huart;
    rx_idx = 0;
    memset(rx_buf, 0, sizeof(rx_buf));
    if (config_load()) {
        respond("Loaded saved config\r\n");
    } else {
        respond("Defaults loaded\r\n");
    }
    respond("Ready. Type HELP for commands\r\n");
}

bool CMD_ConsumeDirty(void)
{
    bool was = g_config_dirty;
    g_config_dirty = false;
    return was;
}

void CMD_Poll(void)
{
    uint8_t ch;
    while (HAL_UART_Receive(cmd_huart, &ch, 1, 1) == HAL_OK) {
        if (ch == '\r' || ch == '\n') {
            if (rx_idx > 0) {
                rx_buf[rx_idx] = '\0';
                cmd_parse(rx_buf);
                rx_idx = 0;
            }
        } else if (rx_idx < (int)sizeof(rx_buf) - 1) {
            rx_buf[rx_idx++] = (char)ch;
        }
    }
}
