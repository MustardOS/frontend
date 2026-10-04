#include "muxshare.h"
#include <common/ui/orientation.h>
#include <common/ui/empty_state.h>
#include <common/ui/more.h>
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
    char record[16];

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

    int first_played;
    int last_played;

    size_t average_time;
    size_t total_time;
    size_t last_session;
    size_t longest_session;

    size_t hour_time[24];
    size_t day_time[7];
} activity_item_t;

typedef struct {
    char core[64];
    int core_count;
    int core_is_muxretro;

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

    int active_hour;   // 0-23, -1 if unknown
    int favourite_day; // 0-6 (Sun-Sat), -1 if unknown

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

#define ACT_ROW 1024

#define CORE_MAP_MAX   256
#define DEVICE_MAP_MAX 256
#define MODE_MAP_MAX   256

static int activity_item_uses_muxretro(const activity_item_t *it) {
    char file_path[MAX_BUFFER_SIZE];
    snprintf(file_path, sizeof(file_path), "%s", it->path);

    char *sys_dir = get_content_path(file_path);
    const char *file_name = get_file_name(file_path);

    const char *def_core = get_content_line(sys_dir, file_name, "cfg", 6);
    const char *sys = get_content_line(sys_dir, file_name, "cfg", 3);
    if (!*def_core) {
        def_core = get_content_line(sys_dir, NULL, "cfg", 5);
        sys = get_content_line(sys_dir, NULL, "cfg", 2);
    }
    if (!*def_core || !*sys) return 0;

    return core_uses_muxretro(def_core);
}

static void show_help(void) {
    show_info_box(lang.muxactivity.title, lang.muxactivity.help, 0);
}

static void image_refresh(void) {
    if (in_detail_view || in_global_view || config.visual.box_art == 8) return;
    if (!activity_items || current_item_index < 0 || (size_t) current_item_index >= activity_count) return;

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

static const char *local_playstyle_name(const local_playstyle_t ps) {
    switch (ps) {
        case local_playstyle_one_and_done:
            return lang.muxactivity.style.local.one;
        case local_playstyle_sampler:
            return lang.muxactivity.style.local.sampler;
        case local_playstyle_short_bursts:
            return lang.muxactivity.style.local.burst;
        case local_playstyle_long_sessions:
            return lang.muxactivity.style.local.lng;
        case local_playstyle_completionist:
            return lang.muxactivity.style.local.completionist;
        case local_playstyle_abandoned:
            return lang.muxactivity.style.local.abandoned;
        case local_playstyle_marathoner:
            return lang.muxactivity.style.local.marathoner;
        case local_playstyle_returner:
            return lang.muxactivity.style.local.returner;
        case local_playstyle_on_off:
            return lang.muxactivity.style.local.on_off;
        case local_playstyle_weekend_warrior:
            return lang.muxactivity.style.local.weekend;
        case local_playstyle_comfort:
            return lang.muxactivity.style.local.comfort;
        case local_playstyle_regular:
            return lang.muxactivity.style.local.regular;
        default:
            return lang.muxactivity.unique;
    }
}

static const char *global_playstyle_name(const global_playstyle_t ps) {
    switch (ps) {
        case global_playstyle_casual:
            return lang.muxactivity.style.global.casual;
        case global_playstyle_core_gamer:
            return lang.muxactivity.style.global.core;
        case global_playstyle_explorer:
            return lang.muxactivity.style.global.explorer;
        case global_playstyle_binger:
            return lang.muxactivity.style.global.binger;
        case global_playstyle_completionist:
            return lang.muxactivity.style.global.completionist;
        case global_playstyle_power_user:
            return lang.muxactivity.style.global.power;
        case global_playstyle_collector:
            return lang.muxactivity.style.global.collector;
        case global_playstyle_specialist:
            return lang.muxactivity.style.global.specialist;
        case global_playstyle_nomad:
            return lang.muxactivity.style.global.nomad;
        case global_playstyle_routine:
            return lang.muxactivity.style.global.routine;
        case global_playstyle_habitual:
            return lang.muxactivity.style.global.habitual;
        case global_playstyle_window:
            return lang.muxactivity.style.global.window;
        default:
            return lang.muxactivity.unique;
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

static void migrate_legacy_activity(void) {
    if (!file_exist(INFO_ACT_PATH "/" PLAYTIME_DATA)) return;

    LOG_INFO(mux_module, "Migrating legacy activity data");

    const char *args[] = {OPT_PATH "script/mux/track.sh", "migrate", NULL};
    run_exec(args, A_SIZE(args), 0, 1, NULL, NULL);
}

static int is_activity_record(const char *name) {
    if (strlen(name) != 13 || strcmp(name + 8, ".json") != 0) return 0;

    for (int i = 0; i < 8; i++) {
        if (!isxdigit((unsigned char) name[i])) return 0;
    }

    return 1;
}

static int delete_activity_entry(const activity_item_t *it) {
    if (!it || !it->record[0]) return 0;

    char path[MAX_BUFFER_SIZE];
    snprintf(path, sizeof(path), "%s/%s", INFO_ACT_PATH, it->record);

    if (remove(path) != 0) {
        LOG_ERROR(mux_module, "Unable to remove activity record: %s", path);
        return 0;
    }

    track_delete = 1;
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
        int muxretro_count;
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

        const int first_played = it->first_played > 0 ? it->first_played : it->last_played;
        if (first_played > 0 && first_played < gs->oldest_content_time) {
            gs->oldest_content_time = first_played;
            snprintf(gs->oldest_content, sizeof(gs->oldest_content), "%s", it->name);
        }

        for (int h = 0; h < 24; h++)
            hour_buckets[h] += it->hour_time[h];

        for (int d = 0; d < 7; d++)
            day_buckets[d] += it->day_time[d];

        if (it->longest_session >= gs->longest_session_duration) {
            gs->longest_session_duration = it->longest_session;
            snprintf(gs->longest_session, sizeof(gs->longest_session), "%s", it->name);
        }

        {
            char norm_core[64];
            normalise_json_values(norm_core, sizeof(norm_core), it->core);

            const int item_is_muxretro = activity_item_uses_muxretro(it) ? it->core_count : 0;

            int found = 0;
            for (int j = 0; j < core_used; j++) {
                if (strcmp(core_map[j].key, norm_core) == 0) {
                    core_map[j].count += it->core_count;
                    core_map[j].muxretro_count += item_is_muxretro;
                    found = 1;
                    break;
                }
            }

            if (!found && norm_core[0] != '\0') {
                if (core_used < CORE_MAP_MAX) {
                    snprintf(core_map[core_used].key, sizeof(core_map[core_used].key), "%s", norm_core);
                    core_map[core_used].count = it->core_count;
                    core_map[core_used].muxretro_count = item_is_muxretro;
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
                gs->core_is_muxretro = core_map[i].muxretro_count * 2 > core_map[i].count;
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

static void bucket_activity_time(activity_item_t *it, const int epoch, const size_t seconds) {
    if (epoch <= 0 || seconds == 0) return;

    const time_t t = epoch;
    struct tm tm_buf;
    const struct tm *tm = localtime_r(&t, &tm_buf);
    if (!tm) return;

    it->hour_time[tm->tm_hour] += seconds;
    it->day_time[tm->tm_wday] += seconds;
}

static int read_launch_count(const struct json map, const char *key) {
    if (!key[0] || json_type(map) != JSON_OBJECT) return 0;
    return (int) json_size_positive(json_object_get(json_object_get(map, key), "launches"));
}

static void load_session_buckets(activity_item_t *it, const struct json sessions) {
    int bucketed = 0;

    if (json_type(sessions) == JSON_ARRAY) {
        for (struct json session = json_first(sessions); json_exists(session); session = json_next(session)) {
            const int start = json_epoch_or_zero(json_object_get(session, "start"));
            const size_t length = json_size_positive(json_object_get(session, "length"));

            if (start > 0 && length > 0) {
                bucket_activity_time(it, start, length);
                bucketed = 1;
            }
        }
    }

    if (!bucketed) bucket_activity_time(it, it->last_played, it->total_time);
}

static int load_activity_record(activity_item_t *it, const char *record) {
    char record_path[MAX_BUFFER_SIZE];
    snprintf(record_path, sizeof(record_path), "%s/%s", INFO_ACT_PATH, record);

    char *data = read_all_char_from(record_path);
    if (!data) return 0;

    if (!json_valid(data)) {
        LOG_WARN(mux_module, "Skipping invalid activity record: %s", record_path);
        free(data);
        return 0;
    }

    const struct json val = json_parse(data);
    const struct json path_json = json_object_get(val, "path");

    if (json_type(val) != JSON_OBJECT || json_type(path_json) != JSON_STRING) {
        free(data);
        return 0;
    }

    memset(it, 0, sizeof(*it));
    snprintf(it->record, sizeof(it->record), "%s", record);
    json_string_copy(path_json, it->path, sizeof(it->path));

    snprintf(it->file_name, sizeof(it->file_name), "%s", get_file_name(it->path));

    const char *last_slash = strrchr(it->path, '/');
    if (last_slash) {
        size_t n = (size_t) (last_slash - it->path + 1);
        if (n >= sizeof(it->dir)) n = sizeof(it->dir) - 1;

        memcpy(it->dir, it->path, n);
        it->dir[n] = '\0';
    }

    resolve_friendly_name(it->path, it->name);
    adjust_content_label(it->name);

    it->total_time = json_size_positive(json_object_get(val, "total_time"));
    it->launch_count = json_size_positive(json_object_get(val, "launches"));
    it->average_time = it->launch_count ? it->total_time / it->launch_count : 0;
    it->last_session = json_size_positive(json_object_get(val, "last_session"));
    it->longest_session = json_size_positive(json_object_get(val, "longest_session"));
    if (it->longest_session < it->last_session) it->longest_session = it->last_session;

    it->first_played = json_epoch_or_zero(json_object_get(val, "first_played"));
    it->last_played = json_epoch_or_zero(json_object_get(val, "last_played"));

    json_string_copy(json_object_get(val, "last_core"), it->core, sizeof(it->core));
    json_string_copy(json_object_get(val, "last_device"), it->device, sizeof(it->device));
    json_string_copy(json_object_get(val, "last_mode"), it->mode, sizeof(it->mode));

    it->core_count = read_launch_count(json_object_get(val, "cores"), it->core);
    it->device_count = read_launch_count(json_object_get(val, "devices"), it->device);
    it->mode_count = read_launch_count(json_object_get(val, "modes"), it->mode);

    load_session_buckets(it, json_object_get(val, "sessions"));

    free(data);
    return it->launch_count > 0;
}

static void load_activity_items(void) {
    activity_count = 0;
    migrate_legacy_activity();

    DIR *dir = opendir(INFO_ACT_PATH);
    if (!dir) return;

    const struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (!is_activity_record(entry->d_name)) continue;
        if (!ensure_activity_capacity()) break;

        if (load_activity_record(&activity_items[activity_count], entry->d_name)) activity_count++;
    }

    closedir(dir);
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
    for (size_t i = 0; i < activity_count && i < limit + (size_t) list_win_peek_rows((int) activity_count); ++i) {
        char label_buffer[MAX_BUFFER_SIZE];
        format_activity_row(&activity_items[i], activity_display_mode, label_buffer);

        gen_label(mux_module, "rom", label_buffer);
    }
    list_win_ungroup_peek((int) activity_count);

    lv_obj_update_layout(ui_pnl_content);
    current_item_index = 0;

    if (ui_count_static == 0) {
        empty_state_show(lang.muxactivity.none, lang.muxactivity.none_hint);

        lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_menu, MU_OBJ_FLAG_HIDE_FLOAT);
        lv_obj_add_flag(ui_lbl_nav_menu_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
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
        detail_first,
        detail_start,
        detail_average,
        detail_total,
        detail_last,
        detail_longest,
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
                snprintf(
                    detail_value, sizeof(detail_value), "%s",
                    format_core_name(core_tmp, 0, activity_item_uses_muxretro(it))
                );

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
            case detail_first:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.muxactivity.detail.first);
                format_timestamp(detail_value, sizeof(detail_value), it->first_played);
                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_start");
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
            case detail_longest:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.muxactivity.detail.longest);
                format_total_time(detail_value, sizeof(detail_value), it->longest_session);
                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_last");
                break;
            case detail_playstyle:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.muxactivity.style.local.label);

                const local_playstyle_t ps = resolve_local_playstyle(it->launch_count, it->total_time);
                snprintf(detail_value, sizeof(detail_value), "%s", local_playstyle_name(ps));

                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_play");
                break;
            default:
                continue;
        }

        ui_count_static++;

        lv_obj_t *ui_pnl_act = lv_obj_create(ui_pnl_content);
        apply_theme_list_panel(ui_pnl_act);

        lv_obj_t *ui_lbl_act_item = lv_label_create(ui_pnl_act);
        apply_theme_option_item_label(&theme, ui_lbl_act_item, detail_label, 1);

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
                snprintf(global_value, sizeof(global_value), "%s", format_core_name(core_tmp, 0, gs.core_is_muxretro));

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
                snprintf(global_value, sizeof(global_value), "%s", global_playstyle_name(gs.global_playstyle));
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
        apply_theme_option_item_label(&theme, ui_lbl_act_item, global_label, 1);

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
    }

    lv_obj_update_layout(ui_pnl_content);
    update_label_scroll();
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
    if (first_open) {
        first_open = 0;
    } else {
        play_sound(snd_navigate);
    }

    const int overview = !in_detail_view && !in_global_view;
    const int visible_count = theme.mux.item.count;
    const int multi_list = overview && (int) activity_count > visible_count;

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, lv_group_get_focused(ui_group));

        if (lv_group_get_focused(ui_group_value)) {
            apply_text_long_dot(&theme, lv_group_get_focused(ui_group_value));
        }

        if (direction < 0) {
            current_item_index = list_nav_wrap_index(current_item_index - 1);
        } else {
            current_item_index = list_nav_wrap_index(current_item_index + 1);
        }

        if (multi_list) {
            update_windowed_list(
                ui_pnl_content, direction, current_item_index, (int) activity_count, visible_count,
                update_activity_list_item, update_activity_list_items
            );
            focus_activity_group(focus_activity_list_index());
            list_win_update_peek_total((int) activity_count, update_activity_list_item);
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

static int skip_confirm = 0;
static mux_more more_menu;

static int remove_allowed(void);
static void start_remove(void);
static mux_dialogue remove_dlg;

static void handle_b(void);
static void show_overview(void);
static void set_display_mode(int mode);

static void do_remove(void) {
    if (!activity_items || overview_item_index < 0 || (size_t) overview_item_index >= activity_count) return;

    LOG_INFO(mux_module, "Purging Playtime Entry: %s", activity_items[overview_item_index].path);

    if (delete_activity_entry(&activity_items[overview_item_index])) {
        play_sound(snd_muos);
        free_activity_items();
        load_activity_items();
        last_sort_mode = -1;

        if (overview_item_index >= (int) activity_count)
            overview_item_index = activity_count > 0 ? (int) activity_count - 1 : 0;

        handle_b();
    } else {
        toast_message(lang.generic.remove_fail, tst_wait_s);
        play_sound(snd_error);
    }
}

static void hide_nav(void) {
    lv_obj_add_flag(ui_img_box, MU_OBJ_FLAG_HIDE_FLOAT);
    if (ui_viewport_objects[0]) lv_obj_add_flag(ui_viewport_objects[0], MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_counter_activity, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);

    lv_obj_add_flag(ui_lbl_nav_menu_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_menu, MU_OBJ_FLAG_HIDE_FLOAT);
}

static void show_nav_x(const char *label) {
    lv_obj_clear_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);

    lv_label_set_text(ui_lbl_nav_x, label);
}

static void show_nav(void) {
    lv_obj_clear_flag(ui_img_box, MU_OBJ_FLAG_HIDE_FLOAT);
    if (ui_viewport_objects[0]) lv_obj_clear_flag(ui_viewport_objects[0], MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lbl_counter_activity, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lbl_nav_a_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lbl_nav_a, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lbl_nav_menu_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lbl_nav_menu, MU_OBJ_FLAG_HIDE_FLOAT);

    lv_obj_add_flag(ui_lbl_nav_x_glyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lbl_nav_x, MU_OBJ_FLAG_HIDE_FLOAT);
}

static void handle_a(void) {
    if (more_active(&more_menu)) {
        const more_id opt = more_take(&more_menu, 0);

        if (opt == more_remove) {
            start_remove();
        } else if (opt == more_overview) {
            show_overview();
        } else if (opt == more_launch_count) {
            set_display_mode(1);
        } else if (opt == more_duration) {
            set_display_mode(0);
        } else if (opt == more_help) {
            play_sound(snd_info_open);
            show_help();
        }
        return;
    }

    if (dialogue_active(&remove_dlg)) {
        const mux_remove_opt opt = (mux_remove_opt) remove_dlg.selected;
        dialogue_dismiss(&remove_dlg);
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
    show_nav_x(lang.generic.remove);

    overview_item_index = current_item_index;

    const size_t idx = current_item_index;
    if (idx >= activity_count) return;

    show_detail_view(&activity_items[idx]);
    nav_moved = 1;
}

static void handle_b(void) {
    if (more_active(&more_menu)) {
        more_cancel(&more_menu);
        return;
    }

    if (dialogue_active(&remove_dlg)) {
        dialogue_mark_cancelled(&remove_dlg);
        dialogue_dismiss(&remove_dlg);
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

static int remove_allowed(void) {
    return in_detail_view && !dialogue_active(&remove_dlg) && overview_item_index >= 0
           && (size_t) overview_item_index < activity_count;
}

static void start_remove(void) {
    if (config.settings.advanced.trust_remove || skip_confirm) {
        do_remove();
        return;
    }

    play_sound(snd_confirm);
    dialogue_open(&remove_dlg, &theme);
}

static void show_overview(void) {
    if (in_detail_view || in_global_view) return;

    video_preview_cancel();
    hide_nav();

    overview_item_index = current_item_index;

    show_global_view();
    nav_moved = 1;
}

static void set_display_mode(const int mode) {
    if (in_detail_view || in_global_view || activity_display_mode == mode) return;

    activity_display_mode = mode;

    overview_item_index = current_item_index;

    refresh_activity_labels();

    if (config.visual.box_art < 4) image_refresh();
    nav_moved = 1;
}

static void handle_x(void) {
    if (orientation_handle_skip()) return;

    if (msgbox_active || !ui_count_static || more_active(&more_menu)) return;

    if (in_detail_view && remove_allowed()) start_remove();
}

static void handle_dpad_up(void) {
    if (more_dpad(&more_menu, &theme, -1, !swap_axis)) return;

    if (dialogue_active(&remove_dlg)) {
        if (!swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, -1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_up();
}

static void handle_dpad_down(void) {
    if (more_dpad(&more_menu, &theme, +1, !swap_axis)) return;

    if (dialogue_active(&remove_dlg)) {
        if (!swap_axis) {
            dialogue_navigate(&remove_dlg, &theme, +1);
            play_sound(snd_navigate);
        }
        return;
    }

    handle_list_nav_down();
}

static void handle_dpad_up_hold(void) {
    if (more_dpad_hold(&more_menu, &theme, -1, !swap_axis)) return;

    if (dialogue_active(&remove_dlg)) {
        dialogue_handle_dpad_hold(&remove_dlg, &theme, -1, !swap_axis);
        return;
    }

    handle_list_nav_up_hold();
}

static void handle_dpad_down_hold(void) {
    if (more_dpad_hold(&more_menu, &theme, +1, !swap_axis)) return;

    if (dialogue_active(&remove_dlg)) {
        dialogue_handle_dpad_hold(&remove_dlg, &theme, +1, !swap_axis);
        return;
    }

    handle_list_nav_down_hold();
}

static void handle_help(void) {
    if (more_active(&more_menu)) {
        play_sound(snd_back);
        more_cancel(&more_menu);
        return;
    }

    if (msgbox_active || progress_onscreen != -1 || !ui_count_static || hold_call) return;
    if (dialogue_active(&remove_dlg)) return;
    if (in_detail_view || in_global_view) return;

    more_entry entries[5];
    int count = 0;

    entries[count++] = (more_entry) {more_overview, 1};
    entries[count++] = (more_entry) {more_launch_count, activity_display_mode != 1};
    entries[count++] = (more_entry) {more_duration, activity_display_mode != 0};
    entries[count++] = (more_entry) {more_help, 1};

    play_sound(snd_info_open);

    more_open(&more_menu, &theme, ui_screen, entries, count);
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
                                  {ui_lbl_nav_a, lang.generic.details, 0},
                                  {ui_lbl_nav_b_glyph, "", 0},
                                  {ui_lbl_nav_b, lang.generic.back, 0},
                                  {ui_lbl_nav_menu_glyph, "", 0},
                                  {ui_lbl_nav_menu, lang.generic.actions, 0},
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

int muxactivity_main(void) {
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

        empty_state_show(lang.muxactivity.none, lang.muxactivity.none_hint);
    } else {
        if (config.visual.box_art < 4) {
            image_refresh();
            if (config.visual.video_preview > 0) video_refresh();
        }
        nav_moved = 1;
    }

    dialogue_init_remove(&remove_dlg, &theme, ui_screen, NULL, lang.generic.select, lang.generic.cancel);

    init_timer(ui_refresh_task, NULL);

    mux_input_options input_opts = {
        .swap_axis = theme.misc.navigation_type == 1,
        .press_handler =
            {
                [mux_input_a] = handle_a,
                [mux_input_b] = handle_b,
                [mux_input_x] = handle_x,
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

    if (orientation_should_show(mux_module)) {
        orientation_mark_shown(mux_module);
        show_help();
    }

    mux_input_task(&input_opts);

    transition_box_art_destroy();
    video_preview_destroy();

    return 0;
}
