idf_component_register(
    SRCS "app.c" "app_screen.c" "app_upload.c"
    INCLUDE_DIRS "include"
    REQUIRES ui theme haptic net
)
