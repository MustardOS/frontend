#include "../lvgl/lvgl.h"
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/language.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/collection.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"
#include "../common/input.h"
#include "../common/input/list_nav.h"

char *mux_module;

static int joy_general;
static int joy_power;
static int joy_volume;
static int joy_extra;

int turbo_mode = 0;
int msgbox_active = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;

struct mux_lang lang;
struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int nav_moved = 1;
lv_obj_t *msgbox_element = NULL;
lv_obj_t *overlay_image = NULL;
lv_obj_t *kiosk_image = NULL;

int progress_onscreen = -1;

size_t item_count = 0;
content_item *items = NULL;

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

lv_group_t *ui_group_installed;

int ui_count = 0;
int current_item_index = 0;
int first_open = 1;

lv_obj_t *ui_mux_panels[5];

void show_help() {
    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     lang.MUXARCHIVE.TITLE, lang.MUXARCHIVE.HELP);
}

void create_archive_items() {
    const char *mount_points[] = {
            device.STORAGE.ROM.MOUNT,
            device.STORAGE.SDCARD.MOUNT,
            device.STORAGE.USB.MOUNT
    };

    const char *subdirs[] = {"/muos/update", "/backup", "/archive"};
    char archive_directories[9][MAX_BUFFER_SIZE];

    for (int i = 0, k = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j, ++k) {
            snprintf(archive_directories[k], sizeof(archive_directories[k]), "%s%s", mount_points[i], subdirs[j]);
        }
    }

    char **file_names = NULL;
    size_t file_count = 0;

    const char *ext_map[][2] = {
            {".muxupd", "UPD"},
            {".muxapp", "APP"},
            {".muxzip", "ZIP"},
            {".muxthm", "THM"},
            {".muxcat", "CAT"},
            {".muxcfg", "CFG"}
    };

    for (size_t dir_index = 0; dir_index < sizeof(archive_directories) / sizeof(archive_directories[0]); ++dir_index) {
        DIR *ad = opendir(archive_directories[dir_index]);
        if (!ad) continue;

        struct dirent *af;
        while ((af = readdir(ad))) {
            if (af->d_type == DT_REG) {
                const char *last_dot = strrchr(af->d_name, '.');
                if (!last_dot) continue;

                const char *prefix = NULL;
                for (size_t i = 0; i < sizeof(ext_map) / sizeof(ext_map[0]); ++i) {
                    if (strcasecmp(last_dot, ext_map[i][0]) == 0) {
                        prefix = ext_map[i][1];
                        break;
                    }
                }

                if (!prefix) continue;

                char base_name[MAX_BUFFER_SIZE];
                strncpy(base_name, af->d_name, last_dot - af->d_name);
                base_name[last_dot - af->d_name] = '\0';

                char full_app_name[MAX_BUFFER_SIZE];
                snprintf(full_app_name, sizeof(full_app_name), "%s/%s", archive_directories[dir_index], af->d_name);

                char **temp = realloc(file_names, (file_count + 1) * sizeof(char *));
                if (!temp) {
                    perror(lang.SYSTEM.FAIL_ALLOCATE_MEM);
                    free(file_names);
                    closedir(ad);
                    return;
                }

                file_names = temp;
                file_names[file_count] = strdup(full_app_name);
                if (!file_names[file_count++]) {
                    perror(lang.SYSTEM.FAIL_DUP_STRING);
                    free(file_names);
                    closedir(ad);
                    return;
                }
            }
        }
        closedir(ad);
    }

    if (!file_names) return;
    qsort(file_names, file_count, sizeof(char *), str_compare);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();
    ui_group_installed = lv_group_create();

    for (size_t i = 0; i < file_count; i++) {
        char *base_filename = file_names[i];
        if (!file_names[i]) continue;

        const char *prefix = NULL;
        for (size_t j = 0; j < sizeof(mount_points) / sizeof(mount_points[0]); ++j) {
            if (strstr(base_filename, mount_points[j])) {
                static char storage_prefix[MAX_BUFFER_SIZE];

                const char *file_extension = strrchr(base_filename, '.');
                const char *ext_type = NULL;
                if (file_extension) {
                    for (size_t i = 0; i < sizeof(ext_map) / sizeof(ext_map[0]); ++i) {
                        if (strcasecmp(file_extension, ext_map[i][0]) == 0) {
                            ext_type = ext_map[i][1];
                            break;
                        }
                    }
                }
                if (!ext_type) ext_type = "UNK";

                snprintf(storage_prefix, sizeof(storage_prefix), "[%s-%s]",
                         j == 0 ? "SD1" : (j == 1 ? "SD2" : "USB"), ext_type);
                prefix = storage_prefix;
                break;
            }
        }

        if (!prefix) continue;

        static char archive_name[MAX_BUFFER_SIZE];
        snprintf(archive_name, sizeof(archive_name), "%s",
                 str_remchar(str_replace(base_filename, strip_dir(base_filename), ""), '/'));

        char install_check[MAX_BUFFER_SIZE];
        snprintf(install_check, sizeof(install_check), "%s/muos/update/installed/%s.done",
                 mount_points[0], archive_name);

        int is_installed = file_exist(install_check);

        char archive_store[MAX_BUFFER_SIZE];
        snprintf(archive_store, sizeof(archive_store), "%s %s", prefix, archive_name);

        char item_glyph[MAX_BUFFER_SIZE];
        snprintf(item_glyph, sizeof(item_glyph), "%s", is_installed ? "installed" : "archive");

        ui_count++;

        add_item(&items, &item_count, base_filename, archive_store, item_glyph, ROM);

        lv_obj_t *ui_pnlArchive = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlArchive);

        lv_obj_t *ui_lblArchiveItem = lv_label_create(ui_pnlArchive);
        apply_theme_list_item(&theme, ui_lblArchiveItem, strip_ext(archive_store));

        lv_obj_t *ui_lblArchiveItemInstalled = lv_label_create(ui_pnlArchive);
        apply_theme_list_value(&theme, ui_lblArchiveItemInstalled, is_installed ? lang.MUXARCHIVE.INSTALLED : "");

        lv_obj_t *ui_lblArchiveItemGlyph = lv_img_create(ui_pnlArchive);
        apply_theme_list_glyph(&theme, ui_lblArchiveItemGlyph, mux_module, items[i].extra_data);

        lv_group_add_obj(ui_group, ui_lblArchiveItem);
        lv_group_add_obj(ui_group_glyph, ui_lblArchiveItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlArchive);
        lv_group_add_obj(ui_group_installed, ui_lblArchiveItemInstalled);

        free(base_filename);
    }

    if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
    free(file_names);
}

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0, 0);
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        nav_prev(ui_group, 1);
        nav_prev(ui_group_glyph, 1);
        nav_prev(ui_group_panel, 1);
        nav_prev(ui_group_installed, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

void list_nav_next(int steps) {
    if (first_open) {
        first_open = 0;
    } else {
        play_sound("navigate", nav_sound, 0, 0);
    }
    for (int step = 0; step < steps; ++step) {
        current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        nav_next(ui_group, 1);
        nav_next(ui_group_glyph, 1);
        nav_next(ui_group_panel, 1);
        nav_next(ui_group_installed, 1);
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

void handle_a() {
    if (msgbox_active) return;

    if (ui_count > 0) {
        play_sound("confirm", nav_sound, 0, 1);

        static char extract_script[MAX_BUFFER_SIZE];
        snprintf(extract_script, sizeof(extract_script),
                 "%s/script/mux/extract.sh", INTERNAL_PATH);

        const char *args[] = {
                (INTERNAL_PATH "bin/fbpad"),
                "-bg", (char *) theme.TERMINAL.BACKGROUND,
                "-fg", (char *) theme.TERMINAL.FOREGROUND,
                extract_script,
                items[current_item_index].name,
                NULL
        };

        setenv("TERM", "xterm-256color", 1);

        if (config.VISUAL.BLACKFADE) {
            fade_to_black(ui_screen);
        } else {
            unload_image_animation();
        }

        run_exec(args);

        write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);

        load_mux("archive");
        mux_input_stop();
    }
}

void handle_b() {
    if (msgbox_active) {
        play_sound("confirm", nav_sound, 0, 0);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound("back", nav_sound, 0, 1);
    mux_input_stop();
}

void handle_menu() {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound("confirm", nav_sound, 0, 0);
        show_help();
    }
}

void init_elements() {
    ui_mux_panels[0] = ui_pnlFooter;
    ui_mux_panels[1] = ui_pnlHeader;
    ui_mux_panels[2] = ui_pnlHelp;
    ui_mux_panels[3] = ui_pnlProgressBrightness;
    ui_mux_panels[4] = ui_pnlProgressVolume;

    adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

    if (bar_footer) lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (bar_header) lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, "");

    lv_label_set_text(ui_lblNavA, lang.GENERIC.EXTRACT);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.BACK);

    lv_obj_t *nav_hide[] = {
            ui_lblNavCGlyph,
            ui_lblNavC,
            ui_lblNavXGlyph,
            ui_lblNavX,
            ui_lblNavYGlyph,
            ui_lblNavY,
            ui_lblNavZGlyph,
            ui_lblNavZ,
            ui_lblNavMenuGlyph,
            ui_lblNavMenu
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
    }

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    kiosk_image = lv_img_create(ui_screen);
    load_kiosk_image(ui_screen, kiosk_image);

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image, theme.MISC.IMAGE_OVERLAY);
}

void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume, ui_icoProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) {
            struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
            lv_obj_set_user_data(element_focused, get_last_subdir(strip_ext(items[current_item_index].name), '/', 4));

            adjust_wallpaper_element(ui_group, 0, ARCHIVE);
        }
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int main(int argc, char *argv[]) {
    (void) argc;

    mux_module = basename(argv[0]);
    setup_background_process();

    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    init_display();
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXARCHIVE.TITLE);
    init_timer(ui_refresh_task, NULL);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());
    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, ARCHIVE);

    init_fonts();
    create_archive_items();
    init_navigation_sound(&nav_sound, mux_module);

    int arc_index = 0;
    if (file_exist(MUOS_IDX_LOAD)) {
        arc_index = read_int_from_file(MUOS_IDX_LOAD, 1);
        remove(MUOS_IDX_LOAD);
    }

    init_input(&joy_general, &joy_power, &joy_volume, &joy_extra);

    int nav_hidden = 0;
    if (ui_count > 0) {
        nav_hidden = 1;
        if (arc_index > -1 && arc_index <= ui_count && current_item_index < ui_count) list_nav_next(arc_index);
    } else {
        lv_label_set_text(ui_lblScreenMessage, lang.MUXARCHIVE.NONE);
    }

    struct nav_flag nav_e[] = {
            {ui_lblNavA,      nav_hidden},
            {ui_lblNavAGlyph, nav_hidden}
    };
    set_nav_flags(nav_e, sizeof(nav_e) / sizeof(nav_e[0]));

    load_kiosk(&kiosk);

    mux_input_options input_opts = {
            .general_fd = joy_general,
            .power_fd = joy_power,
            .volume_fd = joy_volume,
            .extra_fd = joy_extra,
            .max_idle_ms = IDLE_MS,
            .swap_btn = config.SETTINGS.ADVANCED.SWAP,
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .stick_nav = true,
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_MENU_SHORT] = handle_menu,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .combo = {
                    COMBO_BRIGHT(BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_UP)),
                    COMBO_BRIGHT(BIT(MUX_INPUT_MENU_LONG) | BIT(MUX_INPUT_VOL_DOWN)),
                    COMBO_VOLUME(BIT(MUX_INPUT_VOL_UP)),
                    COMBO_VOLUME(BIT(MUX_INPUT_VOL_DOWN)),
            },
            .idle_handler = ui_common_handle_idle,
    };
    mux_input_task(&input_opts);
    safe_quit(0);

    close(joy_general);
    close(joy_power);
    close(joy_volume);
    close(joy_extra);

    return 0;
}
