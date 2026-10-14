idf_component_register(
    SRCS "ui.c" "knob.c" "knob_decode.c" "backlight.c" "ui_panel_init_cmds.c" "fonts/font_muli_28.c"
    INCLUDE_DIRS "include" "fonts"
    REQUIRES driver esp_lcd esp_timer board
)

# Generated LVGL fonts guard their include with LV_LVGL_H_INCLUDE_SIMPLE → use "lvgl.h".
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_LVGL_H_INCLUDE_SIMPLE)
