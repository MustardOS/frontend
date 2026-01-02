#include "muxshare.h"
#include "ui/ui_muxactivity.h"

static lv_obj_t *ui_viewport_objects[7];
static int starter_image = 0;

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
    show_info_box(lang.MUXACTIVITY.TITLE, lang.MUXACTIVITY.HELP, 0);
}

static void adjust_label_value_width(lv_obj_t *panel, lv_obj_t *label, lv_obj_t *value, lv_obj_t *glyph) {
    lv_obj_update_layout(panel);

    lv_coord_t panel_width = lv_obj_get_width(panel);
    if (panel_width <= 0) return;

    const char *label_text = lv_label_get_text(label);
    if (!label_text) return;

    const lv_font_t *font = lv_obj_get_style_text_font(label, LV_PART_MAIN);
    lv_coord_t letter_space = lv_obj_get_style_text_letter_space(label, LV_PART_MAIN);
    lv_coord_t line_space = lv_obj_get_style_text_line_space(label, LV_PART_MAIN);

    lv_point_t text_width;
    lv_txt_get_size(&text_width, label_text, font, letter_space, line_space, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_coord_t label_text_width = text_width.x;

    lv_coord_t glyph_width = glyph ? lv_obj_get_width(glyph) : 0;

    // Okay 64 leaves a good gap for both long static and long animated
    lv_coord_t available = panel_width - label_text_width - glyph_width - 64;

    lv_obj_set_width(value, available);
}

static void update_label_scroll() {
    if (lv_group_get_focused(ui_group_value)) {
        set_label_long_mode(&theme, lv_group_get_focused(ui_group_value));
    }
}

static void image_refresh() {
    if (in_detail_view || in_global_view || config.VISUAL.BOX_ART == 8) return;

    char *item_dir = strip_dir(activity_items[current_item_index].dir);
    char *item_file_name = strdup(activity_items[current_item_index].name);

    char h_core_artwork[MAX_BUFFER_SIZE];
    get_catalogue_name(item_dir, item_file_name, h_core_artwork, sizeof(h_core_artwork));

    char image[MAX_BUFFER_SIZE];
    char image_path[MAX_BUFFER_SIZE];

    char *h_file_name = strip_ext(item_file_name);

    if (strlen(h_core_artwork) <= 1) {
        snprintf(image, sizeof(image), "%s/%simage/none_box.png",
                 config.THEME.STORAGE_THEME, mux_dimension);
        if (!file_exist(image)) {
            snprintf(image, sizeof(image), "%s/image/none_box.png",
                     config.THEME.STORAGE_THEME);
        }
    } else {
        if (!grid_mode_enabled || !config.VISUAL.BOX_ART_HIDE) {
            load_image_catalogue(h_core_artwork, h_file_name, "", "default",
                                 mux_dimension, "box", image, sizeof(image));
        }
    }

    LOG_INFO(mux_module, "Loading 'box' Artwork: %s", image);

    if (strcasecmp(box_image_previous_path, image) != 0) {
        char artwork_config_path[MAX_BUFFER_SIZE];
        snprintf(artwork_config_path, sizeof(artwork_config_path), "%s/%s.ini",
                 INFO_CAT_PATH, h_core_artwork);
        if (!file_exist(artwork_config_path)) {
            snprintf(artwork_config_path, sizeof(artwork_config_path), "%s/default.ini",
                     INFO_CAT_PATH);
        }

        if (file_exist(artwork_config_path)) {
            viewport_refresh(ui_viewport_objects, artwork_config_path, h_core_artwork, h_file_name);
            snprintf(box_image_previous_path, sizeof(box_image_previous_path), "%s", image);
        } else {
            if (file_exist(image)) {
                starter_image = 1;
                snprintf(image_path, sizeof(image_path), "M:%s", image);
                lv_img_set_src(ui_imgBox, image_path);
                snprintf(box_image_previous_path, sizeof(box_image_previous_path), "%s", image);
            } else {
                lv_img_set_src(ui_imgBox, &ui_image_Nothing);
                snprintf(box_image_previous_path, sizeof(box_image_previous_path), " ");
            }
        }
    }
}

static size_t json_size_positive(struct json j) {
    if (!json_exists(j)) return 0;

    int v = json_int(j);
    return (v > 0) ? (size_t) v : 0;
}

static int json_epoch_or_zero(struct json j) {
    if (!json_exists(j)) return 0;

    int v = json_int(j);
    return (v > 0) ? v : 0;
}

static int ensure_activity_capacity(void) {
    if (activity_count < activity_capacity) return 1;

    size_t new_capacity = activity_capacity ? activity_capacity * 2 : 256;
    activity_item_t *items = realloc(activity_items, new_capacity * sizeof(*items));

    if (!items) {
        toast_message("Activity list full (memory)", SHORT);
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

static void hour_label(char *dst, size_t dst_sz, int hour) {
    if (hour < 0 || hour > 23) {
        snprintf(dst, dst_sz, "%s", lang.GENERIC.UNKNOWN);
        return;
    }

    int h = hour % 12;
    if (h == 0) h = 12;

    snprintf(dst, dst_sz, "%d %s", h, hour < 12 ? "AM" : "PM");
}

static void weekday_label(char *dst, size_t dst_sz, int day) {
    static const char *days[] = {
            lang.GENERIC.SUNDAY,
            lang.GENERIC.MONDAY,
            lang.GENERIC.TUESDAY,
            lang.GENERIC.WEDNESDAY,
            lang.GENERIC.THURSDAY,
            lang.GENERIC.FRIDAY,
            lang.GENERIC.SATURDAY
    };

    if (day < 0 || day > 6) {
        snprintf(dst, dst_sz, "%s", lang.GENERIC.UNKNOWN);
        return;
    }

    snprintf(dst, dst_sz, "%s", days[day]);
}

static const char *local_playstyle_name(local_playstyle_t ps, int use_en) {
    switch (ps) {
        case LOCAL_PLAYSTYLE_ONE_AND_DONE:
            return use_en ? "One and Done" : lang.MUXACTIVITY.STYLE.LOCAL.ONE;
        case LOCAL_PLAYSTYLE_SAMPLER:
            return use_en ? "Sampler" : lang.MUXACTIVITY.STYLE.LOCAL.SAMPLER;
        case LOCAL_PLAYSTYLE_SHORT_BURSTS:
            return use_en ? "Short Bursts" : lang.MUXACTIVITY.STYLE.LOCAL.BURST;
        case LOCAL_PLAYSTYLE_LONG_SESSIONS:
            return use_en ? "Long Sessions" : lang.MUXACTIVITY.STYLE.LOCAL.LONG;
        case LOCAL_PLAYSTYLE_COMPLETIONIST:
            return use_en ? "Completionist" : lang.MUXACTIVITY.STYLE.LOCAL.COMPLETIONIST;
        case LOCAL_PLAYSTYLE_ABANDONED:
            return use_en ? "Abandoned" : lang.MUXACTIVITY.STYLE.LOCAL.ABANDONED;
        case LOCAL_PLAYSTYLE_MARATHONER:
            return use_en ? "Marathoner" : lang.MUXACTIVITY.STYLE.LOCAL.MARATHONER;
        case LOCAL_PLAYSTYLE_RETURNER:
            return use_en ? "Returner" : lang.MUXACTIVITY.STYLE.LOCAL.RETURNER;
        case LOCAL_PLAYSTYLE_ON_OFF:
            return use_en ? "On and Off" : lang.MUXACTIVITY.STYLE.LOCAL.ON_OFF;
        case LOCAL_PLAYSTYLE_WEEKEND_WARRIOR:
            return use_en ? "Weekend Warrior" : lang.MUXACTIVITY.STYLE.LOCAL.WEEKEND;
        case LOCAL_PLAYSTYLE_COMFORT:
            return use_en ? "Comfort Game" : lang.MUXACTIVITY.STYLE.LOCAL.COMFORT;
        case LOCAL_PLAYSTYLE_REGULAR:
            return use_en ? "Regular Play" : lang.MUXACTIVITY.STYLE.LOCAL.REGULAR;
        default:
            return use_en ? "Unique" : lang.MUXACTIVITY.UNIQUE;
    }
}

static const char *global_playstyle_name(global_playstyle_t ps, int use_en) {
    switch (ps) {
        case GLOBAL_PLAYSTYLE_CASUAL:
            return use_en ? "Casual" : lang.MUXACTIVITY.STYLE.GLOBAL.CASUAL;
        case GLOBAL_PLAYSTYLE_CORE_GAMER:
            return use_en ? "Core Gamer" : lang.MUXACTIVITY.STYLE.GLOBAL.CORE;
        case GLOBAL_PLAYSTYLE_EXPLORER:
            return use_en ? "Explorer" : lang.MUXACTIVITY.STYLE.GLOBAL.EXPLORER;
        case GLOBAL_PLAYSTYLE_BINGER:
            return use_en ? "Binger" : lang.MUXACTIVITY.STYLE.GLOBAL.BINGER;
        case GLOBAL_PLAYSTYLE_COMPLETIONIST:
            return use_en ? "Completionist" : lang.MUXACTIVITY.STYLE.GLOBAL.COMPLETIONIST;
        case GLOBAL_PLAYSTYLE_POWER_USER:
            return use_en ? "Power Player" : lang.MUXACTIVITY.STYLE.GLOBAL.POWER;
        case GLOBAL_PLAYSTYLE_COLLECTOR:
            return use_en ? "Content Collector" : lang.MUXACTIVITY.STYLE.GLOBAL.COLLECTOR;
        case GLOBAL_PLAYSTYLE_SPECIALIST:
            return use_en ? "Specialist" : lang.MUXACTIVITY.STYLE.GLOBAL.SPECIALIST;
        case GLOBAL_PLAYSTYLE_NOMAD:
            return use_en ? "Device Nomad" : lang.MUXACTIVITY.STYLE.GLOBAL.NOMAD;
        case GLOBAL_PLAYSTYLE_ROUTINE:
            return use_en ? "Routine Player" : lang.MUXACTIVITY.STYLE.GLOBAL.ROUTINE;
        case GLOBAL_PLAYSTYLE_HABITUAL:
            return use_en ? "Habitual Player" : lang.MUXACTIVITY.STYLE.GLOBAL.HABITUAL;
        case GLOBAL_PLAYSTYLE_WINDOW:
            return use_en ? "Window Shopper" : lang.MUXACTIVITY.STYLE.GLOBAL.WINDOW;
        default:
            return use_en ? "Unique" : lang.MUXACTIVITY.UNIQUE;
    }
}

// Static calculated values - much nicer on the brain to calculate!
#define SEC  ((size_t) 1)
#define MIN  (60  *  SEC)
#define HOUR (60  *  MIN)

#define SEC_15M (15 * MIN)
#define SEC_20M (20 * MIN)
#define SEC_30M (30 * MIN)
#define SEC_45M (45 * MIN)
#define SEC_90M (90 * MIN)

#define SEC_1H   (1   * HOUR)
#define SEC_2H   (2   * HOUR)
#define SEC_3H   (3   * HOUR)
#define SEC_8H   (8   * HOUR)
#define SEC_10H  (10  * HOUR)
#define SEC_15H  (15  * HOUR)
#define SEC_20H  (20  * HOUR)
#define SEC_50H  (50  * HOUR)
#define SEC_80H  (80  * HOUR)
#define SEC_100H (100 * HOUR)

// Compile-time sanity checks!
// https://www.gnu.org/software/c-intro-and-ref/manual/html_node/Static-Assertions.html
_Static_assert(SEC_1H == 3600, "1 hour must be exactly 3600 seconds");
_Static_assert(SEC_30M < SEC_1H, "Minute and hour ordering broken");
_Static_assert(SEC_2H > SEC_1H, "Hour scale 2H to 1H broken");
_Static_assert(SEC_100H > SEC_50H, "High end hour thresholds broken");
_Static_assert(SEC_15M < SEC_20M && SEC_20M < SEC_30M && SEC_30M < SEC_45M && SEC_45M < SEC_90M,
               "Minute thresholds must be increasing");
_Static_assert(SEC_1H < SEC_2H && SEC_2H < SEC_3H && SEC_3H < SEC_8H && SEC_8H < SEC_10H &&
               SEC_10H < SEC_15H && SEC_15H < SEC_20H && SEC_20H < SEC_50H &&
               SEC_50H < SEC_80H && SEC_80H < SEC_100H,
               "Hour thresholds must be increasing");

static local_playstyle_t resolve_local_playstyle(size_t launches, size_t total_time) {
    if (launches <= 0 || total_time <= 0)
        return LOCAL_PLAYSTYLE_UNKNOWN;

    const size_t avg = total_time / launches;

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

    const size_t avg = gs->average_time;

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
    time_t now = time(NULL);
    struct tm tm_buf;
    struct tm *tm = localtime_r(&now, &tm_buf);

    if (!tm) {
        snprintf(dst, TIME_BUF, "%s", lang.GENERIC.UNKNOWN);
        return;
    }

    strftime(dst, TIME_BUF, TIME_STRING, tm);
}

static void format_timestamp(char *dst, size_t dst_sz, int epoch) {
    if (epoch <= 0) {
        snprintf(dst, dst_sz, "%s", lang.GENERIC.UNKNOWN);
        return;
    }

    time_t t = (time_t) epoch;
    struct tm tm_buf;
    struct tm *tm = localtime_r(&t, &tm_buf);

    if (!tm) {
        snprintf(dst, dst_sz, "%s", lang.GENERIC.UNKNOWN);
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

static void normalise_json_values(char *dst, size_t dst_size, const char *src) {
    if (!src || !*src) {
        dst[0] = '\0';
        return;
    }

    while (*src && isspace((unsigned char) *src)) src++;

    size_t len = strlen(src);
    while (len > 0 && isspace((unsigned char) src[len - 1])) len--;

    size_t n = (len < dst_size - 1) ? len : (dst_size - 1);
    for (size_t i = 0; i < n; i++) dst[i] = (char) tolower((unsigned char) src[i]);

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
            time_t t = (time_t) it->last_played;
            struct tm tm_buf;
            struct tm *tm = localtime_r(&t, &tm_buf);

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
                    strncpy(core_map[core_used].key, norm_core, sizeof(core_map[core_used].key) - 1);
                    core_map[core_used].key[sizeof(core_map[core_used].key) - 1] = '\0';
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
                    strncpy(device_map[device_used].key, norm_device, sizeof(device_map[device_used].key) - 1);
                    device_map[device_used].key[sizeof(device_map[device_used].key) - 1] = '\0';
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
                    strncpy(mode_map[mode_used].key, norm_mode, sizeof(mode_map[mode_used].key) - 1);
                    mode_map[mode_used].key[sizeof(mode_map[mode_used].key) - 1] = '\0';
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
                strncpy(gs->core, core_map[i].key, sizeof(gs->core) - 1);
                gs->core[sizeof(gs->core) - 1] = '\0';
                gs->core_count = core_map[i].count;
            }
        }
    }

    {
        int max = -1;
        for (int i = 0; i < device_used; i++) {
            if (device_map[i].count > max) {
                max = device_map[i].count;
                strncpy(gs->device, device_map[i].key, sizeof(gs->device) - 1);
                gs->device[sizeof(gs->device) - 1] = '\0';
                gs->device_count = device_map[i].count;
            }
        }
    }

    {
        int max = -1;
        for (int i = 0; i < mode_used; i++) {
            if (mode_map[i].count > max) {
                max = mode_map[i].count;
                strncpy(gs->mode, mode_map[i].key, sizeof(gs->mode) - 1);
                gs->mode[sizeof(gs->mode) - 1] = '\0';
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

    gs->average_time = (gs->total_launches > 0) ? (gs->total_time / gs->total_launches) : 0;

    gs->global_playstyle = resolve_global_playstyle(gs);
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
                if (!ensure_activity_capacity()) break;
                if (activity_count >= activity_capacity) break;

                activity_item_t *it = &activity_items[activity_count];
                memset(it, 0, sizeof(*it));

                char full_path[512];
                json_string_copy(key, full_path, sizeof(full_path));

                it->dir[0] = '\0';

                char *last_slash = strrchr(full_path, '/');
                if (last_slash) {
                    size_t n = (size_t) (last_slash - full_path + 1);
                    if (n >= sizeof(it->dir)) n = sizeof(it->dir) - 1;

                    memcpy(it->dir, full_path, n);
                    it->dir[n] = '\0';
                }

                json_string_copy(name_json, it->name, sizeof(it->name));

                it->total_time = json_size_positive(time_json);
                it->launch_count = json_size_positive(launches_json);

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
                    it->core_count = (int) json_size_positive(core_count_json);
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
                    it->device_count = (int) json_size_positive(dev_count_json);
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

static void format_total_time(char *dst, size_t dst_sz, size_t total_time) {
    size_t days = total_time / 86400;
    size_t hours = (total_time % 86400) / 3600;
    size_t minutes = (total_time % 3600) / 60;

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

static void format_activity_row(const activity_item_t *it, int mode, char *dst) {
    if (mode == 0) {
        char tt[64];
        format_total_time(tt, sizeof(tt), it->total_time);
        snprintf(dst, ACT_ROW, "[%s] %s", tt, it->name);
    } else {
        snprintf(dst, ACT_ROW, "[%zu] %s", it->launch_count, it->name);
    }
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

    if (activity_display_mode != last_sort_mode) {
        if (first_open) {
            qsort(activity_items, activity_count, sizeof(activity_items[0]), cmp_activity_time);
        } else {
            if (activity_display_mode == 0) {
                qsort(activity_items, activity_count, sizeof(activity_items[0]), cmp_activity_time);
            } else {
                qsort(activity_items, activity_count, sizeof(activity_items[0]), cmp_activity_launch);
            }
        }

        last_sort_mode = activity_display_mode;
    }

    for (size_t i = 0; i < activity_count; ++i) {
        ui_count++;

        char label_buffer[MAX_BUFFER_SIZE];
        format_activity_row(&activity_items[i], activity_display_mode, label_buffer);

        gen_label(mux_module, "rom", label_buffer);
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
                snprintf(detail_value, sizeof(detail_value), "%s", format_core_name(core_tmp, 0));

                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_core");
                break;
            case DETAIL_LAUNCH:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.LAUNCH);
                snprintf(detail_value, sizeof(detail_value), "%zu", it->launch_count);
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
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.PLAYED);
                format_timestamp(detail_value, sizeof(detail_value), it->last_played);
                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_start");
                break;
            case DETAIL_AVERAGE:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.AVERAGE);
                format_total_time(detail_value, sizeof(detail_value), it->average_time);
                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_average");
                break;
            case DETAIL_TOTAL:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.TOTAL);
                format_total_time(detail_value, sizeof(detail_value), it->total_time);
                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_total");
                break;
            case DETAIL_LAST:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.DETAIL.LAST);
                format_total_time(detail_value, sizeof(detail_value), it->last_session);
                snprintf(detail_glyph, sizeof(detail_glyph), "%s", "detail_last");
                break;
            case DETAIL_PLAYSTYLE:
                snprintf(detail_label, sizeof(detail_label), "%s", lang.MUXACTIVITY.STYLE.LOCAL.LABEL);

                local_playstyle_t ps = resolve_local_playstyle(it->launch_count, it->total_time);
                snprintf(detail_value, sizeof(detail_value), "%s", local_playstyle_name(ps, 0));

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

        adjust_label_value_width(ui_pnlAct, ui_lblActItem, ui_lblActItemValue, ui_lblActItemGlyph);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblActItemValue);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblActItem, ui_lblActItemGlyph, detail_label);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblActItem);
    }

    lv_obj_update_layout(ui_pnlContent);
    update_label_scroll();
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
                snprintf(global_value, sizeof(global_value), "%s", format_core_name(core_tmp, 0));

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
                snprintf(global_value, sizeof(global_value), "%zu", gs.total_launches);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_launch");
                break;
            case GLOBAL_TOTAL_TIME:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.TOTAL);
                format_total_time(global_value, sizeof(global_value), gs.total_time);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_total");
                break;
            case GLOBAL_AVERAGE_TIME:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.AVERAGE);
                format_total_time(global_value, sizeof(global_value), gs.average_time);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_average");
                break;
            case GLOBAL_FIRST_GAME:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.OLDEST);
                snprintf(global_value, sizeof(global_value), "%s", gs.oldest_content);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_first");
                break;
            case GLOBAL_LONGEST_SESSION:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.LONGEST);
                snprintf(global_value, sizeof(global_value), "%s", gs.longest_session);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_longest");
                break;
            case GLOBAL_PLAYSTYLE:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.OVERALL);
                snprintf(global_value, sizeof(global_value), "%s", global_playstyle_name(gs.global_playstyle, 0));
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
                hour_label(global_value, sizeof(global_value), gs.active_hour);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_active");
                break;
            case GLOBAL_FAVOURITE_DAY:
                snprintf(global_label, sizeof(global_label), "%s", lang.MUXACTIVITY.GLOBAL.FAVOURITE_DAY);
                weekday_label(global_value, sizeof(global_value), gs.favourite_day);
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

        adjust_label_value_width(ui_pnlAct, ui_lblActItem, ui_lblActItemValue, ui_lblActItemGlyph);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblActItemValue);

        apply_size_to_content(&theme, ui_pnlContent, ui_lblActItem, ui_lblActItemGlyph, global_label);
        apply_text_long_dot(&theme, ui_pnlContent, ui_lblActItem);
    }

    lv_obj_update_layout(ui_pnlContent);
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

            "<link rel='preconnect' href='https://fonts.bunny.net'>"
            "<link rel='preconnect' href='https://cdn.datatables.net'>"

            "<link rel='stylesheet' href='https://fonts.bunny.net/css?family=noto-sans:400,600,700&display=swap'>"
            "<link rel='stylesheet' href='https://cdn.datatables.net/2.3.5/css/dataTables.dataTables.min.css'>"

            "<script src='https://code.jquery.com/jquery-3.7.1.slim.min.js' integrity='sha256-kmHvs0B+OpCW5GVHUNjv9rOmY0IvSIRcf7zGUDTDQM8=' crossorigin='anonymous'></script>"
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

    snprintf(global_core_value, sizeof(global_core_value), "%s", format_core_name(tmp_core, 0));
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

    char core_value[MAX_BUFFER_SIZE];
    char device_value[MAX_BUFFER_SIZE];

    char tt_avg[64];
    char tt_total[64];
    char tt_last[64];
    char ts_start[64];

    for (size_t i = 0; i < activity_count; i++) {
        const activity_item_t *it = &activity_items[i];
        local_playstyle_t ps = resolve_local_playstyle(it->launch_count, it->total_time);

        snprintf(tmp_core, sizeof(tmp_core), "%s", it->core);
        snprintf(core_value, sizeof(core_value), "%s", format_core_name(core_value, 0));

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
            "</script></body></html>"
    );

    fclose(f);

    toast_message("Activity statistics exported", MEDIUM);
    refresh_screen(ui_screen);
}

static void generate_activity_items(void) {
    turbo_time(1, 1);

    load_activity_items();

    ui_group = lv_group_create();
    ui_group_value = lv_group_create();
    ui_group_glyph = lv_group_create();
    ui_group_panel = lv_group_create();

    in_detail_view = 0;
    refresh_activity_labels();

    first_open = 0;

    turbo_time(0, 1);
}

static void list_nav_move(int steps, int direction) {
    if (!ui_count) return;
    first_open ? (first_open = 0) : play_sound(SND_NAVIGATE);

    for (int step = 0; step < steps; ++step) {
        apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group));

        if (lv_group_get_focused(ui_group_value)) {
            apply_text_long_dot(&theme, ui_pnlContent, lv_group_get_focused(ui_group_value));
        }

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

    update_label_scroll();
    image_refresh();
    nav_moved = 1;
}

static void list_nav_prev(int steps) {
    list_nav_move(steps, -1);
}

static void list_nav_next(int steps) {
    list_nav_move(steps, +1);
}

static void hide_nav(void) {
    lv_obj_add_flag(ui_imgBox, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lblCounter_activity, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lblNavXGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lblNavX, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lblNavYGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_add_flag(ui_lblNavY, MU_OBJ_FLAG_HIDE_FLOAT);
}

static void show_nav(void) {
    lv_obj_clear_flag(ui_imgBox, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lblCounter_activity, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lblNavAGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lblNavA, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lblNavXGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lblNavX, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lblNavYGlyph, MU_OBJ_FLAG_HIDE_FLOAT);
    lv_obj_clear_flag(ui_lblNavY, MU_OBJ_FLAG_HIDE_FLOAT);
}

static void handle_a(void) {
    if (msgbox_active || !ui_count || hold_call || in_global_view || in_detail_view) return;

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
        first_open = 1;

        list_nav_next(overview_item_index);
        return;
    }

    if (in_global_view) {
        show_nav();
        lv_label_set_text(ui_lblNavX, lang.MUXACTIVITY.GLOBAL.NAV);

        in_global_view = 0;
        refresh_activity_labels();
        nav_moved = 1;
        first_open = 1;

        list_nav_next(overview_item_index);
        return;
    }

    free_activity_items();

    write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "activity");

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
    image_refresh();
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
            ui_lblCounter_activity,
            ui_pnlHelp,
            ui_pnlProgressBrightness,
            ui_pnlProgressVolume,
            NULL
    });
}

static void init_elements(void) {
    lv_obj_set_align(ui_imgBox, config.VISUAL.BOX_ART_ALIGN);
    lv_obj_set_align(ui_viewport_objects[0], config.VISUAL.BOX_ART_ALIGN);

    adjust_box_art();
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
        starter_image = adjust_wallpaper_element(ui_group, starter_image, GENERAL);
        adjust_panels();

        if (!in_detail_view && !in_global_view) update_file_counter(ui_lblCounter_activity, ui_count);
        lv_obj_move_foreground(overlay_image);

        lv_obj_invalidate(ui_pnlContent);
        nav_moved = 0;
    }
}

int muxactivity_main() {
    starter_image = 0;

    init_module(__func__);
    init_theme(1, 0);

    init_ui_common_screen(&theme, &device, &lang, "");
    init_muxactivity(ui_screen, &theme);

    ui_viewport_objects[0] = lv_obj_create(ui_pnlBox);
    ui_viewport_objects[1] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[2] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[3] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[4] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[5] = lv_img_create(ui_viewport_objects[0]);
    ui_viewport_objects[6] = lv_img_create(ui_viewport_objects[0]);

    lv_obj_set_user_data(ui_screen, mux_module);
    lv_label_set_text(ui_lblDatetime, get_datetime());
    lv_label_set_text(ui_lblTitle, lang.MUXACTIVITY.TITLE);

    init_fonts();

    load_wallpaper(ui_screen, NULL, ui_pnlWall, ui_imgWall, GENERAL);

    generate_activity_items();
    init_elements();

    if (ui_count == 0) {
        hide_nav();
        lv_label_set_text(ui_lblScreenMessage, lang.MUXACTIVITY.NONE);
    } else {
        image_refresh();
        nav_moved = 1;
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
