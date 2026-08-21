#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../input/hotkeys.h"
#include "../netplay/netplay.h"

#define ADVISORY_DELAY_MS (5 * 60 * 1000)
#define ADVISORY_HOLD_MS  3000
#define ADVISORY_STEP_MAX 100
#define ADVISORY_TEXT_MAX 128

static const char advisory_token[] = "706f6b656d6f6e";

static const char advisory_text[] = "41726520796f75207265616c6c7920656e6a6f79696e6720746869732067616d653f";

static int advisory_armed = 0;
static int advisory_spent = 0;

static uint32_t advisory_elapsed = 0;
static uint32_t advisory_since = 0;
static uint32_t advisory_last = 0;

static lv_obj_t *ui_pnl_advisory = NULL;
static lv_obj_t *ui_lbl_advisory = NULL;

static void decode_pairs(const char *source, char *out, const size_t size) {
    size_t written = 0;

    while (source[0] && source[1] && written + 1 < size) {
        const char pair[3] = {source[0], source[1], '\0'};
        out[written++] = (char) strtol(pair, NULL, 16);
        source += 2;
    }

    out[written] = '\0';
}

static void normalise_title(const char *path, char *out, const size_t size) {
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;

    size_t written = 0;
    for (const unsigned char *cursor = (const unsigned char *) name; *cursor && written + 1 < size; cursor++) {
        if (cursor[0] == 0xC3 && cursor[1] == 0xA9) {
            out[written++] = 'e';
            cursor++;
            continue;
        }

        out[written++] = cursor[0] == 0xE9 ? 'e' : (char) tolower(cursor[0]);
    }

    out[written] = '\0';
}

void advisory_init(const char *content_path) {
    advisory_armed = 0;
    advisory_spent = 0;
    advisory_elapsed = 0;
    advisory_since = 0;
    advisory_last = 0;
    ui_pnl_advisory = NULL;
    ui_lbl_advisory = NULL;

    if (!content_path || !*content_path) return;

    char token[32];
    decode_pairs(advisory_token, token, sizeof(token));

    char title[PATH_MAX];
    normalise_title(content_path, title, sizeof(title));

    advisory_armed = strstr(title, token) != NULL;
}

static void advisory_conceal(void) {
    if (ui_pnl_advisory) lv_obj_add_flag(ui_pnl_advisory, LV_OBJ_FLAG_HIDDEN);
}

static void advisory_reveal(void) {
    if (!ui_pnl_advisory) {
        ui_pnl_advisory = lv_obj_create(ui_screen);
        if (!ui_pnl_advisory) return;

        lv_obj_set_size(ui_pnl_advisory, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(ui_pnl_advisory, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_bg_opa(ui_pnl_advisory, 140, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_border_width(ui_pnl_advisory, 0, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_all(ui_pnl_advisory, 4, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_radius(ui_pnl_advisory, 4, MU_OBJ_MAIN_DEFAULT);
        lv_obj_clear_flag(ui_pnl_advisory, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

        char text[ADVISORY_TEXT_MAX];
        decode_pairs(advisory_text, text, sizeof(text));

        ui_lbl_advisory = lv_label_create(ui_pnl_advisory);
        lv_obj_set_style_text_font(ui_lbl_advisory, LV_FONT_DEFAULT, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_text_color(ui_lbl_advisory, lv_color_hex(0xFFFFFF), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_text_opa(ui_lbl_advisory, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
        lv_label_set_text(ui_lbl_advisory, text);
    }

    lv_obj_clear_flag(ui_pnl_advisory, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(ui_pnl_advisory, LV_ALIGN_CENTER, 0, 0);
    lv_obj_move_foreground(ui_pnl_advisory);
}

void advisory_tick(const uint32_t now) {
    if (!advisory_armed) return;

    uint32_t step = advisory_last ? now - advisory_last : 0;
    if (step > ADVISORY_STEP_MAX) step = ADVISORY_STEP_MAX;
    advisory_last = now;

    if (advisory_since) {
        if (pause_menu_is_active() || now - advisory_since >= ADVISORY_HOLD_MS) {
            advisory_conceal();
            advisory_since = 0;
            advisory_armed = 0;
        }
        return;
    }

    if (advisory_spent || pause_menu_is_active()) return;
    if (!hotkeys_is_fast_forward_active() || netplay_is_active()) return;

    advisory_elapsed += step;
    if (advisory_elapsed < ADVISORY_DELAY_MS) return;

    advisory_spent = 1;
    advisory_since = now ? now : 1;
    advisory_reveal();
}

void advisory_shutdown(void) {
    if (ui_pnl_advisory && lv_obj_is_valid(ui_pnl_advisory)) lv_obj_del(ui_pnl_advisory);

    ui_pnl_advisory = NULL;
    ui_lbl_advisory = NULL;
    advisory_armed = 0;
    advisory_spent = 0;
    advisory_elapsed = 0;
    advisory_since = 0;
    advisory_last = 0;
}
