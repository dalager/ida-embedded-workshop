#pragma once
// ST77916 vendor init table (ui_panel_init_cmds.c). Component-private: only ui.c uses it.
#include <stddef.h>
#include "esp_lcd_st77916.h"

extern const st77916_lcd_init_cmd_t ui_st77916_init_cmds[];
extern const size_t ui_st77916_init_cmds_count;
