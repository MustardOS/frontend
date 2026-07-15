#include "muxshare.h"
#include "ui/ui_muxsysinfo.h"

#define SYSINFO(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(SYSINFO_ELEMENTS) };
#undef SYSINFO

#define UI_BUFFER 128

static char hostname[32];
static int tap_count = 0;

static struct sysinfo sysinfo_cache;

static void show_help(void) {
    const struct help_msg help_messages[] = {
#define SYSINFO(NAME, UDATA) {UDATA, lang.muxsysinfo.help.NAME},
        SYSINFO_ELEMENTS
#undef SYSINFO
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, items);
}

static int read_file_trim(const char *path, char *out) {
    if (!out) return -1;

    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    if (!fgets(out, UI_BUFFER, fp)) {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    const char *start = out;
    while (*start && isspace((unsigned char) *start))
        start++;

    const char *end = start + strlen(start);
    while (end > start && isspace((unsigned char) end[-1]))
        end--;

    const size_t len = (size_t) (end - start);
    if (start != out) memmove(out, start, len);
    out[len] = '\0';

    return len > 0 ? 0 : -1;
}

static int read_ll_from_file(const char *path, unsigned long long *val) {
    if (!path) return -1;

    char buffer[UI_BUFFER];

    if (read_file_trim(path, buffer) != 0) return -1;
    if (buffer[0] == '-') return -1;

    errno = 0;
    char *end = NULL;
    const unsigned long long v = strtoull(buffer, &end, 10);

    if (errno != 0 || end == buffer || *end != '\0') return -1;

    *val = v;
    return 0;
}

const char *get_cpu_model(void) {
    static char cached[UI_BUFFER];
    static int cached_ok = 0;

    if (cached_ok) return cached;

    char model[64] = {0};
    unsigned long long cpu_cores = 0;

    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            const char *trimmed = line;
            while (*trimmed && isspace((unsigned char) *trimmed))
                trimmed++;

            if (strncmp(trimmed, "processor", 9) == 0) {
                cpu_cores++;
            } else if (!model[0] && strncmp(trimmed, "model name", 10) == 0) {
                char *colon = strchr(trimmed, ':');
                if (!colon) continue;
                char *value = colon + 1;
                while (*value && isspace((unsigned char) *value))
                    value++;
                char *end = value + strlen(value);
                while (end > value && isspace((unsigned char) *(end - 1)))
                    --end;
                *end = '\0';
                snprintf(model, sizeof(model), "%s", value);
            }
        }
        fclose(fp);
    }

    if (!model[0]) {
        FILE *lscpu = popen("lscpu", "r");
        if (lscpu) {
            char line[256];
            while (fgets(line, sizeof(line), lscpu)) {
                char *trimmed = line;
                while (*trimmed && isspace((unsigned char) *trimmed))
                    trimmed++;
                if (strncmp(trimmed, "Model name:", 11) == 0) {
                    char *value = trimmed + 11;
                    while (*value && isspace((unsigned char) *value))
                        value++;
                    char *end = value + strlen(value);
                    while (end > value && isspace((unsigned char) *(end - 1)))
                        --end;
                    *end = '\0';
                    snprintf(model, sizeof(model), "%s", value);
                    break;
                }
            }
            pclose(lscpu);
        }
    }

    if (!model[0]) return lang.generic.unknown;

    if (cpu_cores > 0) {
        snprintf(cached, sizeof(cached), "%s (%llu)", model, cpu_cores);
    } else {
        snprintf(cached, sizeof(cached), "%s", model);
    }

    cached_ok = 1;
    return cached;
}

const char *get_current_frequency(void) {
    static char buffer[UI_BUFFER];

    const char *paths[] = {
        "/sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq",
        "/sys/devices/system/cpu/cpufreq/policy0/cpuinfo_cur_freq",
        "/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq",
        "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_cur_freq",
    };

    unsigned long long khz = 0;

    for (size_t i = 0; i < A_SIZE(paths); i++) {
        if (read_ll_from_file(paths[i], &khz) == 0 && khz > 0) break;
    }

    if (khz == 0) {
        snprintf(buffer, sizeof(buffer), "%s", lang.generic.unknown);
        return buffer;
    }

    const unsigned long long mhz_whole = khz / 1000ULL;
    const unsigned long long mhz_frac = khz % 1000ULL / 10ULL;

    snprintf(buffer, sizeof(buffer), "%llu.%02llu MHz", mhz_whole, mhz_frac);
    return buffer;
}

const char *get_scaling_governor(void) {
    static char buffer[UI_BUFFER];

    if (read_file_trim(device.cpu.governor, buffer) != 0) {
        snprintf(buffer, sizeof(buffer), "%s", lang.generic.unknown);
    }

    return buffer;
}

const char *get_memory_usage(void) {
    static char buffer[UI_BUFFER];

    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        snprintf(buffer, sizeof(buffer), "%s", lang.generic.unknown);
        return buffer;
    }

    unsigned long long total_kb = 0;
    unsigned long long avail_kb = 0;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (!total_kb && strncmp(line, "MemTotal:", 9) == 0) {
            const char *value = line + 9;
            while (*value && (*value < '0' || *value > '9'))
                value++;
            if (*value) {
                errno = 0;
                const unsigned long long mem = strtoull(value, NULL, 10);
                if (errno == 0) total_kb = mem;
            }
        } else if (!avail_kb && strncmp(line, "MemAvailable:", 13) == 0) {
            const char *value = line + 13;
            while (*value && (*value < '0' || *value > '9'))
                value++;
            if (*value) {
                errno = 0;
                const unsigned long long mem = strtoull(value, NULL, 10);
                if (errno == 0) avail_kb = mem;
            }
        }

        if (total_kb && avail_kb) break;
    }

    fclose(fp);

    if (!total_kb) {
        snprintf(buffer, sizeof(buffer), "%s", lang.generic.unknown);
        return buffer;
    }

    const unsigned long long used_kb = total_kb > avail_kb ? total_kb - avail_kb : 0ULL;

    const unsigned long long used_whole = used_kb / 1024ULL;
    const unsigned long long used_frac = used_kb % 1024ULL * 100ULL / 1024ULL;

    const unsigned long long total_whole = total_kb / 1024ULL;
    const unsigned long long total_frac = total_kb % 1024ULL * 100ULL / 1024ULL;

    snprintf(buffer, sizeof(buffer), "%llu.%02llu MB / %llu.%02llu MB", used_whole, used_frac, total_whole, total_frac);

    return buffer;
}

const char *get_swap_usage(void) {
    static char buffer[UI_BUFFER];

    if (sysinfo_cache.totalswap == 0) {
        snprintf(buffer, sizeof(buffer), "0.00 MB / 0.00 MB");
        return buffer;
    }

    const unsigned long long unit = sysinfo_cache.mem_unit;
    if (unit == 0) {
        snprintf(buffer, sizeof(buffer), "%s", lang.generic.unknown);
        return buffer;
    }

    const unsigned long long total = (unsigned long long) sysinfo_cache.totalswap * unit;
    const unsigned long long free = (unsigned long long) sysinfo_cache.freeswap * unit;
    const unsigned long long used = total > free ? total - free : 0ULL;

    snprintf(buffer, sizeof(buffer), "%.2f MB / %.2f MB", (double) used / 1048576.0, (double) total / 1048576.0);

    return buffer;
}

const char *get_temperature(void) {
    static char buffer[UI_BUFFER];

    const char *paths[] = {
        "/sys/class/thermal/thermal_zone0/temp",
        "/sys/class/thermal/thermal_zone1/temp",
    };

    unsigned long long mc = 0;
    for (size_t i = 0; i < A_SIZE(paths); i++) {
        if (read_ll_from_file(paths[i], &mc) == 0 && mc > 0) break;
    }

    if (mc == 0) {
        snprintf(buffer, sizeof(buffer), "%s", lang.generic.unknown);
        return buffer;
    }

    const unsigned long long c_whole = mc / 1000ULL;
    const unsigned long long c_frac = mc % 1000ULL / 10ULL;

    snprintf(buffer, sizeof(buffer), "%llu.%02llu\u00B0C", c_whole, c_frac);
    return buffer;
}

static const char *get_system_uptime(void) {
    static char buffer[UI_BUFFER];

    const unsigned long long total_minutes = (unsigned long long) sysinfo_cache.uptime / 60ULL;

    const unsigned long long days = total_minutes / (24ULL * 60ULL);
    const unsigned long long hours = total_minutes % (24ULL * 60ULL) / 60ULL;
    const unsigned long long minutes = total_minutes % 60ULL;

    if (days > 0) {
        snprintf(
            buffer, sizeof(buffer), "%llu %s%s %llu %s%s %llu %s%s", days, lang.muxsysinfo.day, days == 1ULL ? "" : "s",
            hours, lang.muxsysinfo.hour, hours == 1ULL ? "" : "s", minutes, lang.muxsysinfo.minute,
            minutes == 1ULL ? "" : "s"
        );
    } else if (hours > 0) {
        snprintf(
            buffer, sizeof(buffer), "%llu %s%s %llu %s%s", hours, lang.muxsysinfo.hour, hours == 1ULL ? "" : "s",
            minutes, lang.muxsysinfo.minute, minutes == 1ULL ? "" : "s"
        );
    } else {
        snprintf(buffer, sizeof(buffer), "%llu %s%s", minutes, lang.muxsysinfo.minute, minutes == 1ULL ? "" : "s");
    }

    return buffer;
}

const char *get_device_info(void) {
    static char device_info[UI_BUFFER];
    snprintf(device_info, sizeof(device_info), "%s", board_name());

    return device_info;
}

static char uname_kernel[UI_BUFFER];
static char uname_arch[UI_BUFFER];
static int uname_ready = 0;

static void ensure_uname(void) {
    if (uname_ready) return;

    struct utsname u;
    if (uname(&u) == 0) {
        snprintf(uname_kernel, sizeof(uname_kernel), "%s %s", u.sysname, u.release);
        snprintf(uname_arch, sizeof(uname_arch), "%s", u.machine);
        snprintf(hostname, sizeof(hostname), "%s", u.nodename);
    } else {
        snprintf(uname_kernel, sizeof(uname_kernel), "%s", lang.generic.unknown);
        snprintf(uname_arch, sizeof(uname_arch), "%s", lang.generic.unknown);
    }

    uname_ready = 1;
}

const char *get_kernel_version(void) {
    ensure_uname();
    return uname_kernel;
}

const char *get_cpu_arch(void) {
    ensure_uname();
    return uname_arch;
}

static const char *get_boot_time(void) {
    static char buffer[UI_BUFFER];

    const time_t boot_ts = time(NULL) - sysinfo_cache.uptime;
    struct tm *tm_info = localtime(&boot_ts);
    if (!tm_info) {
        snprintf(buffer, sizeof(buffer), "%s", lang.generic.unknown);
        return buffer;
    }

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", tm_info);
    return buffer;
}

static const char *get_load_average(void) {
    static char buffer[UI_BUFFER];

    const double load1 = (double) sysinfo_cache.loads[0] / 65536.0;
    const double load5 = (double) sysinfo_cache.loads[1] / 65536.0;
    const double load15 = (double) sysinfo_cache.loads[2] / 65536.0;

    snprintf(buffer, sizeof(buffer), "%.2f / %.2f / %.2f", load1, load5, load15);
    return buffer;
}

static void update_system_info(const lv_timer_t *timer) {
    (void) timer;
    if (sysinfo(&sysinfo_cache) != 0) {
        lv_label_set_text(ui_val_uptime_sysinfo, lang.generic.unknown);
        lv_label_set_text(ui_val_boot_time_sysinfo, lang.generic.unknown);
        lv_label_set_text(ui_val_load_avg_sysinfo, lang.generic.unknown);
        lv_label_set_text(ui_val_speed_sysinfo, lang.generic.unknown);
        lv_label_set_text(ui_val_governor_sysinfo, lang.generic.unknown);
        lv_label_set_text(ui_val_memory_sysinfo, lang.generic.unknown);
        lv_label_set_text(ui_val_swap_sysinfo, lang.generic.unknown);
        lv_label_set_text(ui_val_temp_sysinfo, lang.generic.unknown);
        return;
    }

    lv_label_set_text(ui_val_uptime_sysinfo, get_system_uptime());
    lv_label_set_text(ui_val_boot_time_sysinfo, get_boot_time());
    lv_label_set_text(ui_val_load_avg_sysinfo, get_load_average());
    lv_label_set_text(ui_val_speed_sysinfo, get_current_frequency());
    lv_label_set_text(ui_val_governor_sysinfo, get_scaling_governor());
    lv_label_set_text(ui_val_memory_sysinfo, get_memory_usage());
    if (sysinfo_cache.totalswap != 0) lv_label_set_text(ui_val_swap_sysinfo, get_swap_usage());
    lv_label_set_text(ui_val_temp_sysinfo, get_temperature());
}

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

    INIT_VALUE_ITEM(-1, sysinfo, version, lang.muxsysinfo.version, "version", get_version(verify_check));
    INIT_VALUE_ITEM(-1, sysinfo, build, lang.muxsysinfo.build, "build", get_build());
    INIT_VALUE_ITEM(-1, sysinfo, device, lang.muxsysinfo.device, "device", get_device_info());
    INIT_VALUE_ITEM(-1, sysinfo, kernel, lang.muxsysinfo.kernel, "kernel", get_kernel_version());
    INIT_VALUE_ITEM(-1, sysinfo, arch, lang.muxsysinfo.arch, "arch", get_cpu_arch());
    INIT_VALUE_ITEM(-1, sysinfo, uptime, lang.muxsysinfo.uptime, "uptime", get_system_uptime());
    INIT_VALUE_ITEM(-1, sysinfo, boot_time, lang.muxsysinfo.boot_time, "boottime", get_boot_time());
    INIT_VALUE_ITEM(-1, sysinfo, load_avg, lang.muxsysinfo.load_avg, "loadavg", get_load_average());
    INIT_VALUE_ITEM(-1, sysinfo, cpu, lang.muxsysinfo.cpu.info, "cpu", get_cpu_model());
    INIT_VALUE_ITEM(-1, sysinfo, speed, lang.muxsysinfo.cpu.speed, "speed", get_current_frequency());
    INIT_VALUE_ITEM(-1, sysinfo, governor, lang.muxsysinfo.cpu.governor, "governor", get_scaling_governor());
    INIT_VALUE_ITEM(-1, sysinfo, memory, lang.muxsysinfo.memory.info, "memory", get_memory_usage());
    INIT_VALUE_ITEM(-1, sysinfo, swap, lang.muxsysinfo.swap, "swap", get_swap_usage());
    INIT_VALUE_ITEM(-1, sysinfo, temp, lang.muxsysinfo.temp, "temp", get_temperature());
    INIT_VALUE_ITEM(-1, sysinfo, reload, lang.muxsysinfo.reload, "reload", "");

    reset_ui_groups();
    add_ui_groups(ui_objects, ui_objects_value, ui_objects_glyph, ui_objects_panel, 0);

    if (sysinfo_cache.totalswap == 0) HIDE_VALUE_ITEM(sysinfo, swap);
}

static int warn_mode = 0;
static mux_dialogue warn_dlg;

static void show_warn_dialog(void) {
    warn_mode = 1;
    warn_dlg.selected = 1;
    dialogue_show(&warn_dlg);
    dialogue_refresh(&warn_dlg, &theme);
}

static void hide_warn_dialog(void) {
    warn_mode = 0;
    dialogue_hide(&warn_dlg);
}

static void handle_dpad_up(void) {
    if (warn_mode) {
        if (!swap_axis) {
            dialogue_navigate(&warn_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (warn_mode) {
        if (!swap_axis) {
            dialogue_navigate(&warn_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (warn_mode) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (warn_mode) return;

    handle_list_nav_down_hold();
}

static void handle_a(void) {
    if (warn_mode) {
        const int idx = warn_dlg.selected;
        hide_warn_dialog();
        if (idx == 0) {
            char cpath[MAX_BUFFER_SIZE];
            snprintf(cpath, sizeof(cpath), "%scount/warn_device", CONF_CONFIG_PATH);
            create_directories(cpath, 1);
            write_text_to_file(cpath, "w", INT, read_line_int_from(cpath, 1) + 1);
            load_mux("device");
            mux_input_stop();
        }
        return;
    }

    if (msgbox_active || hold_call) return;

    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    if (e_focused == ui_lbl_version_sysinfo) {
        toast_message(verify_check ? lang.generic.modified : lang.generic.clean, tst_wait_s);
        refresh_screen(ui_screen, 1);
        return;
    }

    if (e_focused == ui_lbl_build_sysinfo) {
        play_sound(snd_muos);

        if (++tap_count > 50) {
            tap_count = 0;
            srandom((unsigned) time(NULL));

            char s_rotate_str[8], s_zoom_str[8];

            const int rot = (int) (random() % 181) + 35;
            snprintf(s_rotate_str, sizeof(s_rotate_str), "%d", rot);

            static const float zooms[] = {0.45f, 0.50f, 0.55f, 0.60f, 0.65f, 0.70f, 0.75f};
            const float z = zooms[(size_t) (random() % (long) A_SIZE(zooms))];
            snprintf(s_zoom_str, sizeof(s_zoom_str), "%.2f", z);

            write_text_to_file(CONF_DEVICE_PATH "screen/s_rotate", "w", CHAR, s_rotate_str);
            write_text_to_file(CONF_DEVICE_PATH "screen/s_zoom", "w", CHAR, s_zoom_str);

            refresh_config = 1;
            refresh_device = 1;
            refresh_kiosk = 1;
            refresh_resolution = 1;

            load_mux("launcher");
            if (file_exist(MUOS_PDI_LOAD)) remove(MUOS_PDI_LOAD);

            mux_input_stop();
            return;
        }

        switch (tap_count) {
            case 5:
                toast_message("\x57\x68\x61\x74\x20\x64\x6F\x20\x79\x6F\x75\x20\x77\x61\x6E\x74\x3F", tst_wait_s);
                break;
            case 10:
                toast_message(
                    "\x59\x6F\x75\x20\x73\x75\x72\x65\x20\x61\x72\x65\x20\x70\x65\x72\x73\x69\x73\x74\x65\x6E\x74\x21",
                    tst_wait_s
                );
                break;
            case 20:
                toast_message(
                    "\x57\x68\x61\x74\x20\x61\x72\x65\x20\x79\x6F\x75\x20\x65\x78\x70\x65\x63\x74\x69\x6E\x67\x3F",
                    tst_wait_s
                );
                break;
            case 30:
                toast_message(
                    "\x4F\x6B\x61\x79\x20\x6C\x69\x73\x74\x65\x6E\x20\x68\x65\x72\x65\x20\x79\x6F\x75\x2E\x2E\x2E",
                    tst_wait_s
                );
                break;
            case 40:
                toast_message(
                    "\x54\x68\x69\x73\x20\x69\x73\x20\x79\x6F\x75\x72\x20\x6C\x61\x73\x74\x20\x77\x61\x72\x6E"
                    "\x69\x6E\x67\x21",
                    tst_wait_s
                );
                break;
            case 50:
                toast_message(
                    "\x4F\x6B\x61\x79\x20\x77\x65\x6C\x6C\x20\x79\x6F\x75\x20\x61\x73\x6B\x65\x64\x20\x66\x6F"
                    "\x72\x20\x69\x74",
                    tst_wait_s
                );
                break;
            default:
                toast_message(
                    "\x54\x68\x61\x6E\x6B\x20\x79\x6F\x75\x20\x66\x6F\x72\x20\x75\x73\x69\x6E\x67\x20\x6D\x75"
                    "\x4F\x53\x21",
                    tst_wait_s
                );
                break;
        }

        refresh_screen(ui_screen, 1);
        return;
    }

    if (e_focused == ui_lbl_memory_sysinfo) {
        write_text_to_file("/proc/sys/vm/drop_caches", "w", INT, 3);
        toast_message(lang.muxsysinfo.memory.drop, tst_wait_m);
        refresh_screen(ui_screen, 1);
        return;
    }

    if (e_focused == ui_lbl_kernel_sysinfo) {
        toast_message(hostname, tst_wait_m);
        refresh_screen(ui_screen, 1);
        return;
    }

    if (e_focused == ui_lbl_reload_sysinfo) {
        toast_message(lang.muxsysinfo.reload_run, tst_wait_f);

        refresh_config = 1;
        refresh_device = 1;
        refresh_kiosk = 1;
        refresh_resolution = 1;

        if (file_exist(MUOS_PDI_LOAD)) remove(MUOS_PDI_LOAD);
        load_mux("launcher");

        mux_input_stop();
        return;
    }

    refresh_screen(ui_screen, 1);
}

static void handle_b(void) {
    if (hold_call) return;

    if (warn_mode) {
        hide_warn_dialog();
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "sysinfo");

    mux_input_stop();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call || warn_mode) return;

    play_sound(snd_info_open);
    show_help();
}

static void launch_device(void) {
    if (msgbox_active || hold_call || warn_mode) return;

    if (lv_group_get_focused(ui_group) == ui_lbl_device_sysinfo) show_warn_dialog();
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_b_glyph, "", 0}, {ui_lbl_nav_b, lang.generic.back, 0}, {NULL, NULL, 0}});

#define SYSINFO(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_sysinfo, UDATA);
    SYSINFO_ELEMENTS
#undef SYSINFO

    overlay_display();
}

int muxsysinfo_main(void) {
    verify_check = script_hash_check();

    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxsysinfo.title);
    init_muxsysinfo(ui_pnl_content);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    init_fonts();
    sysinfo(&sysinfo_cache);
    init_navigation_group();

    dialogue_init_warn(&warn_dlg, &theme, ui_screen, lang.muxsysinfo.warn, lang.generic.select, lang.generic.cancel);
    init_timer(ui_gen_refresh_task, update_system_info);
    gen_step_movement(0, +1, 2, 0, 1);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_l1] = handle_list_nav_page_up,
                [mux_input_r1] = handle_list_nav_page_down,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_x] = launch_device,
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_cb_prev_nowrap, list_nav_cb_next_nowrap);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    return 0;
}
