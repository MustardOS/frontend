CSRCS += lv_font.c
CSRCS += lv_font_fmt_txt.c
CSRCS += lv_font_loader.c

CSRCS += lv_font_unscii_8.c

DEPPATH += --dep-path $(LVGL_DIR)/$(LVGL_DIR_NAME)/src/font
VPATH += :$(LVGL_DIR)/$(LVGL_DIR_NAME)/src/font

CFLAGS += "-I$(LVGL_DIR)/$(LVGL_DIR_NAME)/src/font"
