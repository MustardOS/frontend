#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../common/audio.h"
#include "../../common/fileio.h"
#include "../../common/input.h"
#include "../../common/options.h"
#include "../../common/ui/common.h"
#include "../../common/ui/dialogue.h"
#include "../../common/ui/glyph.h"
#include "../../common/ui/image.h"
#include "../../common/ui/osk.h"
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../input/nav_repeat.h"
#include "../settings/submenu.h"
#include "cheevo.h"
#include "ui_cheevo.h"

enum {
    row_account = 0,
    row_mode,
    row_notifications,
    row_refresh,
    row_count
};

typedef enum { entry_none = 0, entry_username, entry_password } entry_state;

enum {
    login_row_username = 0,
    login_row_password,
    login_row_submit,
    login_row_sign_out,
    login_row_count
};

static submenu self;
static submenu details;
static submenu rankings;
static submenu login;
static int self_initialised;
static int login_initialised;
static lv_obj_t *entry_panel;
static lv_obj_t *entry_text;
static entry_state entry_mode;
static char login_username[128];
static char login_password[128];
static int login_submitted;
static mux_dialogue clear_dialogue;
static uint64_t clear_previous_mask;
static uint64_t entry_previous_mask;
static nav_repeat_t entry_up, entry_down, entry_left, entry_right, entry_backspace;

#define DETAIL_CAP 192
#define RANKING_CAP 10
#define DETAIL_PREVIEW_DELAY_MS 80

static cheevo_game_entry detail_entries[DETAIL_CAP];
static const char *detail_labels[DETAIL_CAP];
static const char *detail_glyphs[DETAIL_CAP];
static submenu_def detail_definition;
static submenu_def ranking_definition;
static submenu_def login_definition;
static int details_initialised;
static int rankings_initialised;
static int detail_preview_mode;
static cheevo_achievement_view current_detail_mode;
static cheevo_achievement_sort current_detail_sort;
static int leaderboard_waiting;
static int leaderboard_return_row;
static uint64_t detail_previous_mask;
static nav_repeat_t detail_up, detail_down;
static lv_obj_t *detail_preview_panel;
static lv_obj_t *detail_preview_label;
static lv_obj_t *detail_preview_glyph;
static uint32_t detail_preview_due;
static int detail_preview_pending;
static char detail_loaded_preview[MAX_BUFFER_SIZE];
static cheevo_leaderboard_rank ranking_entries[RANKING_CAP];
static char ranking_label_text[RANKING_CAP][160];
static const char *ranking_labels[RANKING_CAP];
static const char *ranking_glyphs[RANKING_CAP];
static unsigned ranking_total;

static void detail_set_preview(int enabled);
static const char *detail_sort_name(void);

static const char *login_labels[login_row_count];
static const char *login_glyphs[login_row_count] = {"user", "lock", "network", "exit"};

static const char *row_labels[row_count];

static const char *row_glyphs[row_count] = {"user", "performance", "message", "refresh"};

static void detail_nav_show_x(const int show, const char *text) {
    if (show) {
        lv_label_set_text(ui_lbl_nav_x, text);
        lv_obj_clear_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void detail_nav_show_y(const int show, const char *text) {
    if (show) {
        lv_label_set_text(ui_lbl_nav_y, text);
        lv_obj_clear_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void detail_nav_show_modes(void) {
    detail_nav_show_x(1, lang.muxretro.cheevo.mode);
    detail_nav_show_y(current_detail_mode == cheevo_view_achievements, lang.muxretro.cheevo.sort);
    pause_menu_fix_nav_order();
}

static void row_value_text(const int index, char *buffer, const size_t length) {
    cheevo_info info;
    cheevo_get_info(&info);
    switch (index) {
        case row_account:
            if (info.display_name[0] || info.username[0])
                snprintf(
                    buffer, length, lang.muxretro.cheevo.account_score,
                    info.display_name[0] ? info.display_name : info.username, info.score
                );
            else
                snprintf(buffer, length, "%s", lang.muxretro.cheevo.signed_out);
            break;
        case row_refresh:
            buffer[0] = '\0';
            break;
        case row_mode:
            snprintf(
                buffer, length, "%s", info.hardcore ? lang.muxretro.cheevo.hardcore : lang.muxretro.cheevo.softcore
            );
            break;
        case row_notifications:
            snprintf(
                buffer, length, "%s",
                info.notifications == cheevo_notifications_detailed
                    ? lang.muxretro.cheevo.detailed
                    : info.notifications == cheevo_notifications_basic ? lang.muxretro.cheevo.basic
                                                                        : lang.generic.disabled
            );
            break;
        default:
            buffer[0] = '\0';
            break;
    }
}

static int row_is_action(const int index) {
    return index != row_notifications;
}

static void row_cycle(const int index, const int direction) {
    if (index != row_notifications) return;
    cheevo_info info;
    cheevo_get_info(&info);
    int mode = info.notifications + (direction < 0 ? -1 : 1);
    if (mode < cheevo_notifications_disabled)
        mode = cheevo_notifications_detailed;
    else if (mode > cheevo_notifications_detailed)
        mode = cheevo_notifications_disabled;
    cheevo_set_notifications((cheevo_notification_mode) mode);
}

static uint64_t entry_nav_mask(void) {
    return nav_dir_bits() | (mux_input_pressed(mux_input_a) ? BIT(4) : 0)
           | (mux_input_pressed(mux_input_b) ? BIT(5) : 0) | (mux_input_pressed(mux_input_x) ? BIT(6) : 0)
           | (mux_input_pressed(mux_input_y) ? BIT(7) : 0)
           | (mux_input_pressed(mux_input_select) ? BIT(8) : 0)
           | (mux_input_pressed(mux_input_start) ? BIT(9) : 0) | nav_mask_page();
}

static void entry_create(void) {
    if (entry_panel) return;
    entry_panel = lv_obj_create(ui_screen);
    lv_obj_set_size(entry_panel, device.mux.width, device.mux.height);
    lv_obj_center(entry_panel);
    lv_obj_set_flex_flow(entry_panel, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(entry_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(entry_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(entry_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(entry_panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(entry_panel, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(entry_panel, 128, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(entry_panel, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(entry_panel, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(entry_panel, 5, MU_OBJ_MAIN_DEFAULT);

    entry_text = lv_textarea_create(entry_panel);
    lv_obj_set_width(entry_text, device.mux.width * 5 / 6);
    lv_obj_set_height(entry_text, LV_SIZE_CONTENT);
    lv_obj_center(entry_text);
    lv_textarea_set_one_line(entry_text, 1);
    lv_obj_set_style_radius(entry_text, theme.osk.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(entry_text, lv_color_hex(theme.osk.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(entry_text, theme.osk.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(entry_text, 2, MU_OBJ_MAIN_DEFAULT);
}

static void entry_begin(const entry_state mode) {
    entry_create();
    login_submitted = 0;
    entry_mode = mode;
    lv_textarea_set_text(entry_text, mode == entry_username ? login_username : "");
    init_osk(entry_panel, entry_text, 0, mode == entry_password, 127);
    key_show = 1;
    osk_show(entry_panel);
    entry_previous_mask = entry_nav_mask();
}

static void entry_wipe(void) {
    const char *text = lv_textarea_get_text(entry_text);
    if (text) explicit_bzero((char *) text, strlen(text));
    lv_textarea_set_text(entry_text, "");
}

static int entry_row(void) {
    return entry_mode == entry_password ? login_row_password : login_row_username;
}

static void entry_cancel(void) {
    const int row = entry_row();
    entry_wipe();
    entry_mode = entry_none;
    close_osk(key_entry, ui_group, entry_text, entry_panel);
    submenu_reopen_at(&login, row);
}

static void entry_finish(void) {
    const char *text = lv_textarea_get_text(entry_text);
    if (entry_mode == entry_username) {
        snprintf(login_username, sizeof(login_username), "%s", text ? text : "");
    } else {
        explicit_bzero(login_password, sizeof(login_password));
        snprintf(login_password, sizeof(login_password), "%s", text ? text : "");
    }

    const int row = entry_mode == entry_username ? login_row_password : login_row_submit;
    entry_wipe();
    entry_mode = entry_none;
    key_show = 0;
    reset_osk(key_entry);
    lv_group_set_focus_cb(ui_group, NULL);
    osk_hide(entry_panel);
    submenu_reopen_at(&login, row);
}

static void entry_tick(void) {
    const uint64_t mask = entry_nav_mask();
    const uint64_t edge = mask & ~entry_previous_mask;
    entry_previous_mask = mask;
    if (nav_input_halted()) return;

    if (edge & BIT(9)) {
        entry_finish();
        return;
    }
    if (edge & BIT(4)) {
        const char *key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
        if (key && strcasecmp(key, OSK_DONE) == 0)
            entry_finish();
        else
            lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
        return;
    }
    if (edge & BIT(6)) {
        entry_cancel();
        return;
    }
    if (edge & BIT(7)) {
        key_space(entry_text);
        return;
    }
    if (edge & BIT(8)) {
        key_clear(entry_text);
        return;
    }
    if (edge & NAV_PAGE_UP_BIT) {
        key_swap_back();
        return;
    }
    if (edge & NAV_PAGE_DOWN_BIT) {
        key_swap();
        return;
    }

    const uint32_t now = SDL_GetTicks();
    const int up = nav_repeat_step(&entry_up, edge & BIT(0), mask & BIT(0), 1, now);
    const int down = nav_repeat_step(&entry_down, edge & BIT(1), mask & BIT(1), 1, now);
    const int left = nav_repeat_step(&entry_left, edge & BIT(2), mask & BIT(2), 1, now);
    const int right = nav_repeat_step(&entry_right, edge & BIT(3), mask & BIT(3), 1, now);
    const int backspace = nav_repeat_step(&entry_backspace, edge & BIT(5), mask & BIT(5), 1, now);
    if (up)
        key_up();
    else if (down)
        key_down();
    else if (left)
        key_left();
    else if (right)
        key_right();
    else if (backspace)
        key_backspace(entry_text);
}

static int login_account_active(void) {
    const cheevo_status login_status = cheevo_get_status();
    return login_status != cheevo_status_signed_out && login_status != cheevo_status_failed
           && login_status != cheevo_status_disabled;
}

static void login_value_text(const int index, char *buffer, const size_t length) {
    cheevo_info info;
    cheevo_get_info(&info);
    const int account_active = login_account_active();
    const int submission_active = login_submitted && account_active;
    switch (index) {
        case login_row_username:
            snprintf(
                buffer, length, "%s",
                login_username[0]
                    ? login_username
                    : info.username[0] ? info.username : lang.muxretro.cheevo.not_entered
            );
            break;
        case login_row_password:
            snprintf(
                buffer, length, "%s",
                login_password[0] || submission_active
                    ? "********"
                    : account_active ? lang.muxretro.cheevo.session_saved : lang.muxretro.cheevo.not_entered
            );
            break;
        case login_row_submit:
            if (account_active)
                snprintf(
                    buffer, length, "%s",
                    cheevo_is_starting() ? lang.muxretro.cheevo.signing_in : lang.muxretro.cheevo.signed_in
                );
            else
                snprintf(
                    buffer, length, "%s",
                    login_username[0] && login_password[0] ? lang.muxretro.cheevo.ready
                                                           : lang.muxretro.cheevo.details_required
                );
            break;
        case login_row_sign_out:
            buffer[0] = '\0';
            break;
        default:
            buffer[0] = '\0';
            break;
    }
}

static int login_is_action(const int index) {
    return index >= 0 && index < login_row_count;
}

static void login_clear_open(void) {
    clear_previous_mask = entry_nav_mask();
    dialogue_open(&clear_dialogue, &theme);
}

static const char *login_extra_label(const int index) {
    (void) index;
    return lang.generic.reset;
}

static void login_extra_action(const int index) {
    (void) index;
    login_clear_open();
}

static int login_child_tick(void) {
    if (!dialogue_active(&clear_dialogue)) return 0;

    const uint64_t mask = entry_nav_mask();
    const uint64_t edge = mask & ~clear_previous_mask;
    clear_previous_mask = mask;
    if (nav_input_halted()) return 1;

    if (edge & (BIT(0) | BIT(1))) {
        dialogue_handle_dpad(&clear_dialogue, &theme, (edge & BIT(1)) ? 1 : -1, 1);
    } else if (edge & BIT(4)) {
        const mux_confirm_opt option = (mux_confirm_opt) clear_dialogue.selected;
        dialogue_dismiss(&clear_dialogue);
        if (option == mux_confirm_yep) {
            cheevo_logout();
            login_submitted = 0;
            explicit_bzero(login_password, sizeof(login_password));
            explicit_bzero(login_username, sizeof(login_username));
            submenu_reopen_at(&login, login_row_username);
            pause_menu_show_toast_timed(lang.muxretro.cheevo.account_reset, tst_wait_s);
        }
    } else if (edge & BIT(5)) {
        dialogue_mark_cancelled(&clear_dialogue);
        dialogue_dismiss(&clear_dialogue);
        pause_menu_sync_input_mask();
    }
    return 1;
}

static void login_action(const int index) {
    cheevo_info info;
    cheevo_get_info(&info);
    switch (index) {
        case login_row_username:
            if (login_account_active()) {
                pause_menu_show_toast_timed(lang.muxretro.cheevo.change_requires_sign_out, tst_wait_s);
                break;
            }
            entry_begin(entry_username);
            break;
        case login_row_password:
            if (login_account_active()) {
                pause_menu_show_toast_timed(lang.muxretro.cheevo.change_requires_sign_out, tst_wait_s);
                break;
            }
            entry_begin(entry_password);
            break;
        case login_row_submit: {
            if (login_account_active()) {
                pause_menu_show_toast_timed(lang.muxretro.cheevo.already_signed_in, tst_wait_s);
                break;
            }
            if (!login_username[0] || !login_password[0]) {
                pause_menu_show_toast_timed(lang.muxretro.cheevo.enter_details, tst_wait_s);
                break;
            }
            const int result = cheevo_login(login_username, login_password);
            login_submitted = result == 0;
            explicit_bzero(login_password, sizeof(login_password));
            if (result == 0)
                pause_menu_show_toast_timed(lang.muxretro.cheevo.signing_in, tst_wait_s);
            else
                pause_menu_show_toast_timed(lang.muxretro.cheevo.sign_in_start_failed, tst_wait_s);
            submenu_refresh_values(&login);
            break;
        }
        case login_row_sign_out:
            if (login_account_active() || info.username[0])
                login_clear_open();
            else
                pause_menu_show_toast_timed(lang.muxretro.cheevo.already_signed_out, tst_wait_s);
            break;
        default:
            break;
    }
}

static void login_closed(void) {
    login_submitted = 0;
    explicit_bzero(login_password, sizeof(login_password));
    submenu_reopen_at(&self, row_account);
}

static int detail_rarity_band(const float rarity) {
    if (rarity <= 0.0f) return 0;
    if (rarity >= 90.0f) return 9;
    return (int) (rarity / 10.0f);
}

static int detail_compare(const void *left_value, const void *right_value) {
    const cheevo_game_entry *left = left_value;
    const cheevo_game_entry *right = right_value;
    if (current_detail_sort == cheevo_sort_easy_points) {
        const int left_band = detail_rarity_band(left->rarity);
        const int right_band = detail_rarity_band(right->rarity);
        if (left_band != right_band) return left_band < right_band ? 1 : -1;
        if (left->points != right->points) return left->points < right->points ? 1 : -1;
        if (left->rarity != right->rarity) return left->rarity < right->rarity ? 1 : -1;
        return strcasecmp(left->title, right->title);
    }
    if (current_detail_sort == cheevo_sort_unlocked && left->unlocked != right->unlocked)
        return left->unlocked ? -1 : 1;
    if (left->points != right->points) {
        if (current_detail_sort == cheevo_sort_points_highest) return left->points < right->points ? 1 : -1;
        if (current_detail_sort == cheevo_sort_points_lowest) return left->points > right->points ? 1 : -1;
    }
    if (left->rarity != right->rarity) {
        if (current_detail_sort == cheevo_sort_percentage_common) return left->rarity < right->rarity ? 1 : -1;
        if (current_detail_sort == cheevo_sort_percentage_rarest) return left->rarity > right->rarity ? 1 : -1;
    }
    if (current_detail_sort == cheevo_sort_alphanumeric_descending)
        return strcasecmp(right->title, left->title);
    return strcasecmp(left->title, right->title);
}

static int detail_load(const cheevo_achievement_view mode) {
    const cheevo_game_entry_type type = mode == cheevo_view_achievements ? cheevo_game_entry_achievement
                                                                         : cheevo_game_entry_leaderboard;
    detail_definition.row_count = (int) cheevo_game_entries(type, detail_entries, DETAIL_CAP);
    if (mode == cheevo_view_achievements && detail_definition.row_count > 1)
        qsort(detail_entries, (size_t) detail_definition.row_count, sizeof(*detail_entries), detail_compare);
    for (int entry = 0; entry < detail_definition.row_count; entry++) {
        detail_labels[entry] = detail_entries[entry].title;
        detail_glyphs[entry] = mode == cheevo_view_achievements ? "trophy" : "leaderboard";
    }
    return detail_definition.row_count;
}

static void details_open(void) {
    current_detail_mode = cheevo_get_achievement_view();
    current_detail_sort = cheevo_get_achievement_sort();
    if (!detail_load(current_detail_mode)) {
        current_detail_mode = current_detail_mode == cheevo_view_achievements ? cheevo_view_leaderboards
                                                                              : cheevo_view_achievements;
        detail_load(current_detail_mode);
    }
    if (!detail_definition.row_count) {
        pause_menu_show_toast_timed(lang.muxretro.cheevo.no_data, tst_wait_s);
        return;
    }
    if (!details_initialised) {
        submenu_init(&details, &detail_definition);
        details_initialised = 1;
    }
    detail_preview_mode = 0;
    submenu_open(&details);
    detail_previous_mask = entry_nav_mask();
    lv_obj_clear_flag(ui_pnl_box, LV_OBJ_FLAG_HIDDEN);
    detail_set_preview(0);
    detail_nav_show_modes();
}

static void row_action(const int index) {
    cheevo_info info;
    cheevo_get_info(&info);
    switch (index) {
        case row_account:
            if (!login_username[0] && info.username[0])
                snprintf(login_username, sizeof(login_username), "%s", info.username);
            if (!login_initialised) {
                submenu_init(&login, &login_definition);
                login_initialised = 1;
            }
            submenu_open(&login);
            break;
        case row_refresh:
            if (cheevo_refresh_data() == 0)
                pause_menu_show_toast_timed(lang.muxretro.cheevo.refresh_started, tst_wait_s);
            else
                pause_menu_show_toast_timed(lang.muxretro.cheevo.refresh_unavailable, tst_wait_s);
            break;
        case row_mode:
            if (cheevo_set_hardcore(!info.hardcore) != 0)
                pause_menu_show_toast_timed(lang.muxretro.cheevo.hardcore_unavailable, tst_wait_s);
            else
                pause_menu_show_toast_timed(
                    info.hardcore ? lang.muxretro.cheevo.softcore_enabled : lang.muxretro.cheevo.hardcore_enabled,
                    tst_wait_s
                );
            break;
        default:
            break;
    }
}

static void detail_value_text(const int index, char *buffer, const size_t length) {
    if (index < 0 || index >= detail_definition.row_count) {
        buffer[0] = '\0';
        return;
    }

    const cheevo_game_entry *entry = &detail_entries[index];
    if (entry->type == cheevo_game_entry_leaderboard) {
        const char *state = entry->active ? lang.muxretro.cheevo.entry_active : lang.muxretro.cheevo.leaderboard;
        if (entry->progress[0])
            snprintf(buffer, length, lang.muxretro.cheevo.leaderboard_progress, state, entry->progress);
        else
            snprintf(buffer, length, "%s", state);
    } else if (entry->unlocked) {
        snprintf(buffer, length, lang.muxretro.cheevo.unlocked_points, entry->points);
    } else if (entry->progress[0]) {
        snprintf(buffer, length, lang.muxretro.cheevo.progress_points, entry->progress, entry->points);
    } else {
        snprintf(buffer, length, lang.muxretro.cheevo.points_rarity, entry->points, entry->rarity);
    }
}

static int detail_is_action(const int index) {
    return index >= 0 && index < detail_definition.row_count;
}

static void ranking_value_text(const int index, char *buffer, const size_t length) {
    if (index < 0 || index >= ranking_definition.row_count) {
        buffer[0] = '\0';
        return;
    }
    snprintf(buffer, length, "%s", ranking_entries[index].score);
}

static int ranking_is_action(const int index) {
    return index >= 0 && index < ranking_definition.row_count;
}

static void ranking_closed(void) {
    submenu_reopen_at(&details, leaderboard_return_row);
    detail_previous_mask = entry_nav_mask();
    detail_nav_show_modes();
}

static void ranking_open(void) {
    ranking_definition.row_count = (int) cheevo_leaderboard_ranks(
        ranking_entries, RANKING_CAP, &ranking_total
    );
    if (!ranking_definition.row_count) {
        pause_menu_show_toast_timed(lang.muxretro.cheevo.leaderboard_empty, tst_wait_s);
        return;
    }
    if (ranking_total < (unsigned) ranking_definition.row_count)
        ranking_total = (unsigned) ranking_definition.row_count;
    for (int index = 0; index < ranking_definition.row_count; index++) {
        snprintf(
            ranking_label_text[index], sizeof(ranking_label_text[index]), lang.muxretro.cheevo.leaderboard_rank,
            (unsigned) index + 1, ranking_entries[index].user
        );
        ranking_labels[index] = ranking_label_text[index];
        ranking_glyphs[index] = ranking_entries[index].current_user ? "user" : "leaderboard";
    }
    if (!rankings_initialised) {
        submenu_init(&rankings, &ranking_definition);
        rankings_initialised = 1;
    }
    detail_nav_show_x(0, NULL);
    detail_nav_show_y(0, NULL);
    submenu_open(&rankings);
    nav_show_a(0, NULL);
    nav_show_lr(0);
    setup_nav((struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});
    pause_menu_fix_nav_order();
    char message[128];
    snprintf(
        message, sizeof(message), lang.muxretro.cheevo.leaderboard_total,
        (unsigned) ranking_definition.row_count, ranking_total
    );
    pause_menu_show_toast_timed(message, tst_wait_s);
}

static void detail_action(const int index) {
    if (index < 0 || index >= detail_definition.row_count) return;
    const cheevo_game_entry *entry = &detail_entries[index];
    if (entry->type == cheevo_game_entry_leaderboard) {
        leaderboard_return_row = index;
        if (cheevo_leaderboard_fetch(entry->id) != 0) {
            leaderboard_waiting = 0;
            pause_menu_show_toast_timed(lang.muxretro.cheevo.leaderboard_unavailable, tst_wait_s);
        } else if (cheevo_leaderboard_get_state() == cheevo_leaderboard_ready) {
            leaderboard_waiting = 0;
            ranking_open();
        } else {
            leaderboard_waiting = 1;
            pause_menu_show_toast_timed(lang.muxretro.cheevo.leaderboard_loading, tst_wait_s);
        }
        return;
    }
    pause_menu_show_toast_timed(
        entry->description[0] ? entry->description
                              : lang.muxretro.cheevo.achievement_default,
        tst_wait_s
    );
}

static const char *detail_sort_name(void) {
    switch (current_detail_sort) {
        case cheevo_sort_alphanumeric_descending:
            return lang.muxretro.cheevo.sort_alphanumeric_descending;
        case cheevo_sort_points_highest:
            return lang.muxretro.cheevo.sort_points_highest;
        case cheevo_sort_points_lowest:
            return lang.muxretro.cheevo.sort_points_lowest;
        case cheevo_sort_percentage_common:
            return lang.muxretro.cheevo.sort_percentage_common;
        case cheevo_sort_percentage_rarest:
            return lang.muxretro.cheevo.sort_percentage_rarest;
        case cheevo_sort_unlocked:
            return lang.muxretro.cheevo.sort_unlocked;
        case cheevo_sort_easy_points:
            return lang.muxretro.cheevo.sort_easy_points;
        case cheevo_sort_alphanumeric_ascending:
        default:
            return lang.muxretro.cheevo.sort_alphanumeric_ascending;
    }
}

static void detail_switch_mode(void) {
    const cheevo_achievement_view previous_mode = current_detail_mode;
    current_detail_mode = current_detail_mode == cheevo_view_achievements ? cheevo_view_leaderboards
                                                                          : cheevo_view_achievements;
    if (!detail_load(current_detail_mode)) {
        const char *message = current_detail_mode == cheevo_view_achievements
                                  ? lang.muxretro.cheevo.no_achievements
                                  : lang.muxretro.cheevo.no_leaderboards;
        current_detail_mode = previous_mode;
        detail_load(current_detail_mode);
        pause_menu_show_toast_timed(message, tst_wait_s);
        return;
    }
    leaderboard_waiting = 0;
    submenu_reopen_at(&details, 0);
    detail_set_preview(0);
    detail_previous_mask = entry_nav_mask();
    detail_nav_show_modes();
}

static void detail_cycle_sort(void) {
    if (current_detail_mode != cheevo_view_achievements) return;
    current_detail_sort = (cheevo_achievement_sort) ((current_detail_sort + 1) % cheevo_sort_count);
    detail_load(current_detail_mode);
    submenu_reopen_at(&details, 0);
    detail_set_preview(0);
    detail_previous_mask = entry_nav_mask();
    detail_nav_show_modes();
    char message[128];
    snprintf(message, sizeof(message), lang.muxretro.cheevo.sort_selected, detail_sort_name());
    pause_menu_show_toast_timed(message, tst_wait_s);
}

static void detail_refresh_preview(void) {
    if (current_item_index < 0 || current_item_index >= detail_definition.row_count) return;
    const cheevo_game_entry *entry = &detail_entries[current_item_index];
    lv_label_set_text(detail_preview_label, entry->title);

    char glyph_image_embed[MAX_BUFFER_SIZE];
    const char *glyph = entry->type == cheevo_game_entry_achievement ? "trophy" : "leaderboard";
    if (config.visual.list_glyph && (theme.list_default.glyph_alpha || theme.list_focus.glyph_alpha)
        && get_glyph_path("muxretro", glyph, glyph_image_embed, sizeof(glyph_image_embed)))
        set_list_glyph_image(detail_preview_glyph, glyph_image_embed);
    else
        lv_img_set_src(detail_preview_glyph, NULL);

    if (!entry->preview_path[0] || !file_exist(entry->preview_path)) {
        detail_preview_pending = 0;
        detail_loaded_preview[0] = '\0';
        clear_image(ui_img_box);
        return;
    }

    if (strcmp(detail_loaded_preview, entry->preview_path) == 0) {
        detail_preview_pending = 0;
        return;
    }

    if (detail_loaded_preview[0]) {
        detail_loaded_preview[0] = '\0';
        clear_image(ui_img_box);
    }
    detail_preview_due = SDL_GetTicks() + DETAIL_PREVIEW_DELAY_MS;
    detail_preview_pending = 1;
}

static void detail_preview_tick(void) {
    if (!detail_preview_pending || !detail_preview_mode || (int32_t) (SDL_GetTicks() - detail_preview_due) < 0) return;
    detail_preview_pending = 0;
    if (current_item_index < 0 || current_item_index >= detail_definition.row_count) return;
    const cheevo_game_entry *entry = &detail_entries[current_item_index];
    if (!entry->preview_path[0] || !file_exist(entry->preview_path)) {
        detail_loaded_preview[0] = '\0';
        clear_image(ui_img_box);
        return;
    }

    const struct image_settings settings = {
        .image_path = (char *) entry->preview_path,
        .align = LV_ALIGN_BOTTOM_MID,
        .max_width = (int16_t) (device.mux.width - 16),
        .max_height =
            (int16_t) (device.mux.height - theme.header.height - theme.footer.height - 4 - theme.mux.item.height - 12),
    };
    update_image(ui_img_box, settings);
    snprintf(detail_loaded_preview, sizeof(detail_loaded_preview), "%s", entry->preview_path);
}

static void detail_set_preview(const int enabled) {
    detail_preview_mode = enabled;
    if (enabled) {
        detail_loaded_preview[0] = '\0';
        detail_nav_show_x(0, NULL);
        detail_nav_show_y(0, NULL);
        lv_obj_add_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(detail_preview_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_img_box, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(ui_lbl_nav_lr, lang.muxretro.gamestate.list);
        setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                      {ui_lbl_nav_lr, lang.muxretro.gamestate.list, 0},
                                      {ui_lbl_nav_a_glyph, "", 0},
                                      {ui_lbl_nav_a, lang.generic.details, 0},
                                      {ui_lbl_nav_b_glyph, "", 0},
                                      {ui_lbl_nav_b, lang.generic.back, 0},
                                      {NULL, NULL, 0}});
        detail_refresh_preview();
    } else {
        detail_preview_pending = 0;
        detail_loaded_preview[0] = '\0';
        lv_obj_clear_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
        submenu_focus_at(&details, current_item_index);
        lv_obj_add_flag(detail_preview_panel, LV_OBJ_FLAG_HIDDEN);
        clear_image(ui_img_box);
        if (current_detail_mode == cheevo_view_achievements) {
            nav_show_lr(1);
            lv_label_set_text(ui_lbl_nav_lr, lang.generic.preview);
            setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                          {ui_lbl_nav_lr, lang.generic.preview, 0},
                                          {ui_lbl_nav_a_glyph, "", 0},
                                          {ui_lbl_nav_a, lang.generic.details, 0},
                                          {ui_lbl_nav_b_glyph, "", 0},
                                          {ui_lbl_nav_b, lang.generic.back, 0},
                                          {NULL, NULL, 0}});
        } else {
            nav_show_lr(0);
            setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                          {ui_lbl_nav_a, lang.generic.details, 0},
                                          {ui_lbl_nav_b_glyph, "", 0},
                                          {ui_lbl_nav_b, lang.generic.back, 0},
                                          {NULL, NULL, 0}});
        }
        detail_nav_show_modes();
    }
    pause_menu_fix_nav_order();
}

static int detail_child_tick(void) {
    detail_preview_tick();
    const uint64_t mask = entry_nav_mask();
    const uint64_t edge = mask & ~detail_previous_mask;
    const uint64_t vertical = mask & (BIT(0) | BIT(1));
    detail_previous_mask = mask;
    if (nav_input_halted()) return detail_preview_mode;

    if (!detail_preview_mode && (edge & BIT(6))) {
        play_sound(snd_option);
        detail_switch_mode();
        return 1;
    }
    if (!detail_preview_mode && (edge & BIT(7)) && current_detail_mode == cheevo_view_achievements) {
        play_sound(snd_option);
        detail_cycle_sort();
        return 1;
    }

    if (!detail_preview_mode) {
        if (current_detail_mode == cheevo_view_achievements && !vertical && (edge & (BIT(2) | BIT(3)))) {
            play_sound(snd_option);
            details.prev_nav_mask = mask;
            detail_set_preview(1);
            return 1;
        }
        return 0;
    }

    if (edge & BIT(5)) {
        detail_set_preview(0);
        return 0;
    }
    if (!vertical && (edge & (BIT(2) | BIT(3)))) {
        play_sound(snd_option);
        details.prev_nav_mask = mask;
        detail_set_preview(0);
        return 1;
    }
    if (edge & BIT(4)) {
        play_sound(snd_confirm);
        detail_action(current_item_index);
        return 1;
    }

    const uint32_t now = SDL_GetTicks();
    const int up = nav_repeat_step(&detail_up, edge & BIT(0), mask & BIT(0), current_item_index > 0, now);
    const int down = nav_repeat_step(
        &detail_down, edge & BIT(1), mask & BIT(1), current_item_index < detail_definition.row_count - 1, now
    );
    if (up) {
        nav_set_last_dir(nav_dir_up);
        nav_unsuppress_shake();
        gen_step_movement(1, -1, 1, 0, 1);
        detail_refresh_preview();
    } else if (down) {
        nav_set_last_dir(nav_dir_down);
        nav_unsuppress_shake();
        gen_step_movement(1, +1, 1, 0, 1);
        detail_refresh_preview();
    } else if (nav_page_tick(edge, mask, 1)) {
        detail_refresh_preview();
    }
    return 1;
}

static void detail_closed(void) {
    detail_preview_mode = 0;
    detail_preview_pending = 0;
    detail_loaded_preview[0] = '\0';
    leaderboard_waiting = 0;
    cheevo_set_achievement_preferences(current_detail_sort, current_detail_mode);
    clear_image(ui_img_box);
    lv_obj_add_flag(ui_pnl_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_content, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(detail_preview_panel, LV_OBJ_FLAG_HIDDEN);
    detail_nav_show_x(0, NULL);
    detail_nav_show_y(0, NULL);
    pause_menu_rebuild();
    pause_menu_focus_cheevo_item();
    pause_menu_show_nav_hints();
    pause_menu_sync_input_mask();
}

static void settings_closed(void) {
    settings_menu_reopen_cheevo();
}

static submenu_def definition = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .row_count = row_count,
    .value_text = row_value_text,
    .cycle = row_cycle,
    .row_is_action = row_is_action,
    .action = row_action,
    .closed = settings_closed,
    .save_title = NULL,
    .save_desc = NULL,
};

void cheevo_menu_init(void) {
    if (!device.board.has_network) return;

    login_labels[login_row_username] = lang.muxretro.cheevo.username;
    login_labels[login_row_password] = lang.muxretro.cheevo.password;
    login_labels[login_row_submit] = lang.muxretro.cheevo.sign_in;
    login_labels[login_row_sign_out] = lang.muxretro.cheevo.sign_out;

    row_labels[row_account] = lang.muxretro.cheevo.account;
    row_labels[row_refresh] = lang.muxretro.cheevo.refresh_data;
    row_labels[row_mode] = lang.muxretro.cheevo.mode;
    row_labels[row_notifications] = lang.muxretro.cheevo.notifications;

    definition.save_title = lang.muxretro.retroachievements;
    definition.save_desc = lang.muxretro.cheevo.menu_desc;

    dialogue_init_confirm(
        &clear_dialogue, &theme, ui_screen, lang.muxretro.cheevo.reset_title, lang.muxretro.cheevo.reset_desc,
        lang.generic.reset, lang.generic.cancel, lang.generic.select, lang.generic.cancel
    );

    detail_preview_panel = lv_obj_create(ui_screen);
    detail_preview_label = lv_label_create(detail_preview_panel);
    detail_preview_glyph = lv_img_create(detail_preview_panel);
    apply_theme_list_panel(detail_preview_panel);
    apply_theme_list_item(&theme, detail_preview_label, "");
    apply_theme_list_glyph(&theme, detail_preview_glyph, "muxretro", "trophy");
    apply_text_long_dot(&theme, detail_preview_label);
    lv_obj_align(
        detail_preview_panel, LV_ALIGN_TOP_MID, 0, theme.header.height + 2 + theme.misc.content.padding_top
    );
    lv_obj_move_foreground(detail_preview_panel);
    lv_obj_add_state(detail_preview_panel, LV_STATE_FOCUSED);
    lv_obj_add_state(detail_preview_label, LV_STATE_FOCUSED);
    lv_obj_add_state(detail_preview_glyph, LV_STATE_FOCUSED);
    lv_obj_clear_flag(detail_preview_panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(detail_preview_panel, LV_OBJ_FLAG_HIDDEN);

    login_definition = (submenu_def) {
        .labels = login_labels,
        .glyphs = login_glyphs,
        .row_count = login_row_count,
        .value_text = login_value_text,
        .row_is_action = login_is_action,
        .action = login_action,
        .extra_label = login_extra_label,
        .extra_action = login_extra_action,
        .child_tick = login_child_tick,
        .closed = login_closed,
        .save_title = lang.muxretro.cheevo.account_title,
        .save_desc = lang.muxretro.cheevo.account_desc,
    };
    detail_definition = (submenu_def) {
        .labels = detail_labels,
        .glyphs = detail_glyphs,
        .row_count = 0,
        .value_text = detail_value_text,
        .row_is_action = detail_is_action,
        .action = detail_action,
        .child_tick = detail_child_tick,
        .closed = detail_closed,
        .save_title = lang.muxretro.cheevo.achievements,
        .save_desc = lang.muxretro.cheevo.achievements_desc,
    };
    ranking_definition = (submenu_def) {
        .labels = ranking_labels,
        .glyphs = ranking_glyphs,
        .row_count = 0,
        .value_text = ranking_value_text,
        .row_is_action = ranking_is_action,
        .closed = ranking_closed,
        .save_title = lang.muxretro.cheevo.leaderboards,
        .save_desc = lang.muxretro.cheevo.achievements_desc,
    };
}

void cheevo_menu_open(void) {
    if (!device.board.has_network) return;
    details_open();
}

int cheevo_menu_is_active(void) {
    return (details_initialised && submenu_is_active(&details))
           || (rankings_initialised && submenu_is_active(&rankings));
}

void cheevo_menu_tick(void) {
    if (rankings_initialised && submenu_is_active(&rankings)) {
        submenu_tick(&rankings);
        return;
    }
    if (leaderboard_waiting) {
        const cheevo_leaderboard_state state = cheevo_leaderboard_get_state();
        if (state == cheevo_leaderboard_ready) {
            leaderboard_waiting = 0;
            ranking_open();
            return;
        }
        if (state == cheevo_leaderboard_failed || state == cheevo_leaderboard_idle) {
            leaderboard_waiting = 0;
            pause_menu_show_toast_timed(lang.muxretro.cheevo.leaderboard_unavailable, tst_wait_s);
        }
    }
    submenu_tick(&details);
}

void cheevo_settings_menu_open(void) {
    if (!device.board.has_network) return;
    if (!self_initialised) {
        submenu_init(&self, &definition);
        self_initialised = 1;
    }
    submenu_open(&self);
}

int cheevo_settings_menu_is_active(void) {
    return self_initialised && submenu_is_active(&self);
}

void cheevo_settings_menu_tick(void) {
    if (entry_mode != entry_none) {
        entry_tick();
        return;
    }
    if (submenu_is_active(&login)) {
        submenu_refresh_values(&login);
        submenu_tick(&login);
        return;
    }
    submenu_refresh_values(&self);
    submenu_tick(&self);
}
