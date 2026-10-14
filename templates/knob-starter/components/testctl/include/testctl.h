#pragma once
// testctl — development-only serial console on USB Serial/JTAG for automated verification loops
// (see the verify-on-device skill). Protocol: one command per line, replies start with "> ".
//   knob <+N|-N>   inject N detents (sign = direction)        -> "> state=idle counter=N"
//   tap            inject one short tap                       -> "> state=... counter=..."
//   hold           inject one long press                      -> "> state=... counter=..."
//   reset          back to boot state (idle, counter 0)       -> "> state=idle counter=0"
//   state          report the model                           -> "> state=... counter=..."
//   snap           dump the framebuffer: "SNAP BEGIN w h RGB565", 1 row per "S:<base64>" line,
//                  "SNAP END" (logging is muted meanwhile)
//   ping           -> "> pong"
// Depends on app (the app_inject_* hooks) and ui (the LVGL lock). Compiled only with
// CONFIG_KNOB_TESTCTL; the whole component is a no-op in a production image.
void testctl_init(void);
