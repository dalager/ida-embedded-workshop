#pragma once
// Board selection. Include this, never a board_*.h directly: it resolves the KNOB_BOARD Kconfig
// choice to exactly one pin/capability header. Depends on nothing but sdkconfig.h.
#include "sdkconfig.h"

#if defined(CONFIG_KNOB_BOARD_JC3636K518C)
#include "board_jc3636k518c.h"
#else
#error "No KNOB_BOARD selected (Kconfig: Knob board -> Target knob board)"
#endif
