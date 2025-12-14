#include "muxshare.h"

typedef enum {
    LOCAL_PLAYSTYLE_UNKNOWN,         // Something could not be calculated
    LOCAL_PLAYSTYLE_ONE_AND_DONE,    // Played once with short total time
    LOCAL_PLAYSTYLE_SAMPLER,         // Tried multiple times but never stuck
    LOCAL_PLAYSTYLE_SHORT_BURSTS,    // Multiple very short sessions
    LOCAL_PLAYSTYLE_LONG_SESSIONS,   // Fewer but long play sessions
    LOCAL_PLAYSTYLE_COMPLETIONIST,   // Long sessions with high total time
    LOCAL_PLAYSTYLE_ABANDONED,       // Dropped quickly after a try
    LOCAL_PLAYSTYLE_MARATHONER,      // Extremely high total time investment
    LOCAL_PLAYSTYLE_RETURNER,        // Frequently revisited with low commitment
    LOCAL_PLAYSTYLE_ON_OFF,          // Inconsistent play pattern
    LOCAL_PLAYSTYLE_WEEKEND_WARRIOR, // Infrequent but very long sessions
    LOCAL_PLAYSTYLE_COMFORT,         // Regularly revisited with steady enjoyment
    LOCAL_PLAYSTYLE_REGULAR          // Balanced and consistent play sessions
} local_playstyle_t;

typedef enum {
    GLOBAL_PLAYSTYLE_UNKNOWN,       // Something could not be calculated
    GLOBAL_PLAYSTYLE_CASUAL,        // Low time and low engagement overall
    GLOBAL_PLAYSTYLE_CORE_GAMER,    // Regular and consistent engagement
    GLOBAL_PLAYSTYLE_EXPLORER,      // Multiple titles with light commitment
    GLOBAL_PLAYSTYLE_BINGER,        // Long average sessions
    GLOBAL_PLAYSTYLE_COMPLETIONIST, // Deep focus on few titles
    GLOBAL_PLAYSTYLE_POWER_USER,    // Heavy usage across many systems
    GLOBAL_PLAYSTYLE_COLLECTOR,     // Multiple titles with short sessions
    GLOBAL_PLAYSTYLE_SPECIALIST,    // Focused on few cores and emulators
    GLOBAL_PLAYSTYLE_NOMAD,         // Plays across many multiple devices
    GLOBAL_PLAYSTYLE_ROUTINE,       // Consistent play pattern
    GLOBAL_PLAYSTYLE_HABITUAL,      // Frequent launches with steady timing
    GLOBAL_PLAYSTYLE_WINDOW         // Dips into many titles briefly
} global_playstyle_t;

typedef struct {
    char name[256]; // To be fair exFAT is max 255 :D

    char core[64];
    int core_count;

    int launch_count;

    char device[64];
    int device_count;

    char mode[16];
    int mode_count;

    int start_time;
    int average_time;
    int total_time;
    int last_session;
} activity_item_t;

typedef struct {
    char core[64];
    int core_count;

    char device[64];
    int device_count;

    char mode[16];
    int mode_count;

    int total_launches;
    int total_time;
    int average_time;

    char top_time[256];
    int top_time_value;

    char top_launch[256];
    int top_launch_value;

    char first_content[256];
    int first_content_time;

    size_t unique_titles;

    int unique_cores;
    int unique_devices;
    int unique_modes;

    char longest_session[256];
    int longest_session_duration;

    int active_hour; // 0–23, -1 if unknown
    int favourite_day; // 0–6 (Sun–Sat), -1 if unknown

    global_playstyle_t global_playstyle;
} global_stats_t;

// 0 = Time Played
// 1 = Launch Count
static int activity_display_mode = 0;

static int in_detail_view = 0;
static int in_global_view = 0;

static int overview_item_index = 0;

static activity_item_t *activity_items = NULL;
static size_t activity_count = 0;
static size_t activity_capacity = 0;

static char *playtime_json_str = NULL;
static struct json playtime_json_root = {0};
static int playtime_json_loaded = 0;

static void show_help(void) {
    show_info_box(lang.MUXACTIVITY.TITLE, lang.MUXACTIVITY.HELP, 0);
}

static void ensure_activity_capacity(void) {
    if (activity_count < activity_capacity) return;

    size_t new_capacity = activity_capacity ? activity_capacity * 2 : 256;
    activity_item_t *items = realloc(activity_items, new_capacity * sizeof(activity_item_t));

    if (!items) return;

    activity_items = items;
    activity_capacity = new_capacity;
}

void free_activity_items(activity_item_t **activity_items, size_t *count) {
    free(*activity_items);

    *activity_items = NULL;
    *count = 0;
    activity_capacity = 0;

    free(playtime_json_str);

    playtime_json_str = NULL;
    playtime_json_loaded = 0;
}

static const char *hour_label(int hour) {
    static char buf[32];
    if (hour < 0 || hour > 23) return lang.GENERIC.UNKNOWN;

    int h = hour % 12;
    if (h == 0) h = 12;

    snprintf(buf, sizeof(buf), "%d %s", h, hour < 12 ? "AM" : "PM");
    return buf;
}

static const char *weekday_label(int day) {
    static const char *days[] = {
            lang.GENERIC.SUNDAY,
            lang.GENERIC.MONDAY,
            lang.GENERIC.TUESDAY,
            lang.GENERIC.WEDNESDAY,
            lang.GENERIC.THURSDAY,
            lang.GENERIC.FRIDAY,
            lang.GENERIC.SATURDAY
    };

    if (day < 0 || day > 6) return lang.GENERIC.UNKNOWN;
    return days[day];
}

static const char *local_playstyle_name(local_playstyle_t ps) {
    switch (ps) {
        case LOCAL_PLAYSTYLE_ONE_AND_DONE:
            return lang.MUXACTIVITY.STYLE.LOCAL.ONE;
        case LOCAL_PLAYSTYLE_SAMPLER:
            return lang.MUXACTIVITY.STYLE.LOCAL.SAMPLER;
        case LOCAL_PLAYSTYLE_SHORT_BURSTS:
            return lang.MUXACTIVITY.STYLE.LOCAL.BURST;
        case LOCAL_PLAYSTYLE_LONG_SESSIONS:
            return lang.MUXACTIVITY.STYLE.LOCAL.LONG;
        case LOCAL_PLAYSTYLE_COMPLETIONIST:
            return lang.MUXACTIVITY.STYLE.LOCAL.COMPLETIONIST;
        case LOCAL_PLAYSTYLE_ABANDONED:
            return lang.MUXACTIVITY.STYLE.LOCAL.ABANDONED;
        case LOCAL_PLAYSTYLE_MARATHONER:
            return lang.MUXACTIVITY.STYLE.LOCAL.MARATHONER;
        case LOCAL_PLAYSTYLE_RETURNER:
            return lang.MUXACTIVITY.STYLE.LOCAL.RETURNER;
        case LOCAL_PLAYSTYLE_ON_OFF:
            return lang.MUXACTIVITY.STYLE.LOCAL.ON_OFF;
        case LOCAL_PLAYSTYLE_WEEKEND_WARRIOR:
            return lang.MUXACTIVITY.STYLE.LOCAL.WEEKEND;
        case LOCAL_PLAYSTYLE_COMFORT:
            return lang.MUXACTIVITY.STYLE.LOCAL.COMFORT;
        case LOCAL_PLAYSTYLE_REGULAR:
            return lang.MUXACTIVITY.STYLE.LOCAL.REGULAR;
        default:
            return lang.GENERIC.UNKNOWN;
    }
}

static const char *global_playstyle_name(global_playstyle_t ps) {
    switch (ps) {
        case GLOBAL_PLAYSTYLE_CASUAL:
            return lang.MUXACTIVITY.STYLE.GLOBAL.CASUAL;
        case GLOBAL_PLAYSTYLE_CORE_GAMER:
            return lang.MUXACTIVITY.STYLE.GLOBAL.CORE;
        case GLOBAL_PLAYSTYLE_EXPLORER:
            return lang.MUXACTIVITY.STYLE.GLOBAL.EXPLORER;
        case GLOBAL_PLAYSTYLE_BINGER:
            return lang.MUXACTIVITY.STYLE.GLOBAL.BINGER;
        case GLOBAL_PLAYSTYLE_COMPLETIONIST:
            return lang.MUXACTIVITY.STYLE.GLOBAL.COMPLETIONIST;
        case GLOBAL_PLAYSTYLE_POWER_USER:
            return lang.MUXACTIVITY.STYLE.GLOBAL.POWER;
        case GLOBAL_PLAYSTYLE_COLLECTOR:
            return lang.MUXACTIVITY.STYLE.GLOBAL.COLLECTOR;
        case GLOBAL_PLAYSTYLE_SPECIALIST:
            return lang.MUXACTIVITY.STYLE.GLOBAL.SPECIALIST;
        case GLOBAL_PLAYSTYLE_NOMAD:
            return lang.MUXACTIVITY.STYLE.GLOBAL.NOMAD;
        case GLOBAL_PLAYSTYLE_ROUTINE:
            return lang.MUXACTIVITY.STYLE.GLOBAL.ROUTINE;
        case GLOBAL_PLAYSTYLE_HABITUAL:
            return lang.MUXACTIVITY.STYLE.GLOBAL.HABITUAL;
        case GLOBAL_PLAYSTYLE_WINDOW:
            return lang.MUXACTIVITY.STYLE.GLOBAL.WINDOW;
        default:
            return lang.GENERIC.UNKNOWN;
    }
}

// Static calculated values - much nicer on the brain to calculate!
// Abusing the enum system to get the values is quite devilish :D
enum {
    SEC_15M = 15 * 60,
    SEC_20M = 20 * 60,
    SEC_30M = 30 * 60,
    SEC_45M = 45 * 60,
    SEC_90M = 90 * 60,

    SEC_1H = 60 * 60,
    SEC_2H = 2 * 3600,
    SEC_3H = 3 * 3600,
    SEC_8H = 8 * 3600,
    SEC_10H = 10 * 3600,
    SEC_15H = 15 * 3600,
    SEC_20H = 20 * 3600,
    SEC_50H = 50 * 3600,
    SEC_80H = 80 * 3600,
    SEC_100H = 100 * 3600
};

// Compile-time sanity checks!
// https://www.gnu.org/software/c-intro-and-ref/manual/html_node/Static-Assertions.html
_Static_assert(SEC_1H == 3600, "1 hour must be exactly 3600 seconds");
_Static_assert(SEC_30M < SEC_1H, "Minute and hour ordering broken");
_Static_assert(SEC_2H > SEC_1H, "Hour scale 2H to 1H broken");
_Static_assert(SEC_100H > SEC_50H, "High end hour thresholds broken");
_Static_assert(SEC_15M < SEC_20M && SEC_20M < SEC_30M && SEC_30M < SEC_45M && SEC_45M < SEC_90M,
               "Minute thresholds must be increasing...");
_Static_assert(SEC_1H < SEC_2H && SEC_2H < SEC_3H && SEC_3H < SEC_8H && SEC_8H < SEC_10H &&
               SEC_10H < SEC_15H && SEC_15H < SEC_20H && SEC_20H < SEC_50H &&
               SEC_50H < SEC_80H && SEC_80H < SEC_100H,
               "Hour thresholds must be increasing...");

static local_playstyle_t resolve_local_playstyle(int launches, int total_time) {
    if (launches <= 0 || total_time <= 0)
        return LOCAL_PLAYSTYLE_UNKNOWN;

    const int avg = total_time / launches;

    if (launches == 1 && total_time < SEC_2H)
        return LOCAL_PLAYSTYLE_ONE_AND_DONE;

    if (launches <= 2 && total_time < SEC_30M)
        return LOCAL_PLAYSTYLE_ABANDONED;

    if (launches >= 3 && total_time < SEC_1H)
        return LOCAL_PLAYSTYLE_SAMPLER;

    if (total_time >= SEC_100H && launches >= 10)
        return LOCAL_PLAYSTYLE_MARATHONER;

    if (total_time >= SEC_20H && launches >= 5 && avg >= SEC_2H)
        return LOCAL_PLAYSTYLE_COMPLETIONIST;

    if (launches <= 6 && avg >= SEC_3H && total_time >= SEC_8H)
        return LOCAL_PLAYSTYLE_WEEKEND_WARRIOR;

    if (launches >= 10 && avg < SEC_15M && total_time >= SEC_2H)
        return LOCAL_PLAYSTYLE_SHORT_BURSTS;

    if (launches >= 15 && avg < SEC_45M && total_time < SEC_10H)
        return LOCAL_PLAYSTYLE_RETURNER;

    if (launches >= 3 && launches <= 8 && avg < SEC_30M)
        return LOCAL_PLAYSTYLE_ON_OFF;

    if (launches >= 15 && avg >= SEC_20M && avg <= SEC_90M &&
        total_time >= SEC_15H && total_time <= SEC_80H)
        return LOCAL_PLAYSTYLE_COMFORT;

    if (launches >= 5 && avg >= SEC_30M && avg <= SEC_2H)
        return LOCAL_PLAYSTYLE_REGULAR;

    if (launches <= 5 && avg >= SEC_2H)
        return LOCAL_PLAYSTYLE_LONG_SESSIONS;

    return LOCAL_PLAYSTYLE_UNKNOWN;
}

static global_playstyle_t resolve_global_playstyle(const global_stats_t *gs) {
    if (gs->total_time <= 0)
        return GLOBAL_PLAYSTYLE_UNKNOWN;

    const int avg = gs->average_time;

    if (gs->unique_devices >= 3)
        return GLOBAL_PLAYSTYLE_NOMAD;

    if (gs->total_launches >= 200 && gs->unique_cores >= 10)
        return GLOBAL_PLAYSTYLE_POWER_USER;

    if (gs->unique_cores <= 2 && gs->total_time >= SEC_80H)
        return GLOBAL_PLAYSTYLE_SPECIALIST;

    if (gs->unique_titles <= 5 && gs->total_time >= SEC_100H && avg >= SEC_2H)
        return GLOBAL_PLAYSTYLE_COMPLETIONIST;

    if (avg >= SEC_2H)
        return GLOBAL_PLAYSTYLE_BINGER;

    if (gs->unique_titles >= 45 && avg < SEC_20M)
        return GLOBAL_PLAYSTYLE_COLLECTOR;

    if (gs->unique_titles >= 20 && avg < SEC_30M && gs->total_time < SEC_50H)
        return GLOBAL_PLAYSTYLE_WINDOW;

    if (gs->unique_titles >= 10 && gs->unique_titles < 45 &&
        avg < SEC_45M && gs->total_time < SEC_80H)
        return GLOBAL_PLAYSTYLE_EXPLORER;

    if (gs->total_launches >= 75 &&
        avg >= SEC_20M && avg <= SEC_90M &&
        gs->unique_titles >= 5 && gs->unique_titles <= 15)
        return GLOBAL_PLAYSTYLE_ROUTINE;

    if (gs->total_launches >= 100 && avg >= SEC_30M)
        return GLOBAL_PLAYSTYLE_CORE_GAMER;

    if (gs->total_launches >= 50 && avg >= SEC_30M)
        return GLOBAL_PLAYSTYLE_HABITUAL;

    if (gs->total_time < SEC_10H && gs->total_launches < 100)
        return GLOBAL_PLAYSTYLE_CASUAL;

    return GLOBAL_PLAYSTYLE_UNKNOWN;
}

static int cmp_activity_time(const void *a, const void *b) {
    const activity_item_t *x = (const activity_item_t *) a;
    const activity_item_t *y = (const activity_item_t *) b;

    if (y->total_time < x->total_time) return -1;
    if (y->total_time > x->total_time) return 1;

    return 0;
}

static int cmp_activity_launch(const void *a, const void *b) {
    const activity_item_t *x = (const activity_item_t *) a;
    const activity_item_t *y = (const activity_item_t *) b;

    if (y->launch_count < x->launch_count) return -1;
    if (y->launch_count > x->launch_count) return 1;

    return 0;
}

static const char *export_timestamp(void) {
    static char buf[64];

    time_t now = time(NULL);
    struct tm tm_buf;

    struct tm *tm = localtime_r(&now, &tm_buf);
    if (!tm) {
        snprintf(buf, sizeof(buf), "%s", lang.GENERIC.UNKNOWN);
        return buf;
    }

    strftime(buf, sizeof(buf), TIME_STRING, tm);
    return buf;
}

static const char *format_timestamp(int epoch) {
    static char buf[64];

    if (epoch <= 0) {
        snprintf(buf, sizeof(buf), "%s", lang.GENERIC.UNKNOWN);
        return buf;
    }

    time_t t = (time_t) epoch;
    struct tm tm_buf;

    struct tm *tm = localtime_r(&t, &tm_buf);
    if (!tm) {
        snprintf(buf, sizeof(buf), "%s", lang.GENERIC.UNKNOWN);
        return buf;
    }

    strftime(buf, sizeof(buf), TIME_STRING, tm);
    return buf;
}

static void load_playtime_json_once(void) {
    if (playtime_json_loaded) return;

    char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), INFO_ACT_PATH "/" PLAYTIME_DATA);

    if (!file_exist(path)) {
        playtime_json_loaded = 1;
        return;
    }

    playtime_json_str = read_all_char_from(path);
    if (!playtime_json_str) {
        playtime_json_loaded = 1;
        return;
    }

    if (!json_valid(playtime_json_str)) {
        free(playtime_json_str);
        playtime_json_str = NULL;
        playtime_json_loaded = 1;
        return;
    }

    playtime_json_root = json_parse(playtime_json_str);
    playtime_json_loaded = 1;
}

static struct json get_playtime_json(void) {
    load_playtime_json_once();
    return playtime_json_root;
}

static void normalise_json_values(char *dst, size_t dst_size, const char *src) {
    if (!src || !*src) {
        dst[0] = '\0';
        return;
    }

    while (*src == ' ') src++;

    size_t len = strlen(src);
    while (len > 0 && src[len - 1] == ' ') len--;

    size_t n = (len < dst_size - 1) ? len : (dst_size - 1);
    for (size_t i = 0; i < n; i++) dst[i] = (char) tolower((unsigned char) src[i]);

    dst[n] = '\0';
}

static void compute_global_stats(global_stats_t *gs) {
    memset(gs, 0, sizeof(*gs));

    struct {
        char key[64];
        int count;
    } core_map[256];

    struct {
        char key[64];
        int count;
    } device_map[256];

    struct {
        char key[16];
        int count;
    } mode_map[256];

    int core_used = 0;
    int device_used = 0;
    int mode_used = 0;

    // It's pronounced bouquet
    int hour_buckets[24] = {0};
    int day_buckets[7] = {0};

    gs->active_hour = -1;
    gs->favourite_day = -1;
    gs->top_time_value = -1;
    gs->top_launch_value = -1;
    gs->top_time[0] = '\0';
    gs->top_launch[0] = '\0';
    gs->first_content_time = INT_MAX;
    gs->longest_session_duration = 0;

    // Currently one activity item per unique title in JSON
    gs->unique_titles = activity_count;

    for (size_t i = 0; i < activity_count; i++) {
        activity_item_t *it = &activity_items[i];

        gs->total_launches += it->launch_count;
        gs->total_time += it->total_time;

        if (it->total_time > gs->top_time_value) {
            gs->top_time_value = it->total_time;
            snprintf(gs->top_time, sizeof(gs->top_time), "%s", it->name);
        }

        if (it->launch_count > gs->top_launch_value) {
            gs->top_launch_value = it->launch_count;
            snprintf(gs->top_launch, sizeof(gs->top_launch), "%s", it->name);
        }

        if (it->start_time > 0 && it->start_time < gs->first_content_time) {
            gs->first_content_time = it->start_time;
            snprintf(gs->first_content, sizeof(gs->first_content), "%s", it->name);
        }

        if (it->start_time > 0 && it->last_session > 0) {
            time_t t = (time_t) it->start_time;
            struct tm tm_buf;
            struct tm *tm = localtime_r(&t, &tm_buf);

            if (tm) {
                hour_buckets[tm->tm_hour] += it->total_time;
                day_buckets[tm->tm_wday] += it->total_time;
            }
        }

        if (it->last_session > gs->longest_session_duration) {
            gs->longest_session_duration = it->last_session;
            snprintf(gs->longest_session, sizeof(gs->longest_session), "%s", it->name);
        }

        int found;

        char norm_core[64];
        normalise_json_values(norm_core, sizeof(norm_core), it->core);

        found = 0;
        for (int j = 0; j < core_used; j++) {
            if (strcmp(core_map[j].key, norm_core) == 0) {
                core_map[j].count += it->core_count;
                found = 1;
                break;
            }
        }

        if (!found && norm_core[0] != '\0' && core_used < (int) (sizeof(core_map) / sizeof(core_map[0]))) {
            strncpy(core_map[core_used].key, norm_core, sizeof(core_map[core_used].key) - 1);

            core_map[core_used].key[sizeof(core_map[0].key) - 1] = '\0';
            core_map[core_used].count = it->core_count;

            core_used++;
        }

        char norm_device[64];
        normalise_json_values(norm_device, sizeof(norm_device), it->device);

        found = 0;
        for (int j = 0; j < device_used; j++) {
            if (strcmp(device_map[j].key, norm_device) == 0) {
                device_map[j].count += it->device_count;
                found = 1;
                break;
            }
        }

        if (!found && norm_device[0] != '\0' && device_used < (int) (sizeof(device_map) / sizeof(device_map[0]))) {
            strncpy(device_map[device_used].key, norm_device, sizeof(device_map[device_used].key) - 1);

            device_map[device_used].key[sizeof(device_map[0].key) - 1] = '\0';
            device_map[device_used].count = it->device_count;

            device_used++;
        }

        char norm_mode[16];
        normalise_json_values(norm_mode, sizeof(norm_mode), it->mode);

        found = 0;
        for (int j = 0; j < mode_used; j++) {
            if (strcmp(mode_map[j].key, norm_mode) == 0) {
                mode_map[j].count += it->mode_count;
                found = 1;
                break;
            }
        }

        if (!found && norm_mode[0] != '\0' && mode_used < (int) (sizeof(mode_map) / sizeof(mode_map[0]))) {
            strncpy(mode_map[mode_used].key, norm_mode, sizeof(mode_map[mode_used].key) - 1);

            mode_map[mode_used].key[sizeof(mode_map[0].key) - 1] = '\0';
            mode_map[mode_used].count = it->mode_count;

            mode_used++;
        }
    }

    int max = -1;
    for (int i = 0; i < core_used; i++) {
        if (core_map[i].count > max) {
            max = core_map[i].count;
            strncpy(gs->core, core_map[i].key, sizeof(gs->core) - 1);
            gs->core[sizeof(gs->core) - 1] = '\0';
            gs->core_count = core_map[i].count;
        }
    }

    max = -1;
    for (int i = 0; i < device_used; i++) {
        if (device_map[i].count > max) {
            max = device_map[i].count;
            strncpy(gs->device, device_map[i].key, sizeof(gs->device) - 1);
            gs->device[sizeof(gs->device) - 1] = '\0';
            gs->device_count = device_map[i].count;
        }
    }

    max = -1;
    for (int i = 0; i < mode_used; i++) {
        if (mode_map[i].count > max) {
            max = mode_map[i].count;
            strncpy(gs->mode, mode_map[i].key, sizeof(gs->mode) - 1);
            gs->mode[sizeof(gs->mode) - 1] = '\0';
            gs->mode_count = mode_map[i].count;
        }
    }

    max = -1;
    for (int i = 0; i < 24; i++) {
        if (hour_buckets[i] > max) {
            max = hour_buckets[i];
            gs->active_hour = i;
        }
    }

    max = -1;
    for (int i = 0; i < 7; i++) {
        if (day_buckets[i] > max) {
            max = day_buckets[i];
            gs->favourite_day = i;
        }
    }

    if (activity_count > 0) {
        gs->unique_cores = core_used;
        gs->unique_devices = device_used;
        gs->unique_modes = mode_used;

        gs->average_time = gs->total_launches > 0 ? gs->total_time / gs->total_launches : 0;
        gs->global_playstyle = resolve_global_playstyle(gs);
    }
}

static void load_activity_items(void) {
    struct json playtime_json = get_playtime_json();
    if (!json_exists(playtime_json)) return;

    activity_count = 0;

    struct json child = json_first(playtime_json);
    while (json_exists(child)) {
        struct json key = child;
        struct json val = json_next(key);

        if (!json_exists(val)) break;

        if (json_type(val) == JSON_OBJECT) {
            struct json name_json = json_object_get(val, "name");
            struct json time_json = json_object_get(val, "total_time");
            struct json launches_json = json_object_get(val, "launches");

            if (json_exists(name_json) && json_exists(time_json) && json_exists(launches_json)) {
                ensure_activity_capacity();
                if (activity_count >= activity_capacity) break;

                activity_item_t *it = &activity_items[activity_count];
                json_string_copy(name_json, it->name, sizeof(it->name));

                it->total_time = json_int(time_json);
                it->launch_count = json_int(launches_json);

                struct json last_core_json = json_object_get(val, "last_core");
                struct json core_launch_json = json_object_get(val, "core_launches");

                if (json_exists(last_core_json)) {
                    json_string_copy(last_core_json, it->core, sizeof(it->core));
                } else {
                    it->core[0] = '\0';
                }

                it->core_count = 0;
                if (json_exists(core_launch_json) && json_exists(last_core_json)) {
                    char core_key[64];
                    json_string_copy(last_core_json, core_key, sizeof(core_key));

                    struct json core_count_json = json_object_get(core_launch_json, core_key);
                    if (json_exists(core_count_json)) it->core_count = json_int(core_count_json);
                }

                struct json last_device_json = json_object_get(val, "last_device");
                struct json device_launch_json = json_object_get(val, "device_launches");

                if (json_exists(last_device_json)) {
                    json_string_copy(last_device_json, it->device, sizeof(it->device));
                } else {
                    it->device[0] = '\0';
                }

                it->device_count = 0;
                if (json_exists(device_launch_json) && json_exists(last_device_json)) {
                    char dev_key[64];
                    json_string_copy(last_device_json, dev_key, sizeof(dev_key));

                    struct json dev_count_json = json_object_get(device_launch_json, dev_key);
                    if (json_exists(dev_count_json)) it->device_count = json_int(dev_count_json);
                }

                struct json last_mode_json = json_object_get(val, "last_mode");
                struct json mode_launch_json = json_object_get(val, "mode_launches");

                if (json_exists(last_mode_json)) {
                    json_string_copy(last_mode_json, it->mode, sizeof(it->mode));
                } else {
                    it->mode[0] = '\0';
                }

                it->mode_count = 0;
                if (json_exists(mode_launch_json) && json_exists(last_mode_json)) {
                    char mode_key[16];
                    json_string_copy(last_mode_json, mode_key, sizeof(mode_key));

                    struct json mode_count_json = json_object_get(mode_launch_json, mode_key);
                    if (json_exists(mode_count_json)) it->mode_count = json_int(mode_count_json);
                }

                it->start_time = json_int(json_object_get(val, "start_time"));
                it->average_time = json_int(json_object_get(val, "avg_time"));
                it->last_session = json_int(json_object_get(val, "last_session"));

                activity_count++;
            }
        }

        child = json_next(val);
    }
}

static char *format_total_time(int total_time) {
    static char time_buffer[MAX_BUFFER_SIZE] = "0m";

    int days = total_time / 86400;
    int hours = (total_time % 86400) / 3600;
    int minutes = (total_time % 3600) / 60;

    if (days > 0) {
        snprintf(time_buffer, sizeof(time_buffer), "%dd %dh %dm",
                 days, hours, minutes);
    } else if (hours > 0) {
        snprintf(time_buffer, sizeof(time_buffer), "%dh %dm",
                 hours, minutes);
    } else if (minutes > 0) {
        snprintf(time_buffer, sizeof(time_buffer), "%dm",
                 minutes);
    } else {
        snprintf(time_buffer, sizeof(time_buffer), "0m");
    }

    return time_buffer;
}

static void refresh_activity_labels(void) {
    lv_group_remove_all_objs(ui_group);
    lv_group_remove_all_objs(ui_group_value);
    lv_group_remove_all_objs(ui_group_glyph);
    lv_group_remove_all_objs(ui_group_panel);

    char current_activity_mode[MAX_BUFFER_SIZE];
    snprintf(current_activity_mode, sizeof(current_activity_mode), "%s",
             activity_display_mode ? lang.MUXACTIVITY.TIME : lang.MUXACTIVITY.LAUNCH);

    lv_label_set_text(ui_lblNavY, current_activity_mode);

    lv_obj_clean(ui_pnlContent);
    ui_count = 0;

    if (activity_display_mode == 0) {
        qsort(activity_items, activity_count, sizeof(activity_items[0]), cmp_activity_time);
    } else {
        qsort(activity_items, activity_count, sizeof(activity_items[0]), cmp_activity_launch);
    }

    for (size_t i = 0; i < activity_count; ++i) {
        ui_count++;

        char label_buffer[MAX_BUFFER_SIZE];

        if (activity_display_mode == 0) {
            snprintf(label_buffer, sizeof(label_buffer), "[%s] %s",
                     format_total_time(activity_items[i].total_time),
                     activity_items[i].name);
        } else {
            snprintf(label_buffer, sizeof(label_buffer), "[%d] %s",
                     activity_items[i].launch_count,
                     activity_items[i].name);
        }

        lv_obj_t *ui_pnlAct = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlAct);

        lv_obj_set_user_data(ui_pnlAct, activity_items[i].name);

        lv_obj_t *ui_lblActItem = lv_label_create(ui_pnlAct);
        apply_theme_list_item(&theme, ui_lblActItem, label_buffer);

        lv_obj_t *ui_lblActItemGlyph = lv_img_create(ui_pnlAct);
        apply_theme_list_glyph(&theme, ui_lblActItemGlyph, mux_module, "rom");

        lv_group_add_obj(ui_group, ui_lblActItem);
        lv_group_add_obj(ui_group_glyph, ui_lblActItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlAct);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblActItem, ui_lblActItemGlyph, label_buffer);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblActItem);
    }

    lv_obj_update_layout(ui_pnlContent);

    current_item_index = 0;
}

static void show_detail_view(const activity_item_t *it) {
    lv_obj_clean(ui_pnlContent);

    ui_count = 0;
    current_item_index = 0;
    in_detail_view = 1;

    char detail_label[MAX_BUFFER_SIZE];
    char detail_value[MAX_BUFFER_SIZE];
    char detail_glyph[MAX_BUFFER_SIZE];

    enum detail_field {
        DETAIL_NAME,
        DETAIL_CORE,
        DETAIL_LAUNCH,
        DETAIL_DEVICE,
        DETAIL_MODE,
        DETAIL_START,
        DETAIL_AVERAGE,
        DETAIL_TOTAL,
        DETAIL_LAST,
        DETAIL_PLAYSTYLE,
        DETAIL_COUNT
    };

    for (int i = 0; i < DETAIL_COUNT; ++i) {
        detail_label[0] = '\0';
        detail_value[0] = '\0';
        detail_glyph[0] = '\0';

        switch (i) {
            case DETAIL_NAME:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.NAME);
                snprintf(detail_value, sizeof(detail_value), "%s", it->name);
                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_name");
                break;
            case DETAIL_CORE:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.CORE);

                char core_tmp[64];
                snprintf(core_tmp, sizeof(core_tmp), "%s", it->core);
                snprintf(detail_value, sizeof(detail_value), "%s",
                         str_replace(str_capital_all(core_tmp), "_libretro.so", " (RetroArch)"));

                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_core");
                break;
            case DETAIL_LAUNCH:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.LAUNCH);
                snprintf(detail_value, sizeof(detail_value), "%d", it->launch_count);
                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_launch");
                break;
            case DETAIL_DEVICE:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.DEVICE);

                char device_tmp[64];
                snprintf(device_tmp, sizeof(device_tmp), "%s", it->device);
                snprintf(detail_value, sizeof(detail_value), "%s", str_toupper(device_tmp));

                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_device");
                break;
            case DETAIL_MODE:
                if (!device.BOARD.HAS_HDMI) break;

                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.MODE);

                char mode_tmp[16];
                snprintf(mode_tmp, sizeof(mode_tmp), "%s", it->mode);
                snprintf(detail_value, sizeof(detail_value), "%s", str_capital(mode_tmp));

                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_mode");
                break;
            case DETAIL_START:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.START);
                snprintf(detail_value, sizeof(detail_value), "%s", format_timestamp(it->start_time));
                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_start");
                break;
            case DETAIL_AVERAGE:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.AVERAGE);
                snprintf(detail_value, sizeof(detail_value), "%s", format_total_time(it->average_time));
                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_average");
                break;
            case DETAIL_TOTAL:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.TOTAL);
                snprintf(detail_value, sizeof(detail_value), "%s", format_total_time(it->total_time));
                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_total");
                break;
            case DETAIL_LAST:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.LAST);
                snprintf(detail_value, sizeof(detail_value), "%s", format_total_time(it->last_session));
                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_last");
                break;
            case DETAIL_PLAYSTYLE:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.STYLE.LOCAL.LABEL);

                local_playstyle_t ps = resolve_local_playstyle(it->launch_count, it->total_time);
                snprintf(detail_value, sizeof(detail_value), "%s", local_playstyle_name(ps));

                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_play");
                break;
            default:
                continue;
        }

        ui_count++;

        lv_obj_t *ui_pnlAct = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlAct);

        lv_obj_t *ui_lblActItem = lv_label_create(ui_pnlAct);
        apply_theme_list_item(&theme, ui_lblActItem, detail_label);

        lv_obj_t *ui_lblActItemValue = lv_label_create(ui_pnlAct);
        apply_theme_list_value(&theme, ui_lblActItemValue, detail_value);

        lv_obj_t *ui_lblActItemGlyph = lv_img_create(ui_pnlAct);
        apply_theme_list_glyph(&theme, ui_lblActItemGlyph, mux_module, detail_glyph);

        lv_group_add_obj(ui_group, ui_lblActItem);
        lv_group_add_obj(ui_group_value, ui_lblActItemValue);
        lv_group_add_obj(ui_group_glyph, ui_lblActItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlAct);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblActItem, ui_lblActItemGlyph, detail_label);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblActItem);
    }

    lv_obj_update_layout(ui_pnlContent);
}

static void show_global_view(void) {
    lv_obj_clean(ui_pnlContent);

    ui_count = 0;
    current_item_index = 0;
    in_global_view = 1;

    global_stats_t gs;
    compute_global_stats(&gs);

    char global_label[MAX_BUFFER_SIZE];
    char global_value[MAX_BUFFER_SIZE];
    char global_glyph[MAX_BUFFER_SIZE];

    enum global_field {
        GLOBAL_TOP_TIME,
        GLOBAL_TOP_LAUNCH,
        GLOBAL_CORE,
        GLOBAL_DEVICE,
        GLOBAL_MODE,
        GLOBAL_LAUNCHES,
        GLOBAL_TOTAL_TIME,
        GLOBAL_AVERAGE_TIME,
        GLOBAL_FIRST_GAME,
        GLOBAL_LONGEST_SESSION,
        GLOBAL_PLAYSTYLE,
        GLOBAL_UNIQUE_TITLES,
        GLOBAL_UNIQUE_CORES,
        GLOBAL_ACTIVE_TIME,
        GLOBAL_FAVOURITE_DAY,
        GLOBAL_COUNT
    };

    for (int i = 0; i < GLOBAL_COUNT; i++) {
        global_label[0] = '\0';
        global_value[0] = '\0';
        global_glyph[0] = '\0';

        switch (i) {
            case GLOBAL_TOP_TIME:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.TOP_TIME);
                snprintf(global_value, sizeof(global_value), "%s", gs.top_time);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_toptime");
                break;
            case GLOBAL_TOP_LAUNCH:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.TOP_LAUNCH);
                snprintf(global_value, sizeof(global_value), "%s", gs.top_launch);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_toplaunch");
                break;
            case GLOBAL_CORE:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.CORE);

                char core_tmp[64];
                snprintf(core_tmp, sizeof(core_tmp), "%s", gs.core);
                snprintf(global_value, sizeof(global_value), "%s",
                         str_replace(str_capital_all(core_tmp), "_libretro.so", " (RetroArch)"));

                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_core");
                break;
            case GLOBAL_DEVICE:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.DEVICE);

                char device_tmp[64];
                snprintf(device_tmp, sizeof(device_tmp), "%s", gs.device);
                snprintf(global_value, sizeof(global_value), "%s", str_toupper(device_tmp));

                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_device");
                break;
            case GLOBAL_MODE:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.MODE);

                char mode_tmp[16];
                snprintf(mode_tmp, sizeof(mode_tmp), "%s", gs.mode);
                snprintf(global_value, sizeof(global_value), "%s", str_capital(mode_tmp));

                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_mode");
                break;
            case GLOBAL_LAUNCHES:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.LAUNCH);
                snprintf(global_value, sizeof(global_value), "%d", gs.total_launches);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_launch");
                break;
            case GLOBAL_TOTAL_TIME:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.TOTAL);
                snprintf(global_value, sizeof(global_value), "%s", format_total_time(gs.total_time));
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_total");
                break;
            case GLOBAL_AVERAGE_TIME:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.AVERAGE);
                snprintf(global_value, sizeof(global_value), "%s", format_total_time(gs.average_time));
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_average");
                break;
            case GLOBAL_FIRST_GAME:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.FIRST);
                snprintf(global_value, sizeof(global_value), "%s", gs.first_content);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_first");
                break;
            case GLOBAL_LONGEST_SESSION:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.LONGEST);
                snprintf(global_value, sizeof(global_value), "%s", gs.longest_session);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_longest");
                break;
            case GLOBAL_PLAYSTYLE:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.OVERALL);
                snprintf(global_value, sizeof(global_value), "%s", global_playstyle_name(gs.global_playstyle));
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_play");
                break;
            case GLOBAL_UNIQUE_TITLES:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.UNIQUE_PLAY);
                snprintf(global_value, sizeof(global_value), "%zu", gs.unique_titles);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_uniqueplay");
                break;
            case GLOBAL_UNIQUE_CORES:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.UNIQUE_CORE);
                snprintf(global_value, sizeof(global_value), "%d", gs.unique_cores);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_uniquecore");
                break;
            case GLOBAL_ACTIVE_TIME:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.ACTIVE_TIME);
                snprintf(global_value, sizeof(global_value), "%s", hour_label(gs.active_hour));
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_active");
                break;
            case GLOBAL_FAVOURITE_DAY:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.FAVOURITE_DAY);
                snprintf(global_value, sizeof(global_value), "%s", weekday_label(gs.favourite_day));
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_day");
                break;
            default:
                continue;
        }

        ui_count++;

        lv_obj_t *ui_pnlAct = lv_obj_create(ui_pnlContent);
        apply_theme_list_panel(ui_pnlAct);

        lv_obj_t *ui_lblActItem = lv_label_create(ui_pnlAct);
        apply_theme_list_item(&theme, ui_lblActItem, global_label);

        lv_obj_t *ui_lblActItemValue = lv_label_create(ui_pnlAct);
        apply_theme_list_value(&theme, ui_lblActItemValue, global_value);

        lv_obj_t *ui_lblActItemGlyph = lv_img_create(ui_pnlAct);
        apply_theme_list_glyph(&theme, ui_lblActItemGlyph, mux_module, global_glyph);

        lv_group_add_obj(ui_group, ui_lblActItem);
        lv_group_add_obj(ui_group_value, ui_lblActItemValue);
        lv_group_add_obj(ui_group_glyph, ui_lblActItemGlyph);
        lv_group_add_obj(ui_group_panel, ui_pnlAct);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblActItem, ui_lblActItemGlyph, global_label);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblActItem);
    }

    lv_obj_update_layout(ui_pnlContent);
}

static void html_escape(FILE *f, const char *s) {
    for (; *s; s++) {
        switch (*s) {
            case '&':
                fputs("&amp;", f);
                break;
            case '<':
                fputs("&lt;", f);
                break;
            case '>':
                fputs("&gt;", f);
                break;
            case '"':
                fputs("&quot;", f);
                break;
            default:
                fputc(*s, f);
                break;
        }
    }
}

static void export_activity_html(void) {
    char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), INFO_ACT_PATH "/activity_report.html");

    FILE *f = fopen(path, "w");
    if (!f) {
        toast_message("Error exporting statistics", MEDIUM);
        refresh_screen(ui_screen);
        return;
    }

    global_stats_t gs;
    compute_global_stats(&gs);

    fprintf(f,
            "<!DOCTYPE html>"
            "<html>"
            "<head>"
            "<meta http-equiv='Content-type' content='text/html; charset=utf-8'>"
            "<meta name='viewport' content='width=device-width, initial-scale=1'>"
            "<title>MustardOS - Activity Tracker</title>"

            "<link rel='stylesheet' href='https://cdn.datatables.net/2.3.5/css/dataTables.dataTables.min.css'>"

            "<script src='https://code.jquery.com/jquery-3.7.1.slim.min.js' integrity='sha256-kmHvs0B+OpCW5GVHUNjv9rOmY0IvSIRcf7zGUDTDQM8=' crossorigin='anonymous'></script>"
            "<script src='https://cdn.datatables.net/2.3.5/js/dataTables.min.js'></script>"

            "<style>"
            "body { font-family: sans-serif; background: #1f1f1f; color: #ffffff; padding: 20px }"
            "h1, h2 { color: #f7d12e }"
            "table { margin-bottom: 24px }"
            "th, td { border: 1px solid #444444; padding: 6px }"
            "th { background: #222222 }"
            "tr:nth-child(even) { background: #1a1a1a }"
            "#global { width: 400px }"
            "#detail { width: 100%% }"

            ".dt-container { color: #ffffff }"
            ".dt-search input, .dt-length select { background:#111111; color:#ffffff; border:1px solid #444444 }"
            "table.dataTable tbody tr:nth-child(even) { background-color: #1a1a1a }"
            "table.dataTable tbody tr:nth-child(odd) { background-color: #1f1f1f }"
            "table.dataTable tbody tr:hover { background-color: #2c2c2c }"

            "</style>"

            "</head>"
            "<body>"
    );

    char global_core_value[MAX_BUFFER_SIZE];
    char global_device_value[MAX_BUFFER_SIZE];
    char global_mode_value[MAX_BUFFER_SIZE];

    char global_core_tmp[64];
    snprintf(global_core_tmp, sizeof(global_core_tmp), "%s", gs.core);

    char global_device_tmp[64];
    snprintf(global_device_tmp, sizeof(global_device_tmp), "%s", gs.device);

    char global_mode_tmp[16];
    snprintf(global_mode_tmp, sizeof(global_mode_tmp), "%s", gs.mode);

    snprintf(global_core_value, sizeof(global_core_value), "%s",
             str_replace(str_capital_all(global_core_tmp), "_libretro.so", " (RetroArch)"));

    snprintf(global_device_value, sizeof(global_device_value), "%s",
             str_toupper(global_device_tmp));

    snprintf(global_mode_value, sizeof(global_mode_value), "%s",
             str_capital(global_mode_tmp));

    fprintf(f, "<h1>MustardOS - Activity Tracker</h1>");
    fprintf(f, "<p><strong>Exported:</strong> %s</p>", export_timestamp());

    fprintf(f, "<h2>Global Summary</h2><table id='global'><tr><th>Metric</th><th>Value</th></tr>");

    fprintf(f, "<tr><td>Top Content by Time</td><td>%s (%s)</td></tr>",
            gs.top_time, format_total_time(gs.top_time_value));

    fprintf(f, "<tr><td>Top Content by Launch</td><td>%s (%d)</td></tr>",
            gs.top_launch, gs.top_launch_value);

    fprintf(f, "<tr><td>Most Frequent Core</td><td>%s</td></tr>",
            global_core_value);

    fprintf(f, "<tr><td>Most Used Device</td><td>%s</td></tr>",
            global_device_value);

    fprintf(f, "<tr><td>Most Used Mode</td><td>%s</td></tr>",
            global_mode_value);

    fprintf(f, "<tr><td>Total Launch Count</td><td>%d</td></tr>",
            gs.total_launches);

    fprintf(f, "<tr><td>Total Play Time</td><td>%s</td></tr>",
            format_total_time(gs.total_time));

    fprintf(f, "<tr><td>Average Play Time</td><td>%s</td></tr>",
            format_total_time(gs.average_time));

    fprintf(f, "<tr><td>First Game Played</td><td>%s</td></tr>",
            gs.first_content);

    fprintf(f, "<tr><td>Longest Session</td><td>%s</td></tr>",
            gs.longest_session);

    fprintf(f, "<tr><td>Overall Play Style</td><td>%s</td></tr>",
            global_playstyle_name(gs.global_playstyle));

    fprintf(f, "<tr><td>Unique Content Played</td><td>%zu</td></tr>",
            gs.unique_titles);

    fprintf(f, "<tr><td>Unique Cores Used</td><td>%d</td></tr>",
            gs.unique_cores);

    fprintf(f, "<tr><td>Most Active Time</td><td>%s</td></tr>",
            hour_label(gs.active_hour));

    fprintf(f, "<tr><td>Favourite Day</td><td>%s</td></tr>",
            weekday_label(gs.favourite_day));

    fprintf(f, "</table>");

    fprintf(f,
            "<h2>Content Statistics</h2>"
            "<table id='detail' class='display' data-page-length='25'>"
            "<thead><tr>"
            "<th data-orderable='true'>Content Name</th>"
            "<th data-orderable='true'>Core Used</th>"
            "<th data-orderable='true'>Last Device</th>"
            "<th data-orderable='true'>Launch Count</th>"
            "<th data-orderable='true'>Start Time</th>"
            "<th data-orderable='true'>Average Time</th>"
            "<th data-orderable='true'>Total Time</th>"
            "<th data-orderable='true'>Last Session</th>"
            "<th data-orderable='true'>Play Style</th>"
            "</tr></thead><tbody>"
    );

    char detail_core_value[MAX_BUFFER_SIZE];
    char detail_device_value[MAX_BUFFER_SIZE];
    char detail_core_tmp[64];
    char detail_device_tmp[64];

    for (size_t i = 0; i < activity_count; i++) {
        const activity_item_t *it = &activity_items[i];
        local_playstyle_t ps = resolve_local_playstyle(it->launch_count, it->total_time);

        snprintf(detail_core_tmp, sizeof(detail_core_tmp), "%s", it->core);
        snprintf(detail_core_value, sizeof(detail_core_value), "%s",
                 str_replace(str_capital_all(detail_core_tmp), "_libretro.so", " (RetroArch)"));

        snprintf(detail_device_tmp, sizeof(detail_device_tmp), "%s", it->device);
        snprintf(detail_device_value, sizeof(detail_device_value), "%s", str_toupper(detail_device_tmp));

        fprintf(f, "<tr><td>");
        html_escape(f, it->name);
        fprintf(f, "</td>");

        fprintf(f, "<td>%s</td>", detail_core_value);
        fprintf(f, "<td>%s</td>", detail_device_value);
        fprintf(f, "<td>%d</td>", it->launch_count);
        fprintf(f, "<td>%s</td>", format_timestamp(it->start_time));
        fprintf(f, "<td>%s</td>", format_total_time(it->average_time));
        fprintf(f, "<td>%s</td>", format_total_time(it->total_time));
        fprintf(f, "<td>%s</td>", format_total_time(it->last_session));
        fprintf(f, "<td>%s</td>", local_playstyle_name(ps));
        fprintf(f, "</tr>");
    }

    fprintf(f, "</tbody></table>");

    fprintf(f,
            "<script>"
            "document.addEventListener('DOMContentLoaded', function(){"
            "  new DataTable('#detail', {"
            "    order: [[0, 'asc']],"
            "    columnDefs: ["
            "      { targets: 0, type: 'string-utf8', orderDataType: 'string-insensitive' },"
            "      { targets: [1, 8], type: 'string', orderDataType: 'string-insensitive' },"
            "      { targets: 4, type: 'date' },"
            "      { targets: 3, type: 'num' }"
            "    ]"
            "  });"
            "});"
            "</script>"
    );


    fprintf(f,
            "</body>"
            "</html>"
    );

    fclose(f);

    toast_message("Activity statistics exported", MEDIUM);
    refresh_screen(ui_screen);
}

static void generate_activity_items(void) {
    load_activity_items();

    free(playtime_json_str);
    playtime_json_str = NULL;

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    in_detail_view = 0;
    refresh_activity_labels();
}

static void list_nav_move(int steps, int direction) {
    if (!ui_count) return;
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));

        if (direction < 0) {
            current_item_index = (current_item_index == 0) ? ui_count - 1 : current_item_index - 1;
        } else {
            current_item_index = (current_item_index == ui_count - 1) ? 0 : current_item_index + 1;
        }

        nav_move(ui_group, direction);
        nav_move(ui_group_glyph, direction);
        nav_move(ui_group_panel, direction);
        nav_move(ui_group_value, direction);
    }

    update_scroll_position(theme.MUX.ITEM.COUNT, theme.MUX.ITEM.PANEL, ui_count, current_item_index, ui_pnlContent);
    set_label_long_mode(&theme, lv_group_get_focused(ui_group));
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void hide_nav(void) {
    lv_obj_add_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lblNavXGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lblNavX, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lblNavYGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lblNavY, MU_OBJ_FLAG_HIDE_FLOAT);
}

static void show_nav(void) {
    lv_obj_clear_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lblNavXGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lblNavX, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lblNavYGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lblNavY, MU_OBJ_FLAG_HIDE_FLOAT);
}

static void handle_a(void) {
    if (msgbox_active || !ui_count || hold_call || in_global_view) return;
    if (in_detail_view) return;

    play_sound(SND_CONFIRM);
    hide_nav();

    overview_item_index = current_item_index;

    size_t idx = current_item_index;
    if (idx >= activity_count) return;

    show_detail_view(&activity_items[idx]);
    nav_moved = 1;
}

static void handle_b(void) {
    if (hold_call) return;

    if (msgbox_active) {
        play_sound(SND_INFO_CLOSE);
        msgbox_active = 0;
        progress_onscreen = 0;
        lv_obj_add_flag(msgbox_element, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    play_sound(SND_BACK);

    if (in_detail_view) {
        show_nav();

        in_detail_view = 0;
        refresh_activity_labels();
        nav_moved = 1;

        list_nav_next(overview_item_index);
        return;
    }

    if (in_global_view) {
        show_nav();
        lv_label_set_text(ui_lblNavX, lang.MUXACTIVITY.GLOBAL.NAV);

        in_global_view = 0;
        refresh_activity_labels();
        nav_moved = 1;

        list_nav_next(overview_item_index);
        return;
    }

    free_activity_items(&activity_items, &activity_count);

    close_input();
    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || !ui_count || hold_call || in_detail_view) return;

    play_sound(SND_CONFIRM);

    if (in_global_view) {
        export_activity_html();
        return;
    }

    hide_nav();

    lv_obj_clear_flag(ui_lblNavXGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lblNavX, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_label_set_text(ui_lblNavX, lang.MUXACTIVITY.HTML);

    overview_item_index = current_item_index;

    show_global_view();
    nav_moved = 1;
}

static void handle_y(void) {
    if (msgbox_active || !ui_count || hold_call || in_detail_view || in_global_view) return;

    activity_display_mode = !activity_display_mode;
    play_sound(SND_NAVIGATE);

    overview_item_index = current_item_index;

    refresh_activity_labels();
    nav_moved = 1;
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count || hold_call) return;

    play_sound(SND_INFO_OPEN);
    show_help();
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {
            ui_pnlFooter,
            ui_pnlHeader,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements(void) {
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {
            {ui_lblNavAGlyph, "",                          0},
            {ui_lblNavA,      lang.MUXACTIVITY.INFO,       0},
            {ui_lblNavBGlyph, "",                          0},
            {ui_lblNavB,      lang.GENERIC.BACK,           0},
            {ui_lblNavXGlyph, "",                          0},
            {ui_lblNavX,      lang.MUXACTIVITY.GLOBAL.NAV, 0},
            {ui_lblNavYGlyph, "",                          0},
            {ui_lblNavY,      "",                          0},
            {NULL, NULL,                                   0}
    });

    overlay_display();
}

static void ui_refresh_task() {
    if (nav_moved) {
        if (lv_group_get_obj_count(ui_group) > 0) adjust_wallpaper_element(ui_group, 0, GENERAL);
        adjust_panels();

        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxactivity_main() {
    const char *m = "muxactivity";
    set_process_name(m);
    init_module(m);

    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, "");
    init_elements();

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());
    lv_label_set_text(ui_lblTitle, lang.MUXACTIVITY.TITLE);

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);
    init_fonts();

    generate_activity_items();

    if (ui_count == 0) {
        hide_nav();
        lv_label_set_text(ui_lblScreenMessage, lang.MUXACTIVITY.NONE);
    }

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
            .swap_axis = (theme.MISC.NAVIGATION_TYPE == 1),
            .press_handler = {
                    [MUX_INPUT_A] = handle_a,
                    [MUX_INPUT_B] = handle_b,
                    [MUX_INPUT_X] = handle_x,
                    [MUX_INPUT_Y] = handle_y,
                    [MUX_INPUT_MENU_SHORT] = handle_help,
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            },
            .release_handler = {
                    [MUX_INPUT_L2] = hold_call_release,
            },
            .hold_handler = {
                    [MUX_INPUT_DPAD_UP] = handle_list_nav_up_hold,
                    [MUX_INPUT_DPAD_DOWN] = handle_list_nav_down_hold,
                    [MUX_INPUT_L1] = handle_list_nav_page_up,
                    [MUX_INPUT_L2] = hold_call_set,
                    [MUX_INPUT_R1] = handle_list_nav_page_down,
            }
    };
    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, true);
    mux_input_task(&input_opts);

    return 0;
}
