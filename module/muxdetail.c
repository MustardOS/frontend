#include "muxshare.h"
#include "../common/ui/orientation.h"
#include "../common/ui/list_frame.h"
#include "../common/battery.h"
#include "ui/ui_muxdetail.h"

#define DETAIL(NAME, UDATA) 1,
enum { ui_count_dynamic = E_SIZE(DETAIL_ELEMENTS) };

// section offsets follow the element lists so the numbers cannot drift
enum {
    detail_off_system = 0,
    detail_len_system = E_SIZE(DETAIL_SYSTEM_ELEMENTS),
    detail_off_runtime = detail_off_system + detail_len_system,
    detail_len_runtime = E_SIZE(DETAIL_RUNTIME_ELEMENTS),
    detail_off_processor = detail_off_runtime + detail_len_runtime,
    detail_len_processor = E_SIZE(DETAIL_PROCESSOR_ELEMENTS),
    detail_off_battery = detail_off_processor + detail_len_processor,
    detail_len_battery = E_SIZE(DETAIL_BATTERY_ELEMENTS),
    detail_off_power = detail_off_battery + detail_len_battery,
    detail_len_power = E_SIZE(DETAIL_POWER_ELEMENTS),
    detail_off_network = detail_off_power + detail_len_power,
    detail_len_network = E_SIZE(DETAIL_NETWORK_ELEMENTS),
    detail_off_traffic = detail_off_network + detail_len_network,
    detail_len_traffic = E_SIZE(DETAIL_TRAFFIC_ELEMENTS),
};
#undef DETAIL

#define UI_BUFFER 128
#define RUN_BATT  "/run/muos/battery"

static struct sysinfo sysinfo_cache;
static char hostname[32];
static int interface_valid = 0;
static int tap_count = 0;
static int starter_image = 0;

static mux_dialogue warn_dlg;
static mux_dialogue export_dlg;

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

static const char *get_cpu_model(void) {
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
        const char *const argv[] = {"lscpu", NULL};
        char *lscpu = get_execute_result_argv(argv, -1);
        if (lscpu) {
            char *save = NULL;
            for (char *line = strtok_r(lscpu, "\r\n", &save); line; line = strtok_r(NULL, "\r\n", &save)) {
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
            free(lscpu);
        }
    }

    if (!model[0]) return lang.generic.unknown;

    snprintf(cached, sizeof(cached), "%s", model);
    LV_UNUSED(cpu_cores);

    cached_ok = 1;
    return cached;
}

static const char *get_current_frequency(void) {
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

static const char *get_scaling_governor(void) {
    static char buffer[UI_BUFFER];

    if (read_file_trim(device.cpu.governor, buffer) != 0) {
        snprintf(buffer, sizeof(buffer), "%s", lang.generic.unknown);
    }

    return buffer;
}

static const char *get_memory_usage(void) {
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

static const char *get_swap_usage(void) {
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

static const char *get_temperature(void) {
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
            buffer, sizeof(buffer), "%llu %s%s %llu %s%s %llu %s%s", days, lang.muxdetail.day, days == 1ULL ? "" : "s",
            hours, lang.muxdetail.hour, hours == 1ULL ? "" : "s", minutes, lang.muxdetail.minute,
            minutes == 1ULL ? "" : "s"
        );
    } else if (hours > 0) {
        snprintf(
            buffer, sizeof(buffer), "%llu %s%s %llu %s%s", hours, lang.muxdetail.hour, hours == 1ULL ? "" : "s",
            minutes, lang.muxdetail.minute, minutes == 1ULL ? "" : "s"
        );
    } else {
        snprintf(buffer, sizeof(buffer), "%llu %s%s", minutes, lang.muxdetail.minute, minutes == 1ULL ? "" : "s");
    }

    return buffer;
}

static const char *get_device_info(void) {
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

static const char *get_kernel_version(void) {
    ensure_uname();
    return uname_kernel;
}

static const char *get_cpu_arch(void) {
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

static void get_bat_base_dir(char *out) {
    snprintf(out, MAX_BUFFER_SIZE, "%s", device.battery.capacity);
    char *slash = strrchr(out, '/');
    if (slash) *slash = '\0';
}

static int read_bat_file_trim(const char *path, char *out, const size_t out_sz) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    if (!fgets(out, (int) out_sz, fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    const char *trimmed = str_trim(out);
    if (trimmed != out) memmove(out, trimmed, strlen(trimmed) + 1);

    return out[0] != '\0' ? 0 : -1;
}

static const char *get_bat_capacity(void) {
    static char buffer[UI_BUFFER];
    snprintf(buffer, sizeof(buffer), "%d%%", battery_get_capacity());

    return buffer;
}

static const char *get_bat_voltage(void) {
    return battery_get_voltage();
}

static const char *get_bat_status(void) {
    static char buffer[UI_BUFFER];
    char base[MAX_BUFFER_SIZE];
    char path[MAX_BUFFER_SIZE];

    get_bat_base_dir(base);
    snprintf(path, sizeof(path), "%s/status", base);

    if (read_bat_file_trim(path, buffer, sizeof(buffer)) != 0) {
        snprintf(buffer, sizeof(buffer), "%s", lang.generic.unknown);
    }

    return buffer;
}

static const char *get_bat_health(void) {
    static char buffer[UI_BUFFER];

    if (read_bat_file_trim(device.battery.health, buffer, sizeof(buffer)) != 0) {
        snprintf(buffer, sizeof(buffer), "%s", lang.generic.unknown);
    }

    return buffer;
}

static const char *get_bat_design_cap(void) {
    static char buffer[UI_BUFFER];

    if (device.battery.size > 0) {
        snprintf(buffer, sizeof(buffer), "%d mAh", device.battery.size);
    } else {
        snprintf(buffer, sizeof(buffer), "%s", lang.generic.unknown);
    }

    return buffer;
}

static const char *get_last_charged(void) {
    static char buffer[UI_BUFFER];
    char ts_str[32];

    if (read_bat_file_trim(RUN_BATT "_usage/last_charged", ts_str, sizeof(ts_str)) != 0 || ts_str[0] == '\0')
        return "-";

    const long ts = strtol(ts_str, NULL, 10);
    if (ts <= 0) return "-";

    const time_t t = ts;
    struct tm *tm_info = localtime(&t);
    if (!tm_info) return "-";

    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", tm_info);
    return buffer;
}

static const char *get_time_on_battery(void) {
    static char buffer[UI_BUFFER];
    char secs_str[32];

    if (read_bat_file_trim(RUN_BATT "_usage/time_on_battery", secs_str, sizeof(secs_str)) != 0 || secs_str[0] == '\0')
        return "-";

    const long secs = strtol(secs_str, NULL, 10);
    if (secs <= 0) return "-";

    const long hours = secs / 3600;
    const long mins = secs % 3600 / 60;

    if (hours > 0) {
        snprintf(buffer, sizeof(buffer), "%ldh %ldm", hours, mins);
    } else if (mins > 0) {
        snprintf(buffer, sizeof(buffer), "%ldm", mins);
    } else {
        snprintf(buffer, sizeof(buffer), "< 1m");
    }

    return buffer;
}

static const char *get_battery_used(void) {
    static char buffer[UI_BUFFER];
    char unplug_str[32];
    char curr_str[32];

    if (read_bat_file_trim(RUN_BATT "_usage/unplug_capacity", unplug_str, sizeof(unplug_str)) != 0
        || unplug_str[0] == '\0')
        return "-";
    if (read_bat_file_trim(RUN_BATT "/capacity", curr_str, sizeof(curr_str)) != 0 || curr_str[0] == '\0') return "-";

    const int unplug = (int) strtol(unplug_str, NULL, 10);
    const int curr = (int) strtol(curr_str, NULL, 10);

    if (unplug <= 0 || unplug > 100 || curr < 0 || curr > 100) return "-";

    int used = unplug - curr;
    if (used < 0) used = 0;

    snprintf(buffer, sizeof(buffer), "%d%%", used);
    return buffer;
}

static const char *get_bat_charger(void) {
    return battery_is_charging() ? lang.generic.online : lang.generic.offline;
}

static int battery_used_available(void) {
    char buf[32];
    return read_bat_file_trim(RUN_BATT "_usage/unplug_capacity", buf, sizeof(buf)) == 0 && buf[0] != '\0';
}

static int is_valid_interface(const char *iface) {
    if (!iface || !*iface) return 0;
    for (const char *p = iface; *p; p++) {
        const char c = *p;
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return 0;
    }
    return 1;
}

static const char *get_hostname(void) {
    char *result = read_line_char_from("/etc/hostname", 1);
    if (!result || result[0] == '\0') {
        free(result);
        return lang.generic.unknown;
    }

    static char hostname[64];
    snprintf(hostname, sizeof(hostname), "%s", result);
    free(result);

    return hostname;
}

static const char *get_mac_address(void) {
    if (!interface_valid) return lang.generic.unknown;

    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/address", device.network.interface);

    FILE *f = fopen(path, "r");
    if (!f) {
        const char *big_mac = CONF_CONFIG_PATH "network/mac";
        if (file_exist(big_mac)) return read_line_char_from(big_mac, 1);

        return lang.generic.unknown;
    }

    static char mac[32];
    if (!fgets(mac, sizeof(mac), f)) {
        fclose(f);
        const char *big_mac = CONF_CONFIG_PATH "network/mac";
        if (file_exist(big_mac)) return read_line_char_from(big_mac, 1);

        return lang.generic.unknown;
    }

    fclose(f);

    const size_t len = strlen(mac);
    if (len > 0 && mac[len - 1] == '\n') mac[len - 1] = '\0';

    return mac;
}

static int
command_prefixed_value(const char *const argv[], const char *prefix, char *output, const size_t output_size) {
    char *result = get_execute_result_argv(argv, -1);
    if (!result) return 0;

    int found = 0;
    char *save = NULL;
    for (char *line = strtok_r(result, "\r\n", &save); line; line = strtok_r(NULL, "\r\n", &save)) {
        while (*line && isspace((unsigned char) *line))
            line++;
        const size_t prefix_length = strlen(prefix);
        if (strncmp(line, prefix, prefix_length) != 0) continue;

        line += prefix_length;
        while (*line && isspace((unsigned char) *line))
            line++;

        char *end = line + strlen(line);
        while (end > line && isspace((unsigned char) end[-1]))
            end--;
        *end = '\0';

        found = str_copy_checked(output, output_size, line);
        break;
    }

    free(result);
    return found;
}

static const char *get_ip_address(void) {
    if (!interface_valid) return lang.generic.unknown;
    if (!is_network_connected()) return lang.generic.not_connected;

    static char ip[64];
    if (!get_network_ipv4_address(ip, sizeof(ip))) return lang.generic.unknown;

    return ip;
}

static const char *get_ssid(void) {
    if (!interface_valid) return lang.generic.unknown;
    if (!is_network_connected()) return lang.generic.not_connected;

    static char ssid[64];
    const char *const argv[] = {"iw", "dev", device.network.interface, "link", NULL};
    if (!command_prefixed_value(argv, "SSID:", ssid, sizeof(ssid))) return lang.generic.unknown;

    return ssid;
}

static const char *get_gateway(void) {
    if (!is_network_connected()) return lang.generic.not_connected;

    static char gw[64];
    const char *const argv[] = {"ip", "route", NULL};
    char route[256];
    if (!command_prefixed_value(argv, "default ", route, sizeof(route))) return lang.generic.unknown;

    char *save = NULL;
    const char *token = strtok_r(route, " \t", &save);
    while (token) {
        if (strcmp(token, "via") == 0) {
            token = strtok_r(NULL, " \t", &save);
            if (token && str_copy_checked(gw, sizeof(gw), token)) return gw;
            break;
        }
        token = strtok_r(NULL, " \t", &save);
    }

    return lang.generic.unknown;
}

static const char *get_dns_servers(void) {
    if (!is_network_connected()) return lang.generic.not_connected;

    static char dns[128];
    dns[0] = '\0';
    FILE *fp = fopen("/etc/resolv.conf", "r");
    if (!fp) return lang.generic.unknown;

    char line[256];
    size_t used = 0;
    while (fgets(line, sizeof(line), fp)) {
        char server[64];
        if (sscanf(line, " nameserver %63s", server) != 1) continue;
        const int written = snprintf(dns + used, sizeof(dns) - used, "%s%s", used ? " " : "", server);
        if (written < 0 || (size_t) written >= sizeof(dns) - used) break;
        used += (size_t) written;
    }
    fclose(fp);

    return dns[0] ? dns : lang.generic.unknown;
}

static const char *get_signal_strength(void) {
    if (!interface_valid) return lang.generic.unknown;
    if (!is_network_connected()) return lang.generic.not_connected;

    char result[64];
    const char *const argv[] = {"iw", "dev", device.network.interface, "link", NULL};
    if (!command_prefixed_value(argv, "signal:", result, sizeof(result))) return lang.generic.unknown;

    char *space = strchr(result, ' ');
    if (space) *space = '\0';
    const int dbm = safe_atoi(result, -100);

    int percent;
    if (dbm <= -100) {
        percent = 0;
    } else if (dbm >= -10) {
        percent = 100;
    } else {
        percent = (dbm + 100) * 100 / 90;
    }

    static char signal[32];
    snprintf(signal, sizeof(signal), "%d%% (%d dBm)", percent, dbm);

    return signal;
}

static const char *get_channel_info(void) {
    if (!interface_valid) return lang.generic.unknown;
    if (!is_network_connected()) return lang.generic.not_connected;

    char result[64];
    const char *const argv[] = {"iw", "dev", device.network.interface, "link", NULL};
    if (!command_prefixed_value(argv, "freq:", result, sizeof(result))) return lang.generic.unknown;

    char *space = strchr(result, ' ');
    if (space) *space = '\0';
    const int freq = safe_atoi(result, 0);

    static const struct {
        int freq;
        int channel;
    } freq_table[] = {
        // 2.4 GHz
        {2412, 1},
        {2417, 2},
        {2422, 3},
        {2427, 4},
        {2432, 5},
        {2437, 6},
        {2442, 7},
        {2447, 8},
        {2452, 9},
        {2457, 10},
        {2462, 11},
        {2467, 12},
        {2472, 13},
        {2484, 14},

        // 3.65 GHz? No just kidding...?

        // 5 GHz
        {5180, 36},
        {5200, 40},
        {5220, 44},
        {5240, 48},
        {5260, 52},
        {5280, 56},
        {5300, 60},
        {5320, 64},
        {5500, 100},
        {5520, 104},
        {5540, 108},
        {5560, 112},
        {5580, 116},
        {5600, 120},
        {5620, 124},
        {5640, 128},
        {5660, 132},
        {5680, 136},
        {5700, 140},
        {5745, 149},
        {5765, 153},
        {5785, 157},
        {5805, 161},
        {5825, 165}

        // 6 GHz, maybe one day!
    };

    for (size_t i = 0; i < A_SIZE(freq_table); i++) {
        if (freq_table[i].freq == freq) {
            static char info[64];
            snprintf(info, sizeof(info), "%d MHz - %s %d", freq, lang.generic.channel, freq_table[i].channel);
            return info;
        }
    }

    static char unknown[64];
    snprintf(unknown, sizeof(unknown), "%d MHz - %s %s", freq, lang.generic.channel, lang.generic.unknown);
    return unknown;
}

static unsigned long long read_iface_bytes(const char *direction) {
    char path[128];
    snprintf(path, sizeof(path), "/sys/class/net/%s/statistics/%s_bytes", device.network.interface, direction);

    return read_all_long_from(path);
}

static const char *get_ac_traffic(void) {
    if (!interface_valid) return lang.generic.unknown;
    if (!is_network_connected()) return lang.generic.not_connected;

    static unsigned long long last_rx = 0, last_tx = 0;
    static time_t last_time = 0;
    double rx_rate = 0, tx_rate = 0;

    const unsigned long long rx = read_iface_bytes("rx");
    const unsigned long long tx = read_iface_bytes("tx");

    const time_t now = time(NULL);
    if (last_time > 0) {
        const double delta = difftime(now, last_time);
        if (delta > 0) {
            rx_rate = (double) (rx - last_rx) / delta;
            tx_rate = (double) (tx - last_tx) / delta;
        }
    }

    last_rx = rx;
    last_tx = tx;

    last_time = now;

    static char ac_traffic[64];
    snprintf(ac_traffic, sizeof(ac_traffic), "RX: %.1f KB/s TX: %.1f KB/s", rx_rate / 1024.0, tx_rate / 1024.0);

    return ac_traffic;
}

static const char *get_tp_traffic(void) {
    if (!interface_valid) return lang.generic.unknown;
    if (!is_network_connected()) return lang.generic.not_connected;

    const unsigned long long rx = read_iface_bytes("rx");
    const unsigned long long tx = read_iface_bytes("tx");

    static char tp_traffic[64];
    snprintf(
        tp_traffic, sizeof(tp_traffic), "RX: %.1f MB TX: %.1f MB", (double) rx / 1024.0 / 1024.0,
        (double) tx / 1024.0 / 1024.0
    );

    return tp_traffic;
}

static const char *get_serial(void) {
    static char buffer[UI_BUFFER];
    static int initialised = 0;

    if (initialised) return buffer[0] ? buffer : lang.generic.unknown;
    initialised = 1;

    const char *const argv[] = {OPT_PATH "script/system/serial.sh", NULL};
    char *serial = get_execute_result_argv(argv, 0);
    if (!serial) return lang.generic.unknown;
    const int copied = str_copy_checked(buffer, sizeof(buffer), serial);
    free(serial);
    if (!copied) return lang.generic.unknown;
    return buffer[0] ? buffer : lang.generic.unknown;
}

static const char *get_display_mode(void) {
    static char buffer[UI_BUFFER];

    snprintf(buffer, sizeof(buffer), "%dx%d", device.screen.width, device.screen.height);
    return buffer;
}

static const char *get_core_count(void) {
    static char buffer[UI_BUFFER];

    unsigned long long cores = 0;
    FILE *fp = fopen("/proc/cpuinfo", "r");

    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp))
            if (strncmp(line, "processor", 9) == 0) cores++;

        fclose(fp);
    }

    if (cores == 0) return lang.generic.unknown;

    snprintf(buffer, sizeof(buffer), "%llu", cores);
    return buffer;
}

static const char *get_speed_range(void) {
    static char buffer[UI_BUFFER];

    unsigned long long min_khz = 0, max_khz = 0;

    const char *min_paths[] = {
        "/sys/devices/system/cpu/cpufreq/policy0/cpuinfo_min_freq",
        "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq"
    };
    const char *max_paths[] = {
        "/sys/devices/system/cpu/cpufreq/policy0/cpuinfo_max_freq",
        "/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq"
    };

    for (size_t i = 0; i < A_SIZE(min_paths); i++)
        if (read_ll_from_file(min_paths[i], &min_khz) == 0 && min_khz > 0) break;

    for (size_t i = 0; i < A_SIZE(max_paths); i++)
        if (read_ll_from_file(max_paths[i], &max_khz) == 0 && max_khz > 0) break;

    if (min_khz == 0 || max_khz == 0) return lang.generic.unknown;

    snprintf(buffer, sizeof(buffer), "%llu - %llu MHz", min_khz / 1000ULL, max_khz / 1000ULL);
    return buffer;
}

static const char *get_task_count(void) {
    static char buffer[UI_BUFFER];

    if (sysinfo(&sysinfo_cache) != 0) return lang.generic.unknown;

    snprintf(buffer, sizeof(buffer), "%u", sysinfo_cache.procs);
    return buffer;
}

static void set_detail_value(lv_obj_t *label, const char *value) {
    if (!label || !value) return;

    const char *current = lv_label_get_text(label);
    if (current && strcmp(current, value) == 0) return;

    lv_label_set_text(label, value);
}

static void update_detail_info(void) {
    set_detail_value(ui_val_version_detail, get_version(verify_check));
    set_detail_value(ui_val_build_detail, get_build());
    set_detail_value(ui_val_device_detail, get_device_info());
    set_detail_value(ui_val_serial_detail, get_serial());
    set_detail_value(ui_val_kernel_detail, get_kernel_version());
    set_detail_value(ui_val_arch_detail, get_cpu_arch());
    set_detail_value(ui_val_display_detail, get_display_mode());

    set_detail_value(ui_val_uptime_detail, get_system_uptime());
    set_detail_value(ui_val_boot_time_detail, get_boot_time());
    set_detail_value(ui_val_load_avg_detail, get_load_average());
    set_detail_value(ui_val_tasks_detail, get_task_count());
    set_detail_value(ui_val_memory_detail, get_memory_usage());
    set_detail_value(ui_val_swap_detail, get_swap_usage());
    set_detail_value(ui_val_temp_detail, get_temperature());

    set_detail_value(ui_val_cpu_detail, get_cpu_model());
    set_detail_value(ui_val_cores_detail, get_core_count());
    set_detail_value(ui_val_speed_detail, get_current_frequency());
    set_detail_value(ui_val_speed_range_detail, get_speed_range());
    set_detail_value(ui_val_governor_detail, get_scaling_governor());

    set_detail_value(ui_val_capacity_detail, get_bat_capacity());
    set_detail_value(ui_val_voltage_detail, get_bat_voltage());
    set_detail_value(ui_val_status_detail, get_bat_status());
    set_detail_value(ui_val_health_detail, get_bat_health());
    set_detail_value(ui_val_design_cap_detail, get_bat_design_cap());
    set_detail_value(ui_val_charger_detail, get_bat_charger());
    set_detail_value(ui_val_last_charged_detail, get_last_charged());

    set_detail_value(ui_val_time_on_battery_detail, get_time_on_battery());
    set_detail_value(ui_val_battery_used_detail, get_battery_used());

    set_detail_value(ui_val_hostname_detail, get_hostname());
    set_detail_value(ui_val_mac_detail, get_mac_address());
    set_detail_value(ui_val_ip_detail, get_ip_address());
    set_detail_value(ui_val_ssid_detail, get_ssid());
    set_detail_value(ui_val_gateway_detail, get_gateway());
    set_detail_value(ui_val_dns_detail, get_dns_servers());
    set_detail_value(ui_val_signal_detail, get_signal_strength());

    set_detail_value(ui_val_channel_detail, get_channel_info());
    set_detail_value(ui_val_ac_traffic_detail, get_ac_traffic());
    set_detail_value(ui_val_tp_traffic_detail, get_tp_traffic());
}

static void update_detail_info_cb(const lv_timer_t *timer) {
    (void) timer;
    update_detail_info();
}

static void export_diagnostics(void) {
    char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), "%s/sysdiag.txt", device.storage.rom.mount);

    FILE *f = fopen(path, "w");
    if (!f) {
        toast_message(lang.muxdetail.report_fail, tst_wait_m);
        return;
    }

    fprintf(f, "%s: %s\n", lang.muxdetail.label.version, get_version(verify_check));
    fprintf(f, "%s: %s\n", lang.muxdetail.label.build, get_build());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.device, get_device_info());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.serial, get_serial());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.kernel, get_kernel_version());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.arch, get_cpu_arch());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.display, get_display_mode());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.uptime, get_system_uptime());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.boot_time, get_boot_time());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.load_avg, get_load_average());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.tasks, get_task_count());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.memory, get_memory_usage());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.swap, get_swap_usage());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.temp, get_temperature());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.cpu, get_cpu_model());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.cores, get_core_count());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.speed, get_current_frequency());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.speed_range, get_speed_range());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.governor, get_scaling_governor());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.capacity, get_bat_capacity());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.voltage, get_bat_voltage());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.status, get_bat_status());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.health, get_bat_health());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.design_cap, get_bat_design_cap());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.charger, get_bat_charger());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.last_charged, get_last_charged());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.time_on_battery, get_time_on_battery());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.battery_used, get_battery_used());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.hostname, get_hostname());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.mac, get_mac_address());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.ip, get_ip_address());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.ssid, get_ssid());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.gateway, get_gateway());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.dns, get_dns_servers());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.signal, get_signal_strength());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.channel, get_channel_info());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.ac_traffic, get_ac_traffic());
    fprintf(f, "%s: %s\n", lang.muxdetail.label.tp_traffic, get_tp_traffic());

    fclose(f);

    char saved[MAX_BUFFER_SIZE];
    snprintf(saved, sizeof(saved), lang.muxdetail.report_saved, path);

    dialogue_set_description(&export_dlg, saved);
    dialogue_open(&export_dlg, &theme);
}

static void show_help(void) {
    if (list_frame_focused()) {
        list_frame_help();
        return;
    }

    const struct help_msg help_messages[] = {
#define DETAIL(NAME, UDATA) {UDATA, lang.muxdetail.help.NAME},
        DETAIL_ELEMENTS
#undef DETAIL
    };

    gen_help(current_item_index, help_messages, A_SIZE(help_messages), ui_group, NULL);
}

static void list_nav_move(int steps, int direction);
static void nav_refresh(void);

static void init_navigation_group(void) {
    static lv_obj_t *ui_objects[ui_count_dynamic];
    static lv_obj_t *ui_objects_value[ui_count_dynamic];
    static lv_obj_t *ui_objects_glyph[ui_count_dynamic];
    static lv_obj_t *ui_objects_panel[ui_count_dynamic];

#define DETAIL(NAME, UDATA) INIT_VALUE_ITEM(-1, detail, NAME, lang.muxdetail.label.NAME, UDATA, "");
    DETAIL_ELEMENTS
#undef DETAIL

#define DETAIL(NAME, UDATA) lv_obj_set_user_data(ui_lbl_##NAME##_detail, UDATA);
    DETAIL_ELEMENTS
#undef DETAIL

    if (!battery_used_available()) HIDE_VALUE_ITEM(detail, battery_used);

    list_frame frames[7];
    int frame_count = 0;

    frames[frame_count++] = (list_frame) {lang.muxdetail.section.system, detail_off_system, detail_len_system};
    frames[frame_count++] = (list_frame) {lang.muxdetail.section.runtime, detail_off_runtime, detail_len_runtime};
    frames[frame_count++] = (list_frame) {lang.muxdetail.section.processor, detail_off_processor, detail_len_processor};
    frames[frame_count++] = (list_frame) {lang.muxdetail.section.battery, detail_off_battery, detail_len_battery};
    frames[frame_count++] = (list_frame) {lang.muxdetail.section.power, detail_off_power, detail_len_power};

    if (device.board.has_network) {
        frames[frame_count++] = (list_frame) {lang.muxdetail.section.network, detail_off_network, detail_len_network};
        frames[frame_count++] = (list_frame) {lang.muxdetail.section.traffic, detail_off_traffic, detail_len_traffic};
    } else {
        for (int i = detail_off_network; i < ui_count_dynamic; i++)
            lv_obj_add_flag(ui_objects_panel[i], LV_OBJ_FLAG_HIDDEN);
    }

    list_frame_init(
        &theme, ui_pnl_content, frames, frame_count, ui_objects_panel, ui_objects, ui_objects_glyph, ui_objects_value,
        ui_count_dynamic
    );

    list_frame_apply();

    list_nav_move(list_frame_restore(), +1);
    nav_refresh();
}

static void list_nav_move(const int steps, const int direction) {
    gen_step_movement(steps, direction, 2, 0, 1);
    nav_refresh();
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static void nav_refresh(void) {
    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
    const char *action = NULL;

    if (list_frame_focused()) {
        lv_obj_clear_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_lr_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_lr, MU_OBJ_FLAG_HIDE_FLOAT);

        if (e_focused == ui_lbl_hostname_detail) {
            action = lang.generic.edit;
        } else if (e_focused == ui_lbl_version_detail || e_focused == ui_lbl_build_detail
                   || e_focused == ui_lbl_kernel_detail || e_focused == ui_lbl_memory_detail) {
            action = lang.generic.select;
        }
    }

    if (action) {
        lv_label_set_text(ui_lbl_nav_a, action);

        lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
    }
}

static void handle_section_prev(void) {
    if (key_show || dialogue_active(&warn_dlg)) return;

    if (list_frame_move(-1)) {
        play_sound(snd_navigate);
        update_detail_info();
        nav_refresh();

        nav_moved = 1;
    }
}

static void handle_section_next(void) {
    if (key_show || dialogue_active(&warn_dlg)) return;

    if (list_frame_move(+1)) {
        play_sound(snd_navigate);
        update_detail_info();
        nav_refresh();

        nav_moved = 1;
    }
}

static void reload_detail(void) {
    load_mux("detail");
    mux_input_stop();
}

static void handle_keyboard_ok_press(void) {
    key_show = 0;
    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    if (e_focused == ui_lbl_hostname_detail) {
        const char *new_hostname = lv_textarea_get_text(ui_txt_entry_detail);
        if (strlen(new_hostname) < 3) goto clear_osk;

        play_sound(snd_confirm);

        toast_message(lang.muxdetail.save_host, tst_wait_f);

        lv_label_set_text(ui_val_hostname_detail, new_hostname);
        write_text_to_file("/etc/hostname", "w", CHAR, new_hostname);

        list_frame_remember(ui_lbl_hostname_detail);

        const char *hn_set_args[] = {"hostname", new_hostname, NULL};
        run_exec(hn_set_args, A_SIZE(hn_set_args), 0, 0, NULL, NULL);

        reload_detail();
    }

clear_osk:
    reset_osk(key_entry);

    lv_textarea_set_text(ui_txt_entry_detail, "");
    lv_group_set_focus_cb(ui_group, NULL);

    osk_hide(ui_pnl_entry_detail);
}

static void handle_keyboard_press(void) {
    if (first_open) {
        first_open = 0;
    } else {
        play_sound(snd_keypress);
    }

    const char *is_key = lv_btnmatrix_get_btn_text(key_entry, key_curr);
    if (is_key && strcasecmp(is_key, OSK_DONE) == 0) {
        handle_keyboard_ok_press();
    } else {
        lv_event_send(key_entry, LV_EVENT_CLICKED, &key_curr);
    }
}

static void handle_a(void) {
    if (dialogue_active(&export_dlg)) {
        dialogue_dismiss(&export_dlg);
        return;
    }

    if (dialogue_active(&warn_dlg)) {
        const int idx = warn_dlg.selected;
        dialogue_dismiss(&warn_dlg);
        if (idx == 0) {
            char cpath[MAX_BUFFER_SIZE];
            snprintf(cpath, sizeof(cpath), "%scount/warn_device", CONF_CONFIG_PATH);
            create_directories(cpath, 1);
            write_text_to_file(cpath, "w", INT, read_line_int_from(cpath, 1) + 1);

            list_frame_remember(ui_lbl_device_detail);

            load_mux("device");
            mux_input_stop();
        }
        return;
    }

    if (msgbox_active || hold_call) return;

    if (key_show) {
        handle_keyboard_press();
        return;
    }

    if (list_frame_focused()) return;

    const struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);

    if (e_focused == ui_lbl_hostname_detail) {
        if (is_network_connected()) {
            play_sound(snd_error);
            toast_message(lang.muxdetail.edit_blocked, tst_wait_s);

            return;
        }

        lv_obj_clear_flag(key_entry, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(key_entry, LV_STATE_DISABLED);

        key_show = 1;
        osk_show(ui_pnl_entry_detail);
        lv_textarea_set_text(ui_txt_entry_detail, lv_label_get_text(lv_group_get_focused(ui_group_value)));

        return;
    }

    if (e_focused == ui_lbl_version_detail) {
        toast_message(verify_check ? lang.generic.modified : lang.generic.clean, tst_wait_s);
        refresh_screen(ui_screen, 1);
        return;
    }

    if (e_focused == ui_lbl_build_detail) {
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

    if (e_focused == ui_lbl_memory_detail) {
        write_text_to_file("/proc/sys/vm/drop_caches", "w", INT, 3);
        toast_message(lang.muxdetail.memory_drop, tst_wait_m);
        refresh_screen(ui_screen, 1);
        return;
    }

    if (e_focused == ui_lbl_kernel_detail) {
        toast_message(hostname, tst_wait_m);
        refresh_screen(ui_screen, 1);
        return;
    }

    refresh_screen(ui_screen, 1);
}

static void handle_b(void) {
    if (hold_call) return;

    if (key_show) {
        close_osk(key_entry, ui_group, ui_txt_entry_detail, ui_pnl_entry_detail);
        return;
    }

    if (dialogue_active(&export_dlg)) {
        dialogue_cancel(&export_dlg);
        return;
    }

    if (dialogue_active(&warn_dlg)) {
        dialogue_cancel(&warn_dlg);
        return;
    }

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    play_sound(snd_back);

    list_frame_remember_section();
    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "detail");

    mux_input_stop();
}

static void handle_x(void) {
    if (orientation_handle_skip()) return;

    if (msgbox_active || hold_call) return;

    if (key_show) {
        close_osk(key_entry, ui_group, ui_txt_entry_detail, ui_pnl_entry_detail);
        return;
    }

    if (dialogue_active(&warn_dlg)) return;

    play_sound(snd_confirm);
    export_diagnostics();
}

static void handle_y(void) {
    if (msgbox_active || hold_call) return;

    if (key_show) key_space(ui_txt_entry_detail);
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;
    if (key_show || dialogue_active(&warn_dlg)) return;

    play_sound(snd_info_open);
    show_help();
}

static void handle_dpad_up(void) {
    if (key_show) {
        key_up();
        return;
    }

    if (dialogue_active(&warn_dlg)) {
        dialogue_handle_dpad(&warn_dlg, &theme, -1, 1);
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (key_show) {
        key_down();
        return;
    }

    if (dialogue_active(&warn_dlg)) {
        dialogue_handle_dpad(&warn_dlg, &theme, +1, 1);
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (key_show) {
        key_up();
        return;
    }

    if (dialogue_active(&warn_dlg)) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (key_show) {
        key_down();
        return;
    }

    if (dialogue_active(&warn_dlg)) return;

    handle_list_nav_down_hold();
}

static void handle_dpad_left_hold(void) {
    if (key_show) key_left();
}

static void handle_dpad_right_hold(void) {
    if (key_show) key_right();
}

static void handle_l1(void) {
    if (key_show) {
        key_swap_back();
        return;
    }

    handle_section_prev();
}

static void handle_r1(void) {
    if (key_show) {
        key_swap();
        return;
    }

    handle_section_next();
}

static void handle_dpad_left(void) {
    if (key_show) {
        key_left();
        return;
    }

    if (!list_frame_focused()) return;

    handle_section_prev();
}

static void handle_dpad_right(void) {
    if (key_show) {
        key_right();
        return;
    }

    if (!list_frame_focused()) return;

    handle_section_next();
}

static void handle_select(void) {
    if (msgbox_active || hold_call) return;

    if (key_show) key_clear(ui_txt_entry_detail);
}

static void handle_start(void) {
    if (msgbox_active || hold_call) return;

    if (key_show) {
        handle_keyboard_ok_press();
        return;
    }

    if (dialogue_active(&warn_dlg)) return;

    if (lv_group_get_focused(ui_group) == ui_lbl_device_detail) dialogue_open(&warn_dlg, &theme);
}

static void init_elements(void) {
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_lr_glyph, "", 0},
                                  {ui_lbl_nav_lr, lang.generic.change, 0},
                                  {ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, "", 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.generic.output, 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    if (nav_moved) {
        starter_image = adjust_wallpaper_element(ui_group, starter_image, wall_general);
        adjust_panel_priority((lv_obj_t *[]) {ui_pnl_footer, ui_pnl_header, ui_pnl_help, ui_pnl_progress_brightness,
                                              ui_pnl_progress_volume, NULL});

        lv_obj_invalidate(ui_pnl_content);
        nav_moved = 0;
    }
}

int muxdetail_main(void) {
    starter_image = 0;
    tap_count = 0;

    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, lang.muxdetail.title);
    init_muxdetail(ui_screen, ui_pnl_content, &theme);
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    interface_valid = is_valid_interface(device.network.interface);
    if (!interface_valid) LOG_ERROR(mux_module, "Invalid network interface name: '%s'", device.network.interface);

    init_fonts();
    init_navigation_group();
    update_detail_info();

    init_osk(ui_pnl_entry_detail, ui_txt_entry_detail, 0, 0, 1024);

    dialogue_init_confirm(
        &warn_dlg, &theme, ui_screen, lang.generic.warning, lang.muxdetail.warn, lang.generic.understand,
        lang.generic.cancel, lang.generic.select, lang.generic.cancel
    );

    dialogue_init_message(&export_dlg, &theme, ui_screen, lang.muxdetail.title, NULL, "", lang.generic.close);

    init_timer(ui_refresh_task, update_detail_info_cb);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
                [mux_input_select] = handle_select,
                [mux_input_start] = handle_start,
                [mux_input_dpad_up] = handle_dpad_up,
                [mux_input_dpad_down] = handle_dpad_down,
                [mux_input_dpad_left] = handle_dpad_left,
                [mux_input_dpad_right] = handle_dpad_right,
                [mux_input_l1] = handle_l1,
                [mux_input_r1] = handle_r1,
            },
        .release_handler =
            {
                [mux_input_menu] = handle_help,
            },
        .hold_handler = {
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_dpad_left] = handle_dpad_left_hold,
            [mux_input_dpad_right] = handle_dpad_right_hold,
            [mux_input_l1] = handle_l1,
            [mux_input_r1] = handle_r1,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    orientation_introduce(mux_module, lang.muxdetail.title, lang.muxdetail.overview);

    mux_input_task(&input_opts);

    return 0;
}
