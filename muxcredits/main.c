#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/joystick.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <libgen.h>
#include "../common/common.h"
#include "../common/help.h"
#include "../common/options.h"
#include "../common/theme.h"
#include "../common/config.h"
#include "../common/glyph.h"
#include "../common/mini/mini.h"

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
int safe_quit = 0;
int bar_header = 0;
int bar_footer = 0;
char *osd_message;
struct mux_config config;

int nav_moved = 1;
char *current_wall = "";

// Place as many NULL as there are options!
lv_obj_t *labels[] = {};
unsigned int label_count = sizeof(labels) / sizeof(labels[0]);

lv_obj_t *msgbox_element = NULL;

int progress_onscreen = -1;

lv_group_t *ui_group;
lv_group_t *ui_group_glyph;

int ui_count = 0;
int current_item_index = 0;
int first_open = 1;
int content_panel_y = 0;

typedef struct {
    char user[MAX_BUFFER_SIZE];
    char quote[MAX_BUFFER_SIZE];
} kv;

typedef struct {
    kv *data;
    size_t size;
} kv_storage;

kv user_quote[] = {
        {"04wiwe",           ""},
        {"acmeplus",         ""},
        {"antikk",           "Hey, I'm just happy to be here."},
        {"applechipper",     ""},
        {"arobee",           "Where's the money, Lebowski?"},
        {"bazkart",          "By the people, for the people!"},
        {"benzo1985",        ""},
        {"brites2508",       "Proof that nature is wise is that she didn't even know we would wear glasses and notice how she placed our ears."},
        {"brohsnbluffs",     ""},
        {"btreecat",         ""},
        {"cebion",           ""},
        {"combustify",       ""},
        {"dabom123",         ""},
        {"defensemechanism", "I'm a huge gamer most of the time"},
        {"ee1000",           ""},
        {"fifth8241",        "If my calculations are correct, when this baby hits 88 miles per hour, you're gonna see some serious shit."},
        {"ilfordhp5",        "As it happens, I've worked on three CFWs and am a dev on the ChimeraOS project. Fell free to bash me if it makes you feel better. I don't actually care."},
        {"intelliaim",       "Mind the gap!"},
        {"irilivibi",        ""},
        {"joyrider3774",     "i have no such things lol"},
        {"jupyter",          "if i could, i would help thits dumpster fire of an os (no offence) (edited)\n"},
        {"kardus",           "h"},
        {"kiko_lake",        ""},
        {"kloptops",         "How about dem apples"},
        {"koolkidkorey",     "But why no mupen64plus-next?"},
        {"lasereverything",  "Sleep when you're dead!"},
        {"mrminholi",        ""},
        {"phyrex",           ""},
        {"qpla",             "Contrary to popular belief, James Cameron did not raze the entirety of downtown Los Angeles for the Terminator 2 nuke scene"},
        {"reapdg",           ""},
        {"robbiet480",       ""},
        {"sethg911",         ""},
        {"shengy",           ""},
        {"sidnt",            ""},
        {"siliconexarch",    ""},
        {"skyarcher",        "Play keeps the world young as nothing else can. We do not so much quit playing because we grow old, as grow old because we quit playing."},
        {"tokyovigilante",   ""},
        {"tsuica",           "Other than the fast boot, general performance, lots of features, ease of use, ports, save syncing, and 64-bit, what has muOS ever done for us?"},
        {"vagueparade",      "In your hands isn't the real thing... it's BETTER! This community is so lovely and full of smart fellows, thank you to everyone and you for supporting awesome people - Tom"},
        {"x_tremis",         "\"A mind forever Voyaging through strange seas of Thought, alone.\" - William Wordsworth"},
        {"xonglebongle",     "1) Develop Stuff\n2) Break Stuff\n3) Cry\n4) Fix Stuff\n5) Notice something else is broken\n6) Fix that too!\n7) Start over"},
        {"xquader",          ""},
        {"youraveragelord",  ""},
        {"zadeis",           ""}
};

kv_storage kv_sto = {
        .data = user_quote,
        .size = sizeof(user_quote) / sizeof(kv)
};

const char *get_quote(const kv_storage *kv_sto, const char *user) {
    for (size_t i = 0; i < kv_sto->size; ++i) {
        if (strcasecmp(kv_sto->data[i].user, user) == 0) {
            return kv_sto->data[i].quote;
        }
    }
    return "";
}

void show_help() {
    char *title = lv_label_get_text(lv_group_get_focused(ui_group));
    char *message;

    const char *quote = get_quote(&kv_sto, title);

    if (strlen(quote) <= 1) {
        return;
    } else {
        message = malloc(strlen(quote) + 1);
        if (message == NULL) {
            return;
        }
        strcpy(message, quote);
    }

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent, title, message);
    free(message);
}

void create_credit_items() {
    char **keys = NULL;
    size_t key_count = 0;

    for (size_t i = 0; i < kv_sto.size; ++i) {
        keys = realloc(keys, (key_count + 1) * sizeof(char *));
        keys[key_count] = strdup(kv_sto.data[i].user);
        key_count++;
    }

    ui_group = lv_group_create();
    ui_group_glyph = lv_group_create();

    for (size_t i = 0; i < key_count; i++) {
        char *base_key = keys[i];

        ui_count++;

        lv_obj_t * ui_pnlCredit = lv_obj_create(ui_pnlContent);
        lv_obj_set_width(ui_pnlCredit, 640);
        lv_obj_set_height(ui_pnlCredit, 28);
        lv_obj_set_scrollbar_mode(ui_pnlCredit, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_align(ui_pnlCredit, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_pnlCredit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(ui_pnlCredit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_left(ui_pnlCredit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(ui_pnlCredit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(ui_pnlCredit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(ui_pnlCredit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_row(ui_pnlCredit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_column(ui_pnlCredit, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t * ui_lblCreditItem = lv_label_create(ui_pnlCredit);
        lv_label_set_text(ui_lblCreditItem, kv_sto.data[i].user);

        lv_obj_set_width(ui_lblCreditItem, 640);
        lv_obj_set_height(ui_lblCreditItem, 28);

        lv_obj_set_style_border_width(ui_lblCreditItem, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_side(ui_lblCreditItem, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_color(ui_lblCreditItem, lv_color_hex(theme.SYSTEM.BACKGROUND),
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_main_stop(ui_lblCreditItem, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_dir(ui_lblCreditItem, LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_bg_color(ui_lblCreditItem, lv_color_hex(theme.LIST_DEFAULT.BACKGROUND),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(ui_lblCreditItem, theme.LIST_DEFAULT.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_main_stop(ui_lblCreditItem, theme.LIST_DEFAULT.GRADIENT_START,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_stop(ui_lblCreditItem, theme.LIST_DEFAULT.GRADIENT_STOP,
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(ui_lblCreditItem, lv_color_hex(theme.LIST_DEFAULT.INDICATOR),
                                      LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(ui_lblCreditItem, theme.LIST_DEFAULT.INDICATOR_ALPHA,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_lblCreditItem, lv_color_hex(theme.LIST_DEFAULT.TEXT),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_lblCreditItem, theme.LIST_DEFAULT.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_bg_color(ui_lblCreditItem, lv_color_hex(theme.LIST_FOCUS.BACKGROUND),
                                  LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_opa(ui_lblCreditItem, theme.LIST_FOCUS.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_main_stop(ui_lblCreditItem, theme.LIST_FOCUS.GRADIENT_START,
                                      LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_grad_stop(ui_lblCreditItem, theme.LIST_FOCUS.GRADIENT_STOP,
                                      LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_border_color(ui_lblCreditItem, lv_color_hex(theme.LIST_FOCUS.INDICATOR),
                                      LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_border_opa(ui_lblCreditItem, theme.LIST_FOCUS.INDICATOR_ALPHA,
                                    LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(ui_lblCreditItem, lv_color_hex(theme.LIST_FOCUS.TEXT),
                                    LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_text_opa(ui_lblCreditItem, theme.LIST_FOCUS.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);

        lv_obj_set_style_pad_left(ui_lblCreditItem, 32, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(ui_lblCreditItem, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(ui_lblCreditItem, theme.FONT.LIST_PAD_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(ui_lblCreditItem, theme.FONT.LIST_PAD_BOTTOM, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_text_line_space(ui_lblCreditItem, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_long_mode(ui_lblCreditItem, LV_LABEL_LONG_WRAP);

        lv_obj_t * ui_lblCreditItemGlyph = lv_label_create(ui_pnlCredit);

        if (strcmp(kv_sto.data[i].user, "xonglebongle") == 0) {
            lv_label_set_text(ui_lblCreditItemGlyph, "\uF43F");
        } else {
            lv_label_set_text(ui_lblCreditItemGlyph, "\uF441");
        }

        lv_obj_set_width(ui_lblCreditItemGlyph, 640);
        lv_obj_set_height(ui_lblCreditItemGlyph, 28);

        lv_obj_set_style_border_width(ui_lblCreditItemGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_bg_opa(ui_lblCreditItemGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(ui_lblCreditItemGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ui_lblCreditItemGlyph, lv_color_hex(theme.LIST_DEFAULT.TEXT),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(ui_lblCreditItemGlyph, theme.LIST_DEFAULT.TEXT_ALPHA,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_bg_opa(ui_lblCreditItemGlyph, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_border_opa(ui_lblCreditItemGlyph, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(ui_lblCreditItemGlyph, lv_color_hex(theme.LIST_FOCUS.TEXT),
                                    LV_PART_MAIN | LV_STATE_FOCUSED);
        lv_obj_set_style_text_opa(ui_lblCreditItemGlyph, theme.LIST_FOCUS.TEXT_ALPHA, LV_PART_MAIN | LV_STATE_FOCUSED);

        lv_obj_set_style_pad_left(ui_lblCreditItemGlyph, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_right(ui_lblCreditItemGlyph, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(ui_lblCreditItemGlyph, theme.FONT.LIST_ICON_PAD_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_bottom(ui_lblCreditItemGlyph, theme.FONT.LIST_ICON_PAD_BOTTOM,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_set_style_text_align(ui_lblCreditItemGlyph, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(ui_lblCreditItemGlyph, &ui_font_AwesomeSmall, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_group_add_obj(ui_group, ui_lblCreditItem);
        lv_group_add_obj(ui_group_glyph, ui_lblCreditItemGlyph);

        free(base_key);
    }

    free(keys);
}

void list_nav_prev(int steps) {
    for (int step = 0; step < steps; ++step) {
        if (current_item_index >= 1 && ui_count > 13) {
            current_item_index--;
            nav_prev(ui_group, 1);
            nav_prev(ui_group_glyph, 1);
            if (current_item_index > 5 && current_item_index < (ui_count - 7)) {
                content_panel_y -= 30;
                lv_obj_scroll_to_y(ui_pnlContent, content_panel_y, LV_ANIM_OFF);
            }
        } else if (current_item_index >= 0 && ui_count <= 13) {
            if (current_item_index > 0) {
                current_item_index--;
                nav_prev(ui_group, 1);
                nav_prev(ui_group_glyph, 1);
            }
        }
    }

    play_sound("navigate", nav_sound);
    nav_moved = 1;
}

void list_nav_next(int steps) {
    for (int step = 0; step < steps; ++step) {
        if (current_item_index < (ui_count - 1) && ui_count > 13) {
            if (current_item_index < (ui_count - 1)) {
                current_item_index++;
                nav_next(ui_group, 1);
                nav_next(ui_group_glyph, 1);
                if (current_item_index >= 7 && current_item_index < (ui_count - 6)) {
                    content_panel_y += 30;
                    lv_obj_scroll_to_y(ui_pnlContent, content_panel_y, LV_ANIM_OFF);
                }
            }
        } else if (current_item_index < ui_count && ui_count <= 13) {
            if (current_item_index < (ui_count - 1)) {
                current_item_index++;
                nav_next(ui_group, 1);
                nav_next(ui_group_glyph, 1);
            }
        }
    }

    if (first_open) {
        first_open = 0;
    } else {
        play_sound("navigate", nav_sound);
    }
    nav_moved = 1;
}

void *joystick_task() {
    struct input_event ev;
    int epoll_fd;
    struct epoll_event event, events[MAX_EVENTS];

    int JOYUP_pressed = 0;
    int JOYDOWN_pressed = 0;
    int JOYHOTKEY_pressed = 0;

    int nav_hold = 0;
    int nav_delay = UINT8_MAX;

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Error creating EPOLL instance");
        return NULL;
    }

    event.events = EPOLLIN;
    event.data.fd = js_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, js_fd, &event) == -1) {
        perror("Error with EPOLL controller");
        return NULL;
    }

    while (1) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, nav_delay);
        if (num_events == -1) {
            perror("Error with EPOLL wait event timer");
            continue;
        }

        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == js_fd) {
                int ret = read(js_fd, &ev, sizeof(struct input_event));
                if (ret == -1) {
                    perror("Error reading input");
                    continue;
                }

                switch (ev.type) {
                    case EV_KEY:
                        if (ev.value == 1) {
                            if (msgbox_active) {
                                if (ev.code == NAV_B || ev.code == JOY_MENU) {
                                    play_sound("confirm", nav_sound);
                                    msgbox_active = 0;
                                    progress_onscreen = 0;
                                    lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
                                }
                            } else {
                                if (ev.code == JOY_MENU) {
                                    JOYHOTKEY_pressed = 1;
                                } else if (ev.code == NAV_B) {
                                    play_sound("back", nav_sound);
                                    safe_quit = 1;
                                } else if (ev.code == JOY_L1) {
                                    if (current_item_index >= 0 && current_item_index < ui_count) {
                                        list_nav_prev(ITEM_SKIP);
                                        lv_task_handler();
                                    }
                                } else if (ev.code == JOY_R1) {
                                    if (current_item_index >= 0 && current_item_index < ui_count) {
                                        list_nav_next(ITEM_SKIP);
                                        lv_task_handler();
                                    }
                                }
                            }
                        } else {
                            if (ev.code == JOY_MENU) {
                                JOYHOTKEY_pressed = 0;
                                if (progress_onscreen == -1) {
                                    play_sound("confirm", nav_sound);
                                    show_help();
                                }
                            }
                        }
                    case EV_ABS:
                        if (msgbox_active) {
                            break;
                        }
                        if (ev.code == NAV_DPAD_VER || ev.code == NAV_ANLG_VER) {
                            switch (ev.value) {
                                case -4096:
                                case -1:
                                    if (current_item_index == 0) {
                                        int y = (ui_count - 13) * 30;
                                        lv_obj_scroll_to_y(ui_pnlContent, y, LV_ANIM_OFF);
                                        content_panel_y = y;
                                        current_item_index = ui_count - 1;
                                        nav_prev(ui_group, 1);
                                        nav_prev(ui_group_glyph, 1);
                                        lv_task_handler();
                                    } else if (current_item_index > 0) {
                                        JOYUP_pressed = (ev.value != 0);
                                        list_nav_prev(1);
                                        lv_task_handler();
                                    }
                                    break;
                                case 1:
                                case 4096:
                                    if (current_item_index == ui_count - 1) {
                                        lv_obj_scroll_to_y(ui_pnlContent, 0, LV_ANIM_OFF);
                                        content_panel_y = 0;
                                        current_item_index = 0;
                                        nav_next(ui_group, 1);
                                        nav_next(ui_group_glyph, 1);
                                        lv_task_handler();
                                    } else if (current_item_index < ui_count) {
                                        JOYDOWN_pressed = (ev.value != 0);
                                        list_nav_next(1);
                                        lv_task_handler();
                                    }
                                    break;
                                default:
                                    JOYUP_pressed = 0;
                                    JOYDOWN_pressed = 0;
                                    break;
                            }
                        }
                    default:
                        break;
                }
            }
        }

        if (ui_count > 13 && (JOYUP_pressed || JOYDOWN_pressed)) {
            if (nav_hold > 2) {
                if (nav_delay > 16) {
                    nav_delay -= 16;
                }
                if (JOYUP_pressed && current_item_index > 0) {
                    list_nav_prev(1);
                }
                if (JOYDOWN_pressed && current_item_index < ui_count) {
                    list_nav_next(1);
                }
            }
            nav_hold++;
        } else {
            nav_delay = UINT8_MAX;
            nav_hold = 0;
        }

        if (ev.type == EV_KEY && ev.value == 1 && (ev.code == JOY_MINUS || ev.code == JOY_PLUS)) {
            progress_onscreen = 1;
            if (lv_obj_has_flag(ui_pnlProgress, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_clear_flag(ui_pnlProgress, LV_OBJ_FLAG_HIDDEN);
            }
            if (JOYHOTKEY_pressed) {
                lv_label_set_text(ui_icoProgress, "\uF185");
                lv_bar_set_value(ui_barProgress, get_brightness_percentage(get_brightness()), LV_ANIM_OFF);
            } else {
                int volume = get_volume_percentage();
                switch (volume) {
                    case 0:
                        lv_label_set_text(ui_icoProgress, "\uF6A9");
                        break;
                    case 1 ... 46:
                        lv_label_set_text(ui_icoProgress, "\uF026");
                        break;
                    case 47 ... 71:
                        lv_label_set_text(ui_icoProgress, "\uF027");
                        break;
                    case 72 ... 100:
                        lv_label_set_text(ui_icoProgress, "\uF028");
                        break;
                }
                lv_bar_set_value(ui_barProgress, volume, LV_ANIM_OFF);
            }
        }

        lv_task_handler();
        usleep(SCREEN_WAIT);
    }
}

void init_elements() {
    lv_obj_move_foreground(ui_pnlFooter);
    lv_obj_move_foreground(ui_pnlHeader);
    lv_obj_move_foreground(ui_pnlHelp);
    lv_obj_move_foreground(ui_pnlProgress);

    if (bar_footer) {
        lv_obj_set_style_bg_opa(ui_pnlFooter, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (bar_header) {
        lv_obj_set_style_bg_opa(ui_pnlHeader, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    process_visual_element(CLOCK, ui_lblDatetime);
    process_visual_element(BLUETOOTH, ui_staBluetooth);
    process_visual_element(NETWORK, ui_staNetwork);
    process_visual_element(BATTERY, ui_staCapacity);

    lv_label_set_text(ui_lblMessage, osd_message);

    lv_label_set_text(ui_lblNavB, "Back");
    lv_label_set_text(ui_lblNavMenu, "Quote");

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavCGlyph,
            ui_lblNavC,
            ui_lblNavXGlyph,
            ui_lblNavX,
            ui_lblNavYGlyph,
            ui_lblNavY,
            ui_lblNavZGlyph,
            ui_lblNavZ
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void glyph_task() {
    // TODO: Bluetooth connectivity!

/*
    if (is_network_connected() > 0) {
        lv_obj_set_style_text_color(ui_staNetwork, lv_color_hex(theme.STATUS.NETWORK.ACTIVE), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staNetwork, theme.STATUS.NETWORK.ACTIVE_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    else {
        lv_obj_set_style_text_color(ui_staNetwork, lv_color_hex(theme.STATUS.NETWORK.NORMAL), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staNetwork, theme.STATUS.NETWORK.NORMAL_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
*/

    if (atoi(read_text_from_file(BATT_CHARGER))) {
        lv_obj_set_style_text_color(ui_staCapacity, lv_color_hex(theme.STATUS.BATTERY.ACTIVE),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staCapacity, theme.STATUS.BATTERY.ACTIVE_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else if (read_battery_capacity() <= 15) {
        lv_obj_set_style_text_color(ui_staCapacity, lv_color_hex(theme.STATUS.BATTERY.LOW), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staCapacity, theme.STATUS.BATTERY.LOW_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_style_text_color(ui_staCapacity, lv_color_hex(theme.STATUS.BATTERY.NORMAL),
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staCapacity, theme.STATUS.BATTERY.NORMAL_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    if (progress_onscreen > 0) {
        progress_onscreen -= 1;
    } else {
        if (!lv_obj_has_flag(ui_pnlProgress, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_add_flag(ui_pnlProgress, LV_OBJ_FLAG_HIDDEN);
        }
        if (!msgbox_active) {
            progress_onscreen = -1;
        }
    }
}

void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) {
            static char old_wall[MAX_BUFFER_SIZE];
            static char new_wall[MAX_BUFFER_SIZE];

            snprintf(old_wall, sizeof(old_wall), "%s", current_wall);
            snprintf(new_wall, sizeof(new_wall), "%s", load_wallpaper(
                    ui_scrCredits, ui_group, theme.MISC.ANIMATED_BACKGROUND));

            if (strcmp(new_wall, old_wall) != 0) {
                strcpy(current_wall, new_wall);
                if (strlen(new_wall) > 3) {
                    printf("LOADING WALLPAPER: %s\n", new_wall);
                    if (theme.MISC.ANIMATED_BACKGROUND) {
                        lv_obj_t * img = lv_gif_create(ui_pnlWall);
                        lv_gif_set_src(img, new_wall);
                    } else {
                        lv_img_set_src(ui_imgWall, new_wall);
                    }
                    lv_obj_invalidate(ui_pnlWall);
                } else {
                    lv_img_set_src(ui_imgWall, &ui_img_nothing_png);
                }
            }

            static char static_image[MAX_BUFFER_SIZE];
            snprintf(static_image, sizeof(static_image), "%s",
                     load_static_image(ui_scrCredits, ui_group));

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
                        lv_obj_set_height(ui_pnlBox, SCREEN_HEIGHT);
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
                        lv_obj_move_background(ui_pnlBox);
                        lv_obj_move_background(ui_pnlWall);
                        break;
                    case 4: // Fullscreen + Front
                        lv_obj_set_height(ui_pnlBox, SCREEN_HEIGHT);
                        lv_obj_set_align(ui_imgBox, LV_ALIGN_BOTTOM_RIGHT);
                        lv_obj_move_foreground(ui_pnlBox);
                        break;
                }

                lv_img_set_src(ui_imgBox, static_image);
            } else {
                lv_img_set_src(ui_imgBox, &ui_img_nothing_png);
            }
        }
        nav_moved = 0;
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin:/system/bin", 1);
    setenv("NO_COLOR", "1", 1);

    lv_init();
    fbdev_init();

    static lv_color_t buf1[DISP_BUF_SIZE];
    static lv_color_t buf2[DISP_BUF_SIZE];
    static lv_disp_draw_buf_t disp_buf;

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, DISP_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    if (strcasecmp(HARDWARE, "RG28XX") == 0) {
        disp_drv.hor_res = SCREEN_HEIGHT;
        disp_drv.ver_res = SCREEN_WIDTH;
        disp_drv.sw_rotate = 1;
        disp_drv.rotated = LV_DISP_ROT_90;
    } else {
        disp_drv.hor_res = SCREEN_WIDTH;
        disp_drv.ver_res = SCREEN_HEIGHT;
    }
    lv_disp_drv_register(&disp_drv);

    load_config(&config);

    ui_init();
    init_elements();

    lv_obj_set_user_data(ui_scrCredits, basename(argv[0]));

    lv_label_set_text(ui_lblDatetime, get_datetime());
    lv_label_set_text(ui_staCapacity, get_capacity());

    load_theme(&theme, basename(argv[0]));
    apply_theme();

    switch (theme.MISC.NAVIGATION_TYPE) {
        case 1:
            NAV_DPAD_HOR = ABS_HAT0Y;
            NAV_ANLG_HOR = ABS_RX;
            NAV_DPAD_VER = ABS_HAT0X;
            NAV_ANLG_VER = ABS_Z;
            break;
        default:
            NAV_DPAD_HOR = ABS_HAT0X;
            NAV_ANLG_HOR = ABS_Z;
            NAV_DPAD_VER = ABS_HAT0Y;
            NAV_ANLG_VER = ABS_RX;
    }

    switch (config.SETTINGS.ADVANCED.SWAP) {
        case 1:
            NAV_A = JOY_B;
            NAV_B = JOY_A;
            lv_label_set_text(ui_lblNavAGlyph, "\u21D2");
            lv_label_set_text(ui_lblNavBGlyph, "\u21D3");
            break;
        default:
            NAV_A = JOY_A;
            NAV_B = JOY_B;
            lv_label_set_text(ui_lblNavAGlyph, "\u21D3");
            lv_label_set_text(ui_lblNavBGlyph, "\u21D2");
            break;
    }

    current_wall = load_wallpaper(ui_scrCredits, NULL, theme.MISC.ANIMATED_BACKGROUND);
    if (strlen(current_wall) > 3) {
        if (theme.MISC.ANIMATED_BACKGROUND) {
            lv_obj_t * img = lv_gif_create(ui_pnlWall);
            lv_gif_set_src(img, current_wall);
        } else {
            lv_img_set_src(ui_imgWall, current_wall);
        }
    } else {
        lv_img_set_src(ui_imgWall, &ui_img_nothing_png);
    }

    load_font_text(basename(argv[0]), ui_scrCredits);

    if (config.SETTINGS.GENERAL.SOUND == 2) {
        nav_sound = 1;
    }

    create_credit_items();

    struct dt_task_param dt_par;
    struct bat_task_param bat_par;
    struct osd_task_param osd_par;

    dt_par.lblDatetime = ui_lblDatetime;
    bat_par.staCapacity = ui_staCapacity;
    osd_par.lblMessage = ui_lblMessage;
    osd_par.pnlMessage = ui_pnlMessage;
    osd_par.count = 0;

    js_fd = open(JOY_DEVICE, O_RDONLY);
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

    lv_timer_t *osd_timer = lv_timer_create(osd_task, UINT16_MAX / 32, &osd_par);
    lv_timer_ready(osd_timer);

    lv_timer_t *glyph_timer = lv_timer_create(glyph_task, UINT16_MAX / 64, NULL);
    lv_timer_ready(glyph_timer);

    lv_timer_t *ui_refresh_timer = lv_timer_create(ui_refresh_task, UINT8_MAX / 4, NULL);
    lv_timer_ready(ui_refresh_timer);

    if (ui_count == 0) {
        lv_obj_clear_flag(ui_lblCreditsMessage, LV_OBJ_FLAG_HIDDEN);
    }

    if (ui_count > 13) {
        lv_obj_t * last_item = lv_obj_get_child(ui_pnlContent, -1);
        lv_obj_set_height(last_item, lv_obj_get_height(last_item) + 50); // Don't bother asking...
    }

    pthread_t joystick_thread;
    pthread_create(&joystick_thread, NULL, (void *(*)(void *)) joystick_task, NULL);

    init_elements();
    while (!safe_quit) {
        usleep(SCREEN_WAIT);
    }

    pthread_cancel(joystick_thread);

    close(js_fd);

    return 0;
}

uint32_t mux_tick(void) {
    static uint64_t start_ms = 0;

    if (start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);

    uint64_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}
