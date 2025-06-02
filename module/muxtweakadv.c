#include "muxshare.h"
#include "muxtweakadv.h"
#include "ui/ui_muxtweakadv.h"
#include <string.h>
#include <stdlib.h>
#include "../common/init.h"
#include "../common/common.h"
#include "../common/ui_common.h"
#include "../common/input/list_nav.h"

static int Accelerate_original, Swap_original, Thermal_original, Volume_original, Brightness_original, Offset_original,
        Passcode_original, LED_original, Theme_original, RetroWait_original, State_original, Verbose_original,
        Rumble_original, UserInit_original, DPADSwap_original, Overdrive_original, Swapfile_original, Zramfile_original,
        CardMode_original;

#define UI_PANEL 5
static lv_obj_t *ui_mux_panels[UI_PANEL];

struct help_msg {
    lv_obj_t *element;
    char *message;
};

static void show_help(lv_obj_t *element_focused) {
    struct help_msg help_messages[] = {
            {ui_lblAccelerate_tweakadv, lang.MUXTWEAKADV.HELP.SPEED},
            {ui_lblSwap_tweakadv,       lang.MUXTWEAKADV.HELP.SWAP},
            {ui_lblThermal_tweakadv,    lang.MUXTWEAKADV.HELP.THERMAL},
            {ui_lblVolume_tweakadv,     lang.MUXTWEAKADV.HELP.VOLUME},
            {ui_lblBrightness_tweakadv, lang.MUXTWEAKADV.HELP.BRIGHT},
            {ui_lblOffset_tweakadv,     lang.MUXTWEAKADV.HELP.OFFSET},
            {ui_lblPasscode_tweakadv,   lang.MUXTWEAKADV.HELP.LOCK},
            {ui_lblLED_tweakadv,        lang.MUXTWEAKADV.HELP.LED},
            {ui_lblTheme_tweakadv,      lang.MUXTWEAKADV.HELP.RANDOM},
            {ui_lblRetroWait_tweakadv,  lang.MUXTWEAKADV.HELP.NET_WAIT},
            {ui_lblState_tweakadv,      lang.MUXTWEAKADV.HELP.STATE},
            {ui_lblVerbose_tweakadv,    lang.MUXTWEAKADV.HELP.VERBOSE},
            {ui_lblRumble_tweakadv,     lang.MUXTWEAKADV.HELP.RUMBLE},
            {ui_lblUserInit_tweakadv,   lang.MUXTWEAKADV.HELP.USER_INIT},
            {ui_lblDPADSwap_tweakadv,   lang.MUXTWEAKADV.HELP.DPAD},
            {ui_lblOverdrive_tweakadv,  lang.MUXTWEAKADV.HELP.OVERDRIVE},
            {ui_lblSwapfile_tweakadv,   lang.MUXTWEAKADV.HELP.SWAPFILE},
            {ui_lblZramfile_tweakadv,   lang.MUXTWEAKADV.HELP.ZRAMFILE},
            {ui_lblCardMode_tweakadv,   lang.MUXTWEAKADV.HELP.TUNING},
    };

    char *message = lang.GENERIC.NO_HELP;
    int num_messages = sizeof(help_messages) / sizeof(help_messages[0]);

    for (int i = 0; i < num_messages; i++) {
        if (element_focused == help_messages[i].element) {
            message = help_messages[i].message;
            break;
        }
    }

    if (strlen(message) <= 1) message = lang.GENERIC.NO_HELP;

    show_help_msgbox(ui_pnlHelp, ui_lblHelpHeader, ui_lblHelpContent,
                     TS(lv_label_get_text(element_focused)), message);
}

static void init_dropdown_settings() {
    Accelerate_original = lv_dropdown_get_selected(ui_droAccelerate_tweakadv);
    Swap_original = lv_dropdown_get_selected(ui_droSwap_tweakadv);
    Thermal_original = lv_dropdown_get_selected(ui_droThermal_tweakadv);
    Volume_original = lv_dropdown_get_selected(ui_droVolume_tweakadv);
    Brightness_original = lv_dropdown_get_selected(ui_droBrightness_tweakadv);
    Offset_original = lv_dropdown_get_selected(ui_droOffset_tweakadv);
    Passcode_original = lv_dropdown_get_selected(ui_droPasscode_tweakadv);
    LED_original = lv_dropdown_get_selected(ui_droLED_tweakadv);
    Theme_original = lv_dropdown_get_selected(ui_droTheme_tweakadv);
    RetroWait_original = lv_dropdown_get_selected(ui_droRetroWait_tweakadv);
    State_original = lv_dropdown_get_selected(ui_droState_tweakadv);
    Verbose_original = lv_dropdown_get_selected(ui_droVerbose_tweakadv);
    Rumble_original = lv_dropdown_get_selected(ui_droRumble_tweakadv);
    UserInit_original = lv_dropdown_get_selected(ui_droUserInit_tweakadv);
    DPADSwap_original = lv_dropdown_get_selected(ui_droDPADSwap_tweakadv);
    Overdrive_original = lv_dropdown_get_selected(ui_droOverdrive_tweakadv);
    Swapfile_original = lv_dropdown_get_selected(ui_droSwapfile_tweakadv);
    Zramfile_original = lv_dropdown_get_selected(ui_droZramfile_tweakadv);
    CardMode_original = lv_dropdown_get_selected(ui_droCardMode_tweakadv);
}

static void restore_tweak_options() {
    lv_dropdown_set_selected(ui_droVolume_tweakadv,
                             !strcasecmp(config.SETTINGS.ADVANCED.VOLUME, "silent") ? 1 :
                             !strcasecmp(config.SETTINGS.ADVANCED.VOLUME, "soft") ? 2 :
                             !strcasecmp(config.SETTINGS.ADVANCED.VOLUME, "loud") ? 3 : 0);

    lv_dropdown_set_selected(ui_droBrightness_tweakadv,
                             !strcasecmp(config.SETTINGS.ADVANCED.BRIGHTNESS, "low") ? 1 :
                             !strcasecmp(config.SETTINGS.ADVANCED.BRIGHTNESS, "medium") ? 2 :
                             !strcasecmp(config.SETTINGS.ADVANCED.BRIGHTNESS, "high") ? 3 : 0);

    lv_dropdown_set_selected(ui_droSwap_tweakadv, config.SETTINGS.ADVANCED.SWAP);
    lv_dropdown_set_selected(ui_droThermal_tweakadv, config.SETTINGS.ADVANCED.THERMAL);
    lv_dropdown_set_selected(ui_droOffset_tweakadv, config.SETTINGS.ADVANCED.OFFSET);
    lv_dropdown_set_selected(ui_droPasscode_tweakadv, config.SETTINGS.ADVANCED.LOCK);
    lv_dropdown_set_selected(ui_droLED_tweakadv, config.SETTINGS.ADVANCED.LED);
    lv_dropdown_set_selected(ui_droTheme_tweakadv, config.SETTINGS.ADVANCED.THEME);
    lv_dropdown_set_selected(ui_droRetroWait_tweakadv, config.SETTINGS.ADVANCED.RETROWAIT);
    lv_dropdown_set_selected(ui_droVerbose_tweakadv, config.SETTINGS.ADVANCED.VERBOSE);
    lv_dropdown_set_selected(ui_droRumble_tweakadv, config.SETTINGS.ADVANCED.RUMBLE);
    lv_dropdown_set_selected(ui_droUserInit_tweakadv, config.SETTINGS.ADVANCED.USERINIT);
    lv_dropdown_set_selected(ui_droDPADSwap_tweakadv, config.SETTINGS.ADVANCED.DPADSWAP);
    lv_dropdown_set_selected(ui_droOverdrive_tweakadv, config.SETTINGS.ADVANCED.OVERDRIVE);

    lv_dropdown_set_selected(ui_droState_tweakadv, !strcasecmp(config.SETTINGS.ADVANCED.STATE, "freeze"));
    lv_dropdown_set_selected(ui_droCardMode_tweakadv, !strcasecmp(config.SETTINGS.ADVANCED.CARDMODE, "noop"));

    map_drop_down_to_index(ui_droAccelerate_tweakadv, config.SETTINGS.ADVANCED.ACCELERATE, accelerate_values, 17, 6);
    map_drop_down_to_index(ui_droSwapfile_tweakadv, config.SETTINGS.ADVANCED.SWAPFILE, swap_values, 9, 0);
    map_drop_down_to_index(ui_droZramfile_tweakadv, config.SETTINGS.ADVANCED.ZRAMFILE, zram_values, 9, 0);
}

static void save_tweak_options() {
    int is_modified = 0;

    CHECK_AND_SAVE_STD(tweakadv, Swap, "settings/advanced/swap", INT);
    CHECK_AND_SAVE_STD(tweakadv, Thermal, "settings/advanced/thermal", INT);
    CHECK_AND_SAVE_STD(tweakadv, Offset, "settings/advanced/offset", INT);
    CHECK_AND_SAVE_STD(tweakadv, Passcode, "settings/advanced/lock", INT);
    CHECK_AND_SAVE_STD(tweakadv, LED, "settings/advanced/led", INT);
    CHECK_AND_SAVE_STD(tweakadv, Theme, "settings/advanced/random_theme", INT);
    CHECK_AND_SAVE_STD(tweakadv, RetroWait, "settings/advanced/retrowait", INT);
    CHECK_AND_SAVE_STD(tweakadv, Verbose, "settings/advanced/verbose", INT);
    CHECK_AND_SAVE_STD(tweakadv, Rumble, "settings/advanced/rumble", INT);
    CHECK_AND_SAVE_STD(tweakadv, UserInit, "settings/advanced/user_init", INT);
    CHECK_AND_SAVE_STD(tweakadv, DPADSwap, "settings/advanced/dpad_swap", INT);
    CHECK_AND_SAVE_STD(tweakadv, Overdrive, "settings/advanced/overdrive", INT);

    CHECK_AND_SAVE_VAL(tweakadv, Volume, "settings/advanced/volume", CHAR, volume_values);
    CHECK_AND_SAVE_VAL(tweakadv, Brightness, "settings/advanced/brightness", CHAR, brightness_values);
    CHECK_AND_SAVE_VAL(tweakadv, State, "settings/advanced/state", CHAR, state_values);
    CHECK_AND_SAVE_VAL(tweakadv, CardMode, "settings/advanced/cardmode", CHAR, cardmode_values);

    CHECK_AND_SAVE_MAP(tweakadv, Accelerate, "settings/advanced/accelerate", accelerate_values, 17, 6);
    CHECK_AND_SAVE_MAP(tweakadv, Swapfile, "settings/advanced/swapfile", swap_values, 9, 0);
    CHECK_AND_SAVE_MAP(tweakadv, Zramfile, "settings/advanced/zramfile", zram_values, 9, 0);

    if (is_modified > 0) {
        toast_message(lang.GENERIC.SAVING, 0);
        refresh_screen(ui_screen);

        const char *args[] = {(INTERNAL_PATH "script/mux/tweak.sh"), NULL};
        run_exec(args, A_SIZE(args), 0);

        refresh_config = 1;
    }
}

static void init_navigation_group() {
#define UI_COUNT 19

    static lv_obj_t *ui_objects[UI_COUNT];
    static lv_obj_t *ui_objects_value[UI_COUNT];
    static lv_obj_t *ui_objects_glyph[UI_COUNT];
    static lv_obj_t *ui_objects_panel[UI_COUNT];

    int ui_index = 0;

    char *swap_options[] = {
            lang.MUXTWEAKADV.SWAP.RETRO,
            lang.MUXTWEAKADV.SWAP.MODERN
    };

    char *volume_options[] = {
            lang.GENERIC.PREVIOUS,
            lang.MUXTWEAKADV.VOLUME.SILENT,
            lang.MUXTWEAKADV.VOLUME.SOFT,
            lang.MUXTWEAKADV.VOLUME.LOUD
    };

    char *brightness_options[] = {
            lang.GENERIC.PREVIOUS,
            lang.MUXTWEAKADV.BRIGHT.LOW,
            lang.MUXTWEAKADV.BRIGHT.MEDIUM,
            lang.MUXTWEAKADV.BRIGHT.HIGH
    };

    char *rumble_options[] = {
            lang.GENERIC.DISABLED,
            lang.MUXTWEAKADV.RUMBLE.ST,
            lang.MUXTWEAKADV.RUMBLE.SH,
            lang.MUXTWEAKADV.RUMBLE.SL,
            lang.MUXTWEAKADV.RUMBLE.STSH,
            lang.MUXTWEAKADV.RUMBLE.STSL,
            lang.MUXTWEAKADV.RUMBLE.SHSL
    };

    char *state_options[] = {"mem", "freeze"};
    char *cardmode_options[] = {"deadline", "noop"};

    INIT_OPTION_ITEM(tweakadv, Accelerate, lang.MUXTWEAKADV.SPEED, "accelerate", NULL, 0);
    INIT_OPTION_ITEM(tweakadv, Swap, lang.MUXTWEAKADV.SWAP.TITLE, "swap", swap_options, 2);
    INIT_OPTION_ITEM(tweakadv, Thermal, lang.MUXTWEAKADV.THERMAL, "thermal", disabled_enabled, 2);
    INIT_OPTION_ITEM(tweakadv, Volume, lang.MUXTWEAKADV.VOLUME.TITLE, "volume", volume_options, 4);
    INIT_OPTION_ITEM(tweakadv, Brightness, lang.MUXTWEAKADV.BRIGHT.TITLE, "brightness", brightness_options, 4);
    INIT_OPTION_ITEM(tweakadv, Offset, lang.MUXTWEAKADV.OFFSET, "offset", NULL, 0);
    INIT_OPTION_ITEM(tweakadv, Passcode, lang.MUXTWEAKADV.LOCK, "lock", disabled_enabled, 2);
    INIT_OPTION_ITEM(tweakadv, LED, lang.MUXTWEAKADV.LED, "led", disabled_enabled, 2);
    INIT_OPTION_ITEM(tweakadv, Theme, lang.MUXTWEAKADV.RANDOM, "theme", disabled_enabled, 2);
    INIT_OPTION_ITEM(tweakadv, RetroWait, lang.MUXTWEAKADV.NET_WAIT, "retrowait", disabled_enabled, 2);
    INIT_OPTION_ITEM(tweakadv, State, lang.MUXTWEAKADV.STATE, "state", state_options, 2);
    INIT_OPTION_ITEM(tweakadv, Verbose, lang.MUXTWEAKADV.VERBOSE, "verbose", disabled_enabled, 2);
    INIT_OPTION_ITEM(tweakadv, Rumble, lang.MUXTWEAKADV.RUMBLE.TITLE, "rumble", rumble_options, 7);
    INIT_OPTION_ITEM(tweakadv, UserInit, lang.MUXTWEAKADV.USER_INIT, "userinit", disabled_enabled, 2);
    INIT_OPTION_ITEM(tweakadv, DPADSwap, lang.MUXTWEAKADV.DPAD, "dpadswap", disabled_enabled, 2);
    INIT_OPTION_ITEM(tweakadv, Overdrive, lang.MUXTWEAKADV.OVERDRIVE, "overdrive", disabled_enabled, 2);
    INIT_OPTION_ITEM(tweakadv, Swapfile, lang.MUXTWEAKADV.SWAPFILE, "swapfile", NULL, 0);
    INIT_OPTION_ITEM(tweakadv, Zramfile, lang.MUXTWEAKADV.ZRAMFILE, "zramfile", NULL, 0);
    INIT_OPTION_ITEM(tweakadv, CardMode, lang.MUXTWEAKADV.TUNING, "cardmode", cardmode_options, 2);

    char *accelerate_values = generate_number_string(16, 256, 16, lang.GENERIC.DISABLED, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droAccelerate_tweakadv, accelerate_values);
    free(accelerate_values);

    char *offset_values = generate_number_string(-50, 50, 1, NULL, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droOffset_tweakadv, offset_values);
    free(offset_values);

    char *swapfile_values = generate_number_string(64, 512, 64, lang.GENERIC.DISABLED, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droSwapfile_tweakadv, swapfile_values);
    free(swapfile_values);

    char *zramfile_values = generate_number_string(64, 512, 64, lang.GENERIC.DISABLED, NULL, NULL, 0);
    apply_theme_list_drop_down(&theme, ui_droZramfile_tweakadv, zramfile_values);
    free(zramfile_values);

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    ui_count = ui_index;

    for (unsigned int i = 0; i < ui_count; i++) {
        lv_group_add_obj(ui_group, ui_objects[i]);
        lv_group_add_obj(ui_group_value, ui_objects_value[i]);
        lv_group_add_obj(ui_group_glyph, ui_objects_glyph[i]);
        lv_group_add_obj(ui_group_panel, ui_objects_panel[i]);
    }

    if (!device.DEVICE.HAS_NETWORK) {
        lv_obj_add_flag(ui_pnlRetroWait_tweakadv, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        ui_count -= 1;
    }

    {
        // Removal of random theme because it is causing a number of issues
        lv_obj_add_flag(ui_pnlTheme_tweakadv, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        // Removal of verbose messages due to changes to muterm not playing ball
        lv_obj_add_flag(ui_pnlVerbose_tweakadv, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
        ui_count -= 2;
    }
}

static void list_nav_move(int steps, int direction) {
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_value, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void handle_option_prev(void) {
    if (msgbox_active) return;

    decrease_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_option_next(void) {
    if (msgbox_active) return;

    increase_option_value(lv_group_get_focused(ui_group_value));
}

static void handle_confirm(void) {
    if (msgbox_active) return;

    handle_option_next();
}

static void handle_back(void) {
    if (msgbox_active) {
        play_sound(SND_CONFIRM);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    play_sound(SND_BACK);

    save_tweak_options();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "advanced");

    close_input();
    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active) return;

    if (progress_onscreen == -1) {
        play_sound(SND_CONFIRM);
        show_help(lv_group_get_focused(ui_group));
    }
}

static void init_elements() {
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

    lv_label_set_text(ui_lblNavLR, lang.GENERIC.CHANGE);
    lv_label_set_text(ui_lblNavB, lang.GENERIC.SAVE);

    lv_obj_t *nav_hide[] = {
            ui_lblNavLRGlyph,
            ui_lblNavLR,
            ui_lblNavBGlyph,
            ui_lblNavB
    };

    for (int i = 0; i < sizeof(nav_hide) / sizeof(nav_hide[0]); i++) {
        lv_obj_clear_flag(nav_hide[i], LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_FLOATING);
    }

    lv_obj_set_user_data(ui_lblAccelerate_tweakadv, "accelerate");
    lv_obj_set_user_data(ui_lblSwap_tweakadv, "swap");
    lv_obj_set_user_data(ui_lblThermal_tweakadv, "thermal");
    lv_obj_set_user_data(ui_lblVolume_tweakadv, "volume");
    lv_obj_set_user_data(ui_lblBrightness_tweakadv, "brightness");
    lv_obj_set_user_data(ui_lblOffset_tweakadv, "offset");
    lv_obj_set_user_data(ui_lblPasscode_tweakadv, "lock");
    lv_obj_set_user_data(ui_lblLED_tweakadv, "led");
    lv_obj_set_user_data(ui_lblTheme_tweakadv, "theme");
    lv_obj_set_user_data(ui_lblRetroWait_tweakadv, "retrowait");
    lv_obj_set_user_data(ui_lblState_tweakadv, "state");
    lv_obj_set_user_data(ui_lblVerbose_tweakadv, "verbose");
    lv_obj_set_user_data(ui_lblRumble_tweakadv, "rumble");
    lv_obj_set_user_data(ui_lblUserInit_tweakadv, "userinit");
    lv_obj_set_user_data(ui_lblDPADSwap_tweakadv, "dpadswap");
    lv_obj_set_user_data(ui_lblOverdrive_tweakadv, "overdrive");
    lv_obj_set_user_data(ui_lblSwapfile_tweakadv, "swapfile");
    lv_obj_set_user_data(ui_lblZramfile_tweakadv, "zramfile");
    lv_obj_set_user_data(ui_lblCardMode_tweakadv, "cardmode");

#if TEST_IMAGE
    display_testing_message(ui_screen);
#endif

    if (kiosk.ENABLE) {
        kiosk_image = lv_img_create(ui_screen);
        load_kiosk_image(ui_screen, kiosk_image);
    }

    overlay_image = lv_img_create(ui_screen);
    load_overlay_image(ui_screen, overlay_image);
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panel_priority(ui_mux_panels, sizeof(ui_mux_panels) / sizeof(ui_mux_panels[0]));

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxtweakadv_main() {
    init_module("muxtweakadv");

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.MUXTWEAKADV.TITLE);
    init_muxtweakadv(ui_pnlContent);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    init_fonts();
    init_navigation_group();
    init_elements();

    restore_tweak_options();
    init_dropdown_settings();

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_confirm,
                    [MUX_INPUT_B] = handle_back,
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_LEFT] = handle_option_prev,
                    [MUX_INPUT_DPAD_RIGHT] = handle_option_next,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
