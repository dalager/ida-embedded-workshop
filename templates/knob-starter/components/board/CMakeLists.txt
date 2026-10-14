# Header-only board definition. EVERY GPIO number in the firmware lives in include/board_*.h;
# board.h resolves the KNOB_BOARD Kconfig choice to exactly one of them. scripts/check-board-pins.sh
# fails the build if a pin literal appears anywhere else.
idf_component_register(INCLUDE_DIRS "include")
