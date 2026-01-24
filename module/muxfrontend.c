#include <signal.h>
#include <sys/prctl.h>

#include "muxshare.h"
#include "../common/inotify.h"
#include "../lvgl/src/drivers/display/sdl.h"

static volatile sig_atomic_t quit_signal = 0;
static volatile sig_atomic_t shutting_down = 0;

static int first_boot = 1;
static int screen_clean = 1;
static int last_index = 0;
static int forced_flag = 0;
static int is_app = 0;

static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_sys[PATH_MAX];

static char previous_module[MAX_BUFFER_SIZE];
static char splash_image_path[MAX_BUFFER_SIZE];
static char alert_image_path[MAX_BUFFER_SIZE];

/*
 * Sometimes only executed functions (shutdown / reboot / install)
 * No behaviour change from what is understood...
 * Just better locality on small-ish CPUs
 * https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-cold-function-attribute
 */
__attribute__((cold)) static void module_shutdown(void);

__attribute__((cold)) static void module_reboot(void);

__attribute__((cold)) static void module_install(void);

typedef struct {
    char *action;
    char *goback;
    char *module;

    int (*mux_main)(void);

    void (*mux_func)(void);
} ModuleEntry;

static void on_signal(int sig) {
    quit_signal = sig ? sig : 1;
}

static void install_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // Do NOT use SA_RESTART (we want blocking syscalls to unblock)

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
}

static void cleanup_screen(void) {
    if (screen_clean) return;
    screen_clean = 1;

    SAFE_DELETE(toast_timer, lv_timer_del);
    SAFE_DELETE(counter_timer, lv_timer_del);
    SAFE_DELETE(key_entry, lv_obj_del);
    SAFE_DELETE(num_entry, lv_obj_del);

    if (!ui_screen_container) return;

    dispose_input();
    timer_suspend_all();

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

    RESET_PATH(current_wall);
    RESET_PATH(box_image_previous_path);
    RESET_PATH(preview_image_previous_path);
    RESET_PATH(splash_image_previous_path);
}

static void cleanup_all(void) {
    dispose_input();
    timer_destroy_all();
    cleanup_screen();
    sdl_cleanup();
}

static void quit_watchdog(lv_timer_t *timer) {
    LV_UNUSED(timer);

    if (shutting_down) return;
    inotify_check(ino_proc);

    if (safe_quit_exists || quit_signal) {
        LOG_DEBUG("muxfrontend", "Signal %d received, requesting safe quit...", (int) quit_signal);
        shutting_down = 1;
        safe_quit_exists = 0;

        cleanup_all();
        exit(0);
    }
}

static int process_action(const char *action, const char *module) {
    if (!file_exist(action)) return 0;

    char *name = read_line_char_from(action, 1);
    char *dir = read_line_char_from(action, 2);
    char *sys = read_line_char_from(action, 3);

    if (!name || !dir || !sys) {
        remove(action);
        return 0;
    }

    snprintf(rom_name, sizeof(rom_name), "%s", name);
    snprintf(rom_dir, sizeof(rom_dir), "%s", dir);
    snprintf(rom_sys, sizeof(rom_sys), "%s", sys);

    forced_flag = read_line_int_from(action, 4);
    is_app = read_line_int_from(action, 5);

    remove(action);

    if (!is_app) {
        screen_clean = 0;
        load_mux(forced_flag ? "option" : module);
    }

    return 1;
}

void last_index_check(void) {
    last_index = 0;
    if (file_exist(MUOS_IDX_LOAD) && !file_exist(ADD_MODE_WORK)) {
        last_index = safe_atoi(read_line_char_from(MUOS_IDX_LOAD, 1));
        remove(MUOS_IDX_LOAD);
    }
}

static void set_previous_module(char *module) {
    snprintf(previous_module, sizeof(previous_module), "%s", module);
}

int set_splash_image_path(char *splash_image_name) {
    if ((snprintf(splash_image_path, sizeof(splash_image_path), "%s/%simage/%s/%s.png",
                  theme_base, mux_dimension, config.SETTINGS.GENERAL.LANGUAGE, splash_image_name) >= 0 &&
         file_exist(splash_image_path)) ||
        (snprintf(splash_image_path, sizeof(splash_image_path), "%s/%simage/%s.png",
                  theme_base, mux_dimension, splash_image_name) >= 0 && file_exist(splash_image_path)) ||
        (snprintf(splash_image_path, sizeof(splash_image_path), "%s/image/%s/%s.png",
                  theme_base, config.SETTINGS.GENERAL.LANGUAGE, splash_image_name) >= 0 && file_exist(splash_image_path)) ||
        (snprintf(splash_image_path, sizeof(splash_image_path), "%s/image/%s.png",
                  theme_base, splash_image_name) >= 0 && file_exist(splash_image_path)))
        return 1;

    return 0;
}

static int set_alert_image_path(void) {
    if ((snprintf(alert_image_path, sizeof(alert_image_path),
                  OPT_PATH "share/media/alert-%s.png", device.SCREEN.HEIGHT < 720 ? "small" : "big") >= 0 &&
         file_exist(alert_image_path)))
        return 1;

    return 0;
}

static void exec_mux(char *goback, char *module, int (*func_to_exec)(void)) {
    screen_clean = 0;

    LOG_DEBUG("muxfrontend", "GOBACK: %s | MODULE: %s", goback, module);

    load_mux(goback);
    func_to_exec();
    set_previous_module(module);
}

static void module_reset(void) {
    if (config.BOOT.FACTORY_RESET) safe_quit(0);
}

static void module_exit(char *module, bool apply_recolour) {
    if (set_splash_image_path(module)) muxsplash_main(splash_image_path, apply_recolour);

    load_mux(module);
    safe_quit(0);
}

static void module_shutdown(void) {
    module_exit("shutdown", true);
}

static void module_reboot(void) {
    module_exit("reboot", true);
}

static void module_install(void) {
    module_exit("install", false);
}

static void module_credits(void) {
    load_mux("credits");
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

void module_content_list(const char *path, const char *max_depth, int is_collection) {
    last_index_check();

    const char *args[] = {"find", path, "-maxdepth", max_depth,
                          "-type", "f", "-size", "0", "!", "-name", ".nogrid",
                          "-delete", NULL};
    run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);

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
    const char *collection_path = (is_ksk(kiosk.COLLECT.ACCESS) && directory_exist(INFO_CKS_PATH))
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

static void module_theme(void) {
    load_mux("custom");
    muxtheme_main(read_line_char_from(EXPLORE_DIR, 1));
}

void module_run(const char *mux, int (*func_to_exec)(int, char *, char *, char *, int)) {
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

    if (config.SETTINGS.ADVANCED.PASSCODE) {
        load_mux("launcher");

        if (muxpass_main(PCT_LAUNCH) == 1) {
            cleanup_screen();
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
    if (config.SETTINGS.ADVANCED.PASSCODE && strcmp(previous_module, "muxtweakgen") != 0) {
        load_mux("launcher");

        if (muxpass_main(PCT_CONFIG) == 1) {
            cleanup_screen();

            exec_mux("launcher", "muxconfig", muxconfig_main);
        }
    } else {
        exec_mux("launcher", "muxconfig", muxconfig_main);
    }
}

static void module_tweakadv(void) {
    exec_mux("tweakgen", "muxtweakadv", muxtweakadv_main);
}

static void module_danger(void) {
    exec_mux("tweakgen", "muxdanger", muxdanger_main);
}

static void module_device(void) {
    exec_mux("sysinfo", "muxdevice", muxdevice_main);
}

static void module_rtc(void) {
    if (config.BOOT.FACTORY_RESET) {
        exec_mux("installer", "muxrtc", muxrtc_main);
    } else {
        exec_mux("tweakgen", "muxrtc", muxrtc_main);
    }
}

static void module_start(void) {
    if (config.BOOT.FACTORY_RESET) {
        exec_mux("installer", "muxinstall", muxinstall_main);
    } else {
        exec_mux("launcher", "muxlaunch", muxlaunch_main);
    }
}

static void module_refresh(void) {
    if (!(refresh_kiosk | refresh_config | refresh_device)) return;

    if (refresh_kiosk) load_kiosk(&kiosk);
    if (refresh_config) load_config(&config);
    if (refresh_device) load_device(&device);

    refresh_kiosk = refresh_config = refresh_device = 0;
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
        {"theme",      NULL, NULL, NULL, module_theme},
        {"option",     NULL, NULL, NULL, module_option},
        {"appcon",     NULL, NULL, NULL, module_appcon},
        {"app",        NULL, NULL, NULL, module_app},
        {"task",       NULL, NULL, NULL, module_task},
        {"config",     NULL, NULL, NULL, module_config},
        {"tweakadv",   NULL, NULL, NULL, module_tweakadv},
        {"danger",     NULL, NULL, NULL, module_danger},
        {"device",     NULL, NULL, NULL, module_device},
        {"rtc",        NULL, NULL, NULL, module_rtc},
        {"credits",    NULL, NULL, NULL, module_credits},

        // the following modules can be loaded directly
        // without any other functionality
        {"launcher",    "launcher",  "muxlaunch",      muxlaunch_main,      NULL},
        {"info",        "launcher",  "muxinfo",        muxinfo_main,        NULL},
        {"archive",     "app",       "muxarchive",     muxarchive_main,     NULL},
        {"tweakgen",    "config",    "muxtweakgen",    muxtweakgen_main,    NULL},
        {"overlay",     "config",    "muxoverlay",     muxoverlay_main,     NULL},
        {"connect",     "config",    "muxconnect",     muxconnect_main,     NULL},
        {"custom",      "config",    "muxcustom",      muxcustom_main,      NULL},
        {"language",    "config",    "muxlanguage",    muxlanguage_main,    NULL},
        {"network",     "connect",   "muxnetwork",     muxnetwork_main,     NULL},
        {"netadv",      "connect",   "muxnetadv",      muxnetadv_main,      NULL},
        {"webserv",     "connect",   "muxwebserv",     muxwebserv_main,     NULL},
        {"hdmi",        "tweakgen",  "muxhdmi",        muxhdmi_main,        NULL},
        {"storage",     "config",    "muxstorage",     muxstorage_main,     NULL},
        {"backup",      "config",    "muxbackup",      muxbackup_main,      NULL},
        {"power",       "config",    "muxpower",       muxpower_main,       NULL},
        {"visual",      "config",    "muxvisual",      muxvisual_main,      NULL},
        {"kiosk",       "launcher",  "muxkiosk",       muxkiosk_main,       NULL},
        {"net_profile", "network",   "muxnetprofile",  muxnetprofile_main,  NULL},
        {"net_scan",    "network",   "muxnetscan",     muxnetscan_main,     NULL},
        {"timezone",    "rtc",       "muxtimezone",    muxtimezone_main,    NULL},
        {"screenshot",  "info",      "muxshot",        muxshot_main,        NULL},
        {"space",       "info",      "muxspace",       muxspace_main,       NULL},
        {"news",        "info",      "muxnews",        muxnews_main,        NULL},
        {"activity",    "info",      "muxactivity",    muxactivity_main,    NULL},
        {"themedwn",    "picker",    "muxthemedown",   muxthemedown_main,   NULL},
        {"themefilter", "themedwn",  "muxthemefilter", muxthemefilter_main, NULL},
        {"tester",      "info",      "muxtester",      muxtester_main,      NULL},
        {"sysinfo",     "info",      "muxsysinfo",     muxsysinfo_main,     NULL},
        {"netinfo",     "info",      "muxnetinfo",     muxnetinfo_main,     NULL},
        {"chrony",      "info",      "muxchrony",      muxchrony_main,      NULL},
        {"text",        "info",      "muxtext",        muxtext_main,        NULL},

        // these are custom entries specifically for the first time installer
        {"installer",   "installer", "muxinstall",     muxinstall_main,     NULL},
        {"install",    NULL, NULL, NULL, module_install},

        // this is required because it is the end of the table!
        {NULL,         NULL, NULL, NULL,                                    NULL}
};

static int module_dispatch(void) {
    if (!file_exist(MUOS_ACT_LOAD)) return 0;

    char *action = read_line_char_from(MUOS_ACT_LOAD, 1);
    if (!action) return 0;

    remove(MUOS_ACT_LOAD);

    for (size_t i = 0; modules[i].action; ++i) {
        if (strcmp(action, modules[i].action) == 0) {
            screen_clean = 0;

            if (modules[i].mux_func) {
                modules[i].mux_func();
            } else {
                exec_mux(modules[i].goback, modules[i].module, modules[i].mux_main);
            }

            return 1;
        }
    }

    return 0;
}

static void reset_alert(void) {
    if (config.BOOT.FACTORY_RESET) return;

    int show_alert = 0;
    if (!file_exist(DONE_RESET) && read_line_int_from(USED_RESET, 1)) show_alert = 1;

    write_text_to_file(USED_RESET, "w", INT, 1);
    write_text_to_file(DONE_RESET, "w", INT, 1);

    if (show_alert && set_alert_image_path()) {
        muxsplash_main(alert_image_path, false);
        sleep(3);
    }
}

static void init_audio(void) {
    const useconds_t backoff[] = {10000, 25000, 50000, 100000, 200000, 400000, 800000};
    size_t tries = sizeof(backoff) / sizeof(backoff[0]);

    for (size_t i = 0; i < tries; ++i) {
        if (init_audio_backend()) {
            init_fe_snd(&fe_snd, config.SETTINGS.GENERAL.SOUND, 0);
            init_fe_bgm(&fe_bgm, config.SETTINGS.GENERAL.BGM, 0);

            if (!file_exist(CHIME_DONE) &&
                config.SETTINGS.GENERAL.CHIME &&
                !config.SETTINGS.ADVANCED.PASSCODE)
                play_sound(SND_STARTUP);

            write_text_to_file(CHIME_DONE, "w", CHAR, "");
            return;
        }

        usleep(backoff[i]);
    }
}

int main(void) {
    install_signal_handlers();
    verify_check = script_hash_check();

    // If parent (frontend.sh) dies, ask the kernel nicely to send us a SIGTERM hopefully
    prctl(PR_SET_PDEATHSIG, SIGTERM);

    // Close the stupid race where the parent already died before the prctl call or we get a segfault...
    if (getppid() == 1) raise(SIGTERM);

    load_device(&device);
    load_config(&config);
    load_kiosk(&kiosk);

    LOG_SUCCESS("hello", "Welcome to the %s - %s (%s)", MUX_CALLER, get_version(verify_check), get_build());
    if (verify_check) LOG_ERROR("muxfrontend", "Internal script modifications have been detected!");

    // For future reference we need to initialise the theme before we do the display
    // as we call upon the theme variables for specific settings within display init
    init_theme(0, 0);
    init_display();

    lv_timer_create(quit_watchdog, 100, NULL);

    reset_alert();
    init_audio();

    if (config.SETTINGS.ADVANCED.PASSCODE && !file_exist(MUX_BOOT_AUTH)) {
        int result = 0;

        while (result != 1) {
            result = muxpass_main(PCT_BOOT);

            if (result == 2) {
                cleanup_screen();
                module_shutdown();
            }
        }

        cleanup_screen();
        write_text_to_file(MUX_BOOT_AUTH, "w", CHAR, "");
    }

    while (1) {
        if (file_exist(SAFE_QUIT)) {
            LOG_DEBUG("muxfrontend", "Safe Quit Detected... exiting!");
            break;
        }

        if (refresh_resolution) {
            LOG_DEBUG("muxfrontend", "Resolution or Theme Refreshed... exiting!");

            shutting_down = 1;
            safe_quit(0);

            break;
        }

        if (first_boot) {
            first_boot = 0;
            lv_task_handler();
        }

        // Process application option loader
        process_action(MUOS_APL_LOAD, "");

        // Process content association and governor actions
        process_action(MUOS_ASS_LOAD, "assign");
        process_action(MUOS_GOV_LOAD, "governor");

        module_refresh();

        if (file_exist(MUOS_ACT_LOAD)) {
            if (!module_dispatch()) module_start();
        } else {
            module_start();
        }

        cleanup_screen();
    }

    cleanup_all();
    return 0;
}
