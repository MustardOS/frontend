#include "muxshare.h"
#include <common/ui/orientation.h>
#include <common/ui/nav/menu_style.h>
#include <common/ui/nav/grid_layout.h>
#include "ui/ui_muxlaunch.h"

static mux_dialogue confirm_dlg;

static mux_dialogue tour_dlg;
static int pending_power_action = 0;
static uint32_t arc_hold_started = 0;
static int arc_hold_fired = 0;

static void show_confirm_dialog(const char *title) {
    lv_label_set_text(confirm_dlg.title_label, title);
    dialogue_open(&confirm_dlg, &theme);
}

static void show_tour_offer(void) {
    tour_dlg.active = 1;
    dialogue_open(&tour_dlg, &theme);
}

static void answer_tour_offer(const int accepted) {
    dialogue_dismiss(&tour_dlg);

    if (accepted) {
        orientation_accept();
        orientation_introduce(mux_module, lang.muxlaunch.title, lang.muxlaunch.overview);
        return;
    }

    orientation_decline();
}

#define LAUNCH(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(LAUNCH_ELEMENTS) };
#undef LAUNCH

#define LAUNCH(NAME, UDATA) launch_item_##NAME,
enum { LAUNCH_ELEMENTS };
#undef LAUNCH

static const int *order;
static char *shown_labels[ui_count_dynamic];
static char *shown_glyphs[ui_count_dynamic];
static int moving = -1;

static void list_nav_move(int steps, int direction);

static void place_item(void);

static void update_nav(void);

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define LAUNCH(NAME, UDATA) {UDATA, lang.muxlaunch.help.NAME},
        LAUNCH_ELEMENTS
#undef LAUNCH
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    char *item_labels[] = {lang.muxlaunch.explore, lang.muxlaunch.collection, lang.muxlaunch.history,
                           lang.muxlaunch.apps,    lang.muxlaunch.info,       lang.muxlaunch.config,
                           lang.muxlaunch.reboot,  lang.muxlaunch.shutdown};

    char *item_labels_short[] = {lang.muxlaunch.abbr.explore, lang.muxlaunch.abbr.collection,
                                 lang.muxlaunch.abbr.history, lang.muxlaunch.abbr.app,
                                 lang.muxlaunch.abbr.info,    lang.muxlaunch.abbr.config,
                                 lang.muxlaunch.abbr.reboot,  lang.muxlaunch.abbr.shutdown};

    static char *glyph_names[] = {"explore", "collection", "history", "apps", "info", "config", "reboot", "shutdown"};

    order = menu_item_order();
    char *shown_short[ui_count_dynamic];
    for (int i = 0; i < ui_count_dynamic; i++) {
        shown_labels[i] = item_labels[order[i]];
        shown_short[i] = item_labels_short[order[i]];
        shown_glyphs[i] = glyph_names[order[i]];
    }

    reset_ui_groups();

    if (theme.grid.enabled) {
        const int steps = init_grid_static(ui_count_dynamic, shown_labels, shown_short, shown_glyphs);
        if (!config.settings.themeopt.menu_label) lv_obj_add_flag(ui_pnl_grid_current_item, LV_OBJ_FLAG_HIDDEN);
        list_nav_move(steps, +1);
    } else {
        for (int i = 0; i < ui_count_dynamic; i++) {
            switch (order[i]) {
#define LAUNCH(NAME, UDATA)                                                                                            \
    case launch_item_##NAME:                                                                                           \
        INIT_STATIC_ITEM(-1, launch, NAME, shown_labels[i], shown_glyphs[i], 0);                                       \
        break;
                LAUNCH_ELEMENTS
#undef LAUNCH
                default:
                    break;
            }
            lv_obj_move_to_index(ui_objects_panel[i], i);
        }

        add_ui_groups(ui_objects, NULL, ui_objects_glyph, ui_objects_panel, 0);
        list_nav_move(direct_to_previous(ui_objects, ui_count_dynamic, &nav_moved), +1);
    }
}

#define WIGGLE_MAX 16

static lv_obj_t *wiggle_panel[WIGGLE_MAX];
static int wiggle_angle[WIGGLE_MAX];

static void wiggle_exec(void *var, const int32_t v) {
    for (int i = 0; i < WIGGLE_MAX; i++) {
        if (wiggle_panel[i] != var) continue;
        lv_obj_set_style_transform_angle(var, (lv_coord_t) (wiggle_angle[i] + v), MU_OBJ_MAIN_DEFAULT);
        return;
    }
}

static lv_obj_t *wiggle_container(void) {
    return grid_mode_enabled ? ui_pnl_grid : ui_pnl_content;
}

static int wiggle_item(lv_obj_t *panel, const int index) {
    return grid_mode_enabled ? (int) IFU(lv_obj_get_user_data(panel)) : index;
}

static int wiggle_base(lv_obj_t *panel, const int index) {
    if (!grid_mode_enabled) return 0;

    const grid_layout_entry layout = find_grid_layout(wiggle_item(panel, index), index);
    return layout.found ? layout.angle * 10 : 0;
}

static void wiggle_stop(void) {
    lv_obj_t *container = wiggle_container();
    const int count = (int) lv_obj_get_child_cnt(container);

    for (int i = 0; i < count; i++) {
        lv_obj_t *panel = lv_obj_get_child(container, i);
        if (!lv_anim_del(panel, wiggle_exec)) continue;
        lv_obj_set_style_transform_angle(panel, (lv_coord_t) wiggle_base(panel, i), MU_OBJ_MAIN_DEFAULT);
    }
}

static void wiggle_start(void) {
    if (config.visual.reduce_motion) return;

    lv_obj_t *container = wiggle_container();
    lv_obj_update_layout(container);
    const int count = (int) lv_obj_get_child_cnt(container);

    for (int i = 0; i < count && i < WIGGLE_MAX; i++) {
        lv_obj_t *panel = lv_obj_get_child(container, i);
        wiggle_panel[i] = panel;
        wiggle_angle[i] = wiggle_base(panel, i);
        if (wiggle_item(panel, i) == moving || lv_obj_has_flag(panel, LV_OBJ_FLAG_HIDDEN)) continue;

        const int width = lv_obj_get_width(panel);
        const int swing = LV_CLAMP(5, 1700 / (width > 0 ? width : 1), 25);
        const int side = i % 2 ? 1 : -1;

        lv_obj_set_style_transform_pivot_x(panel, (lv_coord_t) (width / 2), MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_transform_pivot_y(panel, (lv_coord_t) (lv_obj_get_height(panel) / 2), MU_OBJ_MAIN_DEFAULT);

        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, panel);
        lv_anim_set_exec_cb(&anim, wiggle_exec);
        lv_anim_set_values(&anim, -swing * side, swing * side);
        lv_anim_set_time(&anim, 140);
        lv_anim_set_playback_time(&anim, 140);
        lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_delay(&anim, (uint32_t) (i * 37 % 140));
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
        lv_anim_start(&anim);
    }
}

static void list_nav_move(const int steps, const int direction) {
    if (moving >= 0) wiggle_stop();

    const int restore = first_open && is_carousel_grid_mode();

    if (first_open) {
        first_open = 0;
    } else {
        nav_unsuppress_shake();
        play_sound(snd_navigate);
    }

    for (int step = 0; step < steps; ++step) {
        if (!grid_mode_enabled) apply_text_long_dot(&theme, lv_group_get_focused(ui_group));

        if (direction < 0) {
            current_item_index = current_item_index == 0 ? ui_count_dynamic - 1 : current_item_index - 1;
        } else {
            current_item_index = current_item_index == ui_count_dynamic - 1 ? 0 : current_item_index + 1;
        }

        if (!is_carousel_grid_mode()) {
            nav_move(ui_group, direction);
            nav_move(ui_group_glyph, direction);
            nav_move(ui_group_panel, direction);
        }

        const int passing = menu_style_active() && !is_carousel_grid_mode() && step < steps - 1;
        if (grid_mode_enabled && !restore && !passing) update_grid(direction);
    }

    if (restore) update_grid_items(1);

    if (!grid_mode_enabled) {
        update_scroll_position(
            theme.mux.item.count, theme.mux.item.panel, ui_count_dynamic, current_item_index, ui_pnl_content
        );
        set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);
    } else {
        lv_label_set_text(ui_lbl_grid_current_item, items[current_item_index].name);
    }

    if (moving >= 0) wiggle_start();
    nav_moved = 1;
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static int spatial_move(const int dx, const int dy) {
    if (!menu_style_spatial()) return 0;

    const int target = grid_spatial_target(dx, dy);
    if (target > current_item_index) list_nav_next(target - current_item_index);
    if (target >= 0 && target < current_item_index) list_nav_prev(current_item_index - target);

    return 1;
}

static void handle_a(void) {
    if (dialogue_active(&tour_dlg)) {
        play_sound(snd_confirm);
        answer_tour_offer(tour_dlg.selected == mux_confirm_yep);
        return;
    }

    if (msgbox_active || hold_call) return;

    if (moving >= 0) {
        place_item();
        return;
    }

    if (dialogue_active(&confirm_dlg)) {
        const mux_confirm_opt opt = (mux_confirm_opt) confirm_dlg.selected;
        dialogue_dismiss(&confirm_dlg);

        if (opt == mux_confirm_yep) {
            if (pending_power_action == 1) {
                toast_message(lang.generic.rebooting, tst_wait_f);
                load_mux("reboot");
            } else {
                toast_message(lang.generic.shutting_down, tst_wait_f);
                load_mux("shutdown");
            }

            mux_input_stop();
        }

        return;
    }

    static int16_t kiosk_pass = 0;

    typedef enum {
        menu_general,
        menu_content,
        menu_reboot,
        menu_shutdown,
    } menu_action;

    typedef struct {
        const char *mux_name;
        int16_t *kiosk_flag;
        menu_action menu_action;
    } menu_entry;

    static const menu_entry entries[ui_count_dynamic] = {
        {"explore", &kiosk.launch.explore, menu_content},
        {"collection", &kiosk.launch.collection, menu_content},
        {"history", &kiosk.launch.history, menu_content},
        {"app", &kiosk.launch.application, menu_general},
        {"info", &kiosk.launch.information, menu_general},
        {"config", &kiosk.launch.configuration, menu_general},
        {"reboot", &kiosk_pass, menu_reboot},
        {"shutdown", &kiosk_pass, menu_shutdown},
    };

    if ((unsigned) current_item_index >= ui_count_dynamic) return;
    const menu_entry *entry = &entries[order[current_item_index]];

    if (is_ksk(*entry->kiosk_flag)) {
        kiosk_denied();
        return;
    }

    switch (entry->menu_action) {
        case menu_general:
            play_sound(snd_confirm);
            break;
        case menu_content:
            invalidate_catalogue_cache();
            play_sound(snd_confirm);
            break;
        case menu_reboot:
            if (!config.settings.advanced.trust_power) {
                pending_power_action = 1;
                show_confirm_dialog(lang.muxlaunch.confirm_reboot);
                return;
            }
            toast_message(lang.generic.rebooting, tst_wait_f);
            break;
        case menu_shutdown:
            if (!config.settings.advanced.trust_power) {
                pending_power_action = 2;
                show_confirm_dialog(lang.muxlaunch.confirm_shutdown);
                return;
            }
            toast_message(lang.generic.shutting_down, tst_wait_f);
            break;
        default:
            return;
    }

    load_mux(entry->mux_name);

    mux_input_stop();
}

static void handle_b(void) {
    if (dialogue_active(&tour_dlg)) return;
    if (hold_call) return;

    if (dialogue_active(&confirm_dlg)) {
        dialogue_mark_cancelled(&confirm_dlg);
        dialogue_dismiss(&confirm_dlg);
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (moving >= 0) {
        wiggle_stop();
        moving = -1;
        update_nav();
        play_sound(snd_back);
        toast_message(lang.muxlaunch.move_cancel, tst_wait_s);
        return;
    }

    if (current_item_index != 0) list_nav_move(current_item_index, -1);
}

static int move_blocked(void) {
    if (dialogue_active(&tour_dlg) || dialogue_active(&confirm_dlg) || msgbox_active || hold_call) return 1;
    if (!is_ksk(kiosk.config.customisation)) return 0;

    kiosk_denied();
    return 1;
}

static void place_item(void) {
    wiggle_stop();

    if (moving == current_item_index) {
        moving = -1;
        update_nav();
        play_sound(snd_back);
        toast_message(lang.muxlaunch.move_cancel, tst_wait_s);
        return;
    }

    char saved[ui_count_dynamic + 1];
    for (int i = 0; i < ui_count_dynamic; i++) {
        const int at = i == moving ? current_item_index : i == current_item_index ? moving : i;
        saved[i] = (char) ('0' + order[at]);
    }
    saved[ui_count_dynamic] = '\0';

    play_sound(snd_confirm);
    write_text_to_file_atomic(CONF_CONFIG_PATH "settings/theme/menu_order", CHAR, saved);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, shown_glyphs[moving]);
    refresh_config = 1;

    load_mux("launcher");
    mux_input_stop();
}

static void handle_y_hold(void) {
    if (moving >= 0 || move_blocked()) return;

    moving = current_item_index;
    play_sound(snd_confirm);
    wiggle_start();
    update_nav();

    char message[MAX_BUFFER_SIZE];
    snprintf(message, sizeof(message), lang.muxlaunch.move_pick, shown_labels[moving]);
    toast_message(message, tst_wait_l);
}

static void handle_x_press(void) {
    if (dialogue_active(&tour_dlg)) return;

    orientation_handle_skip();
}

static void handle_x(void) {
    if (dialogue_active(&tour_dlg)) return;

    if (orientation_handle_skip()) return;

    if (msgbox_active || hold_call) return;

    if (current_item_index == 5) {
        play_sound(snd_confirm);

        load_mux("kiosk");
        mux_input_stop();
    }
}

static int arc_launch_allowed(void) {
    return current_item_index == 3 && !msgbox_active && !dialogue_active(&tour_dlg) && !dialogue_active(&confirm_dlg)
           && !hold_call;
}

static void handle_arc_press(void) {
    arc_hold_started = arc_launch_allowed() ? mux_input_tick() : 0;
    arc_hold_fired = 0;
}

static void handle_arc_hold(void) {
    if (!arc_hold_started || arc_hold_fired || !arc_launch_allowed()) return;
    if (mux_input_tick() - arc_hold_started < 1000) return;

    arc_hold_fired = 1;
    play_sound(snd_confirm);
    load_mux("arc");
    mux_input_stop();
}

static void handle_arc_release(void) {
    arc_hold_started = 0;
    arc_hold_fired = 0;
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || hold_call || dialogue_active(&confirm_dlg)) return;

    play_sound(snd_info_open);
    show_help();
}

static void handle_up(void) {
    if (dialogue_active(&tour_dlg)) {
        dialogue_handle_dpad(&tour_dlg, &theme, -1, !swap_axis);
        return;
    }

    if (msgbox_active) return;

    if (dialogue_active(&confirm_dlg)) {
        dialogue_handle_dpad(&confirm_dlg, &theme, -1, !swap_axis);
        return;
    }

    set_nav_input_dir(nav_dir_up);
    if (spatial_move(0, -1)) return;

    const int grid_steps = theme.grid.enabled ? grid_row_wrap_up_steps() : -1;
    const int misc_steps = !theme.grid.enabled ? misc_even_row_wrap_up_steps(ui_count_dynamic) : -1;

    if (grid_steps >= 0) {
        list_nav_next(grid_steps);
    } else if (misc_steps >= 0) {
        list_nav_next(misc_steps);
    } else if (theme.misc.navigation_type == nav_split_rows_full && !current_item_index) {
        list_nav_next(2);
    } else if (theme.misc.navigation_type == nav_split_rows_full && current_item_index == 3) {
        list_nav_next(4);
    } else {
        list_nav_prev(1);
    }
}

static void handle_down(void) {
    if (dialogue_active(&tour_dlg)) {
        dialogue_handle_dpad(&tour_dlg, &theme, +1, !swap_axis);
        return;
    }

    if (msgbox_active) return;

    if (dialogue_active(&confirm_dlg)) {
        dialogue_handle_dpad(&confirm_dlg, &theme, +1, !swap_axis);
        return;
    }

    set_nav_input_dir(nav_dir_down);
    if (spatial_move(0, 1)) return;

    const int grid_steps = theme.grid.enabled ? grid_row_wrap_down_steps() : -1;
    const int misc_steps = !theme.grid.enabled ? misc_even_row_wrap_down_steps(ui_count_dynamic) : -1;

    if (grid_steps >= 0) {
        list_nav_prev(grid_steps);
    } else if (misc_steps >= 0) {
        list_nav_prev(misc_steps);
    } else if (theme.misc.navigation_type == nav_split_rows_full && current_item_index == 2) {
        list_nav_prev(2);
    } else if (theme.misc.navigation_type == nav_split_rows_full && current_item_index == 7) {
        list_nav_prev(4);
    } else {
        list_nav_next(1);
    }
}

static void handle_up_hold(void) { // prev
    if (dialogue_active(&tour_dlg)) {
        dialogue_handle_dpad_hold(&tour_dlg, &theme, -1, !swap_axis);
        return;
    }

    if (dialogue_active(&confirm_dlg)) {
        dialogue_handle_dpad_hold(&confirm_dlg, &theme, -1, !swap_axis);
        return;
    }

    if (msgbox_active) return;

    const int grid_hold_ok = theme.grid.enabled && grid_row_hold_up_ok();
    const int misc_hold_ok = !theme.grid.enabled && misc_even_row_hold_up_ok(ui_count_dynamic);
    const int regular_ok = theme.misc.navigation_type < nav_even_rows && current_item_index > 0;

    // Don't wrap around when scrolling on hold.
    if (grid_hold_ok || misc_hold_ok || regular_ok
        || (theme.misc.navigation_type == nav_split_rows_full && current_item_index > 0 && current_item_index <= 2)
        || (theme.misc.navigation_type == nav_split_rows_full && current_item_index > 3) || is_carousel_grid_mode()
        || menu_style_spatial()) {
        handle_up();
    }
}

static void handle_down_hold(void) { // next
    if (dialogue_active(&tour_dlg)) {
        dialogue_handle_dpad_hold(&tour_dlg, &theme, +1, !swap_axis);
        return;
    }

    if (dialogue_active(&confirm_dlg)) {
        dialogue_handle_dpad_hold(&confirm_dlg, &theme, +1, !swap_axis);
        return;
    }

    if (msgbox_active) return;

    const int grid_hold_ok = theme.grid.enabled && grid_row_hold_down_ok(ui_count_dynamic);
    const int misc_hold_ok = !theme.grid.enabled && misc_even_row_hold_down_ok(ui_count_dynamic);
    const int regular_ok = theme.misc.navigation_type < nav_even_rows && current_item_index < ui_count_dynamic - 1;

    // Don't wrap around when scrolling on hold.
    if (grid_hold_ok || misc_hold_ok || regular_ok
        || (theme.misc.navigation_type == nav_split_rows_full && current_item_index < ui_count_dynamic - 1
            && current_item_index > 2)
        || (theme.misc.navigation_type == nav_split_rows_full && current_item_index < 2) || is_carousel_grid_mode()
        || menu_style_spatial()) {
        handle_down();
    }
}

static void handle_left(void) {
    if (dialogue_active(&tour_dlg)) {
        dialogue_handle_dpad(&tour_dlg, &theme, -1, swap_axis);
        return;
    }

    if (msgbox_active) return;

    if (dialogue_active(&confirm_dlg)) {
        dialogue_handle_dpad(&confirm_dlg, &theme, -1, swap_axis);
        return;
    }

    set_nav_input_dir(nav_dir_left);
    if (spatial_move(-1, 0)) return;
    if ((is_carousel_grid_mode() && theme.grid.row_count == 1) || menu_style_any_direction()) {
        list_nav_prev(1);
        return;
    }

    if (theme.grid.enabled && is_horizontal_grid_nav()) {
        list_nav_prev(grid_row_left_steps());
    } else if (!theme.grid.enabled && is_horizontal_misc_nav()) {
        list_nav_prev(ui_count_dynamic / 2);
    }

    if (is_split_row_misc_nav()) {
        switch (current_item_index) {
            case 3:
            case 4:
                list_nav_prev(3);
                break;
            case 5:
                list_nav_prev(4);
                break;
            case 6:
            case 7:
                list_nav_prev(5);
                break;
            default:
                break;
        }
    }
}

static void handle_right(void) {
    if (dialogue_active(&tour_dlg)) {
        dialogue_handle_dpad(&tour_dlg, &theme, +1, swap_axis);
        return;
    }

    if (msgbox_active) return;

    if (dialogue_active(&confirm_dlg)) {
        dialogue_handle_dpad(&confirm_dlg, &theme, +1, swap_axis);
        return;
    }

    set_nav_input_dir(nav_dir_right);
    if (spatial_move(1, 0)) return;
    if ((is_carousel_grid_mode() && theme.grid.row_count == 1) || menu_style_any_direction()) {
        list_nav_next(1);
        return;
    }

    if (theme.grid.enabled && is_horizontal_grid_nav()) {
        list_nav_next(grid_row_right_steps(ui_count_dynamic));
    } else if (!theme.grid.enabled && is_horizontal_misc_nav()) {
        list_nav_next(ui_count_dynamic / 2);
    }
    // Horizontal Navigation with 3 item first row, 5 item second row
    if (is_split_row_misc_nav()) {
        switch (current_item_index) {
            case 0:
                list_nav_next(3);
                break;
            case 1:
                list_nav_next(4);
                break;
            case 2:
                list_nav_next(5);
                break;
            default:
                break;
        }
    }
}

static void handle_left_hold(void) {
    if (dialogue_active(&tour_dlg) || msgbox_active || dialogue_active(&confirm_dlg)) return;

    if (grid_row_hold_left_ok() || menu_style_any_direction() || menu_style_spatial()) handle_left();
}

static void handle_right_hold(void) {
    if (dialogue_active(&tour_dlg) || msgbox_active || dialogue_active(&confirm_dlg)) return;

    if (grid_row_hold_right_ok() || menu_style_any_direction() || menu_style_spatial()) handle_right();
}

static void update_nav(void) {
    nav_hide_all();

    if (moving < 0) {
        setup_nav(
            (struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0}, {ui_lbl_nav_a, lang.generic.select, 0}, {NULL, NULL, 0}}
        );
        return;
    }

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.muxlaunch.move_swap, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.cancel, 0},
                                  {NULL, NULL, 0}});
}

static void init_elements(void) {
    header_and_footer_setup();

    update_nav();

#define LAUNCH(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_launch, UDATA);
    LAUNCH_ELEMENTS
#undef LAUNCH

    overlay_display();
}

int muxlaunch_main(void) {
    moving = -1;
    init_module(__func__);
    init_theme(1, 1);

    init_ui_common_screen(&theme, &device, &lang, lang.muxlaunch.title);
    init_muxlaunch(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    init_fonts();
    init_navigation_group();

    if (file_exist(CONTENT_SWITCH_FAILED)) {
        remove(CONTENT_SWITCH_FAILED);
        toast_message(lang.muxmedia.playback_failed, tst_wait_m);
    }

    dialogue_init_confirm(
        &confirm_dlg, &theme, ui_screen, lang.muxlaunch.confirm_reboot, NULL, lang.generic.confirm, lang.generic.cancel,
        lang.generic.select, lang.generic.cancel
    );

    dialogue_init_confirm(
        &tour_dlg, &theme, ui_screen, lang.generic.tour_offer, lang.generic.tour_offer_desc, lang.generic.yes,
        lang.generic.no, lang.generic.select, NULL
    );

    tour_dlg.safe_default = mux_confirm_yep;

    adjust_wallpaper_element(ui_group, 0, wall_general);

    init_timer(ui_gen_refresh_task, NULL);

    if (file_exist(MUOS_SYS_LOAD)) remove(MUOS_SYS_LOAD);

    mux_input_options input_opts = {
        .swap_axis = (theme.grid.enabled && theme.grid.navigation_type >= 1 && theme.grid.navigation_type <= 5)
                     || (!theme.grid.enabled && theme.misc.navigation_type >= 1 && theme.misc.navigation_type <= 5),
        .press_handler =
            {
                [mux_input_x] = handle_x_press,
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_dpad_up] = handle_up,
                [mux_input_dpad_down] = handle_down,
                [mux_input_dpad_left] = handle_left,
                [mux_input_dpad_right] = handle_right,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_x] = handle_x,
            [mux_input_y] = handle_y_hold,
            [mux_input_dpad_up] = handle_up_hold,
            [mux_input_dpad_down] = handle_down_hold,
            [mux_input_dpad_left] = handle_left_hold,
            [mux_input_dpad_right] = handle_right_hold,
        }
    };

    init_input(&input_opts, 1);

    append_combo(
        &input_opts, (mux_input_combo) {
                         .type_mask = BIT(mux_input_start) | BIT(mux_input_x),
                         .press_handler = handle_arc_press,
                         .hold_handler = handle_arc_hold,
                         .release_handler = handle_arc_release,
                     }
    );

    if (orientation_pending())
        show_tour_offer();
    else
        orientation_introduce(mux_module, lang.muxlaunch.title, lang.muxlaunch.overview);

    mux_input_task(&input_opts);

    if (item_count > 0) free_items(&items, &item_count);

    return 0;
}
