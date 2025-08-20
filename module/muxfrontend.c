#include "muxshare.h"
#include "../lvgl/src/drivers/display/sdl.h"

static int last_index = 0;
static int forced_flag = 0;
static int is_app = 0;

static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_sys[PATH_MAX];

static char previous_module[MAX_BUFFER_SIZE];
static char splash_image_path[MAX_BUFFER_SIZE];
static char alert_image_path[MAX_BUFFER_SIZE];

typedef struct {
    char *action;
    char *goback;
    char *module;

    int (*mux_main)(void);

    void (*mux_func)(void);
} ModuleEntry;

static void cleanup_screen(void) {
    SAFE_DELETE(toast_timer, lv_timer_del);
    SAFE_DELETE(counter_timer, lv_timer_del);
    SAFE_DELETE(key_entry, lv_obj_del);
    SAFE_DELETE(num_entry, lv_obj_del);

    if (ui_screen_container == NULL) return;

    init_dispose();
    lv_disp_load_scr(ui_screen_temp);

    if (ui_screen_container && lv_obj_is_valid(ui_screen_container)) {
        lv_obj_del(ui_screen_container);
        ui_screen_container = NULL;
    }

    current_item_index = 0;
    first_open = 1;
    key_curr = 0;
    key_show = 0;
    msgbox_active = 0;
    ui_count = 0;
    grid_mode_enabled = 0;

    snprintf(current_wall, sizeof(current_wall), "");
    snprintf(box_image_previous_path, sizeof(box_image_previous_path), "");
    snprintf(preview_image_previous_path, sizeof(preview_image_previous_path), "");
    snprintf(splash_image_previous_path, sizeof(splash_image_previous_path), "");
}

static void process_action(char *action, char *module) {
    if (!file_exist(action)) return;

    snprintf(rom_name, sizeof(rom_name), "%s", read_line_char_from(action, 1));
    snprintf(rom_dir, sizeof(rom_dir), "%s", read_line_char_from(action, 2));
    snprintf(rom_sys, sizeof(rom_sys), "%s", read_line_char_from(action, 3));

    forced_flag = read_line_int_from(action, 4);
    is_app = read_line_int_from(action, 5);

    remove(action);
    if (!is_app) load_mux(forced_flag ? "option" : module);
}

static void last_index_check(void) {
    last_index = 0;
    if (file_exist(MUOS_IDX_LOAD) && !file_exist(ADD_MODE_WORK)) {
        last_index = safe_atoi(read_line_char_from(MUOS_IDX_LOAD, 1));
        remove(MUOS_IDX_LOAD);
    }
}

static void set_previous_module(char *module) {
    snprintf(previous_module, sizeof(previous_module), "%s", module);
}

static int set_splash_image_path(char *splash_image_name) {
    const char *theme = theme_compat() ? STORAGE_THEME : INTERNAL_THEME;
    if ((snprintf(splash_image_path, sizeof(splash_image_path), "%s/%simage/%s/%s.png",
                  theme, mux_dimension, config.SETTINGS.GENERAL.LANGUAGE, splash_image_name) >= 0 &&
         file_exist(splash_image_path)) ||
        (snprintf(splash_image_path, sizeof(splash_image_path), "%s/%simage/%s.png",
                  theme, mux_dimension, splash_image_name) >= 0 && file_exist(splash_image_path)) ||
        (snprintf(splash_image_path, sizeof(splash_image_path), "%s/image/%s/%s.png",
                  theme, config.SETTINGS.GENERAL.LANGUAGE, splash_image_name) >= 0 && file_exist(splash_image_path)) ||
        (snprintf(splash_image_path, sizeof(splash_image_path), "%s/image/%s.png",
                  theme, splash_image_name) >= 0 && file_exist(splash_image_path))
            )
        return 1;

    return 0;
}

static int set_alert_image_path(void) {
    if ((snprintf(alert_image_path, sizeof(alert_image_path),
                  INTERNAL_PATH "share/media/alert-%s.png", device.SCREEN.HEIGHT < 720 ? "small" : "big") >= 0 &&
         file_exist(alert_image_path)))
        return 1;

    return 0;
}

static void exec_mux(char *goback, char *module, int (*func_to_exec)(void)) {
    LOG_DEBUG("muxfrontend", "GOBACK: %s | MODULE: %s", goback, module)

    load_mux(goback);
    func_to_exec();
    set_previous_module(module);
}

static void module_reset(void) {
    if (config.BOOT.FACTORY_RESET) safe_quit(0);
}

static void module_exit(char *module) {
    if (set_splash_image_path(module)) muxsplash_main(splash_image_path);

    load_mux(module);
    safe_quit(0);
}

static void module_shutdown(void) {
    module_exit("shutdown");
}

static void module_reboot(void) {
    module_exit("reboot");
}

static void module_quit(void) {
    safe_quit(0);
}

static void module_explore(void) {
    last_index_check();

    char *explore_dir = read_line_char_from(EXPLORE_DIR, 1);
    muxassign_main(1, rom_name, explore_dir, "none", 0);
    muxgov_main(1, rom_name, explore_dir, "none", 0);
    muxcontrol_main(1, rom_name, explore_dir, "none", 0);

    load_mux("launcher");
    if (muxplore_main(last_index, explore_dir) == 1) safe_quit(0);
}

static void module_content_list(const char *path, const char *max_depth, int is_collection) {
    last_index_check();

    const char *args[] = {"find", path, "-maxdepth", max_depth, "-type", "f", "-size", "0", "-delete", NULL};
    run_exec(args, A_SIZE(args), 0);

    load_mux("launcher");

    if (is_collection) {
        int add_mode = file_exist(ADD_MODE_WORK);
        if (add_mode) last_index = 0;

        if (muxcollect_main(add_mode, read_line_char_from(COLLECTION_DIR, 1), last_index) == 1) safe_quit(0);
    } else {
        if (muxhistory_main(last_index) == 1) safe_quit(0);
    }
}

static void module_collection(void) {
    const char *collection_path = (kiosk.COLLECT.ACCESS && directory_exist(INFO_CKS_PATH))
                                  ? INFO_CKS_PATH : INFO_COL_PATH;
    module_content_list(collection_path, "2", 1);
}

static void module_history(void) {
    module_content_list(INFO_HIS_PATH, "1", 0);
}

static void module_search(void) {
    load_mux("option");
    muxsearch_main(read_line_char_from(EXPLORE_DIR, 1));

    if (file_exist(MUOS_RES_LOAD)) {
        char *file_path = read_line_char_from(MUOS_RES_LOAD, 1);
        char *ex_directory = strip_dir(file_path);

        write_text_to_file(EXPLORE_DIR, "w", CHAR, ex_directory);
        write_text_to_file(EXPLORE_NAME, "w", CHAR, get_last_dir(file_path));

        cleanup_screen();
        load_mux("explore");
    }
}

static void module_picker(void) {
    load_mux("custom");
    muxpicker_main(read_line_char_from(MUOS_PIK_LOAD, 1), read_line_char_from(EXPLORE_DIR, 1));
}

static void module_run(const char *mux, int (*func_to_exec)(int, char *, char *, char *, int)) {
    load_mux(mux);
    func_to_exec(0, rom_name, rom_dir, rom_sys, is_app);
}

static void module_assign(void) {
    module_run("option", muxassign_main);
}

static void module_download(void) {
    load_mux("assign");
    muxdownload_main("core");
}

static void module_governor(void) {
    module_run("option", muxgov_main);
}

static void module_control(void) {
    module_run("option", muxcontrol_main);
}

static void module_tag(void) {
    module_run("option", muxtag_main);
}

static void module_option(void) {
    module_run("explore", muxoption_main);
}

static void module_appcon(void) {
    module_run("app", muxappcon_main);
}

static void module_app(void) {
    int auth = 0; // no more fights about 's' vs 'z'...

    if (config.SETTINGS.ADVANCED.LOCK && !file_exist(MUX_LAUNCHER_AUTH)) {
        load_mux("launcher");

        if (muxpass_main("launch") == 1) {
            cleanup_screen();
            write_text_to_file(MUX_LAUNCHER_AUTH, "w", CHAR, "");
            auth = 1;
        }
    } else {
        auth = 1;
    }

    if (auth) {
        exec_mux("launcher", "muxapp", muxapp_main);

        if (file_exist(MUOS_APP_LOAD)) {
            char *app = read_line_char_from(MUOS_APP_LOAD, 1);

            if (strcmp(app, "Archive Manager") == 0) {
                remove(MUOS_APP_LOAD);
                load_mux("archive");
            } else if (strcmp(app, "Task Toolkit") == 0) {
                remove(MUOS_APP_LOAD);
                load_mux("task");
            } else {
                load_mux("app");
                safe_quit(0);
            }
        }
    }
}

static void module_task(void) {
    load_mux("app");
    muxtask_main(read_line_char_from(EXPLORE_DIR, 1));
}

static void module_config(void) {
    if (config.SETTINGS.ADVANCED.LOCK && !file_exist(MUX_AUTH) &&
        strcmp(previous_module, "muxtweakgen") != 0) {
        load_mux("launcher");

        if (muxpass_main("setting") == 1) {
            cleanup_screen();

            write_text_to_file(MUX_AUTH, "w", CHAR, "");
            exec_mux("launcher", "muxconfig", muxconfig_main);
        }
    } else {
        exec_mux("launcher", "muxconfig", muxconfig_main);
    }
}

static void clear_auth(void) {
    if (!config.SETTINGS.ADVANCED.LOCK) {
        if (file_exist(MUX_AUTH)) remove(MUX_AUTH);
        if (file_exist(MUX_LAUNCHER_AUTH)) remove(MUX_LAUNCHER_AUTH);
    }
}

static void module_tweakadv(void) {
    exec_mux("tweakgen", "muxtweakadv", muxtweakadv_main);
    clear_auth();
}

static void module_danger(void) {
    exec_mux("tweakgen", "muxdanger", muxdanger_main);
    clear_auth();
}

static void module_device(void) {
    exec_mux("sysinfo", "muxdevice", muxdevice_main);
    clear_auth();
}

static void module_rtc(void) {
    if (config.BOOT.FACTORY_RESET) {
        exec_mux("reset", "muxrtc", muxrtc_main);
    } else {
        exec_mux("tweakgen", "muxrtc", muxrtc_main);
    }
}

static const ModuleEntry modules[] = {
        // these modules have specific functions and are not
        // straight forward module launching
        {"reset",      NULL, NULL, NULL, module_reset},
        {"reboot",     NULL, NULL, NULL, module_reboot},
        {"shutdown",   NULL, NULL, NULL, module_shutdown},
        {"assign",     NULL, NULL, NULL, module_assign},
        {"coredown",   NULL, NULL, NULL, module_download},
        {"governor",   NULL, NULL, NULL, module_governor},
        {"control",    NULL, NULL, NULL, module_control},
        {"tag",        NULL, NULL, NULL, module_tag},
        {"explore",    NULL, NULL, NULL, module_explore},
        {"collection", NULL, NULL, NULL, module_collection},
        {"history",    NULL, NULL, NULL, module_history},
        {"search",     NULL, NULL, NULL, module_search},
        {"picker",     NULL, NULL, NULL, module_picker},
        {"option",     NULL, NULL, NULL, module_option},
        {"appcon",     NULL, NULL, NULL, module_appcon},
        {"app",        NULL, NULL, NULL, module_app},
        {"task",       NULL, NULL, NULL, module_task},
        {"config",     NULL, NULL, NULL, module_config},
        {"tweakadv",   NULL, NULL, NULL, module_tweakadv},
        {"danger",     NULL, NULL, NULL, module_danger},
        {"device",     NULL, NULL, NULL, module_device},
        {"rtc",        NULL, NULL, NULL, module_rtc},
        {"credits",    NULL, NULL, NULL, module_quit},

        // the following modules can be loaded directly
        // without any other functionality
        {"launcher",    "launcher", "muxlaunch",      muxlaunch_main,      NULL},
        {"info",        "launcher", "muxinfo",        muxinfo_main,        NULL},
        {"archive",     "app",      "muxarchive",     muxarchive_main,     NULL},
        {"tweakgen",    "config",   "muxtweakgen",    muxtweakgen_main,    NULL},
        {"connect",     "config",   "muxconnect",     muxconnect_main,     NULL},
        {"custom",      "config",   "muxcustom",      muxcustom_main,      NULL},
        {"language",    "config",   "muxlanguage",    muxlanguage_main,    NULL},
        {"network",     "connect",  "muxnetwork",     muxnetwork_main,     NULL},
        {"webserv",     "connect",  "muxwebserv",     muxwebserv_main,     NULL},
        {"hdmi",        "tweakgen", "muxhdmi",        muxhdmi_main,        NULL},
        {"storage",     "config",   "muxstorage",     muxstorage_main,     NULL},
        {"backup",      "config",   "muxbackup",      muxbackup_main,      NULL},
        {"power",       "config",   "muxpower",       muxpower_main,       NULL},
        {"visual",      "config",   "muxvisual",      muxvisual_main,      NULL},
        {"kiosk",       "launcher", "muxkiosk",       muxkiosk_main,       NULL},
        {"net_profile", "network",  "muxnetprofile",  muxnetprofile_main,  NULL},
        {"net_scan",    "network",  "muxnetscan",     muxnetscan_main,     NULL},
        {"timezone",    "rtc",      "muxtimezone",    muxtimezone_main,    NULL},
        {"screenshot",  "info",     "muxshot",        muxshot_main,        NULL},
        {"space",       "info",     "muxspace",       muxspace_main,       NULL},
        {"themedwn",    "picker",   "muxthemedown",   muxthemedown_main,   NULL},
        {"themefilter", "themedwn", "muxthemefilter", muxthemefilter_main, NULL},
        {"tester",      "info",     "muxtester",      muxtester_main,      NULL},
        {"sysinfo",     "info",     "muxsysinfo",     muxsysinfo_main,     NULL},
        {"netinfo",     "info",     "muxnetinfo",     muxnetinfo_main,     NULL},
        {"text",        "info",     "muxtext",        muxtext_main,        NULL},

        // this is required because it is the end of the table!
        {NULL,         NULL, NULL, NULL,                                   NULL}
};

void init_audio(void) {
    int r = 10;
    while (r-- > 0) {
        if (init_audio_backend()) {
            init_fe_snd(&fe_snd, config.SETTINGS.GENERAL.SOUND, 0);
            init_fe_bgm(&fe_bgm, config.SETTINGS.GENERAL.BGM, 0);

            break;
        }

        // start at 50ms then double exponent - stop at 1s max
        // tbqh if it isn't initialised in this time something
        // is really wrong with the device audio initialisation...
        useconds_t delay = (50000U << r);
        if (delay > 1000000U) delay = 1000000U;
        usleep(delay);
    }

    if (!file_exist(CHIME_DONE) && config.SETTINGS.GENERAL.CHIME) play_sound(SND_STARTUP);
    write_text_to_file(CHIME_DONE, "w", CHAR, "");
}

int main(void) {
    load_device(&device);
    load_config(&config);
    load_kiosk(&kiosk);

    LOG_SUCCESS("hello", "Welcome to the %s - %s", MUX_CALLER, get_build_version())

    // For future reference we need to initialise the theme before we do the display
    // as we call upon the theme variables for specific settings within display init
    init_theme(0, 0);
    init_display(0);

    int show_alert = 0;
    if (!file_exist(DONE_RESET) && read_line_int_from(USED_RESET, 1)) show_alert = 1;

    write_text_to_file(USED_RESET, "w", INT, 1);
    write_text_to_file(DONE_RESET, "w", INT, 1);

    if (!config.BOOT.FACTORY_RESET && show_alert && set_alert_image_path()) {
        muxsplash_main(alert_image_path);
        sleep(3);
    }

    init_audio();

    while (1) {
        if (file_exist(SAFE_QUIT)) {
            LOG_DEBUG("muxfrontend", "Safe Quit Detected... exiting!")
            break;
        }

        char folder[MAX_BUFFER_SIZE];
        snprintf(folder, sizeof(folder), "%s/%dx%d",
                 config.BOOT.FACTORY_RESET ? INTERNAL_THEME : STORAGE_THEME,
                 device.MUX.WIDTH, device.MUX.HEIGHT);

        if (refresh_resolution || !directory_exist(folder)) {
            LOG_DEBUG("muxfrontend", "Resolution or Theme Refreshed... exiting!")
            safe_quit(0);
            break;
        }

        // Process application option loader
        process_action(MUOS_APL_LOAD, "");

        // Process content association and governor actions
        process_action(MUOS_ASS_LOAD, "assign");
        process_action(MUOS_GOV_LOAD, "governor");

        if (file_exist(MUOS_ACT_LOAD)) {
            if (refresh_kiosk) {
                sync();
                load_kiosk(&kiosk);
                refresh_kiosk = 0;
            }

            if (refresh_config) {
                sync();
                load_config(&config);
                refresh_config = 0;
            }

            if (refresh_device) {
                sync();
                load_device(&device);
                refresh_device = 0;
            }

            int go_mux = 0;
            for (size_t i = 0; modules[i].action != NULL; ++i) {
                if (strcmp(read_line_char_from(MUOS_ACT_LOAD, 1), modules[i].action) == 0) {
                    modules[i].mux_func
                    ? modules[i].mux_func()
                    : exec_mux(modules[i].goback, modules[i].module, modules[i].mux_main);
                    go_mux = 1;
                    break;
                }
            }

            if (!go_mux) exec_mux("launcher", "muxlaunch", muxlaunch_main);
        } else {
            exec_mux("launcher", "muxlaunch", muxlaunch_main);
        }

        cleanup_screen();
    }

    sdl_cleanup();
    return 0;
}
