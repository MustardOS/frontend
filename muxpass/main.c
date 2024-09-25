#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
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
#include "../common/passcode.h"
#include "ui/theme.h"

__thread uint64_t start_ms = 0;

char *mux_prog;
static int js_fd;

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
struct mux_passcode passcode;

char *current_wall = "";

char *p_type;
char *p_code;
char *p_msg;

lv_obj_t *msgbox_element = NULL;

lv_group_t *ui_group;

#define UI_COUNT 6
lv_obj_t *ui_objects[UI_COUNT];

void init_navigation_groups() {
    ui_objects[0] = ui_rolComboOne;
    ui_objects[1] = ui_rolComboTwo;
    ui_objects[2] = ui_rolComboThree;
    ui_objects[3] = ui_rolComboFour;
    ui_objects[4] = ui_rolComboFive;
    ui_objects[5] = ui_rolComboSix;

    ui_group = lv_group_create();

    for (unsigned int i = 0; i < sizeof(ui_objects) / sizeof(ui_objects[0]); i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
    }
}

int joystick_task() {
    struct input_event ev;
    int epoll_fd;
    struct epoll_event event, events[device.DEVICE.EVENT];

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Error creating EPOLL instance");
        return 2;
    }

    event.events = EPOLLIN;
    event.data.fd = js_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, js_fd, &event) == -1) {
        perror("Error with EPOLL controller");
        return 2;
    }

    while (1) {
        int num_events = epoll_wait(epoll_fd, events, device.DEVICE.EVENT, 64);
        if (num_events == -1) {
            perror("Error with EPOLL wait event timer");
            continue;
        }

        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == js_fd) {
                ssize_t ret = read(js_fd, &ev, sizeof(struct input_event));
                if (ret == -1) {
                    perror("Error reading input");
                    continue;
                }

                struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
                switch (ev.type) {
                    case EV_KEY:
                        if (ev.value == 1) {
                            if (ev.code == NAV_A || ev.code == device.RAW_INPUT.ANALOG.LEFT.CLICK) {
                                char b1[2], b2[2], b3[2], b4[2], b5[2], b6[2];
                                uint32_t bs = sizeof(b1);

                                lv_roller_get_selected_str(ui_rolComboOne, b1, bs);
                                lv_roller_get_selected_str(ui_rolComboTwo, b2, bs);
                                lv_roller_get_selected_str(ui_rolComboThree, b3, bs);
                                lv_roller_get_selected_str(ui_rolComboFour, b4, bs);
                                lv_roller_get_selected_str(ui_rolComboFive, b5, bs);
                                lv_roller_get_selected_str(ui_rolComboSix, b6, bs);

                                char try_code[13];
                                sprintf(try_code, "%s%s%s%s%s%s", b1, b2, b3, b4, b5, b6);

                                if (strcasecmp(try_code, p_code) == 0) {
                                    return 1;
                                }
                            } else if (ev.code == NAV_B) {
                                return 2;
                            }
                        }
                        break;
                    case EV_ABS:
                        if (ev.code == NAV_DPAD_HOR || ev.code == NAV_ANLG_HOR) {
                            if (ev.value == -device.INPUT.AXIS || ev.value == -1) {
                                play_sound("navigate", nav_sound, 0);
                                nav_prev(ui_group, 1);
                            } else if (ev.value == device.INPUT.AXIS || ev.value == 1) {
                                play_sound("navigate", nav_sound, 0);
                                nav_next(ui_group, 1);
                            }
                        }
                        if (ev.code == NAV_DPAD_VER || ev.code == NAV_ANLG_VER) {
                            if (ev.value == -device.INPUT.AXIS || ev.value == -1) {
                                play_sound("navigate", nav_sound, 0);
                                lv_roller_set_selected(element_focused,
                                                       lv_roller_get_selected(element_focused) - 1,
                                                       LV_ANIM_ON);
                            } else if (ev.value == device.INPUT.AXIS || ev.value == 1) {
                                play_sound("navigate", nav_sound, 0);
                                lv_roller_set_selected(element_focused,
                                                       lv_roller_get_selected(element_focused) + 1,
                                                       LV_ANIM_ON);
                            }
                        }
                        break;
                    default:
                        break;
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
    lv_obj_move_foreground(ui_pnlFooter);
    lv_obj_move_foreground(ui_pnlHeader);

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
            ui_lblNavMenu,
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
}

int main(int argc, char *argv[]) {
    mux_prog = basename(argv[0]);
    load_device(&device);


    char *cmd_help = "\nmuOS Extras - Passcode\nUsage: %s <-t>\n\nOptions:\n"
                     "\t-t Type of passcode lock <boot|launch|setting>\n\n";

    int opt;
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
            case 't':
                p_type = optarg;
                break;
            default:
                fprintf(stderr, cmd_help, argv[0]);
                return 1;
        }
    }

    if (p_type == NULL) {
        fprintf(stderr, cmd_help, argv[0]);
        return 1;
    }

    load_passcode(&passcode, &device);

    if (strcasecmp(p_type, "boot") == 0) {
        p_code = passcode.CODE.BOOT;
        p_msg = passcode.MESSAGE.BOOT;
    } else if (strcasecmp(p_type, "launch") == 0) {
        p_code = passcode.CODE.LAUNCH;
        p_msg = passcode.MESSAGE.LAUNCH;
    } else if (strcasecmp(p_type, "setting") == 0) {
        p_code = passcode.CODE.SETTING;
        p_msg = passcode.MESSAGE.SETTING;
    } else {
        fprintf(stderr, cmd_help, argv[0]);
        return 1;
    }

    if (strcasecmp(p_code, "000000") == 0) return 1;

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

    ui_common_screen_init(&theme, &device, TS("PASSCODE"));
    ui_init(ui_pnlContent);
    init_elements();

    if (strlen(p_msg) > 1) {
        lv_label_set_text(ui_lblMessage, p_msg);
        lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_set_user_data(ui_screen, basename(argv[0]));

    lv_label_set_text(ui_lblDatetime, get_datetime());

    apply_theme();

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

    init_navigation_groups();

    NAV_DPAD_HOR = device.RAW_INPUT.DPAD.RIGHT;
    NAV_ANLG_HOR = device.RAW_INPUT.ANALOG.LEFT.RIGHT;
    NAV_DPAD_VER = device.RAW_INPUT.DPAD.DOWN;
    NAV_ANLG_VER = device.RAW_INPUT.ANALOG.LEFT.DOWN;

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

    struct dt_task_param dt_par;
    struct bat_task_param bat_par;

    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;

    js_fd = open(device.INPUT.EV1, O_RDONLY);
    if (js_fd < 0) {
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

    int safe_quit = joystick_task();

    close(js_fd);

    return safe_quit;
}

uint32_t mux_tick(void) {
    struct timespec tv_now;
    clock_gettime(CLOCK_REALTIME, &tv_now);

    uint64_t now_ms = ((uint64_t) tv_now.tv_sec * 1000) + (tv_now.tv_nsec / 1000000);
    start_ms = start_ms || now_ms;

    return (uint32_t) (now_ms - start_ms);
}
