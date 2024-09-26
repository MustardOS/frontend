#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/joystick.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <libgen.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "../common/img/nothing.h"
#include "../common/common.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/ui_common.h"
#include "../common/config.h"
#include "../common/device.h"

__thread uint64_t start_ms = 0;

char *mux_prog;
static int js_fd;
static int js_fd_sys;

int NAV_DPAD_HOR;
int NAV_ANLG_HOR;
int NAV_DPAD_VER;
int NAV_ANLG_VER;
int NAV_A;
int NAV_B;

int turbo_mode = 0;
int msgbox_active = 0;
int input_disable = 0;
int SD2_found = 0;
int nav_sound = 0;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;

struct mux_config config;
struct mux_device device;
struct theme_config theme;

int nav_moved = 1;
char *current_wall = "";

lv_obj_t *msgbox_element = NULL;
lv_obj_t *wall_img = NULL;

int progress_onscreen = -1;

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;
lv_group_t *ui_group_panel;

int ui_count = 0;
int current_item_index = 0;
int first_open = 1;

void show_help() {
    char *theme_name = lv_label_get_text(lv_group_get_focused(ui_group));
    char theme_archive[MAX_BUFFER_SIZE];

    snprintf(theme_archive, sizeof(theme_archive), "%s/theme/%s.zip", STORAGE_PATH, theme_name);

    char credits[MAX_BUFFER_SIZE];
    if (extract_file_from_zip(theme_archive, "credits.txt", "/tmp/credits.txt")) {
        strcpy(credits, TS("This theme has no attributed credits!"));
    } else {
        strcpy(credits, read_text_from_file("/tmp/credits.txt"));
    }

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(lv_group_get_focused(ui_group))), TS(credits));
}

void image_refresh() {
    // Invalidate the cache for this image path
    lv_img_cache_invalidate_src(lv_img_get_src(ui_imgBox)); 

    char *theme_name = lv_label_get_text(lv_group_get_focused(ui_group));
    char theme_archive[MAX_BUFFER_SIZE];

    snprintf(theme_archive, sizeof(theme_archive), "%s/theme/%s.zip", STORAGE_PATH, theme_name);

    if (extract_file_from_zip(theme_archive, "preview.png", "/tmp/preview.png")) {
        lv_img_set_src(ui_imgBox, &ui_image_Nothing);
        return;
    }

    lv_img_set_src(ui_imgBox, "M:/tmp/preview.png");
}

void create_theme_items() {
    DIR *td;
    struct dirent *tf;

    char theme_dir[PATH_MAX];
    snprintf(theme_dir, sizeof(theme_dir), "%s/theme", STORAGE_PATH);

    td = opendir(theme_dir);
    if (td == NULL) {
        lv_obj_clear_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    char **file_names = NULL;
    size_t file_count = 0;

    while ((tf = readdir(td))) {
        if (tf->d_type == DT_REG) {
            char filename[FILENAME_MAX];
            snprintf(filename, sizeof(filename), "%s/%s", theme_dir, tf->d_name);

            char *last_dot = strrchr(tf->d_name, '.');
            if (last_dot != NULL) {
                *last_dot = '\0';
            }

            file_names = realloc(file_names, (file_count + 1) * sizeof(char *));
            file_names[file_count] = strdup(tf->d_name);
            file_count++;
        }
    }

    closedir(td);
    qsort(file_names, file_count, sizeof(char *), str_compare);

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    if (file_count > 0) {
        for (size_t i = 0; i < file_count; i++) {
            char *base_filename = file_names[i];

            ui_count++;

            lv_obj_t *ui_pnlTheme = lv_obj_create(ui_pnlContent);
            apply_theme_list_panel(&theme, &device, ui_pnlTheme);

            lv_obj_t *ui_lblThemeItem = lv_label_create(ui_pnlTheme);
            apply_theme_list_item(&theme, ui_lblThemeItem, base_filename, false, false);

            lv_obj_t *ui_lblThemeItemGlyph = lv_img_create(ui_pnlTheme);
            apply_theme_list_glyph(&theme, ui_lblThemeItemGlyph, mux_prog, "theme");

            lv_group_add_obj(ui_group, ui_lblThemeItem);
            lv_group_add_obj(ui_group_glyph, ui_lblThemeItemGlyph);
            lv_group_add_obj(ui_group_panel, ui_pnlTheme);

            apply_size_to_content(&theme, ui_pnlContent, ui_lblThemeItem, ui_lblThemeItemGlyph, base_filename);

            free(base_filename);
        }
        if (ui_count > 0) lv_obj_update_layout(ui_pnlContent);
        free(file_names);
    }
}

void list_nav_prev(int steps) {
    play_sound("navigate", nav_sound, 0);
    for (int step = 0; step < steps; ++step) {
        if (current_item_index > 0) {
            current_item_index--;
            nav_prev(ui_group, 1);
            nav_prev(ui_group_glyph, 1);
            nav_prev(ui_group_panel, 1);
        }
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    image_refresh();
    nav_moved = 1;
}

void list_nav_next(int steps) {
    if (first_open) {
        first_open = 0;
    } else {
        play_sound("navigate", nav_sound, 0);
    }
    for (int step = 0; step < steps; ++step) {
        if (current_item_index < (ui_count - 1)) {
            current_item_index++;
            nav_next(ui_group, 1);
            nav_next(ui_group_glyph, 1);
            nav_next(ui_group_panel, 1);
        }
    }
    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    image_refresh();
    nav_moved = 1;
}

void joystick_task() {
    struct input_event ev;
    int epoll_fd;
    struct epoll_event event, events[device.DEVICE.EVENT];

    int JOYUP_pressed = 0;
    int JOYDOWN_pressed = 0;
    int JOYHOTKEY_pressed = 0;
    int JOYHOTKEY_screenshot = 0;

    uint32_t nav_hold = 0; // Delay (millis) before scrolling again when up/down is held.
    uint32_t nav_tick = 0; // Clock tick (millis) when the navigation list was last scrolled.

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Error creating EPOLL instance");
        return;
    }

    event.events = EPOLLIN;
    event.data.fd = js_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, js_fd, &event) == -1) {
        perror("Error with EPOLL controller");
        return;
    }

    event.data.fd = js_fd_sys;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, js_fd_sys, &event) == -1) {
        perror("Error with EPOLL controller");
        return;
    }

    while (1) {
        int num_events = epoll_wait(epoll_fd, events, device.DEVICE.EVENT, config.SETTINGS.ADVANCED.ACCELERATE);
        if (num_events == -1) {
            perror("Error with EPOLL wait event timer");
            continue;
        }

        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == js_fd_sys) {
                ssize_t ret = read(js_fd_sys, &ev, sizeof(struct input_event));
                if (ret == -1) {
                    perror("Error reading input");
                    continue;
                }
                if (JOYHOTKEY_pressed == 1 && ev.type == EV_KEY && ev.value == 1 &&
                    (ev.code == device.RAW_INPUT.BUTTON.POWER_SHORT || ev.code == device.RAW_INPUT.BUTTON.POWER_LONG)) {
                    JOYHOTKEY_screenshot = 1;
                }
            } else if (events[i].data.fd == js_fd) {
                ssize_t ret = read(js_fd, &ev, sizeof(struct input_event));
                if (ret == -1) {
                    perror("Error reading input");
                    continue;
                }
                struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
                switch (ev.type) {
                    case EV_KEY:
                        if (ev.value == 1) {
                            if (msgbox_active) {
                                if (ev.code == NAV_B) {
                                    play_sound("confirm", nav_sound, 1);
                                    msgbox_active = 0;
                                    progress_onscreen = 0;
                                    lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
                                }
                            } else {
                                if (ev.code == device.RAW_INPUT.BUTTON.MENU_LONG) {
                                    JOYHOTKEY_pressed = 1;
                                    JOYHOTKEY_screenshot = 0;
                                } else if (ev.code == NAV_A || ev.code == NAV_B) {
                                    if (ev.code == NAV_A || ev.code == device.RAW_INPUT.ANALOG.LEFT.CLICK) {
                                        play_sound("confirm", nav_sound, 1);
                                        char *chosen_theme = lv_label_get_text(element_focused);
                                        lv_label_set_text(ui_lblMessage, TS("Loading Theme"));
                                        lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);

                                        refresh_screen();

                                        if (theme.MISC.ANIMATED_BACKGROUND == 1 && lv_obj_is_valid(wall_img))
                                            lv_obj_del(wall_img);
                                        if (theme.MISC.ANIMATED_BACKGROUND == 2) unload_image_animation();

                                        static char theme_script[MAX_BUFFER_SIZE];
                                        snprintf(theme_script, sizeof(theme_script),
                                                 "%s/script/mux/theme.sh \"%s\"", INTERNAL_PATH, chosen_theme);

                                        system(theme_script);
                                        load_mux("theme");

                                        write_text_to_file(MUOS_IDX_LOAD, "w", INT, current_item_index);
                                        current_wall = "";
                                    } else {
                                        play_sound("back", nav_sound, 1);
                                    }
                                    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "theme");
                                    return;
                                } else if (ev.code == device.RAW_INPUT.BUTTON.L1) {
                                    if (current_item_index >= 0 && current_item_index < ui_count) {
                                        list_nav_prev(theme.MUX.ITEM.COUNT);
                                    }
                                } else if (ev.code == device.RAW_INPUT.BUTTON.R1) {
                                    if (current_item_index >= 0 && current_item_index < ui_count) {
                                        list_nav_next(theme.MUX.ITEM.COUNT);
                                    }
                                }
                            }
                        } else {
                            if ((ev.code == device.RAW_INPUT.BUTTON.MENU_SHORT ||
                                 ev.code == device.RAW_INPUT.BUTTON.MENU_LONG) && !JOYHOTKEY_screenshot) {
                                JOYHOTKEY_pressed = 0;
                                if (progress_onscreen == -1) {
                                    play_sound("confirm", nav_sound, 1);
                                    show_help();
                                }
                            }
                        }
                        break;
                    case EV_ABS:
                        if (msgbox_active) {
                            break;
                        }
                        if (ev.code == NAV_DPAD_VER || ev.code == NAV_ANLG_VER) {
                            if (ev.value == -device.INPUT.AXIS || ev.value == -1) {
                                if (current_item_index == 0) {
                                    current_item_index = ui_count - 1;
                                    nav_prev(ui_group, 1);
                                    nav_prev(ui_group_glyph, 1);
                                    nav_prev(ui_group_panel, 1);
                                    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count,
                                                           current_item_index, ui_pnlContent);
                                    image_refresh();
                                } else if (current_item_index > 0) {
                                    list_nav_prev(1);
                                }
                                JOYUP_pressed = 1;
                                nav_hold = 2 * config.SETTINGS.ADVANCED.ACCELERATE;
                                nav_tick = mux_tick();
                            } else if (ev.value == device.INPUT.AXIS || ev.value == 1) {
                                if (current_item_index == ui_count - 1) {
                                    current_item_index = 0;
                                    nav_next(ui_group, 1);
                                    nav_next(ui_group_glyph, 1);
                                    nav_next(ui_group_panel, 1);
                                    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count,
                                                           current_item_index, ui_pnlContent);
                                    image_refresh();
                                } else if (current_item_index < ui_count) {
                                    list_nav_next(1);
                                }
                                JOYDOWN_pressed = 1;
                                nav_hold = 2 * config.SETTINGS.ADVANCED.ACCELERATE;
                                nav_tick = mux_tick();
                            } else {
                                JOYUP_pressed = 0;
                                JOYDOWN_pressed = 0;
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
            refresh_screen();
        }

        // Handle menu acceleration.
        if (mux_tick() - nav_tick >= nav_hold) {
            if (JOYUP_pressed && current_item_index > 0) {
                list_nav_prev(1);
                nav_hold = config.SETTINGS.ADVANCED.ACCELERATE;
                nav_tick = mux_tick();
            } else if (JOYDOWN_pressed && current_item_index < ui_count - 1) {
                list_nav_next(1);
                nav_hold = config.SETTINGS.ADVANCED.ACCELERATE;
                nav_tick = mux_tick();
            }
        }

        if (!atoi(read_line_from_file("/tmp/hdmi_in_use", 1)) || config.SETTINGS.ADVANCED.HDMIOUTPUT) {
            if (ev.type == EV_KEY && ev.value == 1 &&
                (ev.code == device.RAW_INPUT.BUTTON.VOLUME_DOWN || ev.code == device.RAW_INPUT.BUTTON.VOLUME_UP)) {
                if (JOYHOTKEY_pressed) {
                    progress_onscreen = 1;
                    lv_obj_add_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN);
                    lv_label_set_text(ui_icoProgressBrightness, "\uF185");
                    lv_bar_set_value(ui_barProgressBrightness, atoi(read_text_from_file(BRIGHT_PERC)), LV_ANIM_OFF);
                } else {
                    progress_onscreen = 2;
                    lv_obj_add_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN);
                    int volume = atoi(read_text_from_file(VOLUME_PERC));
                    switch (volume) {
                        default:
                        case 0:
                            lv_label_set_text(ui_icoProgressVolume, "\uF6A9");
                            break;
                        case 1 ... 46:
                            lv_label_set_text(ui_icoProgressVolume, "\uF026");
                            break;
                        case 47 ... 71:
                            lv_label_set_text(ui_icoProgressVolume, "\uF027");
                            break;
                        case 72 ... 100:
                            lv_label_set_text(ui_icoProgressVolume, "\uF028");
                            break;
                    }
                    lv_bar_set_value(ui_barProgressVolume, volume, LV_ANIM_OFF);
                }
            }
        }

        if (file_exist("/tmp/hdmi_do_refresh")) {
            if (atoi(read_text_from_file("/tmp/hdmi_do_refresh"))) {
                remove("/tmp/hdmi_do_refresh");
                lv_obj_invalidate(ui_pnlHeader);
                lv_obj_invalidate(ui_pnlContent);
                lv_obj_invalidate(ui_pnlFooter);
            }
        }

        refresh_screen();
    }
}

void init_elements() {
    lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
    lv_obj_move_foreground(ui_pnlFooter);
    lv_obj_move_foreground(ui_pnlHeader);
    lv_obj_move_foreground(ui_pnlHelp);
    lv_obj_move_foreground(ui_pnlProgressBrightness);
    lv_obj_move_foreground(ui_pnlProgressVolume);

    if (bar_footer) {
        lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (bar_header) {
        lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    lv_label_set_text(ui_lblPreviewHeader, "");
    lv_label_set_text(ui_lblPreviewHeaderGlyph, "");

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, osd_message);

    lv_label_set_text(ui_lblNavA, TG("Select"));
    lv_label_set_text(ui_lblNavB, TG("Back"));

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
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_FLOATING);
    }

    char *overlay = load_overlay_image();
    if (strlen(overlay) > 0 && theme.MISC.IMAGE_OVERLAY) {
        lv_obj_t *overlay_img = lv_img_create(ui_screen);
        lv_img_set_src(overlay_img, overlay);
        lv_obj_move_foreground(overlay_img);
    }

    if (TEST_IMAGE) display_testing_message(ui_screen);
}

void glyph_task() {
    // TODO: Bluetooth connectivity!
    //update_bluetooth_status(ui_staBluetooth, &theme);

    update_network_status(ui_staNetwork, &theme);
    update_battery_capacity(ui_staCapacity, &theme);

    if (progress_onscreen > 0) {
        progress_onscreen -= 1;
    } else {
        if (!lv_obj_has_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlProgressBrightness, LV_OBJ_FLAG_HIDDEN);
        }
        if (!lv_obj_has_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlProgressVolume, LV_OBJ_FLAG_HIDDEN);
        }
        if (!msgbox_active) {
            progress_onscreen = -1;
        }
    }
}

void ui_refresh_task() {
    update_bars(ui_barProgressBrightness, ui_barProgressVolume);

    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) {
            static char old_wall[MAX_BUFFER_SIZE];
            static char new_wall[MAX_BUFFER_SIZE];

            snprintf(old_wall, sizeof(old_wall), "%s", current_wall);
            snprintf(new_wall, sizeof(new_wall), "%s", load_wallpaper(
                    ui_screen, ui_group, theme.MISC.ANIMATED_BACKGROUND, theme.MISC.RANDOM_BACKGROUND));

            if (strcasecmp(new_wall, old_wall) != 0) {
                strcpy(current_wall, new_wall);
                if (strlen(new_wall) > 3) {
                    if (theme.MISC.RANDOM_BACKGROUND) {
                        load_image_random(ui_imgWall, new_wall);
                    } else {
                        switch (theme.MISC.ANIMATED_BACKGROUND) {
                            case 1:
                                lv_gif_set_src(lv_gif_create(ui_pnlWall), new_wall);
                                break;
                            case 2:
                                load_image_animation(ui_imgWall, theme.ANIMATION.ANIMATION_DELAY, new_wall);
                                break;
                            default:
                                lv_img_set_src(ui_imgWall, new_wall);
                                break;
                        }
                    }
                } else {
                    lv_img_set_src(ui_imgWall, &ui_image_Nothing);
                }
            }

            static char static_image[MAX_BUFFER_SIZE];
            snprintf(static_image, sizeof(static_image), "%s",
                     load_static_image(ui_screen, ui_group));

            if (strlen(static_image) > 0) {
                printf("LOADING STATIC IMAGE: %s\n", static_image);

                switch (theme.MISC.STATIC_ALIGNMENT) {
                    case 0: // Bottom + Front
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
                        lv_obj_move_foreground(ui_pnlBox);
                        break;
                    case 1: // Middle + Front
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_RIGHT_MID);
                        lv_obj_move_foreground(ui_pnlBox);
                        break;
                    case 2: // Top + Front
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_TOP_RIGHT);
                        lv_obj_move_foreground(ui_pnlBox);
                        break;
                    case 3: // Fullscreen + Behind
                        lv_obj_set_height(ui_pnlBox, device.MUX.HEIGHT);
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
                        lv_obj_move_background(ui_pnlBox);
                        lv_obj_move_background(ui_pnlWall);
                        break;
                    case 4: // Fullscreen + Front
                        lv_obj_set_height(ui_pnlBox, device.MUX.HEIGHT);
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
                        lv_obj_move_foreground(ui_pnlBox);
                        break;
                }

                lv_img_set_src(ui_imgBox, static_image);
            } else {
                lv_img_set_src(ui_imgBox, &ui_image_Nothing);
            }
        }
        image_refresh();
        lv_obj_invalidate(ui_pnlBox);

        nav_moved = 0;
    }
}

int main(int argc, char *argv[]) {
    (void) argc;

    mux_prog = basename(argv[0]);
    load_device(&device);


    lv_init();
    fbdev_init(device.SCREEN.DEVICE);

    static lv_disp_draw_buf_t disp_buf;
    uint32_t disp_buf_size = device.SCREEN.WIDTH * device.SCREEN.HEIGHT;

    lv_color_t * buf1 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));
    lv_color_t * buf2 = (lv_color_t *) malloc(disp_buf_size * sizeof(lv_color_t));

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, disp_buf_size);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res = device.SCREEN.WIDTH;
    disp_drv.ver_res = device.SCREEN.HEIGHT;
    disp_drv.sw_rotate = device.SCREEN.ROTATE;
    disp_drv.rotated = device.SCREEN.ROTATE;
    disp_drv.full_refresh = 0;
    disp_drv.direct_mode = 0;
    lv_disp_drv_register(&disp_drv);

    load_config(&config);
    load_theme(&theme, &config, &device, basename(argv[0]));
    load_language(mux_prog);

    ui_common_screen_init(&theme, &device, TS("THEME PICKER"));
    init_elements();
    lv_label_set_text(ui_lblScreenMessage, TS("No Themes Found"));

    lv_obj_set_user_data(ui_screen, basename(argv[0]));

    lv_label_set_text(ui_lblDatetime, get_datetime());

    switch (theme.MISC.NAVIGATION_TYPE) {
        case 1:
            NAV_DPAD_HOR = device.RAW_INPUT.DPAD.DOWN;
            NAV_ANLG_HOR = device.RAW_INPUT.ANALOG.LEFT.DOWN;
            NAV_DPAD_VER = device.RAW_INPUT.DPAD.RIGHT;
            NAV_ANLG_VER = device.RAW_INPUT.ANALOG.LEFT.RIGHT;
            break;
        default:
            NAV_DPAD_HOR = device.RAW_INPUT.DPAD.RIGHT;
            NAV_ANLG_HOR = device.RAW_INPUT.ANALOG.LEFT.RIGHT;
            NAV_DPAD_VER = device.RAW_INPUT.DPAD.DOWN;
            NAV_ANLG_VER = device.RAW_INPUT.ANALOG.LEFT.DOWN;
    }

    switch (config.SETTINGS.ADVANCED.SWAP) {
        case 1:
            NAV_A = device.RAW_INPUT.BUTTON.B;
            NAV_B = device.RAW_INPUT.BUTTON.A;
            break;
        default:
            NAV_A = device.RAW_INPUT.BUTTON.A;
            NAV_B = device.RAW_INPUT.BUTTON.B;
            break;
    }

    current_wall = load_wallpaper(ui_screen, NULL, theme.MISC.ANIMATED_BACKGROUND, theme.MISC.RANDOM_BACKGROUND);
    if (strlen(current_wall) > 3) {
        if (theme.MISC.RANDOM_BACKGROUND) {
            load_image_random(ui_imgWall, current_wall);
        } else {
            switch (theme.MISC.ANIMATED_BACKGROUND) {
                case 1:
                    lv_gif_set_src(lv_gif_create(ui_pnlWall), current_wall);
                    break;
                case 2:
                    load_image_animation(ui_imgWall, theme.ANIMATION.ANIMATION_DELAY, current_wall);
                    break;
                default:
                    lv_img_set_src(ui_imgWall, current_wall);
                    break;
            }
        }
    } else {
        lv_img_set_src(ui_imgWall, &ui_image_Nothing);
    }

    load_font_text(basename(argv[0]), ui_screen);
    load_font_section(basename(argv[0]), FONT_PANEL_FOLDER, ui_pnlContent);
    load_font_section(mux_prog, FONT_HEADER_FOLDER, ui_pnlHeader);
    load_font_section(mux_prog, FONT_FOOTER_FOLDER, ui_pnlFooter);

    if (config.SETTINGS.GENERAL.SOUND) {
        if (SDL_Init(SDL_INIT_AUDIO) >= 0) {
            Mix_Init(0);
            Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
            printf("SDL init success!\n");
            nav_sound = 1;
        } else {
            fprintf(stderr, "Failed to init SDL\n");
        }
    }

    create_theme_items();

    int sys_index = 0;
    if (file_exist(MUOS_IDX_LOAD)) {
        sys_index = atoi(read_line_from_file(MUOS_IDX_LOAD, 1));
        remove(MUOS_IDX_LOAD);
    }

    struct dt_task_param dt_par;
    struct bat_task_param bat_par;

    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;

    js_fd = open(device.INPUT.EV1, O_RDONLY);
    if (js_fd < 0) {
        perror("Failed to open joystick device");
        return 1;
    }

    js_fd_sys = open(device.INPUT.EV0, O_RDONLY);
    if (js_fd_sys < 0) {
        perror("Failed to open joystick device");
        return 1;
    }

    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);

    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = evdev_read;
    indev_drv.user_data = (void *) (intptr_t) js_fd;

    lv_indev_drv_register(&indev_drv);

    lv_timer_t *datetime_timer = lv_timer_create(datetime_task, UINT16_MAX / 2, &dt_par);
    lv_timer_ready(datetime_timer);

    lv_timer_t *capacity_timer = lv_timer_create(capacity_task, UINT16_MAX / 2, &bat_par);
    lv_timer_ready(capacity_timer);

    lv_timer_t *glyph_timer = lv_timer_create(glyph_task, UINT16_MAX / 64, NULL);
    lv_timer_ready(glyph_timer);

    lv_timer_t *ui_refresh_timer = lv_timer_create(ui_refresh_task, UINT8_MAX / 4, NULL);
    lv_timer_ready(ui_refresh_timer);

    if (ui_count > 0) {
        if (sys_index > -1 && sys_index <= ui_count && current_item_index < ui_count) {
            list_nav_next(sys_index);
        }
    } else {
        lv_obj_clear_flag(ui_lblScreenMessage, LV_OBJ_FLAG_HIDDEN);
    }

    refresh_screen();
    joystick_task();

    close(js_fd);
    close(js_fd_sys);

    return 0;
}

uint32_t mux_tick(void) {
    struct timespec tv_now;
    clock_gettime(CLOCK_REALTIME, &tv_now);

    uint64_t now_ms = ((uint64_t) tv_now.tv_sec * 1000) + (tv_now.tv_nsec / 1000000);
    start_ms = start_ms || now_ms;

    return (uint32_t) (now_ms - start_ms);
}
