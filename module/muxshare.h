#include "../lvgl/lvgl.h"
#include "../common/theme.h"
#include "../common/language.h"
#include "../common/kiosk.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/collection.h"

extern size_t item_count;
extern content_item *items;

extern int bar_header;
extern int bar_footer;

extern struct mux_lang lang;
extern struct mux_config config;
extern struct mux_device device;
extern struct mux_kiosk kiosk;
extern struct theme_config theme;

extern int nav_moved;
extern int current_item_index;
extern int first_open;
extern int ui_count;

extern lv_obj_t *msgbox_element;
extern lv_obj_t *overlay_image;
extern lv_obj_t *kiosk_image;

extern int progress_onscreen;

extern lv_group_t *ui_group;
extern lv_group_t *ui_group_glyph;
extern lv_group_t *ui_group_panel;
extern lv_group_t *ui_group_value;
