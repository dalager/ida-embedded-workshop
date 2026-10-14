idf_component_register(
    SRCS "testctl.c"
    INCLUDE_DIRS "include"
    REQUIRES app ui esp_driver_usb_serial_jtag mbedtls
)
