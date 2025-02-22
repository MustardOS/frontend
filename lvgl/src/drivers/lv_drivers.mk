CSRCS += display/sdl.c
CSRCS += input/evdev.c

DEPPATH += --dep-path $(LVGL_DIR)/$(LVGL_DIR_NAME)/src/drivers
VPATH += :$(LVGL_DIR)/$(LVGL_DIR_NAME)/src/drivers

CFLAGS += "-I$(LVGL_DIR)/$(LVGL_DIR_NAME)/src/drivers"
