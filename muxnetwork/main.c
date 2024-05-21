#include "../lvgl/lvgl.h"
#include "../lvgl/drivers/display/fbdev.h"
#include "../lvgl/drivers/indev/evdev.h"
#include "ui/ui.h"
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <linux/joystick.h>
#include <linux/rtc.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
lv_obj_t *labels[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
unsigned int label_count = sizeof(labels) / sizeof(labels[0]);

lv_obj_t *msgbox_element = NULL;

int progress_onscreen = -1;

int key_show = 0;
int key_curr = 0;
int key_map = 0;

char *lblCurrentValue;
int lblEnabledValue;
int lblIdentifierValue;
int lblPasswordValue;
int lblStatusValue;

lv_obj_t *key_entry;
lv_obj_t *num_entry;

lv_group_t *ui_group;
lv_group_t *ui_group_value;
lv_group_t *ui_group_glyph;

static const char *key_lower_map[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
                                      "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "\n",
                                      "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
                                      "§", "z", "x", "c", "v", "b", "n", "m", "§", "\n",
                                      "ABC", " ", "OK", NULL
};

static const char *key_upper_map[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
                                      "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
                                      "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
                                      "§", "Z", "X", "C", "V", "B", "N", "M", "§", "\n",
                                      "!@#", " ", "OK", NULL
};

static const char *key_special_map[] = {"!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "\n",
                                        "`", "~", "_", "-", "+", "=", "{", "}", "[", "]", "\n",
                                        "|", "\\", ":", ";", "\"", "'", "!", "@", "#", "\n",
                                        "§", "<", ">", ",", ".", "?", "/", "$", "§", "\n",
                                        "abc", " ", "OK", NULL
};

static const char *key_number_map[] = {"7", "8", "9", "\n",
                                       "4", "5", "6", "\n",
                                       "1", "2", "3", "\n",
                                       "0", ".", "OK", NULL};

void show_help(lv_obj_t *element_focused) {
    char *message = "HELP NOT FOUND";

    if (element_focused == ui_lblEnable) {
        message = MUXNETWORK_ENABLED;
    } else if (element_focused == ui_lblIdentifier) {
        message = MUXNETWORK_IDENTIFIER;
    } else if (element_focused == ui_lblPassword) {
        message = MUXNETWORK_PASSWORD;
    } else if (element_focused == ui_lblType) {
        message = MUXNETWORK_TYPE;
    } else if (element_focused == ui_lblAddress) {
        message = MUXNETWORK_ADDRESS;
    } else if (element_focused == ui_lblSubnet) {
        message = MUXNETWORK_SUBNET;
    } else if (element_focused == ui_lblGateway) {
        message = MUXNETWORK_GATEWAY;
    } else if (element_focused == ui_lblStatus) {
        message = MUXNETWORK_STATUS;
    } else if (element_focused == ui_lblConnect) {
        message = MUXNETWORK_CONNECT;
    }

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent, lv_label_get_text(element_focused), message);
}

void get_current_ip() {
    if (!config.NETWORK.ENABLED) {
        lv_label_set_text(ui_lblStatusValue, "Network Disabled");
        return;
    }

    const char *curr_ip = read_text_from_file(NETWORK_ADDRESS);
    static char net_message[MAX_BUFFER_SIZE];

    if (strcasecmp(curr_ip, "0.0.0.0") == 0 ||
        strcasecmp(curr_ip, "127.0.0.1") == 0 ||
        strcasecmp(curr_ip, "") == 0 ||
        curr_ip == NULL) {
        lv_label_set_text(ui_lblStatusValue, "Not Connected");
        lv_task_handler();
        return;
    } else {
        snprintf(net_message, sizeof(net_message), "Connected - %s", curr_ip);
        lv_label_set_text(ui_lblStatusValue, net_message);
        lv_task_handler();
        return;
    }
}

void restore_network_values() {
    if (config.NETWORK.ENABLED) {
        lv_label_set_text(ui_lblEnableValue, "True");
    } else {
        lv_label_set_text(ui_lblEnableValue, "False");
    }

    if (config.NETWORK.TYPE) {
        lv_label_set_text(ui_lblTypeValue, "Static");
    } else {
        lv_label_set_text(ui_lblTypeValue, "DHCP");
    }

    lv_label_set_text(ui_lblPasswordValue, "");

    lv_label_set_text(ui_lblIdentifierValue, config.NETWORK.SSID);
    lv_label_set_text(ui_lblAddressValue, config.NETWORK.ADDRESS);
    lv_label_set_text(ui_lblSubnetValue, config.NETWORK.SUBNET);
    lv_label_set_text(ui_lblGatewayValue, config.NETWORK.GATEWAY);
    lv_label_set_text(ui_lblDNSValue, config.NETWORK.DNS);

    get_current_ip();
}

void save_network_config() {
    int idx_enable = 0;
    if (strcasecmp(lv_label_get_text(ui_lblEnableValue), "true") == 0) {
        idx_enable = 1;
    }

    int idx_type = 0;
    if (strcasecmp(lv_label_get_text(ui_lblTypeValue), "static") == 0) {
        idx_type = 1;
    }

    mini_t * muos_config = mini_try_load(MUOS_CONFIG_FILE);

    mini_set_int(muos_config, "network", "enabled", idx_enable);
    mini_set_int(muos_config, "network", "type", idx_type);

    mini_set_string(muos_config, "network", "ssid", lv_label_get_text(ui_lblIdentifierValue));
    mini_set_string(muos_config, "network", "address", lv_label_get_text(ui_lblAddressValue));
    mini_set_string(muos_config, "network", "subnet", lv_label_get_text(ui_lblSubnetValue));
    mini_set_string(muos_config, "network", "gateway", lv_label_get_text(ui_lblGatewayValue));
    mini_set_string(muos_config, "network", "dns", lv_label_get_text(ui_lblDNSValue));

    mini_save(muos_config, MINI_FLAGS_SKIP_EMPTY_GROUPS);
    mini_free(muos_config);
}

void init_navigation_groups() {
    lv_obj_t *ui_objects[] = {
            ui_lblEnable,
            ui_lblIdentifier,
            ui_lblPassword,
            ui_lblType,
            ui_lblAddress,
            ui_lblSubnet,
            ui_lblGateway,
            ui_lblDNS,
            ui_lblConnect
    };

    lv_obj_t *ui_objects_value[] = {
            ui_lblEnableValue,
            ui_lblIdentifierValue,
            ui_lblPasswordValue,
            ui_lblTypeValue,
            ui_lblAddressValue,
            ui_lblSubnetValue,
            ui_lblGatewayValue,
            ui_lblDNSValue,
            ui_lblConnectValue
    };

    lv_obj_t *ui_objects_icon[] = {
            ui_icoEnable,
            ui_icoIdentifier,
            ui_icoPassword,
            ui_icoType,
            ui_icoAddress,
            ui_icoSubnet,
            ui_icoGateway,
            ui_icoDNS,
            ui_icoConnect
    };

    labels[0] = ui_lblEnableValue;
    labels[1] = ui_lblIdentifierValue;
    labels[2] = ui_lblPasswordValue;
    labels[3] = ui_lblTypeValue;
    labels[4] = ui_lblAddressValue;
    labels[5] = ui_lblSubnetValue;
    labels[6] = ui_lblGatewayValue;
    labels[7] = ui_lblDNSValue;
    labels[8] = ui_lblStatusValue;

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();

    for (unsigned int i = 0; i < sizeof(ui_objects) / sizeof(ui_objects[0]); i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
    }

    for (unsigned int i = 0; i < sizeof(ui_objects_value) / sizeof(ui_objects_value[0]); i++) {
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
    }

    for (unsigned int i = 0; i < sizeof(ui_objects_icon) / sizeof(ui_objects_icon[0]); i++) {
        lv_group_add_obj(ui_group_glyph, ui_objects_icon[i]);
    }
}

void reset_osk() {
    key_curr = 0;
    lv_btnmatrix_clear_btn_ctrl_all(key_entry, LV_BTNMATRIX_CTRL_CHECKED);
    lv_btnmatrix_clear_btn_ctrl_all(num_entry, LV_BTNMATRIX_CTRL_CHECKED);
    lv_btnmatrix_set_selected_btn(key_entry, key_curr);
    lv_btnmatrix_set_selected_btn(num_entry, key_curr);
    lv_btnmatrix_set_btn_ctrl(key_entry, lv_btnmatrix_get_selected_btn(key_entry), LV_BTNMATRIX_CTRL_CHECKED);
    lv_btnmatrix_set_btn_ctrl(num_entry, lv_btnmatrix_get_selected_btn(num_entry), LV_BTNMATRIX_CTRL_CHECKED);
}

void *joystick_task() {
    struct input_event ev;
    int epoll_fd;
    struct epoll_event event, events[MAX_EVENTS];

    int JOYHOTKEY_pressed = 0;

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
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, 64);
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

                struct _lv_obj_t *element_focused = lv_group_get_focused(ui_group);
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
                            } else if (key_show > 0) {
                                const char *is_key;
                                if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
                                    is_key = lv_btnmatrix_get_btn_text(num_entry, key_curr);
                                } else {
                                    is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
                                }
                                if (ev.code == NAV_A) {
                                    if (strcasecmp(is_key, "OK") == 0) {
                                        key_show = 0;
                                        if (element_focused == ui_lblIdentifier) {
                                            lv_label_set_text(ui_lblIdentifierValue,
                                                              lv_textarea_get_text(ui_txtEntry));
                                        } else if (element_focused == ui_lblPassword) {
                                            char passphrase[MAX_BUFFER_SIZE];
                                            snprintf(passphrase, sizeof(passphrase), "%s",
                                                     lv_textarea_get_text(ui_txtEntry));
                                            if (strlen(passphrase) > 0) {
                                                lv_label_set_text(ui_lblPasswordValue, "********");
                                                lv_obj_set_user_data(ui_lblPasswordValue, passphrase);
                                            }
                                        } else if (element_focused == ui_lblAddress) {
                                            lv_label_set_text(ui_lblAddressValue,
                                                              lv_textarea_get_text(ui_txtEntry));
                                        } else if (element_focused == ui_lblSubnet) {
                                            lv_label_set_text(ui_lblSubnetValue, lv_textarea_get_text(ui_txtEntry));
                                        } else if (element_focused == ui_lblGateway) {
                                            lv_label_set_text(ui_lblGatewayValue,
                                                              lv_textarea_get_text(ui_txtEntry));
                                        } else if (element_focused == ui_lblDNS) {
                                            lv_label_set_text(ui_lblDNSValue, lv_textarea_get_text(ui_txtEntry));
                                        }
                                        reset_osk();
                                        lv_textarea_set_text(ui_txtEntry, "");
                                        lv_group_set_focus_cb(ui_group, NULL);
                                        lv_obj_add_flag(ui_pnlEntry, LV_OBJ_FLAG_HIDDEN);
                                        lv_task_handler();
                                    } else if (strcmp(is_key, "ABC") == 0) {
                                        lv_btnmatrix_set_map(key_entry, key_upper_map);
                                    } else if (strcmp(is_key, "!@#") == 0) {
                                        lv_btnmatrix_set_map(key_entry, key_special_map);
                                    } else if (strcmp(is_key, "abc") == 0) {
                                        lv_btnmatrix_set_map(key_entry, key_lower_map);
                                    } else {
                                        if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
                                            lv_event_send(num_entry, LV_EVENT_CLICKED, &key_curr);
                                        } else {
                                            lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
                                        }
                                    }
                                } else if (ev.code == NAV_B) {
                                    key_show = 0;
                                    reset_osk();
                                    lv_textarea_set_text(ui_txtEntry, "");
                                    lv_group_set_focus_cb(ui_group, NULL);
                                    lv_obj_add_flag(ui_pnlEntry, LV_OBJ_FLAG_HIDDEN);
                                } else if (ev.code == JOY_X) {
                                    lv_textarea_del_char(ui_txtEntry);
                                } else if (ev.code == JOY_Y) {
                                    if (key_show == 1) {
                                        switch (key_map) {
                                            case 0:
                                                lv_btnmatrix_set_map(key_entry, key_upper_map);
                                                key_map = 1;
                                                break;
                                            case 1:
                                                lv_btnmatrix_set_map(key_entry, key_special_map);
                                                key_map = 2;
                                                break;
                                            case 2:
                                                lv_btnmatrix_set_map(key_entry, key_lower_map);
                                                key_map = 0;
                                                break;
                                            default:
                                                break;
                                        }
                                    }
                                }
                            } else {
                                if (ev.code == JOY_MENU) {
                                    JOYHOTKEY_pressed = 1;
                                    break;
                                } else if (ev.code == NAV_A) {
                                    play_sound("confirm", nav_sound);

                                    if (strcasecmp(lv_label_get_text(element_focused), "Enabled") == 0) {
                                        if (strcasecmp(lv_label_get_text(ui_lblEnableValue), "True") == 0) {
                                            lv_label_set_text(ui_lblEnableValue, "False");
                                            lv_obj_add_flag(ui_lblIdentifier, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_lblPassword, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_lblType, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_lblAddress, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_lblSubnet, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_lblGateway, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_lblDNS, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_lblStatus, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_lblConnect, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_icoIdentifier, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_icoPassword, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_icoType, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_icoAddress, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_icoSubnet, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_icoGateway, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_icoDNS, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_icoStatus, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_icoConnect, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_lblIdentifierValue, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_lblPasswordValue, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_lblTypeValue, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_lblAddressValue, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_lblSubnetValue, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_lblGatewayValue, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_lblDNSValue, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_lblStatusValue, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_lblConnectValue, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
                                        } else {
                                            lv_label_set_text(ui_lblEnableValue, "True");
                                            lv_obj_clear_flag(ui_lblIdentifier, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_lblPassword, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_lblType, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_lblAddress, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_lblSubnet, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_lblGateway, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_lblDNS, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_lblStatus, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_lblConnect, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_icoIdentifier, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_icoPassword, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_icoType, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_icoAddress, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_icoSubnet, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_icoGateway, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_icoDNS, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_icoStatus, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_icoConnect, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_lblIdentifierValue, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_lblPasswordValue, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_lblTypeValue, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_lblAddressValue, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_lblSubnetValue, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_lblGatewayValue, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_lblDNSValue, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_lblStatusValue, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_lblConnectValue, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
                                        }
                                    } else if (strcasecmp(lv_label_get_text(element_focused), "Network Type") == 0) {
                                        if (strcasecmp(lv_label_get_text(ui_lblTypeValue), "Static") == 0) {
                                            lv_label_set_text(ui_lblTypeValue, "DHCP");
                                            lv_obj_add_state(ui_lblAddress, LV_STATE_DISABLED);
                                            lv_obj_add_state(ui_lblSubnet, LV_STATE_DISABLED);
                                            lv_obj_add_state(ui_lblGateway, LV_STATE_DISABLED);
                                            lv_obj_add_state(ui_lblDNS, LV_STATE_DISABLED);
                                            lv_obj_add_state(ui_icoAddress, LV_STATE_DISABLED);
                                            lv_obj_add_state(ui_icoSubnet, LV_STATE_DISABLED);
                                            lv_obj_add_state(ui_icoGateway, LV_STATE_DISABLED);
                                            lv_obj_add_state(ui_icoDNS, LV_STATE_DISABLED);
                                            lv_obj_add_state(ui_lblAddressValue, LV_STATE_DISABLED);
                                            lv_obj_add_state(ui_lblSubnetValue, LV_STATE_DISABLED);
                                            lv_obj_add_state(ui_lblGatewayValue, LV_STATE_DISABLED);
                                            lv_obj_add_state(ui_lblDNSValue, LV_STATE_DISABLED);
                                        } else {
                                            lv_label_set_text(ui_lblTypeValue, "static");
                                            lv_obj_clear_state(ui_lblAddress, LV_STATE_DISABLED);
                                            lv_obj_clear_state(ui_lblSubnet, LV_STATE_DISABLED);
                                            lv_obj_clear_state(ui_lblGateway, LV_STATE_DISABLED);
                                            lv_obj_clear_state(ui_lblDNS, LV_STATE_DISABLED);
                                            lv_obj_clear_state(ui_icoAddress, LV_STATE_DISABLED);
                                            lv_obj_clear_state(ui_icoSubnet, LV_STATE_DISABLED);
                                            lv_obj_clear_state(ui_icoGateway, LV_STATE_DISABLED);
                                            lv_obj_clear_state(ui_icoDNS, LV_STATE_DISABLED);
                                            lv_obj_clear_state(ui_lblAddressValue, LV_STATE_DISABLED);
                                            lv_obj_clear_state(ui_lblSubnetValue, LV_STATE_DISABLED);
                                            lv_obj_clear_state(ui_lblGatewayValue, LV_STATE_DISABLED);
                                            lv_obj_clear_state(ui_lblDNSValue, LV_STATE_DISABLED);
                                        }
                                    } else if (strcasecmp(lv_label_get_text(element_focused), "Connect") == 0) {
                                        lv_label_set_text(ui_lblStatusValue, "Trying to Connect...");
                                        lv_task_handler();
                                        save_network_config();
                                        if (config.NETWORK.ENABLED) {
                                            input_disable = 1;
                                            usleep(100000);

                                            static char passphrase[MAX_BUFFER_SIZE];
                                            snprintf(passphrase, sizeof(passphrase), "%s",
                                                     (char *) lv_obj_get_user_data(ui_lblPasswordValue));

                                            static char set_wpa_passphrase[MAX_BUFFER_SIZE];
                                            snprintf(set_wpa_passphrase, sizeof(set_wpa_passphrase),
                                                     "/opt/muos/script/web/password.sh \"%s\" %s",
                                                     lv_label_get_text(ui_lblIdentifierValue), passphrase);

                                            system(set_wpa_passphrase);
                                            run_shell_script("/opt/muos/script/system/network.sh");

                                            usleep(100000);
                                            get_current_ip();
                                            input_disable = 0;
                                        } else {
                                            lv_label_set_text(ui_lblStatusValue, "Network Disabled");
                                            lv_task_handler();
                                        }
                                    } else {
                                        key_curr = 0;
                                        if (element_focused == ui_lblIdentifier || element_focused == ui_lblPassword) {
                                            lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

                                            lv_obj_add_flag(num_entry, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_state(num_entry, LV_STATE_DISABLED);

                                            key_show = 1;
                                        } else {
                                            lv_obj_clear_flag(num_entry, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_clear_state(num_entry, LV_STATE_DISABLED);

                                            lv_obj_add_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
                                            lv_obj_add_state(key_entry, LV_STATE_DISABLED);

                                            key_show = 2;
                                        }
                                        lv_obj_clear_flag(ui_pnlEntry, LV_OBJ_FLAG_HIDDEN);
                                        if (element_focused == ui_lblPassword) {
                                            lv_textarea_set_text(ui_txtEntry, "");
                                        } else {
                                            lv_textarea_set_text(ui_txtEntry, lblCurrentValue);
                                        }
                                    }
                                } else if (ev.code == NAV_B) {
                                    play_sound("back", nav_sound);

                                    input_disable = 1;
                                    save_network_config();
                                    if (strcasecmp(lv_label_get_text(ui_lblEnableValue), "False") == 0) {
                                        mini_t * muos_config = mini_try_load(MUOS_CONFIG_FILE);

                                        set_ini_int(muos_config, "network", "enabled", 0);
                                        set_ini_string(muos_config, "network", "interface", "wlan0");
                                        set_ini_int(muos_config, "network", "type", 0);
                                        set_ini_string(muos_config, "network", "ssid", "");
                                        set_ini_string(muos_config, "network", "address", "192.168.0.123");
                                        set_ini_string(muos_config, "network", "gateway", "192.168.0.1");
                                        set_ini_int(muos_config, "network", "subnet", 24);
                                        set_ini_string(muos_config, "network", "dns", "1.1.1.1");

                                        mini_save(muos_config, MINI_FLAGS_SKIP_EMPTY_GROUPS);
                                        mini_free(muos_config);

                                        system("/opt/muos/script/system/network.sh");
                                    }

                                    osd_message = "Changes Saved";
                                    lv_label_set_text(ui_lblMessage, osd_message);
                                    lv_obj_clear_flag(ui_pnlMessage, LV_OBJ_FLAG_HIDDEN);
                                    usleep(100000);
                                    safe_quit = 1;
                                } else if (ev.code == JOY_X) {
                                    if (strcasecmp(lv_label_get_text(ui_lblEnableValue), "True") == 0) {
                                        play_sound("confirm", nav_sound);

                                        input_disable = 1;
                                        save_network_config();
                                        load_mux("net_scan");

                                        safe_quit = 1;
                                    }
                                }
                            }
                        } else {
                            if (ev.code == JOY_MENU) {
                                JOYHOTKEY_pressed = 0;
                                if (progress_onscreen == -1) {
                                    play_sound("confirm", nav_sound);
                                    show_help(element_focused);
                                }
                            }
                        }
                        break;
                    case EV_ABS:
                        if (msgbox_active) {
                            break;
                        }
                        if (ev.code == NAV_DPAD_HOR || ev.code == NAV_ANLG_HOR) {
                            switch (ev.value) {
                                case -4100 ... -4000:
                                case -1:
                                    if (key_show > 0) {
                                        if (key_curr >= 1) {
                                            key_curr--;
                                            if (key_curr < 0) {
                                                key_curr = 0;
                                            }
                                            if (strcasecmp(lv_btnmatrix_get_btn_text(key_entry, key_curr), "§") == 0) {
                                                key_curr--;
                                            }
                                            lv_event_send(key_entry, LV_EVENT_SCROLL, &key_curr);
                                            lv_event_send(num_entry, LV_EVENT_SCROLL, &key_curr);
                                        }
                                    } else {
                                        if (element_focused == ui_lblEnable) {
                                            play_sound("navigate", nav_sound);

                                            if (strcasecmp(lv_label_get_text(ui_lblEnableValue), "True") == 0) {
                                                lv_label_set_text(ui_lblEnableValue, "False");
                                                lv_obj_add_flag(ui_lblIdentifier, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblPassword, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblType, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblAddress, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblSubnet, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblGateway, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblDNS, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblStatus, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblConnect, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_icoIdentifier, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_icoPassword, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_icoType, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_icoAddress, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_icoSubnet, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_icoGateway, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_icoDNS, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_icoStatus, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_icoConnect, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblIdentifierValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblPasswordValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblTypeValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblAddressValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblSubnetValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblGatewayValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblDNSValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblStatusValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblConnectValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
                                            } else {
                                                lv_label_set_text(ui_lblEnableValue, "True");
                                                lv_obj_clear_flag(ui_lblIdentifier, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblPassword, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblType, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblAddress, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblSubnet, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblGateway, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblDNS, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblStatus, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblConnect, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_icoIdentifier, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_icoPassword, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_icoType, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_icoAddress, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_icoSubnet, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_icoGateway, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_icoDNS, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_icoStatus, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_icoConnect, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblIdentifierValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblPasswordValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblTypeValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblAddressValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblSubnetValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblGatewayValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblDNSValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblStatusValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblConnectValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
                                            }
                                            break;
                                        }
                                        if (element_focused == ui_lblType) {
                                            play_sound("navigate", nav_sound);

                                            if (strcasecmp(lv_label_get_text(ui_lblTypeValue), "Static") == 0) {
                                                lv_label_set_text(ui_lblTypeValue, "DHCP");
                                                lv_obj_add_state(ui_lblAddress, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_lblSubnet, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_lblGateway, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_lblDNS, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_icoAddress, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_icoSubnet, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_icoGateway, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_icoDNS, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_lblAddressValue, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_lblSubnetValue, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_lblGatewayValue, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_lblDNSValue, LV_STATE_DISABLED);
                                            } else {
                                                lv_label_set_text(ui_lblTypeValue, "Static");
                                                lv_obj_clear_state(ui_lblAddress, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_lblSubnet, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_lblGateway, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_lblDNS, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_icoAddress, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_icoSubnet, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_icoGateway, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_icoDNS, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_lblAddressValue, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_lblSubnetValue, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_lblGatewayValue, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_lblDNSValue, LV_STATE_DISABLED);
                                            }
                                            break;
                                        }
                                    }
                                    break;
                                case 1:
                                case 4000 ... 4100:
                                    if (key_show > 0) {
                                        int max_key;
                                        if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
                                            max_key = 11;
                                        } else {
                                            max_key = 40;
                                        }
                                        if (key_curr <= max_key) {
                                            key_curr++;
                                            if (key_curr > max_key) {
                                                key_curr = max_key;
                                            }
                                            if (strcasecmp(lv_btnmatrix_get_btn_text(key_entry, key_curr), "§") == 0) {
                                                key_curr++;
                                            }
                                            lv_event_send(key_entry, LV_EVENT_SCROLL, &key_curr);
                                            lv_event_send(num_entry, LV_EVENT_SCROLL, &key_curr);
                                        }
                                    } else {
                                        if (element_focused == ui_lblEnable) {
                                            play_sound("navigate", nav_sound);

                                            if (strcasecmp(lv_label_get_text(ui_lblEnableValue), "True") == 0) {
                                                lv_label_set_text(ui_lblEnableValue, "False");
                                                lv_obj_add_flag(ui_lblIdentifier, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblPassword, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblType, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblAddress, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblSubnet, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblGateway, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblDNS, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblStatus, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblConnect, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_icoIdentifier, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_icoPassword, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_icoType, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_icoAddress, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_icoSubnet, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_icoGateway, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_icoDNS, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_icoStatus, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_icoConnect, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblIdentifierValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblPasswordValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblTypeValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblAddressValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblSubnetValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblGatewayValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblDNSValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblStatusValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblConnectValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_add_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
                                            } else {
                                                lv_label_set_text(ui_lblEnableValue, "True");
                                                lv_obj_clear_flag(ui_lblIdentifier, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblPassword, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblType, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblAddress, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblSubnet, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblGateway, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblDNS, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblStatus, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblConnect, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_icoIdentifier, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_icoPassword, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_icoType, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_icoAddress, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_icoSubnet, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_icoGateway, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_icoDNS, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_icoStatus, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_icoConnect, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblIdentifierValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblPasswordValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblTypeValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblAddressValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblSubnetValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblGatewayValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblDNSValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblStatusValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblConnectValue, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
                                                lv_obj_clear_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
                                            }
                                            break;
                                        }
                                        if (element_focused == ui_lblType) {
                                            play_sound("navigate", nav_sound);

                                            if (strcasecmp(lv_label_get_text(ui_lblTypeValue), "Static") == 0) {
                                                lv_label_set_text(ui_lblTypeValue, "DHCP");
                                                lv_obj_add_state(ui_lblAddress, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_lblSubnet, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_lblGateway, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_lblDNS, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_icoAddress, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_icoSubnet, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_icoGateway, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_icoDNS, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_lblAddressValue, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_lblSubnetValue, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_lblGatewayValue, LV_STATE_DISABLED);
                                                lv_obj_add_state(ui_lblDNSValue, LV_STATE_DISABLED);
                                            } else {
                                                lv_label_set_text(ui_lblTypeValue, "Static");
                                                lv_obj_clear_state(ui_lblAddress, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_lblSubnet, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_lblGateway, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_lblDNS, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_icoAddress, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_icoSubnet, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_icoGateway, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_icoDNS, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_lblAddressValue, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_lblSubnetValue, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_lblGatewayValue, LV_STATE_DISABLED);
                                                lv_obj_clear_state(ui_lblDNSValue, LV_STATE_DISABLED);
                                            }
                                            break;
                                        }
                                    }
                                    break;
                                default:
                                    break;
                            }
                        } else if (ev.code == NAV_DPAD_VER || ev.code == NAV_ANLG_VER) {
                            switch (ev.value) {
                                case -4100 ... -4000:
                                case -1:
                                    if (key_show > 0) {
                                        if (key_curr >= 1) {
                                            switch (key_curr) {
                                                case 26:
                                                    key_curr = 17;
                                                    break;
                                                case 27:
                                                    key_curr = 18;
                                                    break;
                                                case 28:
                                                    key_curr = 19;
                                                    break;
                                                case 30 ... 36:
                                                    key_curr = key_curr - 9;
                                                    break;
                                                case 38:
                                                    key_curr = 30;
                                                    break;
                                                case 39:
                                                    key_curr = 33;
                                                    break;
                                                case 40:
                                                    key_curr = 36;
                                                    break;
                                                default:
                                                    if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
                                                        key_curr = key_curr - 3;
                                                    } else {
                                                        key_curr = key_curr - 10;
                                                    }
                                                    break;
                                            }
                                            if (key_curr < 0) {
                                                key_curr = 0;
                                            }
                                        }
                                        if (strcasecmp(lv_btnmatrix_get_btn_text(key_entry, key_curr), "§") == 0) {
                                            key_curr--;
                                        }
                                        lv_event_send(key_entry, LV_EVENT_SCROLL, &key_curr);
                                        lv_event_send(num_entry, LV_EVENT_SCROLL, &key_curr);
                                    } else {
                                        nav_prev(ui_group, 1);
                                        nav_prev(ui_group_value, 1);
                                        nav_prev(ui_group_glyph, 1);
                                        lblCurrentValue = lv_label_get_text(lv_group_get_focused(ui_group_value));
                                        play_sound("navigate", nav_sound);
                                        nav_moved = 1;
                                        break;
                                    }
                                    break;
                                case 1:
                                case 4000 ... 4100:
                                    if (key_show > 0) {
                                        int max_key;
                                        if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
                                            max_key = 11;
                                        } else {
                                            max_key = 40;
                                        }
                                        if (key_curr <= max_key) {
                                            switch (key_curr) {
                                                case 17:
                                                    key_curr = 26;
                                                    break;
                                                case 18:
                                                    key_curr = 27;
                                                    break;
                                                case 19:
                                                    key_curr = 28;
                                                    break;
                                                case 21 ... 27:
                                                    key_curr = key_curr + 9;
                                                    break;
                                                case 28:
                                                    key_curr = 36;
                                                    break;
                                                case 30:
                                                    key_curr = 38;
                                                    break;
                                                case 31 ... 35:
                                                    key_curr = 39;
                                                    break;
                                                case 36:
                                                    key_curr = 40;
                                                    break;
                                                default:
                                                    if (lv_obj_has_flag(key_entry, LV_OBJ_FLAG_HIDDEN)) {
                                                        key_curr = key_curr + 3;
                                                    } else {
                                                        key_curr = key_curr + 10;
                                                    }
                                                    break;
                                            }
                                            if (key_curr > max_key) {
                                                key_curr = max_key;
                                            }
                                        }
                                        if (strcasecmp(lv_btnmatrix_get_btn_text(key_entry, key_curr), "§") == 0) {
                                            key_curr++;
                                        }
                                        lv_event_send(key_entry, LV_EVENT_SCROLL, &key_curr);
                                        lv_event_send(num_entry, LV_EVENT_SCROLL, &key_curr);
                                    } else {
                                        nav_next(ui_group, 1);
                                        nav_next(ui_group_value, 1);
                                        nav_next(ui_group_glyph, 1);
                                        lblCurrentValue = lv_label_get_text(lv_group_get_focused(ui_group_value));
                                        play_sound("navigate", nav_sound);
                                        nav_moved = 1;
                                        break;
                                    }
                                    break;
                                default:
                                    break;
                            }
                        }
                    default:
                        break;
                }
            }
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

static void osk_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);

    switch (code) {
        case LV_EVENT_SCROLL:
            lv_btnmatrix_set_selected_btn(obj, key_curr);
            lv_btnmatrix_set_btn_ctrl(obj, lv_btnmatrix_get_selected_btn(obj), LV_BTNMATRIX_CTRL_CHECKED);
            break;
        case LV_EVENT_CLICKED:
            lv_textarea_add_text(ui_txtEntry, lv_btnmatrix_get_btn_text(obj, lv_btnmatrix_get_selected_btn(obj)));
            break;
        default:
            break;
    }
}

static void num_handler(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);

    switch (code) {
        case LV_EVENT_SCROLL:
            lv_btnmatrix_set_selected_btn(obj, key_curr);
            lv_btnmatrix_set_btn_ctrl(obj, lv_btnmatrix_get_selected_btn(obj), LV_BTNMATRIX_CTRL_CHECKED);
            break;
        case LV_EVENT_CLICKED:
            lv_textarea_add_text(ui_txtEntry, lv_btnmatrix_get_btn_text(obj, lv_btnmatrix_get_selected_btn(obj)));
            break;
        default:
            break;
    }
}

void init_elements() {
    lv_obj_move_foreground(ui_pnlFooter);
    lv_obj_move_foreground(ui_pnlHeader);
    lv_obj_move_foreground(ui_pnlHelp);
    lv_obj_move_foreground(ui_pnlEntry);
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
    lv_label_set_text(ui_lblNavX, "Scan");
    lv_label_set_text(ui_lblNavMenu, "Help");

    lv_obj_t *nav_hide[] = {
            ui_lblNavAGlyph,
            ui_lblNavA,
            ui_lblNavCGlyph,
            ui_lblNavC,
            ui_lblNavYGlyph,
            ui_lblNavY,
            ui_lblNavZGlyph,
            ui_lblNavZ
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_add_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_set_user_data(ui_lblEnable, "enable");
    lv_obj_set_user_data(ui_lblIdentifier, "identifier");
    lv_obj_set_user_data(ui_lblPassword, "password");
    lv_obj_set_user_data(ui_lblType, "type");
    lv_obj_set_user_data(ui_lblAddress, "address");
    lv_obj_set_user_data(ui_lblSubnet, "subnet");
    lv_obj_set_user_data(ui_lblGateway, "gateway");
    lv_obj_set_user_data(ui_lblDNS, "dns");
    lv_obj_set_user_data(ui_lblStatus, "status");
    lv_obj_set_user_data(ui_lblConnect, "connect");

    char *overlay = load_overlay_image();
    if (strlen(overlay) > 0 && theme.MISC.IMAGE_OVERLAY) {
        lv_obj_t * overlay_img = lv_img_create(ui_scrNetwork);
        lv_img_set_src(overlay_img, overlay);
        lv_obj_move_foreground(overlay_img);
    }

    if (strcasecmp(lv_label_get_text(ui_lblEnableValue), "False") == 0) {
        lv_obj_add_flag(ui_lblIdentifier, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblPassword, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblType, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblAddress, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblSubnet, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblGateway, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblDNS, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblStatus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblConnect, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_icoIdentifier, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_icoPassword, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_icoType, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_icoAddress, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_icoSubnet, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_icoGateway, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_icoDNS, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_icoStatus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_icoConnect, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblIdentifierValue, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblPasswordValue, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblTypeValue, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblAddressValue, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblSubnetValue, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblGatewayValue, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblDNSValue, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblStatusValue, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblConnectValue, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(ui_lblIdentifier, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblPassword, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblType, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblAddress, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblSubnet, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblGateway, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblDNS, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblStatus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblConnect, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_icoIdentifier, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_icoPassword, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_icoType, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_icoAddress, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_icoSubnet, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_icoGateway, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_icoDNS, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_icoStatus, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_icoConnect, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblIdentifierValue, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblPasswordValue, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblTypeValue, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblAddressValue, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblSubnetValue, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblGatewayValue, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblDNSValue, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblStatusValue, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblConnectValue, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblNavX, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui_lblNavXGlyph, LV_OBJ_FLAG_HIDDEN);
    }

    if (strcasecmp(lv_label_get_text(ui_lblTypeValue), "DHCP") == 0) {
        lv_obj_add_state(ui_lblAddress, LV_STATE_DISABLED);
        lv_obj_add_state(ui_lblSubnet, LV_STATE_DISABLED);
        lv_obj_add_state(ui_lblGateway, LV_STATE_DISABLED);
        lv_obj_add_state(ui_lblDNS, LV_STATE_DISABLED);
        lv_obj_add_state(ui_icoAddress, LV_STATE_DISABLED);
        lv_obj_add_state(ui_icoSubnet, LV_STATE_DISABLED);
        lv_obj_add_state(ui_icoGateway, LV_STATE_DISABLED);
        lv_obj_add_state(ui_icoDNS, LV_STATE_DISABLED);
        lv_obj_add_state(ui_lblAddressValue, LV_STATE_DISABLED);
        lv_obj_add_state(ui_lblSubnetValue, LV_STATE_DISABLED);
        lv_obj_add_state(ui_lblGatewayValue, LV_STATE_DISABLED);
        lv_obj_add_state(ui_lblDNSValue, LV_STATE_DISABLED);
    } else {
        lv_obj_clear_state(ui_lblAddress, LV_STATE_DISABLED);
        lv_obj_clear_state(ui_lblSubnet, LV_STATE_DISABLED);
        lv_obj_clear_state(ui_lblGateway, LV_STATE_DISABLED);
        lv_obj_clear_state(ui_lblDNS, LV_STATE_DISABLED);
        lv_obj_clear_state(ui_icoAddress, LV_STATE_DISABLED);
        lv_obj_clear_state(ui_icoSubnet, LV_STATE_DISABLED);
        lv_obj_clear_state(ui_icoGateway, LV_STATE_DISABLED);
        lv_obj_clear_state(ui_icoDNS, LV_STATE_DISABLED);
        lv_obj_clear_state(ui_lblAddressValue, LV_STATE_DISABLED);
        lv_obj_clear_state(ui_lblSubnetValue, LV_STATE_DISABLED);
        lv_obj_clear_state(ui_lblGatewayValue, LV_STATE_DISABLED);
        lv_obj_clear_state(ui_lblDNSValue, LV_STATE_DISABLED);
    }

    if (TEST_IMAGE) display_testing_message(ui_scrNetwork);
}

void init_osk() {
    key_entry = lv_btnmatrix_create(ui_pnlEntry);
    num_entry = lv_btnmatrix_create(ui_pnlEntry);

    lv_obj_set_width(key_entry, 550);
    lv_obj_set_height(key_entry, 320);
    lv_obj_set_width(num_entry, 375);
    lv_obj_set_height(num_entry, 320);

    lv_btnmatrix_set_one_checked(key_entry, 1);
    lv_btnmatrix_set_one_checked(num_entry, 1);

    lv_btnmatrix_set_map(key_entry, key_lower_map);
    lv_btnmatrix_set_map(num_entry, key_number_map);

    lv_btnmatrix_set_btn_ctrl(key_entry, 29, LV_BTNMATRIX_CTRL_HIDDEN);
    lv_btnmatrix_set_btn_ctrl(key_entry, 37, LV_BTNMATRIX_CTRL_HIDDEN);

    lv_btnmatrix_set_btn_width(key_entry, 39, 3);

    lv_btnmatrix_set_selected_btn(key_entry, key_curr);
    lv_btnmatrix_set_selected_btn(num_entry, key_curr);

    lv_btnmatrix_set_btn_ctrl(key_entry, lv_btnmatrix_get_selected_btn(key_entry), LV_BTNMATRIX_CTRL_CHECKED);
    lv_btnmatrix_set_btn_ctrl(num_entry, lv_btnmatrix_get_selected_btn(num_entry), LV_BTNMATRIX_CTRL_CHECKED);

    lv_obj_align(key_entry, LV_ALIGN_CENTER, 0, 0);
    lv_obj_align(num_entry, LV_ALIGN_CENTER, 0, 0);

    lv_obj_add_event_cb(key_entry, osk_handler, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(num_entry, num_handler, LV_EVENT_ALL, NULL);

    lv_obj_set_style_border_width(key_entry, 3, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(key_entry, 1, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(num_entry, 3, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(num_entry, 1, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(key_entry, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(num_entry, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(key_entry, lv_color_hex(theme.OSK.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(num_entry, lv_color_hex(theme.OSK.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(key_entry, lv_color_hex(theme.OSK.ITEM.BACKGROUND), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(num_entry, lv_color_hex(theme.OSK.ITEM.BACKGROUND), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(key_entry, lv_color_hex(theme.OSK.ITEM.BACKGROUND_FOCUS),
                              LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(num_entry, lv_color_hex(theme.OSK.ITEM.BACKGROUND_FOCUS),
                              LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_bg_opa(key_entry, theme.OSK.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(num_entry, theme.OSK.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(key_entry, theme.OSK.ITEM.BACKGROUND_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(num_entry, theme.OSK.ITEM.BACKGROUND_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(key_entry, theme.OSK.ITEM.BACKGROUND_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(num_entry, theme.OSK.ITEM.BACKGROUND_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_text_color(key_entry, lv_color_hex(theme.OSK.TEXT), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(num_entry, lv_color_hex(theme.OSK.TEXT), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(key_entry, lv_color_hex(theme.OSK.TEXT_FOCUS), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(num_entry, lv_color_hex(theme.OSK.TEXT_FOCUS), LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_text_opa(key_entry, theme.OSK.TEXT_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(num_entry, theme.OSK.TEXT_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(key_entry, theme.OSK.TEXT_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(num_entry, theme.OSK.TEXT_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_border_color(key_entry, lv_color_hex(theme.OSK.BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(num_entry, lv_color_hex(theme.OSK.BORDER), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(key_entry, lv_color_hex(theme.OSK.ITEM.BORDER), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(num_entry, lv_color_hex(theme.OSK.ITEM.BORDER), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(key_entry, lv_color_hex(theme.OSK.ITEM.BORDER_FOCUS),
                                  LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(num_entry, lv_color_hex(theme.OSK.ITEM.BORDER_FOCUS),
                                  LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_border_opa(key_entry, theme.OSK.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(num_entry, theme.OSK.BORDER_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(key_entry, theme.OSK.ITEM.BORDER_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(num_entry, theme.OSK.ITEM.BORDER_ALPHA, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(key_entry, theme.OSK.ITEM.BORDER_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_opa(num_entry, theme.OSK.ITEM.BORDER_FOCUS_ALPHA, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_radius(key_entry, theme.OSK.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(num_entry, theme.OSK.RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(key_entry, theme.OSK.ITEM.RADIUS, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(num_entry, theme.OSK.ITEM.RADIUS, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(key_entry, theme.OSK.ITEM.RADIUS, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_radius(num_entry, theme.OSK.ITEM.RADIUS, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_set_style_pad_top(key_entry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(num_entry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(key_entry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(num_entry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(key_entry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(num_entry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(key_entry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(num_entry, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_gap(key_entry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_gap(num_entry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_height(ui_txtEntry, 48);
    lv_obj_set_style_bg_color(ui_txtEntry, lv_color_hex(theme.OSK.BACKGROUND), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_txtEntry, theme.OSK.BACKGROUND_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_txtEntry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_txtEntry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_txtEntry, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void glyph_task() {
    // TODO: Bluetooth connectivity!

    if (is_network_connected()) {
        lv_obj_set_style_text_color(ui_staNetwork, lv_color_hex(theme.STATUS.NETWORK.ACTIVE), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staNetwork, theme.STATUS.NETWORK.ACTIVE_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    else {
        lv_obj_set_style_text_color(ui_staNetwork, lv_color_hex(theme.STATUS.NETWORK.NORMAL), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_opa(ui_staNetwork, theme.STATUS.NETWORK.NORMAL_ALPHA, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

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
                    ui_scrNetwork, ui_group, theme.MISC.ANIMATED_BACKGROUND));

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
                     load_static_image(ui_scrNetwork, ui_group));

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
        lv_obj_invalidate(ui_pnlContent);
        lv_task_handler();
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

    lv_obj_set_user_data(ui_scrNetwork, basename(argv[0]));

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

    current_wall = load_wallpaper(ui_scrNetwork, NULL, theme.MISC.ANIMATED_BACKGROUND);
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

    load_font_text(basename(argv[0]), ui_scrNetwork);

    if (config.SETTINGS.GENERAL.SOUND == 2) {
        nav_sound = 1;
    }

    init_navigation_groups();
    restore_network_values();

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

    pthread_t joystick_thread;
    pthread_create(&joystick_thread, NULL, (void *(*)(void *)) joystick_task, NULL);

    init_osk();
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
