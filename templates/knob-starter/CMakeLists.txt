# ESP-IDF project for the knob-starter firmware (ESP32-S3 round knob display).
# Build: scripts/build.sh (Docker espressif/idf:release-v5.3). Flash: scripts/flash.sh (host esptool).
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Move LVGL's object pool (CONFIG_LV_MEM_SIZE_KILOBYTES, 128K) from internal SRAM to PSRAM.
# lv_mem_core_builtin.c's LV_ATTRIBUTE_LARGE_RAM_ARRAY has no Kconfig binding in this LVGL
# version, so it must be predefined globally here (before the component compiles). Freeing this
# 128K of internal RAM is what lets lwIP allocate a TCP send buffer during an upload (measured
# on this board: 8 KB free / 1.5 KB largest block at the point of the stall before this change).
# The pool holds widget/style metadata, not the pixel-draw hot path, so PSRAM latency does not
# cost UI responsiveness. Needs CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY (sdkconfig.defaults).
idf_build_set_property(COMPILE_DEFINITIONS "LV_ATTRIBUTE_LARGE_RAM_ARRAY=EXT_RAM_BSS_ATTR" APPEND)

project(knob_starter)
