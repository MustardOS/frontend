#include "muxshare.h"
#include "ui/ui_muxactivity.h"

static lv_obj_t *ui_viewport_objects[7];
static lv_obj_t *ui_img_splash;
static int starter_image = 0;
static int splash_valid = 0;
static int track_delete = 0;

typedef enum {
    local_playstyle_unknown,         // Something could not be calculated
    local_playstyle_one_and_done,    // Played once with short total time
    local_playstyle_sampler,         // Tried multiple times but never stuck
    local_playstyle_short_bursts,    // Multiple very short sessions
    local_playstyle_long_sessions,   // Fewer but long play sessions
    local_playstyle_completionist,   // Long sessions with high total time
    local_playstyle_abandoned,       // Dropped quickly after a try
    local_playstyle_marathoner,      // Extremely high total time investment
    local_playstyle_returner,        // Frequently revisited with low commitment
    local_playstyle_on_off,          // Inconsistent play pattern
    local_playstyle_weekend_warrior, // Infrequent but very long sessions
    local_playstyle_comfort,         // Regularly revisited with steady enjoyment
    local_playstyle_regular          // Balanced and consistent play sessions
} local_playstyle_t;

typedef enum {
    global_playstyle_unknown,       // Something could not be calculated
    global_playstyle_casual,        // Low time and low engagement overall
    global_playstyle_core_gamer,    // Regular and consistent engagement
    global_playstyle_explorer,      // Multiple titles with light commitment
    global_playstyle_binger,        // Long average sessions
    global_playstyle_completionist, // Deep focus on few titles
    global_playstyle_power_user,    // Heavy usage across many systems
    global_playstyle_collector,     // Multiple titles with short sessions
    global_playstyle_specialist,    // Focused on few cores and emulators
    global_playstyle_nomad,         // Plays across many multiple devices
    global_playstyle_routine,       // Consistent play pattern
    global_playstyle_habitual,      // Frequent launches with steady timing
    global_playstyle_window         // Dips into many titles briefly
} global_playstyle_t;

typedef struct {
    char path[PATH_MAX];

    char name[256];
    char file_name[256];
    char dir[512];

    char core[64];
    int core_count;

    size_t launch_count;

    char device[64];
    int device_count;

    char mode[16];
    int mode_count;

    int last_played;

    size_t average_time;
    size_t total_time;
    size_t last_session;
} activity_item_t;

typedef struct {
    char core[64];
    int core_count;

    char device[64];
    int device_count;

    char mode[16];
    int mode_count;

    size_t total_launches;
    size_t total_time;
    size_t average_time;

    char top_time[256];
    size_t top_time_value;

    char top_launch[256];
    size_t top_launch_value;

    char oldest_content[256];
    int oldest_content_time;

    size_t unique_titles;

    int unique_cores;
    int unique_devices;
    int unique_modes;

    char longest_session[256];
    size_t longest_session_duration;

    int active_hour;   // 0–23, -1 if unknown
    int favourite_day; // 0–6 (Sun–Sat), -1 if unknown

    global_playstyle_t global_playstyle;
} global_stats_t;

// 0 = Time Played
// 1 = Launch Count
static int activity_display_mode = 0;

static int in_detail_view = 0;
static int in_global_view = 0;

static int overview_item_index = 0;
static int last_sort_mode = -1;

static activity_item_t *activity_items = NULL;
static size_t activity_count = 0;
static size_t activity_capacity = 0;

static char *playtime_json_str = NULL;
static struct json playtime_json_root = {0};
static int playtime_json_loaded = 0;

#define TIME_BUF 64
#define ACT_ROW  1024

#define CORE_MAP_MAX   256
#define DEVICE_MAP_MAX 256
#define MODE_MAP_MAX   256

static void show_help(void) {
    show_info_box(lang.muxactivity.title, lang.muxactivity.help, 0);
}

static void image_refresh() {
    if (in_detail_view || in_global_view || config.visual.box_art == 8) return;

    char full_path[MAX_BUFFER_SIZE];
    snprintf(
        full_path, sizeof(full_path), "%s%s", activity_items[current_item_index].dir,
        activity_items[current_item_index].file_name
    );

    char *item_dir = get_content_path(full_path);

    char item_file_name[MAX_BUFFER_SIZE];
    snprintf(item_file_name, sizeof(item_file_name), "%s", activity_items[current_item_index].file_name);

    char h_core_artwork[MAX_BUFFER_SIZE];
    get_catalogue_name(item_dir, item_file_name, h_core_artwork, sizeof(h_core_artwork));

    char h_file_name[MAX_BUFFER_SIZE];
    snprintf(h_file_name, sizeof(h_file_name), "%s", item_file_name);

    char *dot = strrchr(h_file_name, '.');
    if (dot) *dot = '\0';

    render_image_refresh(
        "box", h_core_artwork, h_file_name, ui_img_splash, ui_viewport_objects, &starter_image, &splash_valid
    );
}

static void video_refresh(void) {
    if (in_detail_view || in_global_view || !ui_count_static) return;

    char full_path[MAX_BUFFER_SIZE];
    snprintf(
        full_path, sizeof(full_path), "%s%s", activity_items[current_item_index].dir,
        activity_items[current_item_index].file_name
    );

    char *item_dir = get_content_path(full_path);

    char item_file_name[MAX_BUFFER_SIZE];
    snprintf(item_file_name, sizeof(item_file_name), "%s", activity_items[current_item_index].file_name);

    char h_core_artwork[MAX_BUFFER_SIZE];
    get_catalogue_name(item_dir, item_file_name, h_core_artwork, sizeof(h_core_artwork));

    char h_file_name[MAX_BUFFER_SIZE];
    snprintf(h_file_name, sizeof(h_file_name), "%s", item_file_name);

    char *dot = strrchr(h_file_name, '.');
    if (dot) *dot = '\0';

    render_video_refresh(h_core_artwork, h_file_name);
}

static void image_refresh_transition(void) {
    image_refresh();
    transition_box_art_apply_in(config.visual.box_art_transition);
    if (config.visual.video_preview > 0) video_refresh();
}

static size_t json_size_positive(const struct json j) {
    if (!json_exists(j)) return 0;

    const int v = json_int(j);
    return v > 0 ? (size_t) v : 0;
}

static int json_epoch_or_zero(const struct json j) {
    if (!json_exists(j)) return 0;

    const int v = json_int(j);
    return v > 0 ? v : 0;
}

static int ensure_activity_capacity(void) {
    if (activity_count < activity_capacity) return 1;

    const size_t new_capacity = activity_capacity ? activity_capacity * 2 : 256;
    activity_item_t *items = realloc(activity_items, new_capacity * sizeof(*items));

    if (!items) {
        LOG_ERROR(mux_module, "Activity list memory overflow!");
        return 0;
    }

    activity_items = items;
    activity_capacity = new_capacity;

    return 1;
}

static void free_activity_items(void) {
    free(activity_items);
    activity_items = NULL;
    activity_count = 0;
    activity_capacity = 0;

    free(playtime_json_str);

    playtime_json_str = NULL;
    playtime_json_root = (struct json) {0};
    playtime_json_loaded = 0;
}

static void hour_label(char *dst, const size_t dst_sz, const int hour) {
    if (hour < 0 || hour > 23) {
        snprintf(dst, dst_sz, "%s", lang.generic.unknown);
        return;
    }

    int h = hour % 12;
    if (h == 0) h = 12;

    snprintf(dst, dst_sz, "%d %s", h, hour < 12 ? "AM" : "PM");
}

static void weekday_label(char *dst, const size_t dst_sz, const int day) {
    static const char *days[] = {lang.generic.sunday,    lang.generic.monday,   lang.generic.tuesday,
                                 lang.generic.wednesday, lang.generic.thursday, lang.generic.friday,
                                 lang.generic.saturday};

    if (day < 0 || day > 6) {
        snprintf(dst, dst_sz, "%s", lang.generic.unknown);
        return;
    }

    snprintf(dst, dst_sz, "%s", days[day]);
}

static const char *local_playstyle_name(const local_playstyle_t ps, const int use_en) {
    switch (ps) {
        case local_playstyle_one_and_done:
            return use_en ? "One and Done" : lang.muxactivity.style.local.one;
        case local_playstyle_sampler:
            return use_en ? "Sampler" : lang.muxactivity.style.local.sampler;
        case local_playstyle_short_bursts:
            return use_en ? "Short Bursts" : lang.muxactivity.style.local.burst;
        case local_playstyle_long_sessions:
            return use_en ? "Long Sessions" : lang.muxactivity.style.local.lng;
        case local_playstyle_completionist:
            return use_en ? "Completionist" : lang.muxactivity.style.local.completionist;
        case local_playstyle_abandoned:
            return use_en ? "Abandoned" : lang.muxactivity.style.local.abandoned;
        case local_playstyle_marathoner:
            return use_en ? "Marathoner" : lang.muxactivity.style.local.marathoner;
        case local_playstyle_returner:
            return use_en ? "Returner" : lang.muxactivity.style.local.returner;
        case local_playstyle_on_off:
            return use_en ? "On and Off" : lang.muxactivity.style.local.on_off;
        case local_playstyle_weekend_warrior:
            return use_en ? "Weekend Warrior" : lang.muxactivity.style.local.weekend;
        case local_playstyle_comfort:
            return use_en ? "Comfort Game" : lang.muxactivity.style.local.comfort;
        case local_playstyle_regular:
            return use_en ? "Regular Play" : lang.muxactivity.style.local.regular;
        default:
            return use_en ? "Unique" : lang.muxactivity.unique;
    }
}

static const char *global_playstyle_name(const global_playstyle_t ps, const int use_en) {
    switch (ps) {
        case global_playstyle_casual:
            return use_en ? "Casual" : lang.muxactivity.style.global.casual;
        case global_playstyle_core_gamer:
            return use_en ? "Core Gamer" : lang.muxactivity.style.global.core;
        case global_playstyle_explorer:
            return use_en ? "Explorer" : lang.muxactivity.style.global.explorer;
        case global_playstyle_binger:
            return use_en ? "Binger" : lang.muxactivity.style.global.binger;
        case global_playstyle_completionist:
            return use_en ? "Completionist" : lang.muxactivity.style.global.completionist;
        case global_playstyle_power_user:
            return use_en ? "Power Player" : lang.muxactivity.style.global.power;
        case global_playstyle_collector:
            return use_en ? "Content Collector" : lang.muxactivity.style.global.collector;
        case global_playstyle_specialist:
            return use_en ? "Specialist" : lang.muxactivity.style.global.specialist;
        case global_playstyle_nomad:
            return use_en ? "Device Nomad" : lang.muxactivity.style.global.nomad;
        case global_playstyle_routine:
            return use_en ? "Routine Player" : lang.muxactivity.style.global.routine;
        case global_playstyle_habitual:
            return use_en ? "Habitual Player" : lang.muxactivity.style.global.habitual;
        case global_playstyle_window:
            return use_en ? "Window Shopper" : lang.muxactivity.style.global.window;
        default:
            return use_en ? "Unique" : lang.muxactivity.unique;
    }
}

// Static calculated values - much nicer on the brain to calculate!
#define SEC  ((size_t) 1)
#define MIN  (60 * SEC)
#define HOUR (60 * MIN)

#define SEC_15_M (15 * MIN)
#define SEC_20_M (20 * MIN)
#define SEC_30_M (30 * MIN)
#define SEC_45_M (45 * MIN)
#define SEC_90_M (90 * MIN)

#define SEC_1_H   (1 * HOUR)
#define SEC_2_H   (2 * HOUR)
#define SEC_3_H   (3 * HOUR)
#define SEC_8_H   (8 * HOUR)
#define SEC_10_H  (10 * HOUR)
#define SEC_15_H  (15 * HOUR)
#define SEC_20_H  (20 * HOUR)
#define SEC_50_H  (50 * HOUR)
#define SEC_80_H  (80 * HOUR)
#define SEC_100_H (100 * HOUR)

// Compile-time sanity checks!
// https://www.gnu.org/software/c-intro-and-ref/manual/html_node/Static-Assertions.html
_Static_assert(SEC_1_H == 3600, "1 hour must be exactly 3600 seconds");
_Static_assert(SEC_30_M < SEC_1_H, "Minute and hour ordering broken");
_Static_assert(SEC_2_H > SEC_1_H, "Hour scale 2H to 1H broken");
_Static_assert(SEC_100_H > SEC_50_H, "High end hour thresholds broken");
_Static_assert(
    SEC_15_M < SEC_20_M && SEC_20_M < SEC_30_M && SEC_30_M < SEC_45_M && SEC_45_M < SEC_90_M,
    "Minute thresholds must be increasing"
);
_Static_assert(
    SEC_1_H < SEC_2_H && SEC_2_H < SEC_3_H && SEC_3_H < SEC_8_H && SEC_8_H < SEC_10_H && SEC_10_H < SEC_15_H
        && SEC_15_H < SEC_20_H && SEC_20_H < SEC_50_H && SEC_50_H < SEC_80_H && SEC_80_H < SEC_100_H,
    "Hour thresholds must be increasing"
);

static local_playstyle_t resolve_local_playstyle(const size_t launches, const size_t total_time) {
    if (launches <= 0 || total_time <= 0) return local_playstyle_unknown;

    const size_t avg = total_time / launches;

    if (launches == 1 && total_time < SEC_2_H) return local_playstyle_one_and_done;

    if (launches <= 2 && total_time < SEC_30_M) return local_playstyle_abandoned;

    if (launches >= 3 && total_time < SEC_1_H) return local_playstyle_sampler;

    if (total_time >= SEC_100_H && launches >= 10) return local_playstyle_marathoner;

    if (total_time >= SEC_20_H && launches >= 5 && avg >= SEC_2_H) return local_playstyle_completionist;

    if (launches <= 6 && avg >= SEC_3_H && total_time >= SEC_8_H) return local_playstyle_weekend_warrior;

    if (launches >= 10 && avg < SEC_15_M && total_time >= SEC_2_H) return local_playstyle_short_bursts;

    if (launches >= 15 && avg < SEC_45_M && total_time < SEC_10_H) return local_playstyle_returner;

    if (launches >= 3 && launches <= 8 && avg < SEC_30_M) return local_playstyle_on_off;

    if (launches >= 15 && avg >= SEC_20_M && avg <= SEC_90_M && total_time >= SEC_15_H && total_time <= SEC_80_H)
        return local_playstyle_comfort;

    if (launches >= 5 && avg >= SEC_30_M && avg <= SEC_2_H) return local_playstyle_regular;

    if (launches <= 5 && avg >= SEC_2_H) return local_playstyle_long_sessions;

    return local_playstyle_unknown;
}

static global_playstyle_t resolve_global_playstyle(const global_stats_t *gs) {
    if (gs->total_time <= 0) return global_playstyle_unknown;

    const size_t avg = gs->average_time;

    if (gs->unique_devices >= 3) return global_playstyle_nomad;

    if (gs->total_launches >= 200 && gs->unique_cores >= 10) return global_playstyle_power_user;

    if (gs->unique_cores <= 2 && gs->total_time >= SEC_80_H) return global_playstyle_specialist;

    if (gs->unique_titles <= 5 && gs->total_time >= SEC_100_H && avg >= SEC_2_H) return global_playstyle_completionist;

    if (avg >= SEC_2_H) return global_playstyle_binger;

    if (gs->unique_titles >= 45 && avg < SEC_20_M) return global_playstyle_collector;

    if (gs->unique_titles >= 20 && avg < SEC_30_M && gs->total_time < SEC_50_H) return global_playstyle_window;

    if (gs->unique_titles >= 10 && gs->unique_titles < 45 && avg < SEC_45_M && gs->total_time < SEC_80_H)
        return global_playstyle_explorer;

    if (gs->total_launches >= 75 && avg >= SEC_20_M && avg <= SEC_90_M && gs->unique_titles >= 5
        && gs->unique_titles <= 15)
        return global_playstyle_routine;

    if (gs->total_launches >= 100 && avg >= SEC_30_M) return global_playstyle_core_gamer;

    if (gs->total_launches >= 50 && avg >= SEC_30_M) return global_playstyle_habitual;

    if (gs->total_time < SEC_10_H && gs->total_launches < 100) return global_playstyle_casual;

    return global_playstyle_unknown;
}

static int cmp_activity_time(const void *a, const void *b) {
    const activity_item_t *x = a;
    const activity_item_t *y = b;

    if (y->total_time < x->total_time) return -1;
    if (y->total_time > x->total_time) return 1;

    return 0;
}

static int cmp_activity_launch(const void *a, const void *b) {
    const activity_item_t *x = a;
    const activity_item_t *y = b;

    if (y->launch_count < x->launch_count) return -1;
    if (y->launch_count > x->launch_count) return 1;

    return 0;
}

static void export_timestamp(char *dst) {
    const time_t now = time(NULL);
    struct tm tm_buf;
    struct tm *tm = localtime_r(&now, &tm_buf);

    if (!tm) {
        snprintf(dst, TIME_BUF, "%s", lang.generic.unknown);
        return;
    }

    strftime(dst, TIME_BUF, TIME_STRING, tm);
}

static void format_timestamp(char *dst, const size_t dst_sz, const int epoch) {
    if (epoch <= 0) {
        snprintf(dst, dst_sz, "%s", lang.generic.unknown);
        return;
    }

    const time_t t = epoch;
    struct tm tm_buf;
    struct tm *tm = localtime_r(&t, &tm_buf);

    if (!tm) {
        snprintf(dst, dst_sz, "%s", lang.generic.unknown);
        return;
    }

    strftime(dst, dst_sz, TIME_STRING, tm);
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

static int ensure_capacity(char **buf, size_t *cap, const size_t len, const size_t need) {
    if (len + need < *cap) return 1;

    size_t new_cap = *cap ? *cap * 2 : 4096;
    while (len + need >= new_cap)
        new_cap *= 2;

    char *tmp = realloc(*buf, new_cap);
    if (!tmp) return 0;

    *buf = tmp;
    *cap = new_cap;

    return 1;
}

static int delete_activity_entry(const char *target_path) {
    if (!target_path || !*target_path) return 0;

    const struct json root = get_playtime_json();
    if (!json_exists(root)) return 0;

    size_t cap = 4096;
    size_t len = 0;

    char *output = malloc(cap);
    if (!output) return 0;

    output[len++] = '{';

    int first = 1;

    struct json child = json_first(root);
    while (json_exists(child)) {
        const struct json key = child;
        const struct json val = json_next(key);

        if (!json_exists(val)) break;

        char key_buf[PATH_MAX];
        json_string_copy(key, key_buf, sizeof(key_buf));

        if (strcasecmp(key_buf, target_path) != 0) {
            if (!first) {
                if (!ensure_capacity(&output, &cap, len, 1)) {
                    LOG_ERROR(mux_module, "Capacity overflow...");
                    free(output);
                    return 0;
                }
                output[len++] = ',';
            }
            first = 0;

            const int needed = snprintf(NULL, 0, "\"%s\":", key_buf);
            if (needed < 0 || !ensure_capacity(&output, &cap, len, (size_t) needed + 1)) {
                free(output);
                return 0;
            }

            len += snprintf(output + len, cap - len, "\"%s\":", key_buf);

            const char *raw = json_raw(val);
            const size_t raw_len = json_raw_length(val);

            if (!raw || raw_len == 0) {
                LOG_ERROR(mux_module, "Invalid JSON raw value...");
                free(output);
                return 0;
            }

            if (!ensure_capacity(&output, &cap, len, raw_len)) {
                LOG_ERROR(mux_module, "Capacity overflow...");
                free(output);
                return 0;
            }

            memcpy(output + len, raw, raw_len);
            len += raw_len;
        }

        child = json_next(val);
    }

    if (!ensure_capacity(&output, &cap, len, 2)) {
        free(output);
        return 0;
    }

    output[len++] = '}';
    output[len] = '\0';

    char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), "%s/%s", INFO_ACT_PATH, PLAYTIME_DATA);

    write_text_to_file(path, "w", CHAR, output);
    track_delete = 1;

    free(output);
    return 1;
}

static void normalise_json_values(char *dst, const size_t dst_size, const char *src) {
    if (!src || !*src) {
        dst[0] = '\0';
        return;
    }

    while (*src && isspace((unsigned char) *src))
        src++;

    size_t len = strlen(src);
    while (len > 0 && isspace((unsigned char) src[len - 1]))
        len--;

    const size_t n = len < dst_size - 1 ? len : dst_size - 1;
    for (size_t i = 0; i < n; i++)
        dst[i] = (char) tolower((unsigned char) src[i]);

    dst[n] = '\0';
}

static void compute_global_stats(global_stats_t *gs) {
    memset(gs, 0, sizeof(*gs));

    struct {
        char key[64];
        int count;
    } core_map[CORE_MAP_MAX];

    struct {
        char key[64];
        int count;
    } device_map[DEVICE_MAP_MAX];

    struct {
        char key[16];
        int count;
    } mode_map[MODE_MAP_MAX];

    int core_used = 0;
    int device_used = 0;
    int mode_used = 0;

    int core_overflow = 0;
    int device_overflow = 0;
    int mode_overflow = 0;

    size_t hour_buckets[24] = {0};
    size_t day_buckets[7] = {0};

    gs->active_hour = -1;
    gs->favourite_day = -1;

    gs->top_time_value = 0;
    gs->top_launch_value = 0;
    gs->top_time[0] = '\0';
    gs->top_launch[0] = '\0';

    gs->oldest_content_time = INT_MAX;
    gs->oldest_content[0] = '\0';

    gs->longest_session_duration = 0;
    gs->longest_session[0] = '\0';

    // Currently one activity item per unique title in JSON
    gs->unique_titles = activity_count;

    for (size_t i = 0; i < activity_count; i++) {
        activity_item_t *it = &activity_items[i];

        gs->total_launches += it->launch_count;
        gs->total_time += it->total_time;

        if (it->total_time >= gs->top_time_value) {
            gs->top_time_value = it->total_time;
            snprintf(gs->top_time, sizeof(gs->top_time), "%s", it->name);
        }

        if (it->launch_count >= gs->top_launch_value) {
            gs->top_launch_value = it->launch_count;
            snprintf(gs->top_launch, sizeof(gs->top_launch), "%s", it->name);
        }

        if (it->last_played > 0 && it->last_played < gs->oldest_content_time) {
            gs->oldest_content_time = it->last_played;
            snprintf(gs->oldest_content, sizeof(gs->oldest_content), "%s", it->name);
        }

        if (it->last_played > 0) {
            time_t t = it->last_played;
            struct tm tm_buf;
            const struct tm *tm = localtime_r(&t, &tm_buf);

            if (tm) {
                hour_buckets[tm->tm_hour] += it->total_time;
                day_buckets[tm->tm_wday] += it->total_time;
            }
        }

        if (it->last_session >= gs->longest_session_duration) {
            gs->longest_session_duration = it->last_session;
            snprintf(gs->longest_session, sizeof(gs->longest_session), "%s", it->name);
        }

        {
            char norm_core[64];
            normalise_json_values(norm_core, sizeof(norm_core), it->core);

            int found = 0;
            for (int j = 0; j < core_used; j++) {
                if (strcmp(core_map[j].key, norm_core) == 0) {
                    core_map[j].count += it->core_count;
                    found = 1;
                    break;
                }
            }

            if (!found && norm_core[0] != '\0') {
                if (core_used < CORE_MAP_MAX) {
                    snprintf(core_map[core_used].key, sizeof(core_map[core_used].key), "%s", norm_core);
                    core_map[core_used].count = it->core_count;
                    core_used++;
                } else {
                    core_overflow = 1;
                }
            }
        }

        {
            char norm_device[64];
            normalise_json_values(norm_device, sizeof(norm_device), it->device);

            int found = 0;
            for (int j = 0; j < device_used; j++) {
                if (strcmp(device_map[j].key, norm_device) == 0) {
                    device_map[j].count += it->device_count;
                    found = 1;
                    break;
                }
            }

            if (!found && norm_device[0] != '\0') {
                if (device_used < DEVICE_MAP_MAX) {
                    snprintf(device_map[device_used].key, sizeof(device_map[device_used].key), "%s", norm_device);
                    device_map[device_used].count = it->device_count;
                    device_used++;
                } else {
                    device_overflow = 1;
                }
            }
        }

        {
            char norm_mode[16];
            normalise_json_values(norm_mode, sizeof(norm_mode), it->mode);

            int found = 0;
            for (int j = 0; j < mode_used; j++) {
                if (strcmp(mode_map[j].key, norm_mode) == 0) {
                    mode_map[j].count += it->mode_count;
                    found = 1;
                    break;
                }
            }

            if (!found && norm_mode[0] != '\0') {
                if (mode_used < MODE_MAP_MAX) {
                    snprintf(mode_map[mode_used].key, sizeof(mode_map[mode_used].key), "%s", norm_mode);
                    mode_map[mode_used].count = it->mode_count;
                    mode_used++;
                } else {
                    mode_overflow = 1;
                }
            }
        }
    }

    {
        int max = -1;
        for (int i = 0; i < core_used; i++) {
            if (core_map[i].count > max) {
                max = core_map[i].count;
                snprintf(gs->core, sizeof(gs->core), "%s", core_map[i].key);
                gs->core_count = core_map[i].count;
            }
        }
    }

    {
        int max = -1;
        for (int i = 0; i < device_used; i++) {
            if (device_map[i].count > max) {
                max = device_map[i].count;
                snprintf(gs->device, sizeof(gs->device), "%s", device_map[i].key);
                gs->device_count = device_map[i].count;
            }
        }
    }

    {
        int max = -1;
        for (int i = 0; i < mode_used; i++) {
            if (mode_map[i].count > max) {
                max = mode_map[i].count;
                snprintf(gs->mode, sizeof(gs->mode), "%s", mode_map[i].key);
                gs->mode_count = mode_map[i].count;
            }
        }
    }

    {
        size_t max = 0;
        int best = -1;
        for (int i = 0; i < 24; i++) {
            if (hour_buckets[i] > max) {
                max = hour_buckets[i];
                best = i;
            }
        }
        gs->active_hour = best;
    }

    {
        size_t max = 0;
        int best = -1;
        for (int i = 0; i < 7; i++) {
            if (day_buckets[i] > max) {
                max = day_buckets[i];
                best = i;
            }
        }
        gs->favourite_day = best;
    }

    gs->unique_cores = core_used + (core_overflow ? 1 : 0);
    gs->unique_devices = device_used + (device_overflow ? 1 : 0);
    gs->unique_modes = mode_used + (mode_overflow ? 1 : 0);

    gs->average_time = gs->total_launches > 0 ? gs->total_time / gs->total_launches : 0;

    gs->global_playstyle = resolve_global_playstyle(gs);
}

static void load_activity_items(void) {
    const struct json playtime_json = get_playtime_json();
    if (!json_exists(playtime_json)) return;

    activity_count = 0;

    struct json child = json_first(playtime_json);
    while (json_exists(child)) {
        const struct json key = child;
        const struct json val = json_next(key);

        if (!json_exists(val)) break;

        if (json_type(val) == JSON_OBJECT) {
            const struct json name_json = json_object_get(val, "name");
            const struct json time_json = json_object_get(val, "total_time");
            const struct json launches_json = json_object_get(val, "launches");

            if (json_exists(name_json) && json_exists(time_json) && json_exists(launches_json)) {
                if (!ensure_activity_capacity()) break;
                if (activity_count >= activity_capacity) break;

                activity_item_t *it = &activity_items[activity_count];
                memset(it, 0, sizeof(*it));

                json_string_copy(key, it->path, sizeof(it->path));

                char full_path[512];
                snprintf(full_path, sizeof(full_path), "%s", it->path);

                char *item_file_name = get_file_name(strdup(full_path));
                snprintf(it->file_name, sizeof(it->file_name), "%s", item_file_name);

                it->dir[0] = '\0';

                const char *last_slash = strrchr(full_path, '/');
                if (last_slash) {
                    size_t n = (size_t) (last_slash - full_path + 1);
                    if (n >= sizeof(it->dir)) n = sizeof(it->dir) - 1;

                    memcpy(it->dir, full_path, n);
                    it->dir[n] = '\0';
                }

                char raw_name[MAX_BUFFER_SIZE];
                json_string_copy(name_json, raw_name, sizeof(raw_name));
                resolve_friendly_name(full_path, it->name);
                adjust_visual_label(it->name, config.visual.name, config.visual.dash);

                it->total_time = json_size_positive(time_json);
                it->launch_count = json_size_positive(launches_json);

                const struct json last_core_json = json_object_get(val, "last_core");
                const struct json core_launch_json = json_object_get(val, "core_launches");

                if (json_exists(last_core_json)) {
                    json_string_copy(last_core_json, it->core, sizeof(it->core));
                } else {
                    it->core[0] = '\0';
                }

                it->core_count = 0;
                if (json_exists(core_launch_json) && json_exists(last_core_json)) {
                    char core_key[64];
                    json_string_copy(last_core_json, core_key, sizeof(core_key));

                    const struct json core_count_json = json_object_get(core_launch_json, core_key);
                    it->core_count = (int) json_size_positive(core_count_json);
                }

                const struct json last_device_json = json_object_get(val, "last_device");
                const struct json device_launch_json = json_object_get(val, "device_launches");

                if (json_exists(last_device_json)) {
                    json_string_copy(last_device_json, it->device, sizeof(it->device));
                } else {
                    it->device[0] = '\0';
                }

                it->device_count = 0;
                if (json_exists(device_launch_json) && json_exists(last_device_json)) {
                    char dev_key[64];
                    json_string_copy(last_device_json, dev_key, sizeof(dev_key));

                    const struct json dev_count_json = json_object_get(device_launch_json, dev_key);
                    it->device_count = (int) json_size_positive(dev_count_json);
                }

                const struct json last_mode_json = json_object_get(val, "last_mode");
                const struct json mode_launch_json = json_object_get(val, "mode_launches");

                if (json_exists(last_mode_json)) {
                    json_string_copy(last_mode_json, it->mode, sizeof(it->mode));
                } else {
                    it->mode[0] = '\0';
                }

                it->mode_count = 0;
                if (json_exists(mode_launch_json) && json_exists(last_mode_json)) {
                    char mode_key[16];
                    json_string_copy(last_mode_json, mode_key, sizeof(mode_key));

                    const struct json mode_count_json = json_object_get(mode_launch_json, mode_key);
                    it->mode_count = (int) json_size_positive(mode_count_json);
                }

                it->last_played = json_epoch_or_zero(json_object_get(val, "start_time"));
                it->average_time = json_size_positive(json_object_get(val, "avg_time"));
                it->last_session = json_size_positive(json_object_get(val, "last_session"));

                activity_count++;
            }
        }

        child = json_next(val);
    }
}

static void format_total_time(char *dst, const size_t dst_sz, const size_t total_time) {
    const size_t days = total_time / 86400;
    const size_t hours = total_time % 86400 / 3600;
    const size_t minutes = total_time % 3600 / 60;

    if (days) {
        snprintf(dst, dst_sz, "%zud %zuh %zum", days, hours, minutes);
    } else if (hours) {
        snprintf(dst, dst_sz, "%zuh %zum", hours, minutes);
    } else if (minutes) {
        snprintf(dst, dst_sz, "%zum", minutes);
    } else {
        snprintf(dst, dst_sz, "0m");
    }
}

static void format_activity_row(const activity_item_t *it, const int mode, char *dst) {
    if (mode == 0) {
        char tt[64];
        format_total_time(tt, sizeof(tt), it->total_time);
        snprintf(dst, ACT_ROW, "[%s] %s", tt, it->name);
    } else {
        snprintf(dst, ACT_ROW, "[%zu] %s", it->launch_count, it->name);
    }
}

static void refresh_activity_labels(void) {
    snprintf(mux_module, sizeof(mux_module), "%s", "muxactivity");
    init_theme(1, 0);

    lv_group_remove_all_objs(ui_group);
    lv_group_remove_all_objs(ui_group_value);
    lv_group_remove_all_objs(ui_group_glyph);
    lv_group_remove_all_objs(ui_group_panel);

    char current_activity_mode[MAX_BUFFER_SIZE];
    snprintf(
        current_activity_mode, sizeof(current_activity_mode), "%s",
        activity_display_mode ? lang.muxactivity.time : lang.muxactivity.launch
    );

    lv_label_set_text(ui_lbl_nav_y, current_activity_mode);

    lv_obj_clean(ui_pnl_content);
    ui_count_static = 0;

    if (activity_display_mode != last_sort_mode) {
        if (activity_display_mode == 0) {
            qsort(activity_items, activity_count, sizeof(activity_items[0]), cmp_activity_time);
        } else {
            qsort(activity_items, activity_count, sizeof(activity_items[0]), cmp_activity_launch);
        }

        last_sort_mode = activity_display_mode;
    }

    ui_count_static = (int) activity_count;

    const size_t limit = theme.mux.item.count;
    for (size_t i = 0; i < activity_count && i < limit; ++i) {
        char label_buffer[MAX_BUFFER_SIZE];
        format_activity_row(&activity_items[i], activity_display_mode, label_buffer);

        gen_label(mux_module, "rom", label_buffer);
    }

    lv_obj_update_layout(ui_pnl_content);
    current_item_index = 0;

    if (ui_count_static == 0) {
        lv_label_set_text(ui_lbl_screen_message, lang.muxactivity.none);

        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    } else {
        lv_label_set_text(ui_lbl_screen_message, "");
    }
}

static void show_detail_view(const activity_item_t *it) {
    lv_obj_clean(ui_pnl_content);

    snprintf(mux_module, sizeof(mux_module), "%s", "muxactdetail");
    init_theme(1, 0);

    ui_count_static = 0;
    current_item_index = 0;
    in_detail_view = 1;

    char detail_label[MAX_BUFFER_SIZE];
    char detail_value[MAX_BUFFER_SIZE];
    char detail_glyph[MAX_BUFFER_SIZE];

    enum detail_field {
        detail_name,
        detail_core,
        detail_launch,
        detail_device,
        detail_mode,
        detail_start,
        detail_average,
        detail_total,
        detail_last,
        detail_playstyle,
        detail_count
    };

    for (int i = 0; i < detail_count; ++i) {
        detail_label[0] = '\0';
        detail_value[0] = '\0';
        detail_glyph[0] = '\0';

        switch (i) {
            case detail_name:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.muxactivity.detail.name);
                snprintf(detail_value, sizeof(detail_value), "%s", it->name);
                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_name");
                break;
            case detail_core:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.muxactivity.detail.core);

                char core_tmp[64];
                snprintf(core_tmp, sizeof(core_tmp), "%s", it->core);
                snprintf(detail_value, sizeof(detail_value), "%s", format_core_name(core_tmp, 0, 0));

                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_core");
                break;
            case detail_launch:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.muxactivity.detail.launch);
                snprintf(detail_value, sizeof(detail_value), "%zu", it->launch_count);
                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_launch");
                break;
            case detail_device:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.muxactivity.detail.device);

                char device_tmp[64];
                snprintf(device_tmp, sizeof(device_tmp), "%s", it->device);
                snprintf(detail_value, sizeof(detail_value), "%s", str_toupper(device_tmp));

                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_device");
                break;
            case detail_mode:
                if (!device.board.has_hdmi) continue;

                snprintf(detail_label, sizeof(detail_label), "%s", lang.muxactivity.detail.mode);

                char mode_tmp[16];
                snprintf(mode_tmp, sizeof(mode_tmp), "%s", it->mode);
                snprintf(detail_value, sizeof(detail_value), "%s", str_capital(mode_tmp));

                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_mode");
                break;
            case detail_start:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.muxactivity.detail.played);
                format_timestamp(detail_value, sizeof(detail_value), it->last_played);
                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_start");
                break;
            case detail_average:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.muxactivity.detail.average);
                format_total_time(detail_value, sizeof(detail_value), it->average_time);
                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_average");
                break;
            case detail_total:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.muxactivity.detail.total);
                format_total_time(detail_value, sizeof(detail_value), it->total_time);
                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_total");
                break;
            case detail_last:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.muxactivity.detail.last);
                format_total_time(detail_value, sizeof(detail_value), it->last_session);
                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_last");
                break;
            case detail_playstyle:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.muxactivity.style.local.label);

                const local_playstyle_t ps = resolve_local_playstyle(it->launch_count, it->total_time);
                snprintf(detail_value, sizeof(detail_value), "%s", local_playstyle_name(ps, 0));

                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_play");
                break;
            default:
                continue;
        }

        ui_count_static++;

        lv_obj_t *ui_pnl_act = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_act);

        lv_obj_t *ui_lbl_act_item = lv_label_create(ui_pnl_act);
        apply_theme_list_item(&theme, ui_lbl_act_item, detail_label);

        lv_obj_t *ui_lbl_act_item_value = lv_label_create(ui_pnl_act);
        apply_theme_list_value(&theme, ui_lbl_act_item_value, detail_value);

        lv_obj_t *ui_lbl_act_item_glyph = lv_img_create(ui_pnl_act);
        apply_theme_list_glyph(&theme, ui_lbl_act_item_glyph, "muxactivity", detail_glyph);

        lv_group_add_obj(ui_group, ui_lbl_act_item);
        lv_group_add_obj(ui_group_value, ui_lbl_act_item_value);
        lv_group_add_obj(ui_group_glyph, ui_lbl_act_item_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_act);

        adjust_label_value_width(ui_pnl_act, ui_lbl_act_item, ui_lbl_act_item_value);
        apply_text_long_dot(&theme, ui_lbl_act_item_value);

        apply_text_long_dot(&theme, ui_lbl_act_item);
    }

    lv_obj_update_layout(ui_pnl_content);
    update_label_scroll();
}

static void show_global_view(void) {
    lv_obj_clean(ui_pnl_content);

    snprintf(mux_module, sizeof(mux_module), "%s", "muxactglobal");
    init_theme(1, 0);

    ui_count_static = 0;
    current_item_index = 0;
    in_global_view = 1;

    global_stats_t gs;
    compute_global_stats(&gs);

    char global_label[MAX_BUFFER_SIZE];
    char global_value[MAX_BUFFER_SIZE];
    char global_glyph[MAX_BUFFER_SIZE];

    enum global_field {
        global_top_time,
        global_top_launch,
        global_core,
        global_device,
        global_mode,
        global_launches,
        global_total_time,
        global_average_time,
        global_first_game,
        global_longest_session,
        global_playstyle,
        global_unique_titles,
        global_unique_cores,
        global_active_time,
        global_favourite_day,
        global_count
    };

    for (int i = 0; i < global_count; i++) {
        global_label[0] = '\0';
        global_value[0] = '\0';
        global_glyph[0] = '\0';

        switch (i) {
            case global_top_time:
                snprintf(global_label, sizeof(global_label), "%s", lang.muxactivity.global.top_time);
                snprintf(global_value, sizeof(global_value), "%s", gs.top_time);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_toptime");
                break;
            case global_top_launch:
                snprintf(global_label, sizeof(global_label), "%s", lang.muxactivity.global.top_launch);
                snprintf(global_value, sizeof(global_value), "%s", gs.top_launch);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_toplaunch");
                break;
            case global_core:
                snprintf(global_label, sizeof(global_label), "%s", lang.muxactivity.global.core);

                char core_tmp[64];
                snprintf(core_tmp, sizeof(core_tmp), "%s", gs.core);
                snprintf(global_value, sizeof(global_value), "%s", format_core_name(core_tmp, 0, 0));

                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_core");
                break;
            case global_device:
                snprintf(global_label, sizeof(global_label), "%s", lang.muxactivity.global.device);

                char device_tmp[64];
                snprintf(device_tmp, sizeof(device_tmp), "%s", gs.device);
                snprintf(global_value, sizeof(global_value), "%s", str_toupper(device_tmp));

                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_device");
                break;
            case global_mode:
                snprintf(global_label, sizeof(global_label), "%s", lang.muxactivity.global.mode);

                char mode_tmp[16];
                snprintf(mode_tmp, sizeof(mode_tmp), "%s", gs.mode);
                snprintf(global_value, sizeof(global_value), "%s", str_capital(mode_tmp));

                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_mode");
                break;
            case global_launches:
                snprintf(global_label, sizeof(global_label), "%s", lang.muxactivity.global.launch);
                snprintf(global_value, sizeof(global_value), "%zu", gs.total_launches);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_launch");
                break;
            case global_total_time:
                snprintf(global_label, sizeof(global_label), "%s", lang.muxactivity.global.total);
                format_total_time(global_value, sizeof(global_value), gs.total_time);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_total");
                break;
            case global_average_time:
                snprintf(global_label, sizeof(global_label), "%s", lang.muxactivity.global.average);
                format_total_time(global_value, sizeof(global_value), gs.average_time);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_average");
                break;
            case global_first_game:
                snprintf(global_label, sizeof(global_label), "%s", lang.muxactivity.global.oldest);
                snprintf(global_value, sizeof(global_value), "%s", gs.oldest_content);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_first");
                break;
            case global_longest_session:
                snprintf(global_label, sizeof(global_label), "%s", lang.muxactivity.global.longest);
                snprintf(global_value, sizeof(global_value), "%s", gs.longest_session);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_longest");
                break;
            case global_playstyle:
                snprintf(global_label, sizeof(global_label), "%s", lang.muxactivity.global.overall);
                snprintf(global_value, sizeof(global_value), "%s", global_playstyle_name(gs.global_playstyle, 0));
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_play");
                break;
            case global_unique_titles:
                snprintf(global_label, sizeof(global_label), "%s", lang.muxactivity.global.unique_play);
                snprintf(global_value, sizeof(global_value), "%zu", gs.unique_titles);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_uniqueplay");
                break;
            case global_unique_cores:
                snprintf(global_label, sizeof(global_label), "%s", lang.muxactivity.global.unique_core);
                snprintf(global_value, sizeof(global_value), "%d", gs.unique_cores);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_uniquecore");
                break;
            case global_active_time:
                snprintf(global_label, sizeof(global_label), "%s", lang.muxactivity.global.active_time);
                hour_label(global_value, sizeof(global_value), gs.active_hour);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_active");
                break;
            case global_favourite_day:
                snprintf(global_label, sizeof(global_label), "%s", lang.muxactivity.global.favourite_day);
                weekday_label(global_value, sizeof(global_value), gs.favourite_day);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_day");
                break;
            default:
                continue;
        }

        ui_count_static++;

        lv_obj_t *ui_pnl_act = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_act);

        lv_obj_t *ui_lbl_act_item = lv_label_create(ui_pnl_act);
        apply_theme_list_item(&theme, ui_lbl_act_item, global_label);

        lv_obj_t *ui_lbl_act_item_value = lv_label_create(ui_pnl_act);
        apply_theme_list_value(&theme, ui_lbl_act_item_value, global_value);

        lv_obj_t *ui_lbl_act_item_glyph = lv_img_create(ui_pnl_act);
        apply_theme_list_glyph(&theme, ui_lbl_act_item_glyph, "muxactivity", global_glyph);

        lv_group_add_obj(ui_group, ui_lbl_act_item);
        lv_group_add_obj(ui_group_value, ui_lbl_act_item_value);
        lv_group_add_obj(ui_group_glyph, ui_lbl_act_item_glyph);
        lv_group_add_obj(ui_group_panel, ui_pnl_act);

        adjust_label_value_width(ui_pnl_act, ui_lbl_act_item, ui_lbl_act_item_value);
        apply_text_long_dot(&theme, ui_lbl_act_item_value);

        apply_text_long_dot(&theme, ui_lbl_act_item);
    }

    lv_obj_update_layout(ui_pnl_content);
    update_label_scroll();
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
    char html_export[MAX_BUFFER_SIZE];
    snprintf(html_export, sizeof(html_export), "%s/activity_report.html", device.storage.rom.mount);

    FILE *f = fopen(html_export, "w");
    if (!f) {
        toast_message(lang.muxactivity.export_error, tst_wait_m);
        refresh_screen(ui_screen, 1);
        return;
    }

    global_stats_t gs;
    compute_global_stats(&gs);

    fprintf(
        f, "<!DOCTYPE html>"
           "<html>"
           "<head>"
           "<meta http-equiv='Content-type' content='text/html; charset=utf-8'>"
           "<meta name='viewport' content='width=device-width, initial-scale=1'>"
           "<title>MustardOS - Activity Tracker</title>"

           "<link rel='preconnect' href='https://fonts.bunny.net'>"
           "<link rel='preconnect' href='https://cdn.datatables.net'>"

           "<link rel='stylesheet' href='https://fonts.bunny.net/css?family=noto-sans:400,600,700&display=swap'>"
           "<link rel='stylesheet' href='https://cdn.datatables.net/2.3.5/css/dataTables.dataTables.min.css'>"

           "<script src='https://code.jquery.com/jquery-3.7.1.slim.min.js' "
           "integrity='sha256-kmHvs0B+OpCW5GVHUNjv9rOmY0IvSIRcf7zGUDTDQM8=' crossorigin='anonymous'></script>"
           "<script src='https://cdn.datatables.net/2.3.5/js/dataTables.min.js'></script>"

           "<style>"
           "body { font-family:'Noto Sans', sans-serif; background:#1f1f1f; color:#ffffff; padding:20px }"
           "h1, h2 { color:#f7d12e }"
           "th, td { padding:8px }"
           "th { background:#222222 }"
           "tr:nth-child(even) { background:#1a1a1a }"
           ".dt-container { color: #ffffff }"
           ".dt-search input, .dt-length select { background:#111111; color:#ffffff; border:1px solid #444444 }"
           "table.dataTable tbody tr:nth-child(even) { background-color: #1a1a1a }"
           "table.dataTable tbody tr:nth-child(odd) { background-color: #1f1f1f }"
           "table.dataTable tbody tr:hover { background-color: #2c2c2c }"
           "</style>"

           "</head>"
           "<body>"
    );

    char exported[TIME_BUF];
    export_timestamp(exported);

    fprintf(f, "<h1>MustardOS - Activity Tracker</h1>");
    fprintf(f, "<p><strong>Exported:</strong> ");
    html_escape(f, exported);
    fprintf(f, "</p>");

    char global_total[64];
    char global_avg[64];
    char global_top_time_val[64];

    format_total_time(global_total, sizeof(global_total), gs.total_time);
    format_total_time(global_avg, sizeof(global_avg), gs.average_time);
    format_total_time(global_top_time_val, sizeof(global_top_time_val), gs.top_time_value);

    char active_time[32];
    char fav_day[32];

    hour_label(active_time, sizeof(active_time), gs.active_hour);
    weekday_label(fav_day, sizeof(fav_day), gs.favourite_day);

    char global_core_value[MAX_BUFFER_SIZE];
    char global_device_value[MAX_BUFFER_SIZE];
    char global_mode_value[MAX_BUFFER_SIZE];

    char tmp_core[64];
    char tmp_device[64];
    char tmp_mode[16];

    snprintf(tmp_core, sizeof(tmp_core), "%s", gs.core);
    snprintf(tmp_device, sizeof(tmp_device), "%s", gs.device);
    snprintf(tmp_mode, sizeof(tmp_mode), "%s", gs.mode);

    snprintf(global_core_value, sizeof(global_core_value), "%s", format_core_name(tmp_core, 0, 0));
    snprintf(global_device_value, sizeof(global_device_value), "%s", str_toupper(tmp_device));
    snprintf(global_mode_value, sizeof(global_mode_value), "%s", str_capital(tmp_mode));

    fprintf(f, "<h2>Global Summary</h2>");
    fprintf(f, "<table id='global'><tr><th id='g_metric'>Metric</th><th id='g_value'>Value</th></tr>");

    fprintf(f, "<tr><td>Top Content by Time</td><td>");
    html_escape(f, gs.top_time);
    fprintf(f, " (");
    html_escape(f, global_top_time_val);
    fprintf(f, ")</td></tr>");

    fprintf(f, "<tr><td>Top Content by Launch</td><td>");
    html_escape(f, gs.top_launch);
    fprintf(f, " (%zu)</td></tr>", gs.top_launch_value);

    fprintf(f, "<tr><td>Most Frequent Core</td><td>");
    html_escape(f, global_core_value);
    fprintf(f, "</td></tr>");

    fprintf(f, "<tr><td>Most Used Device</td><td>");
    html_escape(f, global_device_value);
    fprintf(f, "</td></tr>");

    fprintf(f, "<tr><td>Most Used Mode</td><td>");
    html_escape(f, global_mode_value);
    fprintf(f, "</td></tr>");

    fprintf(f, "<tr><td>Total Launch Count</td><td>%zu</td></tr>", gs.total_launches);

    fprintf(f, "<tr><td>Total Play Time</td><td>");
    html_escape(f, global_total);
    fprintf(f, "</td></tr>");

    fprintf(f, "<tr><td>Average Play Time</td><td>");
    html_escape(f, global_avg);
    fprintf(f, "</td></tr>");

    fprintf(f, "<tr><td>Oldest Content</td><td>");
    html_escape(f, gs.oldest_content);
    fprintf(f, "</td></tr>");

    fprintf(f, "<tr><td>Longest Session</td><td>");
    html_escape(f, gs.longest_session);
    fprintf(f, "</td></tr>");

    fprintf(f, "<tr><td>Overall Play Style</td><td>");
    html_escape(f, global_playstyle_name(gs.global_playstyle, 1));
    fprintf(f, "</td></tr>");

    fprintf(f, "<tr><td>Unique Content Played</td><td>%zu</td></tr>", gs.unique_titles);
    fprintf(f, "<tr><td>Unique Cores Used</td><td>%d</td></tr>", gs.unique_cores);

    fprintf(f, "<tr><td>Most Active Time</td><td>");
    html_escape(f, active_time);
    fprintf(f, "</td></tr>");

    fprintf(f, "<tr><td>Favourite Day</td><td>");
    html_escape(f, fav_day);
    fprintf(f, "</td></tr>");

    fprintf(f, "</table>");

    fprintf(
        f, "<h2>Content Statistics</h2>"
           "<table id='detail' class='display' data-page-length='25'>"
           "<thead><tr>"
           "<th data-orderable='1'>Content Name</th>"
           "<th data-orderable='1'>Core Used</th>"
           "<th data-orderable='1'>Last Device</th>"
           "<th data-orderable='1'>Launch Count</th>"
           "<th data-orderable='1'>Start Time</th>"
           "<th data-orderable='1'>Average Time</th>"
           "<th data-orderable='1'>Total Time</th>"
           "<th data-orderable='1'>Last Session</th>"
           "<th data-orderable='1'>Play Style</th>"
           "</tr></thead><tbody>"
    );

    char core_value[MAX_BUFFER_SIZE];
    char device_value[MAX_BUFFER_SIZE];

    char tt_avg[64];
    char tt_total[64];
    char tt_last[64];
    char ts_start[64];

    for (size_t i = 0; i < activity_count; i++) {
        const activity_item_t *it = &activity_items[i];
        const local_playstyle_t ps = resolve_local_playstyle(it->launch_count, it->total_time);

        snprintf(tmp_core, sizeof(tmp_core), "%s", it->core);
        snprintf(core_value, sizeof(core_value), "%s", format_core_name(core_value, 0, 0));

        snprintf(tmp_device, sizeof(tmp_device), "%s", it->device);
        snprintf(device_value, sizeof(device_value), "%s", str_toupper(tmp_device));

        format_timestamp(ts_start, sizeof(ts_start), it->last_played);
        format_total_time(tt_avg, sizeof(tt_avg), it->average_time);
        format_total_time(tt_total, sizeof(tt_total), it->total_time);
        format_total_time(tt_last, sizeof(tt_last), it->last_session);

        fprintf(f, "<tr><td>");
        html_escape(f, it->name);

        fprintf(f, "</td><td>");
        html_escape(f, core_value);

        fprintf(f, "</td><td>");
        html_escape(f, device_value);

        fprintf(f, "</td><td>%zu</td><td>", it->launch_count);
        html_escape(f, ts_start);

        fprintf(f, "</td><td>");
        html_escape(f, tt_avg);

        fprintf(f, "</td><td>");
        html_escape(f, tt_total);

        fprintf(f, "</td><td>");
        html_escape(f, tt_last);

        fprintf(f, "</td><td>");
        html_escape(f, local_playstyle_name(ps, 1));

        fprintf(f, "</td></tr>");
    }

    fprintf(f, "</tbody></table>");

    fprintf(
        f, "<script>"
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
           "</script></body></html>"
    );

    fclose(f);

    toast_message(lang.muxactivity.export_success, tst_wait_m);
    refresh_screen(ui_screen, 1);
}

static void generate_activity_items(void) {
    turbo_time(1, 1);

    last_sort_mode = -1;

    load_activity_items();
    reset_ui_groups();

    in_detail_view = 0;
    refresh_activity_labels();

    first_open = 0;

    turbo_time(0, 1);
}

static void update_activity_list_item(lv_obj_t *ui_lbl_item, lv_obj_t *ui_lbl_item_glyph, const int index) {
    char label_buffer[MAX_BUFFER_SIZE];
    format_activity_row(&activity_items[index], activity_display_mode, label_buffer);
    lv_label_set_text(ui_lbl_item, label_buffer);

    char glyph_image_embed[MAX_BUFFER_SIZE];
    if (config.visual.list_glyph && theme.list_default.glyph_alpha > 0 && theme.list_focus.glyph_alpha > 0) {
        get_glyph_path(mux_module, "rom", glyph_image_embed, MAX_BUFFER_SIZE);
        set_list_glyph_image(ui_lbl_item_glyph, glyph_image_embed);
    }

    apply_size_to_content(&theme, ui_pnl_content, ui_lbl_item, ui_lbl_item_glyph, label_buffer);
    apply_text_long_dot(&theme, ui_lbl_item);
}

static void update_activity_list_items(const int start_index) {
    const int max = (int) activity_count - start_index;
    if (max <= 0) return;

    int count = theme.mux.item.count;
    if (count > max) count = max;

    for (int index = 0; index < count; ++index) {
        const lv_obj_t *panel_item = lv_obj_get_child(ui_pnl_content, index);
        update_activity_list_item(
            lv_obj_get_child(panel_item, 0), lv_obj_get_child(panel_item, 1), start_index + index
        );
    }
}

static void focus_activity_group(const int index) {
    if (index < 0 || index >= theme.mux.item.count) return;
    lv_obj_t *panel = lv_obj_get_child(ui_pnl_content, index);

    if (!panel) return;

    lv_group_focus_obj(panel);
    lv_group_focus_obj(lv_obj_get_child(panel, 0));
    lv_group_focus_obj(lv_obj_get_child(panel, 1));
}

static int focus_activity_list_index(void) {
    const int before = (theme.mux.item.count - theme.mux.item.count % 2) / 2;
    const int after = (theme.mux.item.count - 1) / 2;

    if (current_item_index < before) return current_item_index;
    if (current_item_index >= (int) activity_count - after)
        return theme.mux.item.count - ((int) activity_count - current_item_index);

    return before;
}

static void list_nav_move(const int steps, const int direction) {
    if (!ui_count_static) return;
    first_open ? (first_open = 0) : play_sound(snd_navigate);

    const int overview = !in_detail_view && !in_global_view;
    const int visible_count = theme.mux.item.count;
    const int multi_list = overview && (int) activity_count > visible_count;

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, lv_group_get_focused(ui_group));

        if (lv_group_get_focused(ui_group_value)) {
            apply_text_long_dot(&theme, lv_group_get_focused(ui_group_value));
        }

        if (direction < 0) {
            current_item_index = current_item_index == 0 ? ui_count_static - 1 : current_item_index - 1;
        } else {
            current_item_index = current_item_index == ui_count_static - 1 ? 0 : current_item_index + 1;
        }

        if (multi_list) {
            update_windowed_list(
                ui_pnl_content, direction, current_item_index, (int) activity_count, visible_count,
                update_activity_list_item, update_activity_list_items
            );
            focus_activity_group(focus_activity_list_index());
        } else {
            nav_move(ui_group, direction);
            nav_move(ui_group_glyph, direction);
            nav_move(ui_group_panel, direction);
            nav_move(ui_group_value, direction);
        }
    }

    if (!multi_list) {
        update_scroll_position(
            theme.mux.item.count, theme.mux.item.panel, ui_count_static, current_item_index, ui_pnl_content
        );
    }
    set_label_long_mode(&theme, lv_group_get_focused(ui_group), config.visual.name_scroll);

    update_label_scroll();

    video_preview_cancel();

    if (config.visual.box_art < 4) {
        if (config.visual.box_art_transition != TSN_DISABLED) {
            transition_box_art_nav_activity();
        } else {
            image_refresh();
            if (config.visual.video_preview > 0) video_refresh();
        }
    }

    nav_moved = 1;
}

static void list_nav_prev(const int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(const int steps) {
    list_nav_move(steps, +1);
}

static int remove_mode = 0;
static int skip_confirm = 0;
static mux_dialogue remove_dlg;

static void handle_b(void);

static void show_remove_dialog(void) {
    remove_mode = 1;
    remove_dlg.selected = 0;
    dialogue_show(&remove_dlg);
    dialogue_refresh(&remove_dlg, &theme);
}

static void hide_remove_dialog(void) {
    remove_mode = 0;
    dialogue_hide(&remove_dlg);
}

static void do_remove(void) {
    if (overview_item_index < 0 || (size_t) overview_item_index >= activity_count) return;

    LOG_INFO(mux_module, "Purging Playtime Entry: %s", activity_items[overview_item_index].path);

    if (delete_activity_entry(activity_items[overview_item_index].path)) {
        play_sound(snd_muos);
        free_activity_items();
        load_activity_items();
        last_sort_mode = -1;
        handle_b();
    } else {
        toast_message(lang.generic.remove_fail, tst_wait_s);
        play_sound(snd_error);
    }
}

static void hide_nav(void) {
    lv_obj_add_flag(ui_img_box, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_counter_activity, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);

    lv_label_set_text(ui_lbl_nav_x, lang.generic.remove);
}

static void show_nav(void) {
    lv_obj_clear_flag(ui_img_box, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lbl_counter_activity, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lbl_nav_y_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lbl_nav_y, MU_OBJ_FLAG_HIDE_FLOAT);

    lv_label_set_text(ui_lbl_nav_x, lang.muxactivity.global.nav);
}

static void handle_a(void) {
    if (remove_mode) {
        const mux_remove_opt opt = (mux_remove_opt) remove_dlg.selected;
        hide_remove_dialog();
        if (opt == mux_remove_yep) {
            do_remove();
        } else if (opt == mux_remove_skip) {
            skip_confirm = 1;
            do_remove();
        }
        return;
    }

    if (msgbox_active || !ui_count_static || hold_call || in_global_view || in_detail_view) return;

    play_sound(snd_confirm);
    video_preview_cancel();
    hide_nav();

    overview_item_index = current_item_index;

    const size_t idx = current_item_index;
    if (idx >= activity_count) return;

    show_detail_view(&activity_items[idx]);
    nav_moved = 1;
}

static void handle_b(void) {
    if (remove_mode) {
        hide_remove_dialog();
        return;
    }

    if (hold_call && !track_delete) return;

    if (msgbox_active) {
        handle_msgbox_dismiss();
        return;
    }

    if (video_preview_active()) {
        video_preview_cancel();
        play_sound(snd_back);
        return;
    }

    if (track_delete) {
        toast_message(lang.muxactivity.removed, tst_wait_m);
        track_delete = 0;
    } else {
        play_sound(snd_back);
    }

    if (in_detail_view) {
        show_nav();

        in_detail_view = 0;
        refresh_activity_labels();
        nav_moved = 1;
        first_open = 1;

        list_nav_next(overview_item_index);
        return;
    }

    if (in_global_view) {
        show_nav();
        lv_label_set_text(ui_lbl_nav_x, lang.muxactivity.global.nav);

        in_global_view = 0;
        refresh_activity_labels();
        nav_moved = 1;
        first_open = 1;

        list_nav_next(overview_item_index);
        return;
    }

    video_preview_cancel();
    free_activity_items();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "activity");

    skip_confirm = 0;
    mux_input_stop();
}

static void handle_x(void) {
    if (msgbox_active || !ui_count_static) return;

    if (in_detail_view) {
        if (remove_mode || overview_item_index < 0 || (size_t) overview_item_index >= activity_count) return;

        if (config.settings.advanced.trust_remove || skip_confirm) {
            do_remove();
            return;
        }

        play_sound(snd_confirm);
        show_remove_dialog();
        return;
    }

    play_sound(snd_confirm);

    if (in_global_view) {
        export_activity_html();
        return;
    }

    video_preview_cancel();
    hide_nav();

    lv_obj_clear_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_label_set_text(ui_lbl_nav_x, lang.muxactivity.html);

    overview_item_index = current_item_index;

    show_global_view();
    nav_moved = 1;
}

static void handle_y(void) {
    if (msgbox_active || !ui_count_static || hold_call || in_detail_view || in_global_view) return;

    activity_display_mode = !activity_display_mode;
    play_sound(snd_navigate);

    overview_item_index = current_item_index;

    refresh_activity_labels();

    if (config.visual.box_art < 4) image_refresh();
    nav_moved = 1;
}

static void handle_dpad_up(void) {
    if (remove_mode) {
        if (!swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (remove_mode) {
        if (!swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (remove_mode) return;

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (remove_mode) return;

    handle_list_nav_down_hold();
}

static void handle_help(void) {
    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;

    play_sound(snd_info_open);
    show_help();
}

static void adjust_panels(void) {
    adjust_panel_priority((lv_obj_t *[]) {ui_pnl_footer, ui_pnl_header, ui_lbl_counter_activity, ui_pnl_help,
                                          ui_pnl_progress_brightness, ui_pnl_progress_volume, NULL});
    if (config.visual.box_art == 3) lv_obj_move_foreground(ui_pnl_box);
}

static void init_elements(void) {
    lv_obj_set_align(ui_img_box, config.visual.box_art_align);
    lv_obj_set_align(ui_viewport_objects[0], config.visual.box_art_align);

    adjust_box_art();
    adjust_panels();
    header_and_footer_setup();

    setup_nav((struct nav_bar[]) {{ui_lbl_nav_a_glyph, "", 0},
                                  {ui_lbl_nav_a, lang.muxactivity.info, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_x_glyph, "", 0},
                                  {ui_lbl_nav_x, lang.muxactivity.global.nav, 0},
                                  {ui_lbl_nav_y_glyph, "", 0},
                                  {ui_lbl_nav_y, "", 0},
                                  {NULL, NULL, 0}});

    overlay_display();
}

static void ui_refresh_task(lv_timer_t *timer __attribute__((unused))) {
    if (nav_moved) {
        starter_image = adjust_wallpaper_element(ui_group, starter_image, wall_general);
        adjust_panels();

        if (!in_detail_view && !in_global_view) update_file_counter(ui_lbl_counter_activity, ui_count_static);
        if (overlay_image) lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnl_content);
        nav_moved = 0;
    }
}

int muxactivity_main() {
    starter_image = 0;

    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, "");
    init_muxactivity(ui_screen, &theme);

    ui_viewport_objects[0] = lv_obj_create(ui_pnl_box);
    ui_viewport_objects[1] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[2] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[3] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[4] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[5] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[6] = lv_img_create(ui_viewport_objects[0]);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lbl_datetime, get_datetime());
    lv_label_set_text(ui_lbl_title, lang.muxactivity.title);

    init_fonts();

    load_wallpaper(ui_screen, NULL, ui_img_wall, wall_general);

    generate_activity_items();
    init_elements();

    transition_box_art_init(image_refresh_transition);

    if (ui_count_static == 0) {
        hide_nav();

        lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);

        lv_label_set_text(ui_lbl_screen_message, lang.muxactivity.none);
    } else {
        if (config.visual.box_art < 4) {
            image_refresh();
            if (config.visual.video_preview > 0) video_refresh();
        }
        nav_moved = 1;
    }

    dialogue_init_remove(&remove_dlg, &theme, ui_screen, NULL, lang.generic.select, lang.generic.back);
    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
                [mux_input_y] = handle_y,
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
            [mux_input_dpad_up] = handle_dpad_up_hold,
            [mux_input_dpad_down] = handle_dpad_down_hold,
            [mux_input_l1] = handle_list_nav_page_up,
            [mux_input_r1] = handle_list_nav_page_down,
        }
    };

    list_nav_set_callbacks(list_nav_prev, list_nav_next);
    init_input(&input_opts, 1);
    mux_input_task(&input_opts);

    transition_box_art_destroy();
    video_preview_destroy();

    return 0;
}
