#include "muxshare.h"
#include <dirent.h>
#include <sys/wait.h>
#include <ctype.h>
#include <common/base/randname.h>
#include <common/ui/osk.h>
#include <common/ui/orientation.h>

#define PROFILE_SCRIPT     OPT_PATH "script/system/profile.sh"
#define PROFILE_SHARE      OPT_SHARE_PATH "profile"
#define PROFILE_BUILTIN    PROFILE_SHARE "/accessibility"
#define PROFILE_OEM        PROFILE_SHARE "/oem"
#define PROFILE_PREVIOUS   PROFILE_SHARE "/state/previous.conf"
#define PROFILE_ACTIVE     PROFILE_SHARE "/state/active"
#define PROFILE_USER       RUN_STORAGE_PATH "profile"
#define PROFILE_MAX        96
#define PROFILE_TEXT_MAX   256
#define PROFILE_NAME_LIMIT 48

typedef enum { row_save = 0, row_restore, row_accessibility, row_oem, row_user } row_kind;

typedef struct {
    row_kind kind;
    char path[MAX_BUFFER_SIZE];
    char name[PROFILE_TEXT_MAX];
    char description[PROFILE_TEXT_MAX];
} profile_row;

typedef enum { apply_merge = 0, apply_replace, apply_cancel } apply_choice;

static profile_row rows[PROFILE_MAX];
static int row_count;
static int pending_row = -1;
static int restore_index;
static char active_name[PROFILE_TEXT_MAX];

static lv_obj_t *ui_pnl_entry_profile;
static lv_obj_t *ui_txt_entry_profile;

static mux_dialogue apply_dlg;
static mux_dialogue delete_dlg;

static void read_profile_header(const char *path, char *name, char *description) {
    name[0] = '\0';
    description[0] = '\0';

    FILE *file = fopen(path, "r");
    if (!file) return;

    char line[PROFILE_TEXT_MAX + 32];
    for (int lines = 0; lines < 8 && fgets(line, sizeof(line), file); lines++) {
        line[strcspn(line, "\r\n")] = '\0';
        const char *text = strncmp(line, "\xEF\xBB\xBF", 3) == 0 ? line + 3 : line;

        if (strncmp(text, "name=", 5) == 0) snprintf(name, PROFILE_TEXT_MAX, "%s", text + 5);
        if (strncmp(text, "description=", 12) == 0) snprintf(description, PROFILE_TEXT_MAX, "%s", text + 12);
    }

    fclose(file);
}

static int compare_names(const void *a, const void *b) {
    return strcmp(*(const char *const *) a, *(const char *const *) b);
}

static void add_row(const row_kind kind, const char *path, const char *name, const char *description) {
    if (row_count >= PROFILE_MAX) return;

    profile_row *row = &rows[row_count++];
    row->kind = kind;
    snprintf(row->path, sizeof(row->path), "%s", path ? path : "");
    snprintf(row->name, sizeof(row->name), "%s", name ? name : "");
    snprintf(row->description, sizeof(row->description), "%s", description ? description : "");
}

static void scan_profiles(const char *dir, const row_kind kind) {
    DIR *handle = opendir(dir);
    if (!handle) return;

    char *names[PROFILE_MAX];
    int count = 0;

    const struct dirent *entry;
    while ((entry = readdir(handle)) && count < PROFILE_MAX) {
        const size_t len = strlen(entry->d_name);
        if (entry->d_name[0] == '.' || len < 6 || strcasecmp(entry->d_name + len - 5, ".conf") != 0) continue;
        names[count++] = strdup(entry->d_name);
    }
    closedir(handle);

    qsort(names, (size_t) count, sizeof(names[0]), compare_names);

    for (int i = 0; i < count; i++) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);

        char name[PROFILE_TEXT_MAX];
        char description[PROFILE_TEXT_MAX];
        read_profile_header(path, name, description);

        if (!name[0]) {
            snprintf(name, sizeof(name), "%s", names[i]);
            name[strlen(name) - 5] = '\0';
        }

        add_row(kind, path, name, description);
        free(names[i]);
    }
}

static void build_rows(void) {
    row_count = 0;

    add_row(row_save, NULL, lang.muxprofile.save_current, lang.muxprofile.help.save);
    if (file_exist(PROFILE_PREVIOUS))
        add_row(row_restore, PROFILE_PREVIOUS, lang.muxprofile.restore_previous, lang.muxprofile.help.restore);

    scan_profiles(PROFILE_BUILTIN, row_accessibility);
    scan_profiles(PROFILE_OEM, row_oem);
    scan_profiles(PROFILE_USER, row_user);

    active_name[0] = '\0';
    char *active = read_line_char_from(PROFILE_ACTIVE, 1);
    if (active) {
        snprintf(active_name, sizeof(active_name), "%s", active);
        free(active);
    }
}

static const char *row_value(const profile_row *row) {
    switch (row->kind) {
        case row_accessibility:
        case row_oem:
        case row_user:
            if (active_name[0] && strcmp(active_name, row->name) == 0) return lang.muxprofile.active;
            return row->kind == row_accessibility ? lang.muxprofile.type.accessibility
                   : row->kind == row_oem         ? lang.muxprofile.type.oem
                                                  : lang.muxprofile.type.user;
        default:
            return "";
    }
}

static const char *row_glyph(const profile_row *row) {
    switch (row->kind) {
        case row_save:
            return "save";
        case row_restore:
            return "restore";
        case row_accessibility:
            return "accessibility";
        case row_oem:
            return "oem";
        default:
            return "user";
    }
}

static void create_rows(void) {
    reset_ui_groups();

    for (int i = 0; i < row_count; i++) {
        ui_count_static++;

        lv_obj_t *ui_pnl_row = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_row);

        lv_obj_t *ui_lbl_row = lv_label_create(ui_pnl_row);
        apply_theme_list_item(&theme, ui_lbl_row, rows[i].name);

        lv_obj_t *ui_lbl_row_glyph = lv_img_create(ui_pnl_row);
        apply_theme_list_glyph(&theme, ui_lbl_row_glyph, mux_module, row_glyph(&rows[i]));

        lv_obj_t *ui_lbl_row_value = lv_label_create(ui_pnl_row);
        apply_theme_list_value(&theme, ui_lbl_row_value, row_value(&rows[i]));

        lv_group_add_obj(ui_group, ui_lbl_row);
        lv_group_add_obj(ui_group_value, ui_lbl_row_value);
        lv_group_add_obj(ui_group_glyph, ui_lbl_row_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_row);

        apply_size_to_content(&theme, ui_pnl_content, ui_lbl_row, ui_lbl_row_glyph, rows[i].name);
        apply_text_long_dot(&theme, ui_lbl_row);
    }

    if (ui_count_static > 0) lv_obj_update_layout(ui_pnl_content);
}

static int run_profile_script(const char *const *args) {
    const pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        execv(args[0], (char *const *) args);
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static void reload_module(void) {
    restore_index = current_item_index;
    load_mux("profile");
    mux_input_stop();
}

static void finish_change(const int result, const char *success) {
    if (result == 0) {
        run_tweak_script(success);
        reload_module();
        return;
    }

    play_sound(snd_error);
    const char *message = lang.muxprofile.failed;
    if (result == 3) message = lang.muxprofile.empty;
    if (result == 4) message = lang.muxprofile.invalid;

    toast_message(message, tst_wait_l);
}

static void apply_profile(const profile_row *row, const apply_choice choice) {
    toast_message(lang.generic.loading, tst_wait_f);
    refresh_screen(ui_screen, 1);

    const char *mode = choice == apply_replace ? "replace" : "merge";
    const char *args[] = {PROFILE_SCRIPT, "apply", row->path, mode, NULL};

    finish_change(run_profile_script(args), lang.muxprofile.applied);
}

static void restore_previous(void) {
    toast_message(lang.generic.loading, tst_wait_f);
    refresh_screen(ui_screen, 1);

    const char *args[] = {PROFILE_SCRIPT, "undo", NULL};
    finish_change(run_profile_script(args), lang.muxprofile.restored);
}

static void unique_profile_path(const char *name, char *out, const size_t len) {
    char safe[PROFILE_TEXT_MAX];
    str_safe_filename(name, safe, sizeof(safe), "Profile");

    snprintf(out, len, PROFILE_USER "/%s.conf", safe);
    for (int i = 2; file_exist(out) && i < 100; i++)
        snprintf(out, len, PROFILE_USER "/%s (%d).conf", safe, i);
}

static void save_current(const char *name) {
    char trimmed[PROFILE_TEXT_MAX];
    snprintf(trimmed, sizeof(trimmed), "%s", str_trim((char *) name));
    if (!trimmed[0]) snprintf(trimmed, sizeof(trimmed), "%s", lang.muxprofile.default_name);

    create_directories(PROFILE_USER, 0);

    char path[PATH_MAX];
    unique_profile_path(trimmed, path, sizeof(path));

    const char *args[] = {PROFILE_SCRIPT, "save", path, trimmed, "", NULL};
    const int result = run_profile_script(args);

    if (result == 0) {
        toast_message(lang.muxprofile.saved, tst_wait_m);
        reload_module();
        return;
    }

    play_sound(snd_error);
    toast_message(lang.muxprofile.failed, tst_wait_m);
}

static void suggest_profile_name(char *out, const size_t len) {
    if (randname_generate_with_separator(out, len, " ") != 0) {
        snprintf(out, len, "%s", lang.muxprofile.default_name);
        return;
    }

    for (size_t i = 0; out[i]; i++)
        if (i == 0 || out[i - 1] == ' ') out[i] = (char) toupper((unsigned char) out[i]);
}

static void open_name_entry(void) {
    lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

    char suggestion[RANDNAME_MAX_LEN + 1];
    suggest_profile_name(suggestion, sizeof(suggestion));

    key_show = 1;
    osk_show(ui_pnl_entry_profile);
    lv_textarea_set_text(ui_txt_entry_profile, suggestion);
}

static void handle_keyboard_ok_press(void) {
    key_show = 0;
    close_osk(key_entry, ui_group, ui_txt_entry_profile, ui_pnl_entry_profile);
    save_current(lv_textarea_get_text(ui_txt_entry_profile));
}

static void handle_keyboard_press(void) {
    play_sound(snd_keypress);

    const char *is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
    if (is_key && strcasecmp(is_key, OSK_DONE) == 0) {
        handle_keyboard_ok_press();
    } else {
        lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
    }
}

static const profile_row *focused_row(void) {
    if (current_item_index < 0 || current_item_index >= row_count) return NULL;
    return &rows[current_item_index];
}

static void leave_module(void) {
    play_sound(snd_back);

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "access");
    restore_index = 0;
    if (config.boot.factory_reset) load_mux("installer");

    mux_input_stop();
}

static mux_dialogue *open_dialogue(void) {
    if (dialogue_active(&apply_dlg)) return &apply_dlg;
    if (dialogue_active(&delete_dlg)) return &delete_dlg;
    return NULL;
}

static void handle_a(void) {
    if (hold_call || msgbox_active) return;

    if (key_show) {
        handle_keyboard_press();
        return;
    }

    if (dialogue_active(&apply_dlg)) {
        const apply_choice choice = (apply_choice) apply_dlg.selected;
        dialogue_dismiss(&apply_dlg);

        if (choice != apply_cancel && pending_row >= 0 && pending_row < row_count)
            apply_profile(&rows[pending_row], choice);

        pending_row = -1;
        return;
    }

    if (dialogue_active(&delete_dlg)) {
        const mux_confirm_opt choice = (mux_confirm_opt) delete_dlg.selected;
        dialogue_dismiss(&delete_dlg);

        if (choice == mux_confirm_yep && pending_row >= 0 && pending_row < row_count) {
            if (remove(rows[pending_row].path) == 0) {
                toast_message(lang.muxprofile.deleted, tst_wait_m);
                restore_index = current_item_index > 0 ? current_item_index - 1 : 0;
                load_mux("profile");
                mux_input_stop();
            } else {
                play_sound(snd_error);
                toast_message(lang.muxprofile.failed, tst_wait_m);
            }
        }

        pending_row = -1;
        return;
    }

    const profile_row *row = focused_row();
    if (!row) return;

    play_sound(snd_confirm);

    switch (row->kind) {
        case row_save:
            open_name_entry();
            break;
        case row_restore:
            restore_previous();
            break;
        default:
            pending_row = current_item_index;
            dialogue_set_description(&apply_dlg, lang.muxprofile.apply_desc);
            dialogue_open(&apply_dlg, &theme);
            break;
    }
}

static void handle_b(void) {
    if (hold_call) return;

    if (key_show) {
        key_show = 0;
        close_osk(key_entry, ui_group, ui_txt_entry_profile, ui_pnl_entry_profile);
        return;
    }

    mux_dialogue *dlg = open_dialogue();
    if (dlg) {
        dialogue_cancel(dlg);
        pending_row = -1;
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    leave_module();
}

static void handle_x(void) {
    if (key_show) {
        key_backspace(ui_txt_entry_profile);
        return;
    }

    if (hold_call || msgbox_active || open_dialogue()) return;

    const profile_row *row = focused_row();
    if (!row || row->kind != row_user) return;

    play_sound(snd_confirm);
    pending_row = current_item_index;
    dialogue_open(&delete_dlg, &theme);
}

static void handle_y(void) {
    if (key_show) {
        key_space(ui_txt_entry_profile);
        return;
    }

    if (hold_call || msgbox_active || open_dialogue()) return;

    play_sound(snd_confirm);
    open_name_entry();
}

static void handle_help(void) {
    if (key_show || msgbox_active || progress_onscreen != -1 || hold_call || open_dialogue()) return;

    const profile_row *row = focused_row();
    if (!row) return;

    play_sound(snd_info_open);
    show_info_box(row->name, row->description[0] ? row->description : lang.muxprofile.help.profile, 0);
}

static void handle_dpad(const int direction, const int held) {
    if (key_show) {
        if (direction < 0)
            key_up();
        else
            key_down();
        return;
    }

    mux_dialogue *dlg = open_dialogue();
    if (dlg) {
        if (held)
            dialogue_handle_dpad_hold(dlg, &theme, direction, !swap_axis);
        else
            dialogue_handle_dpad(dlg, &theme, direction, !swap_axis);
        return;
    }

    if (direction < 0) {
        if (held)
            handle_list_nav_up_hold();
        else
            handle_list_nav_up();
    } else {
        if (held)
            handle_list_nav_down_hold();
        else
            handle_list_nav_down();
    }
}

static void handle_up(void) {
    handle_dpad(-1, 0);
}

static void handle_down(void) {
    handle_dpad(+1, 0);
}

static void handle_up_hold(void) {
    handle_dpad(-1, 1);
}

static void handle_down_hold(void) {
    handle_dpad(+1, 1);
}

static void handle_side(const int direction) {
    if (key_show) {
        if (direction < 0)
            key_left();
        else
            key_right();
        return;
    }

    mux_dialogue *dlg = open_dialogue();
    if (dlg && swap_axis) dialogue_handle_dpad(dlg, &theme, direction, 1);
}

static void handle_left(void) {
    handle_side(-1);
}

static void handle_right(void) {
    handle_side(+1);
}

static void handle_l1(void) {
    if (key_show) {
        key_swap_back();
        return;
    }

    if (!open_dialogue()) handle_list_nav_page_up();
}

static void handle_r1(void) {
    if (key_show) {
        key_swap();
        return;
    }

    if (!open_dialogue()) handle_list_nav_page_down();
}

static void handle_select(void) {
    if (key_show) key_clear(ui_txt_entry_profile);
}

static void handle_start(void) {
    if (key_show) handle_keyboard_ok_press();
}

static void on_key_event(const struct input_event ev) {
    if (!key_show) return;

    if (ev.code == KEY_ENTER && ev.value == 1) {
        handle_keyboard_ok_press();
        return;
    }

    if (ev.code == KEY_ESC && ev.value == 1) {
        handle_b();
    } else {
        process_key_event(&ev, ui_txt_entry_profile);
    }
}

static void init_name_entry(void) {
    ui_pnl_entry_profile = lv_obj_create(ui_screen);
    lv_obj_set_width(ui_pnl_entry_profile, device.mux.width);
    lv_obj_set_height(ui_pnl_entry_profile, device.mux.height);
    lv_obj_set_align(ui_pnl_entry_profile, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(ui_pnl_entry_profile, LV_FLEX_FLOW_COLUMN_WRAP);
    lv_obj_set_flex_align(ui_pnl_entry_profile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_pnl_entry_profile, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ui_pnl_entry_profile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(ui_pnl_entry_profile, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_color(ui_pnl_entry_profile, lv_color_hex(0x000000), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(ui_pnl_entry_profile, 128, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_pnl_entry_profile, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(ui_pnl_entry_profile, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_row(ui_pnl_entry_profile, 5, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_column(ui_pnl_entry_profile, 5, MU_OBJ_MAIN_DEFAULT);

    ui_txt_entry_profile = lv_textarea_create(ui_pnl_entry_profile);
    lv_obj_set_width(ui_txt_entry_profile, device.mux.width * 5 / 6);
    lv_obj_set_height(ui_txt_entry_profile, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_txt_entry_profile, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(ui_txt_entry_profile, PROFILE_NAME_LIMIT);
    lv_textarea_set_one_line(ui_txt_entry_profile, 1);
    lv_textarea_set_placeholder_text(ui_txt_entry_profile, lang.muxprofile.name_prompt);
    lv_obj_set_style_radius(ui_txt_entry_profile, theme.osk.radius, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_color(ui_txt_entry_profile, lv_color_hex(theme.osk.border), MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_opa(ui_txt_entry_profile, theme.osk.border_alpha, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(ui_txt_entry_profile, 2, MU_OBJ_MAIN_DEFAULT);
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.remove, 0},
                                  {ui_lbl_nav_y_glyph, "", 0},
                                  {ui_lbl_nav_y, lang.generic.save, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

int muxprofile_main(void) {
    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxprofile.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);
    init_fonts();

    build_rows();
    create_rows();
    init_elements();
    init_name_entry();

    const char *apply_options[] = {lang.muxprofile.merge, lang.muxprofile.replace, lang.generic.cancel};
    dialogue_init_choice(
        &apply_dlg, &theme, ui_screen, lang.muxprofile.apply_title, lang.muxprofile.apply_desc, apply_options,
        A_SIZE(apply_options), lang.generic.select, lang.generic.cancel
    );
    dialogue_init_confirm(
        &delete_dlg, &theme, ui_screen, lang.muxprofile.delete_title, lang.muxprofile.delete_desc, lang.generic.remove,
        lang.generic.cancel, lang.generic.select, lang.generic.cancel
    );

    init_osk(ui_pnl_entry_profile, ui_txt_entry_profile, 0, 0, PROFILE_NAME_LIMIT);
    init_timer(ui_gen_refresh_task, NULL);

    const int start = restore_index > 0 && restore_index < ui_count_static ? restore_index : 0;
    restore_index = 0;
    if (ui_count_static > 0) gen_step_movement(start, +1, 1, 0, 1);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
                [mux_input_dpad_up] = handle_up,
                [mux_input_dpad_down] = handle_down,
                [mux_input_dpad_left] = handle_left,
                [mux_input_dpad_right] = handle_right,
                [mux_input_l1] = handle_l1,
                [mux_input_r1] = handle_r1,
                [mux_input_select] = handle_select,
                [mux_input_start] = handle_start,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_up] = handle_up_hold,
            [mux_input_dpad_down] = handle_down_hold,
            [mux_input_dpad_left] = handle_left,
            [mux_input_dpad_right] = handle_right,
            [mux_input_l1] = handle_l1,
            [mux_input_r1] = handle_r1,
        }
    };

    orientation_introduce(mux_module, lang.muxprofile.title, lang.muxprofile.overview);

    list_nav_set_callbacks(list_nav_cb_prev, list_nav_cb_next);
    init_input(&input_opts, 1);
    register_key_event_callback(on_key_event);
    mux_input_task(&input_opts);

    return 0;
}
