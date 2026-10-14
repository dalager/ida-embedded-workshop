# DRV2605L haptic driver on the touch I2C bus. Compiled out when BOARD_HAS_HAPTIC is 0 and turned
# into no-ops at runtime when the chip does not answer the probe, so callers never need to check.
idf_component_register(
    SRCS "haptic.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_i2c board
)
