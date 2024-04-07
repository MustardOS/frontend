LV_DRIVERS_DIR_NAME ?= drivers

override CFLAGS := -I$(LVGL_DIR) $(CFLAGS)

CSRCS += $(wildcard $(LVGL_DIR)/lvgl/$(LV_DRIVERS_DIR_NAME)/*.c)
CSRCS += $(wildcard $(LVGL_DIR)/lvgl/$(LV_DRIVERS_DIR_NAME)/indev/*.c)
CSRCS += $(wildcard $(LVGL_DIR)/lvgl/$(LV_DRIVERS_DIR_NAME)/display/*.c)
