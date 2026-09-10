#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <common/runtime/init.h>
#include <common/runtime/log.h>
#include <common/platform/device.h>
#include <common/config/config.h>
#include <common/storage/fileio.h>
#include <common/base/strutil.h>

int is_network_connected(void) {
    if (file_exist(device.network.state)) {
        char *state = read_all_char_from(device.network.state);
        const int up = strcasecmp("up", state) == 0;
        free(state);

        if (up) return 1;
    }

    return 0;
}

static int signal_percent_from_dbm(const float dbm) {
    int percent = (int) ((dbm + 100.0f) * 100.0f / 60.0f + 0.5f);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    return percent;
}

static int proc_network_signal_percent(void) {
    if (!device.network.interface[0]) return -1;

    FILE *wireless = fopen("/proc/net/wireless", "r");
    if (!wireless) return -1;

    char line[256];
    int percent = -1;
    while (fgets(line, sizeof(line), wireless)) {
        char interface[IFNAMSIZ];
        unsigned status = 0;
        float quality = 0.0f;
        float level = 0.0f;
        if (sscanf(line, " %15[^:]: %x %f %f", interface, &status, &quality, &level) != 4) continue;
        if (strcmp(interface, device.network.interface) != 0) continue;

        (void) status;
        if (level > 127.0f && level <= 255.0f) level -= 256.0f;

        if (level < 0.0f) {
            percent = signal_percent_from_dbm(level);
        } else if (quality >= 0.0f) {
            percent = quality <= 70.0f ? (int) (quality * 100.0f / 70.0f + 0.5f) : (int) (quality + 0.5f);
        }

        if (percent < 0) percent = 0;
        if (percent > 100) percent = 100;
        break;
    }

    fclose(wireless);
    return percent;
}

struct signal_request {
    char interface[IFNAMSIZ];
};

static pthread_mutex_t signal_mutex = PTHREAD_MUTEX_INITIALIZER;
static int signal_cached = -1;
static int signal_worker_running = 0;
static uint64_t signal_next_poll_ms = 0;

static uint64_t monotonic_ms(void) {
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time) != 0) return 0;
    return (uint64_t) time.tv_sec * 1000u + (uint64_t) time.tv_nsec / 1000000u;
}

static void *network_signal_worker(void *opaque) {
    struct signal_request *request = opaque;
    const char *const argv[] = {"iw", "dev", request->interface, "link", NULL};
    char *result = get_execute_result_argv(argv, -1);
    free(request);

    int percent = -1;
    if (result) {
        const char *value = strstr(result, "signal:");
        if (value) {
            value += strlen("signal:");
            while (*value == ' ' || *value == '\t')
                value++;

            char *end = NULL;
            const long dbm = strtol(value, &end, 10);
            if (end != value && dbm >= -150 && dbm <= 0) percent = signal_percent_from_dbm((float) dbm);
        }
        free(result);
    }

    pthread_mutex_lock(&signal_mutex);
    signal_cached = percent;
    signal_worker_running = 0;
    pthread_mutex_unlock(&signal_mutex);
    return NULL;
}

int get_network_signal_percent(void) {
    if (!is_network_connected() || !device.network.interface[0]) {
        pthread_mutex_lock(&signal_mutex);
        signal_cached = -1;
        signal_next_poll_ms = 0;
        pthread_mutex_unlock(&signal_mutex);
        return -1;
    }

    const int proc_signal = proc_network_signal_percent();
    if (proc_signal >= 0) return proc_signal;

    const uint64_t now = monotonic_ms();
    pthread_mutex_lock(&signal_mutex);
    const int cached = signal_cached;

    if (!signal_worker_running && now >= signal_next_poll_ms) {
        struct signal_request *request = malloc(sizeof(*request));
        if (request) {
            snprintf(request->interface, sizeof(request->interface), "%s", device.network.interface);

            pthread_t worker;
            signal_worker_running = 1;
            signal_next_poll_ms = now + 15000;
            if (pthread_create(&worker, NULL, network_signal_worker, request) == 0) {
                pthread_detach(worker);
            } else {
                signal_worker_running = 0;
                signal_next_poll_ms = now + 1000;
                free(request);
            }
        }
    }

    pthread_mutex_unlock(&signal_mutex);
    return cached;
}

static int scan_ipv4_address(const char *wanted, char *output, const size_t output_size) {
    output[0] = '\0';

    struct ifaddrs *interfaces = NULL;
    if (getifaddrs(&interfaces) != 0) return 0;

    for (const struct ifaddrs *entry = interfaces; entry; entry = entry->ifa_next) {
        if (!entry->ifa_addr || entry->ifa_addr->sa_family != AF_INET) continue;

        if (wanted) {
            if (strcmp(entry->ifa_name, wanted) != 0) continue;
        } else {
            if (!(entry->ifa_flags & IFF_UP) || !(entry->ifa_flags & IFF_RUNNING)) continue;
            if (entry->ifa_flags & IFF_LOOPBACK) continue;
        }

        const struct sockaddr_in *address = (const struct sockaddr_in *) entry->ifa_addr;
        if (address->sin_addr.s_addr == htonl(INADDR_ANY)) continue;

        if (inet_ntop(AF_INET, &address->sin_addr, output, output_size)) break;
    }

    freeifaddrs(interfaces);
    return output[0] ? 1 : 0;
}

static int saved_ipv4_address(char *output, const size_t output_size) {
    char *saved = read_line_char_from(CONF_CONFIG_PATH "network/address", 1);
    if (!saved) return 0;

    struct in_addr address;
    const int valid = inet_pton(AF_INET, saved, &address) == 1 && address.s_addr != htonl(INADDR_ANY)
                      && str_copy_checked(output, output_size, saved);
    free(saved);

    return valid;
}

int get_network_ipv4_address(char *output, const size_t output_size) {
    if (!output || output_size == 0 || !device.network.interface[0]) return 0;
    if (scan_ipv4_address(device.network.interface, output, output_size)) return 1;

    return saved_ipv4_address(output, output_size);
}

int get_any_ipv4_address(char *output, const size_t output_size) {
    if (!output || output_size == 0) return 0;

    if (device.network.interface[0] && scan_ipv4_address(device.network.interface, output, output_size)) return 1;
    if (scan_ipv4_address(NULL, output, output_size)) return 1;

    return saved_ipv4_address(output, output_size);
}

int is_bluetooth_connected(void) {
    FILE *paired = fopen(CONF_CONFIG_PATH "bluetooth/paired", "r");
    if (!paired) return 0;

    char line[160];
    int connected = 0;

    while (fgets(line, sizeof(line), paired)) {
        const char *space = strchr(line, ' ');
        if (space && strtol(space + 1, NULL, 10) == 1) {
            connected = 1;
            break;
        }
    }

    fclose(paired);
    return connected;
}

int resolution_check(const char *theme_path) {
    LOG_INFO(mux_module, "Inspecting theme for supported resolutions: %s", theme_path);
    const char *resolutions[] = {"640x480", "720x480", "720x576", "720x720", "1024x768", "1280x720", "1920x1080"};

    // Check if the folder name matches any target resolutions
    for (size_t j = 0; j < A_SIZE(resolutions); j++) {
        char theme_resolution_path[MAX_BUFFER_SIZE];
        snprintf(theme_resolution_path, sizeof(theme_resolution_path), "%s/%s", theme_path, resolutions[j]);
        if (dir_exist(theme_resolution_path)) {
            LOG_SUCCESS(mux_module, "Found supported resolution: %s", resolutions[j]);
            return 1;
        }
    }

    LOG_WARN(mux_module, "No supported resolutions found");

    return 0;
}

struct screen_dimension get_device_dimensions(void) {
    struct screen_dimension dims;
    if (read_line_int_from(device.screen.hdmi, 1)) {
        dims.width = device.screen.external.width;
        dims.height = device.screen.external.height;
    } else {
        dims.width = device.screen.internal.width;
        dims.height = device.screen.internal.height;
    }

    LOG_INFO(mux_module, "Screen Output dims: %dx%d", dims.width, dims.height);
    return dims;
}

int brightness_to_percent(const int val) {
    if (device.screen.bright <= 0) return 0;
    return val * 100 / device.screen.bright;
}

int volume_to_percent(const int val) {
    const int max = config.settings.advanced.overdrive ? 200 : 100;
    return val * 100 / max;
}

char *get_version(const int verify) {
    static char version[64];
    char *display_version = str_replace(config.system.version, "_", " ");
    snprintf(version, sizeof(version), "%s%s", display_version, verify ? "*" : "");
    free(display_version);
    return version;
}

char *get_build(void) {
    static char build[16];
    snprintf(build, sizeof(build), "%s", config.system.build);
    return build;
}

char *get_storage_label(const char *path, const char *primary, const char *secondary, const char *external) {
    if (!path) return "Unknown";

    if (strncmp(path, device.storage.rom.mount, strlen(device.storage.rom.mount)) == 0) return (char *) primary;
    if (strncmp(path, device.storage.sdcard.mount, strlen(device.storage.sdcard.mount)) == 0) return (char *) secondary;
    if (strncmp(path, device.storage.usb.mount, strlen(device.storage.usb.mount)) == 0) return (char *) external;

    return "Unknown";
}

const char *resolve_info_path(const char *rel) {
    if (rel[0] == '/') return NULL;

    static char path[PATH_MAX];

    const struct {
        const char *base;
        const char *sub;
    } sources[] = {
        {device.storage.usb.mount, "MUOS/info"},
        {device.storage.sdcard.mount, "MUOS/info"},
        {device.storage.rom.mount, "MUOS/info"},
        {"/opt/muos/share", "info"}
    };

    for (size_t i = 0; i < sizeof(sources) / sizeof(sources[0]); i++) {
        if (!sources[i].base || !*sources[i].base) continue;

        snprintf(path, sizeof(path), "%s/%s/%s", sources[i].base, sources[i].sub, rel);

        remove_double_slashes(path);
        if (file_exist(path)) return path;
    }

    return NULL;
}
