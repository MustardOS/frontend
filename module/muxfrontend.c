#include "./ui/ui_muxlaunch.h"
#include "muxshare.h"
#include <limits.h>
#include "../common/common.h"
#include "../common/init.h"
#include "../common/log.h"
#include "../common/ui_common.h"
#include "../common/osk.h"
#include "../common/input/list_nav.h"

#include "muxapp.h"
#include "muxarchive.h"
#include "muxassign.h"
#include "muxcollect.h"
#include "muxconfig.h"
#include "muxconnect.h"
#include "muxcustom.h"
#include "muxgov.h"
#include "muxhdmi.h"
#include "muxhistory.h"
#include "muxinfo.h"
#include "muxlanguage.h"
#include "muxlaunch.h"
#include "muxnetprofile.h"
#include "muxnetscan.h"
#include "muxnetwork.h"
#include "muxoption.h"
#include "muxpass.h"
#include "muxpicker.h"
#include "muxpower.h"
#include "muxplore.h"
#include "muxrtc.h"
#include "muxsearch.h"
#include "muxshot.h"
#include "muxsnapshot.h"
#include "muxspace.h"
#include "muxsplash.h"
#include "muxstorage.h"
#include "muxsysinfo.h"
#include "muxtask.h"
#include "muxtester.h"
#include "muxtimezone.h"
#include "muxtweakadv.h"
#include "muxtweakgen.h"
#include "muxvisual.h"
#include "muxwebserv.h"

static int last_index = 0;
static char rom_name[PATH_MAX];
static char rom_dir[PATH_MAX];
static char rom_sys[PATH_MAX];
static char forced_flag[PATH_MAX];

static char previous_module[MAX_BUFFER_SIZE];
static char splash_image_path[MAX_BUFFER_SIZE];

static void cleanup_screen() {
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
}

static void process_content_action(char *action, char *module) {
    if (!file_exist(action)) return;

    snprintf(rom_name, sizeof(rom_name), "%s", read_line_from_file(action, 1));
    snprintf(rom_dir, sizeof(rom_dir), "%s", read_line_from_file(action, 2));
    snprintf(rom_sys, sizeof(rom_sys), "%s", read_line_from_file(action, 3));
    snprintf(forced_flag, sizeof(forced_flag), "%s", read_line_from_file(action, 4));

    remove(action);
    load_mux((strcmp(forced_flag, "1") == 0) ? "option" : module);
}

static void last_index_check() {
    last_index = 0;
    if (file_exist(MUOS_IDX_LOAD) && !file_exist(ADD_MODE_WORK)) {
        last_index = safe_atoi(read_line_from_file(MUOS_IDX_LOAD, 1));
        remove(MUOS_IDX_LOAD);
    }
}

static void set_previous_module(char *module) {
    snprintf(previous_module, sizeof(previous_module), "%s", module);
}

static int set_splash_image_path(char *splash_image_name) {
    char mux_dimension[15];
    get_mux_dimension(mux_dimension, sizeof(mux_dimension));
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

static void exec_mux(char *goback, char *module, int (*func_to_exec)(void)) {
    LOG_DEBUG("muxfrontend", "GOBACK: %s | MODULE: %s", goback, module)
    load_mux(goback);
    func_to_exec();
    set_previous_module(module);
}

int main() {
    setup_background_process();

    load_device(&device);
    load_config(&config);
    init_theme(0, 0);
    init_display();

    int r = 10;
    while (r-- > 0) {
        if (init_audio_backend()) {
            init_fe_snd(&fe_snd, config.SETTINGS.GENERAL.SOUND, 0);
            init_fe_bgm(&fe_bgm, config.SETTINGS.GENERAL.BGM, 0);
            break;
        }
        usleep(250000);
    }

    play_sound(SND_STARTUP, 0);

    while (1) {
        char *theme_location = config.BOOT.FACTORY_RESET ? INTERNAL_THEME : STORAGE_THEME;
        char theme_device_folder[MAX_BUFFER_SIZE];
        snprintf(theme_device_folder, sizeof(theme_device_folder), "%s/%dx%d",
                 theme_location, device.MUX.WIDTH, device.MUX.HEIGHT);
        if (refresh_resolution || !directory_exist(theme_device_folder)) {
            safe_quit(0);
            break;
        }
        // Process content association and governor actions
        process_content_action(MUOS_ASS_LOAD, "assign");
        process_content_action(MUOS_GOV_LOAD, "governor");

        if (file_exist(MUOS_ACT_LOAD)) {
            if (refresh_config) {
                load_config(&config);
                refresh_config = 0;
            }
            char *action = read_line_from_file(MUOS_ACT_LOAD, 1);
            if (strcmp(action, "reset") == 0) {
                if (config.BOOT.FACTORY_RESET) {
                    safe_quit(0);
                    break;
                }
            } else if (strcmp(action, "reboot") == 0) {
                if (set_splash_image_path("reboot")) muxsplash_main(splash_image_path);
                safe_quit(0);
                break;
            } else if (strcmp(action, "shutdown") == 0) {
                if (set_splash_image_path("shutdown")) muxsplash_main(splash_image_path);
                safe_quit(0);
                break;
            } else if (strcmp(action, "assign") == 0) {
                load_mux("option");
                muxassign_main(0, rom_name, rom_dir, rom_sys);
            } else if (strcmp(action, "governor") == 0) {
                load_mux("option");
                muxgov_main(0, rom_name, rom_dir, rom_sys);
            } else if (strcmp(action, "explore") == 0) {
                last_index_check();
                char *explore_dir = read_line_from_file(EXPLORE_DIR, 1);
                muxassign_main(1, rom_name, explore_dir, "none");
                muxgov_main(1, rom_name, explore_dir, "none");
                load_mux("launcher");
                if (muxplore_main(last_index, explore_dir) == 1) {
                    safe_quit(0);
                    break;
                }
            } else if (strcmp(action, "collection") == 0) {
                last_index_check();
                int add_mode = 0;
                if (file_exist(ADD_MODE_WORK)) {
                    add_mode = 1;
                    last_index = 0;
                }
                const char *args[] = {"find", (const char *) INFO_COL_PATH, "-maxdepth", "2", "-type", "f",
                                      "-size", "0", "-delete", NULL};
                run_exec(args, A_SIZE(args), 0);
                load_mux("launcher");
                if (muxcollect_main(add_mode, read_line_from_file(COLLECTION_DIR, 1), last_index) == 1) {
                    safe_quit(0);
                    break;
                }
            } else if (strcmp(action, "history") == 0) {
                last_index_check();
                const char *args[] = {"find", (const char *) INFO_HIS_PATH, "-maxdepth", "1", "-type", "f",
                                      "-size", "0", "-delete", NULL};
                run_exec(args, A_SIZE(args), 0);
                load_mux("launcher");
                if (muxhistory_main(last_index) == 1) {
                    safe_quit(0);
                    break;
                }
            } else if (strcmp(action, "search") == 0) {
                load_mux("option");
                muxsearch_main(read_line_from_file(EXPLORE_DIR, 1));
                if (file_exist(MUOS_RES_LOAD)) {
                    char *file_path = read_line_from_file(MUOS_RES_LOAD, 1);
                    char *ex_directory = strip_dir(file_path);
                    write_text_to_file(EXPLORE_DIR, "w", CHAR, ex_directory);
                    write_text_to_file(EXPLORE_NAME, "w", CHAR, get_last_dir(file_path));
                    cleanup_screen();
                    load_mux("explore");
                }
            } else if (strcmp(action, "app") == 0) {
                int authorized = 0;
                if (config.SETTINGS.ADVANCED.LOCK && !file_exist(MUX_LAUNCHER_AUTH)) {
                    load_mux("launcher");
                    if (muxpass_main("launch") == 1) {
                        cleanup_screen();
                        write_text_to_file(MUX_LAUNCHER_AUTH, "w", CHAR, "");
                        authorized = 1;
                    }
                } else {
                    authorized = 1;
                }
                if (authorized) {
                    exec_mux("launcher", "muxapp", muxapp_main);
                    if (file_exist(MUOS_APP_LOAD)) {
                        char *app = read_line_from_file(MUOS_APP_LOAD, 1);
                        if (strcmp(app, "Archive Manager") == 0) {
                            remove(MUOS_APP_LOAD);
                            load_mux("archive");
                        } else if (strcmp(app, "Task Toolkit") == 0) {
                            remove(MUOS_APP_LOAD);
                            load_mux("task");
                        } else {
                            load_mux("app");
                            safe_quit(0);
                            break;
                        }
                    }
                }
            } else if (strcmp(action, "config") == 0) {
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
            } else if (strcmp(action, "tweakadv") == 0) {
                exec_mux("tweakgen", "muxtweakadv", muxtweakadv_main);
                if (!config.SETTINGS.ADVANCED.LOCK) {
                    if (file_exist(MUX_AUTH)) remove(MUX_AUTH);
                    if (file_exist(MUX_LAUNCHER_AUTH)) remove(MUX_LAUNCHER_AUTH);
                }
            } else if (strcmp(action, "credits") == 0) {
                safe_quit(0);
                break;
            } else if (strcmp(action, "option") == 0) {
                load_mux("explore");
                muxoption_main(rom_name, rom_dir, rom_sys);
            } else if (strcmp(action, "picker") == 0) {
                load_mux("custom");
                muxpicker_main(read_line_from_file(MUOS_PIK_LOAD, 1), read_line_from_file(EXPLORE_DIR, 1));
            } else if (strcmp(action, "info") == 0) {
                exec_mux("launcher", "muxinfo", muxinfo_main);
            } else if (strcmp(action, "archive") == 0) {
                exec_mux("app", "muxarchive", muxarchive_main);
            } else if (strcmp(action, "task") == 0) {
                exec_mux("app", "muxtask", muxtask_main);
            } else if (strcmp(action, "tweakgen") == 0) {
                exec_mux("config", "muxtweakgen", muxtweakgen_main);
            } else if (strcmp(action, "connect") == 0) {
                exec_mux("config", "muxconnect", muxconnect_main);
            } else if (strcmp(action, "custom") == 0) {
                exec_mux("config", "muxcustom", muxcustom_main);
            } else if (strcmp(action, "network") == 0) {
                exec_mux("connect", "muxnetwork", muxnetwork_main);
            } else if (strcmp(action, "language") == 0) {
                exec_mux("config", "muxlanguage", muxlanguage_main);
            } else if (strcmp(action, "webserv") == 0) {
                exec_mux("connect", "muxwebserv", muxwebserv_main);
            } else if (strcmp(action, "hdmi") == 0) {
                exec_mux("tweakgen", "muxhdmi", muxhdmi_main);
            } else if (strcmp(action, "rtc") == 0) {
                if (config.BOOT.FACTORY_RESET) {
                    exec_mux("reset", "muxrtc", muxrtc_main);
                } else {
                    exec_mux("tweakgen", "muxrtc", muxrtc_main);
                }
            } else if (strcmp(action, "storage") == 0) {
                exec_mux("config", "muxstorage", muxstorage_main);
            } else if (strcmp(action, "power") == 0) {
                exec_mux("config", "muxpower", muxpower_main);
            } else if (strcmp(action, "visual") == 0) {
                exec_mux("config", "muxvisual", muxvisual_main);
            } else if (strcmp(action, "net_profile") == 0) {
                exec_mux("network", "muxnetprofile", muxnetprofile_main);
            } else if (strcmp(action, "net_scan") == 0) {
                exec_mux("network", "muxnetscan", muxnetscan_main);
            } else if (strcmp(action, "timezone") == 0) {
                exec_mux("rtc", "muxtimezone", muxtimezone_main);
            } else if (strcmp(action, "screenshot") == 0) {
                exec_mux("info", "muxshot", muxshot_main);
            } else if (strcmp(action, "space") == 0) {
                exec_mux("info", "muxspace", muxspace_main);
            } else if (strcmp(action, "tester") == 0) {
                exec_mux("info", "muxtester", muxtester_main);
            } else if (strcmp(action, "system") == 0) {
                exec_mux("info", "muxsysinfo", muxsysinfo_main);
            } else {
                exec_mux("launcher", "muxlaunch", muxlaunch_main);
            }
        } else {
            exec_mux("launcher", "muxlaunch", muxlaunch_main);
        }
        cleanup_screen();
    }

    return 0;
}
