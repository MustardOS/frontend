#include "empty_state.h"
#include "../options.h"
#include "common.h"
#include "glyph.h"

#define EMPTY_STATE_WIDTH_PCT 80

static lv_obj_t *panel = NULL;
static lv_obj_t *lbl_headline = NULL;
static lv_obj_t *lbl_guidance = NULL;
static lv_obj_t *action_row = NULL;
static lv_obj_t *action_glyph = NULL;
static lv_obj_t *action_label = NULL;

void empty_state_reset(void) {
    panel = NULL;

    lbl_headline = NULL;
    lbl_guidance = NULL;

    action_row = NULL;
    action_glyph = NULL;
    action_label = NULL;
}

static int usable(void) {
    if (panel && lv_obj_is_valid(panel)) return 1;
    if (!ui_screen || !lv_obj_is_valid(ui_screen)) return 0;

    empty_state_init(&theme, &device, ui_screen);
    return panel != NULL;
}

void empty_state_init(struct theme_config *t, struct mux_device *d, lv_obj_t *parent) {
    panel = NULL;

    lbl_headline = NULL;
    lbl_guidance = NULL;

    action_row = NULL;
    action_glyph = NULL;
    action_label = NULL;

    panel = lv_obj_create(parent);
    lv_obj_set_width(panel, d->mux.width * EMPTY_STATE_WIDTH_PCT / 100);
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_set_align(panel, LV_ALIGN_CENTER);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, MU_OBJ_FLAG_HIDE_FLOAT);

    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(panel, 10, MU_OBJ_MAIN_DEFAULT);

    lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lbl_headline = lv_label_create(panel);
    lv_obj_set_width(lbl_headline, lv_pct(100));
    lv_label_set_long_mode(lbl_headline, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(lbl_headline, lv_color_hex(t->list_default.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(lbl_headline, t->list_default.text_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(lbl_headline, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_label_set_text(lbl_headline, "");

    // TODO: Will eventually have to make this theme based - it's the helpful description
    // TODO: underneath the main "Content not found" messages etc.
    lbl_guidance = lv_label_create(panel);
    lv_obj_set_width(lbl_guidance, lv_pct(100));
    lv_label_set_long_mode(lbl_guidance, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(lbl_guidance, lv_color_hex(t->list_default.text), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_opa(lbl_guidance, 150, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_align(lbl_guidance, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_label_set_text(lbl_guidance, "");

    action_row = lv_obj_create(panel);
    lv_obj_set_width(action_row, LV_SIZE_CONTENT);
    lv_obj_set_height(action_row, LV_SIZE_CONTENT);
    lv_obj_clear_flag(action_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(action_row, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(action_row, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(action_row, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_flex_flow(action_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(action_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(action_row, MU_OBJ_FLAG_HIDE_FLOAT);

    action_glyph = create_footer_glyph(action_row, t, "x", t->nav.x, 0);
    action_label = create_footer_text(action_row, t, t->nav.x.text, t->nav.x.text_alpha, 0);
    lv_label_set_text(action_label, "");
}

void empty_state_show_action(const char *headline, const char *guidance, const char *glyph, const char *label) {
    empty_state_show(headline, guidance);

    if (!usable() || !glyph || !label || !label[0]) return;

    char embed[MAX_BUFFER_SIZE];
    if (get_glyph_path("footer", glyph, embed, sizeof(embed))) set_footer_glyph_image(action_glyph, embed);

    lv_label_set_text(action_label, label);
    lv_obj_clear_flag(action_row, MU_OBJ_FLAG_HIDE_FLOAT);
}

void empty_state_show(const char *headline, const char *guidance) {
    if (!usable()) return;

    lv_obj_add_flag(action_row, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_label_set_text(lbl_headline, headline ? headline : "");

    if (guidance && guidance[0]) {
        lv_label_set_text(lbl_guidance, guidance);
        lv_obj_clear_flag(lbl_guidance, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(lbl_guidance, MU_OBJ_FLAG_HIDE_FLOAT);
    }

    lv_obj_clear_flag(panel, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_move_foreground(panel);
}

void empty_state_hide(void) {
    if (!panel || !lv_obj_is_valid(panel)) return;

    lv_obj_add_flag(panel, MU_OBJ_FLAG_HIDE_FLOAT);
}
