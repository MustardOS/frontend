#include <common/content/core/coredb.h>
#include "muxshare.h"
#include <common/ui/orientation.h>

static char explore_dir[PATH_MAX];

static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_system[PATH_MAX];

static int is_dir = 0;
static int level_is_namespace = 0;

#define ASSIGN_NAMESPACE_TAG "namespace:"
#define ASSIGN_CORE_MAX      64

static char core_ids[ASSIGN_CORE_MAX][COREDB_NAME_MAX];
static enum core_runtime core_runtimes[ASSIGN_CORE_MAX];
static int core_row_count;
static int prefer_directory_scope = 0;

static mux_dialogue assign_dlg;

static int find_assigned_system(char *out_system) {
    // File Spec CFG: line 3 = sys
    // Directory CFG: line 2 = sys

    const char *sys = get_content_line(rom_dir, rom_name, "cfg", 3);
    if (!sys || !*sys || strcasecmp(sys, "none") == 0) {
        sys = get_content_line(rom_dir, NULL, "cfg", 2);
    }

    if (!sys || !*sys || strcasecmp(sys, "none") == 0) return 0;

    char name_space[COREDB_NAME_MAX];
    if (!coredb_system_namespace(sys, name_space, sizeof(name_space))) return 0;

    snprintf(out_system, 4096, "%s", sys);
    return 1;
}

static int find_system_item_index(const char *system_name) {
    char name_space[COREDB_NAME_MAX];
    if (!coredb_system_namespace(system_name, name_space, sizeof(name_space))) return 0;

    const int count = coredb_system_count(name_space);
    for (int i = 0; i < count; i++) {
        struct coredb_system system;
        if (coredb_system_at(name_space, i, &system) && strcmp(system.id, system_name) == 0) return i;
    }

    return 0;
}

static int find_namespace_item_index(const char *name_space) {
    int visible = 0;
    for (int i = 0; i < coredb_namespace_count(); i++) {
        const char *candidate = coredb_namespace_at(i);
        if (coredb_system_count(candidate) == 0) continue;
        if (strcmp(candidate, name_space) == 0) return visible;
        visible++;
    }

    return 0;
}

static const char *assigned_stem_file(void) {
    return get_content_line(rom_dir, rom_name, "cfg", 6);
}

static const char *assigned_stem_dir(void) {
    return get_content_line(rom_dir, NULL, "cfg", 5);
}

static int assigned_row = -1;

static int find_core_item_index(void) {
    return assigned_row < 0 ? 0 : assigned_row;
}

static void show_help(void) {
    show_info_box(lang.muxassign.title, lang.muxassign.help, 0);
}

static int take_forced_pick(char *out, const size_t out_size) {
    out[0] = '\0';
    if (!file_exist(MUOS_ASS_SYSP)) return 0;

    char *picked = read_line_char_from(MUOS_ASS_SYSP, 1);
    snprintf(out, out_size, "%s", picked ? picked : "");
    free(picked);
    remove(MUOS_ASS_SYSP);

    return 1;
}

static int take_stored_index(void) {
    if (!file_exist(MUOS_AIX_LOAD)) return 0;

    const int index = read_line_int_from(MUOS_AIX_LOAD, 1);
    remove(MUOS_AIX_LOAD);

    return index;
}

static void load_namespace_level(const char *name_space) {
    char target[PATH_MAX];
    snprintf(target, sizeof(target), ASSIGN_NAMESPACE_TAG "%s", name_space);

    load_assign(MUOS_ASS_LOAD, rom_name, explore_dir, target, 0, 0);
}

static void add_list_row(const char *label, const char *user_data, const char *glyph) {
    ui_count_static++;

    lv_obj_t *ui_pnl_row = lv_obj_create(ui_pnl_content);
    apply_theme_list_panel(ui_pnl_row);

    lv_obj_t *ui_lbl_row = lv_label_create(ui_pnl_row);
    apply_theme_list_item(&theme, ui_lbl_row, label);

    lv_obj_t *ui_lbl_row_glyph = lv_img_create(ui_pnl_row);
    apply_theme_list_glyph(&theme, ui_lbl_row_glyph, mux_module, glyph);

    lv_group_add_obj(ui_group, ui_lbl_row);
    lv_group_add_obj(ui_group_glyph, ui_lbl_row_glyph);
    lv_group_add_obj(ui_group_panel, ui_pnl_row);

    if (user_data) set_owned_user_data(ui_lbl_row, strdup(user_data));

    apply_size_to_content(&theme, ui_pnl_content, ui_lbl_row, ui_lbl_row_glyph, label);
    apply_text_long_dot(&theme, ui_lbl_row);
}

static void create_namespace_items(void) {
    reset_ui_groups();
    core_row_count = 0;

    for (int i = 0; i < coredb_namespace_count(); i++) {
        const char *name = coredb_namespace_at(i);
        if (coredb_system_count(name) == 0) continue;

        add_list_row(name, name, "system");
    }

    if (ui_count_static > 0) lv_obj_update_layout(ui_pnl_content);
}

static void create_system_items(const char *name_space) {
    reset_ui_groups();
    core_row_count = 0;

    const int count = coredb_system_count(name_space);
    for (int i = 0; i < count; i++) {
        struct coredb_system system;
        if (!coredb_system_at(name_space, i, &system)) continue;

        add_list_row(system.name, system.id, "system");
    }

    if (ui_count_static > 0) lv_obj_update_layout(ui_pnl_content);
}

static void stored_stem(const char *id, const enum core_runtime runtime, char *out, const size_t out_size) {
    coredb_assign_tag(id, runtime, out, out_size);
}

#define ASSIGN_VALUE_CHILD 2

static lv_obj_t *row_value_label(const lv_obj_t *ui_lbl_item) {
    lv_obj_t *panel = lv_obj_get_parent(ui_lbl_item);
    if (!panel || lv_obj_get_child_cnt(panel) <= ASSIGN_VALUE_CHILD) return NULL;

    return lv_obj_get_child(panel, ASSIGN_VALUE_CHILD);
}

static int runtime_supported(const char *system, const char *id, const enum core_runtime runtime) {
    return coredb_core_find(system, runtime, id, NULL);
}

static enum core_runtime
shift_runtime(const char *system, const char *id, const enum core_runtime from, const int direction) {
    for (int step = 1; step <= core_runtime_count; step++) {
        const int offset = direction >= 0 ? step : core_runtime_count - step;
        const enum core_runtime candidate = (enum core_runtime)((from + offset) % core_runtime_count);
        if (runtime_supported(system, id, candidate)) return candidate;
    }

    return from;
}

static void refresh_runtime_values(void) {
    for (int i = 0; i < core_row_count; i++) {
        lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, i);
        if (!panel || lv_obj_get_child_cnt(panel) <= ASSIGN_VALUE_CHILD) continue;

        lv_obj_t *value = lv_obj_get_child(panel, ASSIGN_VALUE_CHILD);
        if (!value) continue;

        lv_label_set_text(value, i == current_item_index ? coredb_runtime_label(core_runtimes[i]) : "");
    }
}

static void assign_nav_prev(const int steps) {
    gen_step_movement(steps, -1, 1, 0, 1);
    refresh_runtime_values();
}

static void assign_nav_next(const int steps) {
    gen_step_movement(steps, +1, 1, 0, 1);
    refresh_runtime_values();
}

static void refresh_focused_runtime(const int direction) {
    if (core_row_count <= 0 || current_item_index < 0 || current_item_index >= core_row_count) return;

    const enum core_runtime moved =
        shift_runtime(rom_system, core_ids[current_item_index], core_runtimes[current_item_index], direction);
    if (moved == core_runtimes[current_item_index]) return;

    core_runtimes[current_item_index] = moved;

    lv_obj_t *focused = lv_group_get_focused(ui_group);
    lv_obj_t *value = row_value_label(focused);
    if (value) lv_label_set_text(value, coredb_runtime_label(moved));

    char stem[FILENAME_MAX];
    stored_stem(core_ids[current_item_index], moved, stem, sizeof(stem));

    free(lv_obj_get_user_data(focused));
    lv_obj_set_user_data(focused, strdup(stem));

    play_sound(snd_navigate);
}

static void create_core_items(const char *target) {
    char default_assign[FILENAME_MAX];
    if (!coredb_system_default(target, default_assign, sizeof(default_assign))) default_assign[0] = '\0';

    core_row_count = 0;
    for (int r = 0; r < core_runtime_count; r++) {
        const int count = coredb_core_count(target, (enum core_runtime) r);
        for (int i = 0; i < count && core_row_count < ASSIGN_CORE_MAX; i++) {
            struct coredb_core core;
            if (!coredb_core_at(target, (enum core_runtime) r, i, &core)) continue;

            int seen = 0;
            for (int k = 0; k < core_row_count; k++)
                if (strcmp(core_ids[k], core.id) == 0) seen = 1;
            if (seen) continue;

            snprintf(core_ids[core_row_count], COREDB_NAME_MAX, "%s", core.id);
            core_runtimes[core_row_count] = (enum core_runtime) r;
            core_row_count++;
        }
    }

    reset_ui_groups();

    struct assigned_core {
        char id[COREDB_NAME_MAX];
        enum core_runtime runtime;
        int valid;
    };

    struct assigned_core from_file = {{0}, core_runtime_pickles, 0};
    struct assigned_core from_dir = {{0}, core_runtime_pickles, 0};

    from_file.valid =
        coredb_assign_resolve(target, assigned_stem_file(), from_file.id, sizeof(from_file.id), &from_file.runtime);
    from_dir.valid =
        coredb_assign_resolve(target, assigned_stem_dir(), from_dir.id, sizeof(from_dir.id), &from_dir.runtime);

    const int same_assignment = from_file.valid && from_dir.valid && from_file.runtime == from_dir.runtime
                                && strcasecmp(from_file.id, from_dir.id) == 0;

    assigned_row = -1;

    for (int i = 0; i < core_row_count; i++) {
        const struct assigned_core *match = NULL;
        if (from_file.valid && strcasecmp(from_file.id, core_ids[i]) == 0) {
            match = &from_file;
        } else if (from_dir.valid && strcasecmp(from_dir.id, core_ids[i]) == 0) {
            match = &from_dir;
        }

        // Assignments name one runtime so the row it belongs to already selects pickles or ra or external
        if (match) core_runtimes[i] = match->runtime;

        struct coredb_core core;
        if (!coredb_core_find(target, core_runtimes[i], core_ids[i], &core)) continue;

        char stem[FILENAME_MAX];
        stored_stem(core_ids[i], core_runtimes[i], stem, sizeof(stem));

        char display_name[MAX_BUFFER_SIZE];
        if (match == &from_file && !same_assignment) {
            snprintf(display_name, sizeof(display_name), "%s (%s)", core.name, lang.muxassign.file);
        } else if (match) {
            snprintf(display_name, sizeof(display_name), "%s (%s)", core.name, lang.muxassign.dir);
        } else {
            snprintf(display_name, sizeof(display_name), "%s", core.name);
        }

        ui_count_static++;

        if (match && (match == &from_file || assigned_row < 0)) assigned_row = ui_count_static - 1;

        lv_obj_t *ui_pnl_core = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_core);

        lv_obj_t *ui_lbl_core_item = lv_label_create(ui_pnl_core);
        apply_theme_option_item_label(&theme, ui_lbl_core_item, display_name, 1);
        set_owned_user_data(ui_lbl_core_item, strdup(stem));

        lv_obj_t *ui_lbl_core_item_glyph = lv_img_create(ui_pnl_core);
        const char *glyph = strcasecmp(core_ids[i], default_assign) == 0 ? "default" : "core";
        apply_theme_list_glyph(&theme, ui_lbl_core_item_glyph, mux_module, glyph);

        lv_obj_t *ui_lbl_core_item_value = lv_label_create(ui_pnl_core);
        apply_theme_list_value(&theme, ui_lbl_core_item_value, coredb_runtime_label(core_runtimes[i]));

        lv_group_add_obj(ui_group, ui_lbl_core_item);
        lv_group_add_obj(ui_group_value, ui_lbl_core_item_value);
        lv_group_add_obj(ui_group_glyph, ui_lbl_core_item_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_core);

        apply_text_long_dot(&theme, ui_lbl_core_item);
    }

    if (assigned_row >= 0 && assigned_row < core_row_count) {
        LOG_INFO(
            mux_module, "Assigned Core: '%s' as %s on row %d", core_ids[assigned_row],
            coredb_runtime_label(core_runtimes[assigned_row]), assigned_row
        );
    }

    if (ui_count_static > 0) lv_obj_update_layout(ui_pnl_content);
}

static void load_return_module(void) {
    if (file_exist(MUOS_ASS_FROM)) {
        remove(OPTION_SKIP);
        char *origin = read_all_char_from(MUOS_ASS_FROM);
        load_mux(origin);
        free(origin);
        remove(MUOS_ASS_FROM);
        remove(MUOS_SYS_LOAD);
    }
}

static void handle_x(void) {
    orientation_handle_skip();
}

static void handle_b(void) {
    if (hold_call) return;

    if (dialogue_active(&assign_dlg)) {
        dialogue_cancel(&assign_dlg);
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    if (strcasecmp(rom_system, "none") == 0) {
        FILE *file = fopen(MUOS_SYS_LOAD, "w");
        fprintf(file, "%s", "");
        fclose(file);
        load_return_module();
    } else if (level_is_namespace) {
        write_text_to_file(MUOS_ASS_SYSP, "w", CHAR, rom_system);
        load_assign(MUOS_ASS_LOAD, rom_name, explore_dir, "none", 0, 0);
    } else {
        char name_space[COREDB_NAME_MAX];
        const int known = coredb_system_namespace(rom_system, name_space, sizeof(name_space));

        // Completely skip a system if its just the single one in a namespace selection.
        // No point in selecting just the single one!
        if (!known || coredb_system_count(name_space) <= 1) {
            if (known) write_text_to_file(MUOS_ASS_SYSP, "w", CHAR, name_space);
            load_assign(MUOS_ASS_LOAD, rom_name, explore_dir, "none", 0, 0);
        } else {
            write_text_to_file(MUOS_ASS_SYSP, "w", CHAR, rom_system);
            load_namespace_level(name_space);
        }
    }

    remove(MUOS_SAA_LOAD);

    mux_input_stop();
}

static void handle_core_assignment(const char *log_msg, const int assignment_mode) {
    LOG_INFO(mux_module, "%s", log_msg);

    char *item_data = lv_obj_get_user_data(lv_group_get_focused(ui_group));
    char *selected_item = str_tolower(item_data);
    LOG_INFO(mux_module, "Selected Core: %s (%s)", selected_item, item_data);

    const char *core_id = item_data;
    const enum core_runtime runtime = coredb_assign_runtime(item_data, &core_id);

    struct coredb_core core;
    if (!coredb_core_find(rom_system, runtime, core_id, &core)
        && !coredb_core_find(rom_system, core_runtime_external, core_id, &core)) {
        LOG_ERROR(mux_module, "No definition for core '%s' under '%s'", core_id, rom_system);
        toast_message(lang.muxassign.misconfigured, tst_wait_l);
        return;
    }

    static char core_catalogue[MAX_BUFFER_SIZE];
    coredb_system_catalogue(rom_system, core_catalogue, sizeof(core_catalogue));
    LOG_INFO(mux_module, "Content Core Catalogue: %s", core_catalogue);

    static char core_governor[MAX_BUFFER_SIZE];
    snprintf(core_governor, sizeof(core_governor), "%s", core.governor[0] ? core.governor : device.cpu.dflt);
    LOG_INFO(mux_module, "Content Core Governor: %s", core_governor);

    static char core_control[MAX_BUFFER_SIZE];
    snprintf(core_control, sizeof(core_control), "%s", core.control[0] ? core.control : "system");
    LOG_INFO(mux_module, "Content Core Control: %s", core_control);

    static char core_retroarch[MAX_BUFFER_SIZE];
    snprintf(core_retroarch, sizeof(core_retroarch), "%s", "false");

    const int core_lookup = coredb_system_lookup(rom_system);
    LOG_INFO(mux_module, "Content Core Lookup: %d", core_lookup);

    static char core_launch[MAX_BUFFER_SIZE];
    snprintf(core_launch, sizeof(core_launch), "%s", core.core);
    LOG_INFO(mux_module, "Content Core Launcher: %s", core_launch);

    create_core_assignment(
        selected_item, rom_dir, core_launch, rom_system, core_catalogue, rom_name, core_governor, core_control,
        core_retroarch, core_lookup, assignment_mode
    );

    load_return_module();
}

static void handle_a(void) {
    if (dialogue_active(&assign_dlg)) {
        const int method = assign_dlg.option_data[assign_dlg.selected];
        if (method < 0) dialogue_mark_silent(&assign_dlg);

        dialogue_dismiss(&assign_dlg);

        if (method < 0) {
            handle_b();
            return;
        }

        char log_msg[64];
        snprintf(log_msg, sizeof(log_msg), "Core Assignment Triggered (method %d)", method);
        handle_core_assignment(log_msg, method);

        remove(MUOS_SYS_LOAD);
        remove(OPTION_SKIP);

        write_text_to_file(MUOS_AIX_LOAD, "w", INT, current_item_index);

        mux_input_stop();
        return;
    }

    if (msgbox_active || hold_call) return;

    if (strcasecmp(rom_system, "none") == 0 || level_is_namespace) {
        play_sound(snd_confirm);

        const char *chosen = lv_obj_get_user_data(lv_group_get_focused(ui_group));
        if (!chosen || !*chosen) chosen = lv_label_get_text(lv_group_get_focused(ui_group));

        if (level_is_namespace) {
            load_assign(MUOS_ASS_LOAD, rom_name, explore_dir, chosen, 0, 0);
        } else {
            load_namespace_level(chosen);
        }
    } else {
        play_sound(snd_confirm);
        dialogue_open(&assign_dlg, &theme);

        if (prefer_directory_scope) {
            for (int i = 0; i < assign_dlg.option_count; i++) {
                if (assign_dlg.option_data[i] == casn_dir) {
                    assign_dlg.selected = i;
                    dialogue_refresh(&assign_dlg, &theme);
                    break;
                }
            }
        }

        return;
    }

    remove(MUOS_SYS_LOAD);
    remove(OPTION_SKIP);

    write_text_to_file(MUOS_AIX_LOAD, "w", INT, current_item_index);

    mux_input_stop();
}

static void handle_dpad_left(void) {
    if (msgbox_active || hold_call || dialogue_active(&assign_dlg)) return;
    if (strcasecmp(rom_system, "none") == 0 || level_is_namespace) return;

    refresh_focused_runtime(-1);
}

static void handle_dpad_right(void) {
    if (msgbox_active || hold_call || dialogue_active(&assign_dlg)) return;
    if (strcasecmp(rom_system, "none") == 0 || level_is_namespace) return;

    refresh_focused_runtime(1);
}

static void handle_dpad_up(void) {
    if (dialogue_active(&assign_dlg)) {
        dialogue_handle_dpad(&assign_dlg, &theme, -1, 1);
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (dialogue_active(&assign_dlg)) {
        dialogue_handle_dpad(&assign_dlg, &theme, +1, 1);
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (dialogue_active(&assign_dlg)) {
        dialogue_handle_dpad_hold(&assign_dlg, &theme, -1, !swap_axis);
        return;
    }

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (dialogue_active(&assign_dlg)) {
        dialogue_handle_dpad_hold(&assign_dlg, &theme, +1, !swap_axis);
        return;
    }

    handle_list_nav_down_hold();
}

static void handle_page_up(void) {
    if (dialogue_active(&assign_dlg)) return;

    handle_list_nav_page_up();
}

static void handle_page_down(void) {
    if (dialogue_active(&assign_dlg)) return;

    handle_list_nav_page_down();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call || dialogue_active(&assign_dlg))
        return;

    play_sound(snd_info_open);
    show_help();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.generic.select, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

void muxassign_main(const int auto_assign, const char *name, const char *dir, const char *sys, const int app) {
    (void) app;

    snprintf(rom_dir, sizeof(rom_dir), "%s/%s", dir, name);

    is_dir = dir_exist(rom_dir) && !folder_is_content(dir, name);
    if (!is_dir) snprintf(rom_dir, sizeof(rom_dir), "%s", dir);

    prefer_directory_scope = 0;
    if (file_exist(MUOS_ASS_FROM)) {
        char *launched_from = read_all_char_from(MUOS_ASS_FROM);
        if (launched_from) {
            prefer_directory_scope =
                strcasecmp(launched_from, "history") == 0 || strcasecmp(launched_from, "collection") == 0;
            free(launched_from);
        }
    }

    snprintf(rom_name, sizeof(rom_name), "%s", get_file_name(name));
    snprintf(explore_dir, sizeof(explore_dir), "%s", dir);
    snprintf(rom_system, sizeof(rom_system), "%s", sys);

    if (!coredb_load()) LOG_ERROR(mux_module, "Assign Core could not read the core definitions");

    level_is_namespace = 0;
    if (strncmp(rom_system, ASSIGN_NAMESPACE_TAG, strlen(ASSIGN_NAMESPACE_TAG)) == 0) {
        char stripped[PATH_MAX];
        snprintf(stripped, sizeof(stripped), "%s", rom_system + strlen(ASSIGN_NAMESPACE_TAG));
        snprintf(rom_system, sizeof(rom_system), "%s", stripped);

        level_is_namespace = 1;

        struct coredb_system only;
        if (coredb_system_count(rom_system) == 1 && coredb_system_at(rom_system, 0, &only)) {
            snprintf(rom_system, sizeof(rom_system), "%s", only.id);
            level_is_namespace = 0;
        }
    }

    init_module(__func__);

    LOG_INFO(mux_module, "Assign Core explore_dir: \"%s\"", explore_dir);
    LOG_INFO(mux_module, "Assign Core ROM_NAME: \"%s\"", rom_name);
    LOG_INFO(mux_module, "Assign Core ROM_DIR: \"%s\"", rom_dir);
    LOG_INFO(mux_module, "Assign Core ROM_SYS: \"%s\"", rom_system);

    if (auto_assign && !file_exist(MUOS_SAA_LOAD)) {
        if (automatic_assign_core(rom_dir) || strcmp(rom_system, "none") == 0) return;
    }

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxassign.title);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);
    init_fonts();

    int ass_index = 0;

    if (level_is_namespace) {
        char force_sys_name[PATH_MAX] = "";
        take_forced_pick(force_sys_name, sizeof(force_sys_name));

        create_system_items(rom_system);

        ass_index = *force_sys_name ? find_system_item_index(force_sys_name) : take_stored_index();
    } else if (strcasecmp(rom_system, "none") == 0) {
        char force_sys_name[PATH_MAX] = "";
        const int force_sys_picker = take_forced_pick(force_sys_name, sizeof(force_sys_name));

        char detected_system[PATH_MAX];
        if (!force_sys_picker && find_assigned_system(detected_system)) {
            LOG_INFO(mux_module, "Detected assigned system: '%s'... skipping system picker!", detected_system);
            snprintf(rom_system, sizeof(rom_system), "%s", detected_system);
            create_core_items(rom_system);
            ass_index = find_core_item_index();
        } else {
            create_namespace_items();

            ass_index = *force_sys_name ? find_namespace_item_index(force_sys_name) : take_stored_index();
        }
    } else {
        create_core_items(rom_system);
        ass_index = find_core_item_index();
    }

    init_elements();

    dialogue_init_assign_scope(
        &assign_dlg, &theme, ui_screen, lang.muxoption.core, is_dir, 0, at_base(rom_dir, MAIN_ROM_DIR),
        lang.generic.select, lang.generic.cancel
    );

    if (ui_count_static > 0) {
        if (level_is_namespace || strcasecmp(rom_system, "none") == 0) {
            LOG_SUCCESS(mux_module, "%d System%s Detected", ui_count_static, ui_count_static == 1 ? "" : "s");
        } else {
            LOG_SUCCESS(mux_module, "%d Core%s Detected", ui_count_static, ui_count_static == 1 ? "" : "s");
        }

        if (ui_count_static > 0 && ass_index > -1 && ass_index <= ui_count_static
            && current_item_index < ui_count_static) {
            gen_step_movement(ass_index, +1, 1, 0, 1);
        }

        refresh_runtime_values();
    } else {
        LOG_ERROR(mux_module, "No Cores Detected - Check Directory!");
        lv_label_set_text(ui_lbl_screen_message, lang.muxassign.none);
    }

    init_timer(ui_gen_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_dpad_left] = handle_dpad_left,
                [mux_input_dpad_right] = handle_dpad_right,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_l1] = handle_page_up,
                [mux_input_r1] = handle_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_page_up,
            [mux_input_r1] = handle_page_down,
        }
    };

    list_nav_set_callbacks(assign_nav_prev, assign_nav_next);
    init_input(&input_opts, 1);
    orientation_introduce(mux_module, lang.muxassign.title, lang.muxassign.overview);

    mux_input_task(&input_opts);
}
