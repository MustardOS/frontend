#include <stdlib.h>
#include <string.h>
#include <common/display/theme.h>
#include <common/ui/common.h>
#include <common/platform/display.h>
#include <common/platform/device.h>
#include <common/ui/font.h>
#include <common/ui/dialogue.h>
#include <common/ui/image.h>
#include <common/platform/input.h>
#include <common/platform/video.h>
#include <common/content/catalogue.h>
#include <common/content/core/common.h>
#include <common/base/strutil.h>
#include <common/config/config.h>
#include <common/runtime/init.h>
#include <lvgl/lvgl.h>
#include <SDL2/SDL.h>
#include "ui_loading.h"

static lv_obj_t *loading_root = NULL;
static lv_obj_t *loading_dim = NULL;
static lv_obj_t *loading_panel = NULL;
static lv_obj_t *loading_label = NULL;
static lv_obj_t *loading_splash = NULL;
static int owns_root = 0;

static void style_plain(lv_obj_t *obj) {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(obj, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(obj, 0, MU_OBJ_MAIN_DEFAULT);
}

static lv_obj_t *loading_parent(void) {
    if (ui_screen && lv_obj_is_valid(ui_screen)) {
        owns_root = 0;
        return ui_screen;
    }

    if (owns_root && loading_root && lv_obj_is_valid(loading_root)) return loading_root;

    loading_root = lv_obj_create(NULL);
    style_plain(loading_root);
    lv_obj_set_style_bg_color(loading_root, lv_color_hex(theme.system.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(loading_root, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_disp_load_scr(loading_root);
    load_font_text(loading_root);
    owns_root = 1;

    return loading_root;
}

static void loading_present(void) {
    lv_task_handler();
    lv_refr_now(NULL);
    display_composite_frame();
}

static int loading_splash_size(const int width, const int height, int *target_w, int *target_h) {
    if (width <= 0 || height <= 0) return 0;

    const float width_ratio = (float) device.mux.width / (float) width;
    const float height_ratio = (float) device.mux.height / (float) height;
    const float ratio = width_ratio < height_ratio ? width_ratio : height_ratio;
    *target_w = (int) ((float) width * ratio);
    *target_h = (int) ((float) height * ratio);
    return *target_w > 0 && *target_h > 0;
}

static void loading_splash_hide(void) {
    if (!loading_splash) return;

    video_preview_cancel();
    if (lv_obj_is_valid(loading_splash)) {
        clear_image(loading_splash);
        lv_obj_del(loading_splash);
    }
    loading_splash = NULL;
}

int loading_splash_show(const char *content_path) {
    if (!config.visual.launchsplash || !content_path || !content_path[0]) return 0;

    char catalogue_name[MAX_BUFFER_SIZE];
    get_catalogue_name_for_content(content_path, catalogue_name, sizeof(catalogue_name));

    char *program = strip_ext(get_file_name(content_path));
    if (!program) return 0;

    char splash_path[MAX_BUFFER_SIZE];
    const int found = load_splash_catalogue(
        catalogue_name, program, get_file_name(content_path), mux_dim, splash_path, sizeof(splash_path)
    );
    free(program);
    if (!found) return 0;

    const char *extension = strrchr(splash_path, '.');
    const int animated = catalogue_media_is_animated(splash_path);
    if (config.visual.reduce_motion && animated && extension && strcasecmp(extension + 1, "mp4") == 0) return 0;

    lv_obj_t *parent = loading_parent();
    loading_splash_hide();
    loading_splash = lv_img_create(parent);
    lv_obj_set_size(loading_splash, device.mux.width, device.mux.height);
    lv_obj_align(loading_splash, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(loading_splash, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(loading_splash, LV_OPA_COVER, MU_OBJ_MAIN_DEFAULT);
    lv_img_set_src(loading_splash, &ui_img_blank);

    if (animated && !config.visual.reduce_motion) {
        video_preview_arm_image(splash_path, loading_splash, loading_splash_size);
    } else {
        const struct image_settings settings = {
            splash_path, LV_ALIGN_CENTER, device.mux.width, device.mux.height, 0, 0, 0, 0
        };
        update_image(loading_splash, settings);
    }

    for (int i = 0; i < 4; i++) {
        loading_present();
        SDL_Delay(4);
    }

    return 1;
}

void loading_startup_progress(const char *message, const int show_message) {
    if (show_message) {
        loading_message_show(message);
    } else if (loading_splash && lv_obj_is_valid(loading_splash)) {
        loading_present();
    }
}

void loading_message_show(const char *message) {
    if (!message || !*message) return;

    if (loading_label && lv_obj_is_valid(loading_label)) {
        lv_label_set_text(loading_label, message);
        lv_obj_move_foreground(loading_dim);
        lv_obj_move_foreground(loading_panel);
        lv_obj_update_layout(loading_panel);
        lv_obj_invalidate(lv_scr_act());
        lv_refr_now(NULL);
        display_composite_frame();
        return;
    }

    lv_obj_t *parent = loading_parent();

    loading_dim = lv_obj_create(parent);
    style_plain(loading_dim);
    lv_obj_set_size(loading_dim, LV_HOR_RES, LV_VER_RES);
    lv_obj_align(loading_dim, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(loading_dim, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(loading_dim, theme.dialogue.dim_alpha, MU_OBJ_MAIN_DEFAULT);

    loading_panel = lv_obj_create(parent);
    lv_obj_clear_flag(loading_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(loading_panel, lv_pct(70));
    lv_obj_set_height(loading_panel, LV_SIZE_CONTENT);
    lv_obj_align(loading_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(loading_panel, lv_color_hex(theme.dialogue.background), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(loading_panel, theme.dialogue.background_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(loading_panel, 1, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(loading_panel, lv_color_hex(theme.dialogue.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(loading_panel, theme.dialogue.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_radius(loading_panel, theme.dialogue.radius.main, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(loading_panel, 18, MU_OBJ_MAIN_DEFAULT);

    loading_label = lv_label_create(loading_panel);
    lv_label_set_text(loading_label, message);
    lv_label_set_long_mode(loading_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(loading_label, LV_PCT(100));
    lv_obj_set_style_text_align(loading_label, LV_TEXT_ALIGN_CENTER, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_text_color(loading_label, lv_color_hex(theme.dialogue.content), MU_OBJ_MAIN_DEFAULT);

    lv_obj_move_foreground(loading_dim);
    lv_obj_move_foreground(loading_panel);
    lv_obj_update_layout(parent);
    lv_obj_invalidate(parent);
    lv_refr_now(NULL);
    display_composite_frame();
}

void loading_message_hide(void) {
    loading_splash_hide();
    if (owns_root && loading_root && lv_obj_is_valid(loading_root) && lv_scr_act() == loading_root) return;

    if (loading_panel && lv_obj_is_valid(loading_panel)) lv_obj_del(loading_panel);
    if (loading_dim && lv_obj_is_valid(loading_dim)) lv_obj_del(loading_dim);

    loading_panel = NULL;
    loading_dim = NULL;
    loading_label = NULL;

    if (owns_root && loading_root && lv_obj_is_valid(loading_root)) {
        lv_obj_del(loading_root);
        loading_root = NULL;
        owns_root = 0;
        display_clear_ui_layer();
        if (ui_screen && lv_obj_is_valid(ui_screen)) {
            lv_obj_invalidate(ui_screen);
            lv_refr_now(NULL);
        }
    } else {
        loading_root = NULL;
        owns_root = 0;
        if (ui_screen && lv_obj_is_valid(ui_screen)) {
            lv_obj_invalidate(ui_screen);
            lv_refr_now(NULL);
        }
    }
}

void loading_prompt_close(const char *title, const char *description, const char *message, const char *nav_b) {
    if (loading_panel && lv_obj_is_valid(loading_panel)) lv_obj_del(loading_panel);
    if (loading_dim && lv_obj_is_valid(loading_dim)) lv_obj_del(loading_dim);
    loading_panel = NULL;
    loading_dim = NULL;
    loading_label = NULL;

    lv_obj_t *parent = loading_parent();

    mux_dialogue prompt = {0};
    dialogue_init_message(&prompt, &theme, parent, title, description, message, nav_b);
    dialogue_show(&prompt);

    mux_input_poll();
    int b_was = mux_input_pressed(mux_input_b);

    for (;;) {
        loading_present();
        SDL_Delay(16);

        mux_input_poll();
        const int b_now = mux_input_pressed(mux_input_b);
        if (b_now && !b_was) break;
        b_was = b_now;
    }

    dialogue_hide(&prompt);
    if (prompt.panel && lv_obj_is_valid(prompt.panel)) lv_obj_del(prompt.panel);
    if (prompt.dim && lv_obj_is_valid(prompt.dim)) lv_obj_del(prompt.dim);
    loading_present();
}
