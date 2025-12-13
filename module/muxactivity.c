#include "muxshare.h"

typedef enum {
    LOCAL_PLAYSTYLE_UNKNOWN,
    LOCAL_PLAYSTYLE_ONE_AND_DONE,
    LOCAL_PLAYSTYLE_SAMPLER,
    LOCAL_PLAYSTYLE_SHORT_BURSTS,
    LOCAL_PLAYSTYLE_LONG_SESSIONS,
    LOCAL_PLAYSTYLE_COMPLETIONIST,
    LOCAL_PLAYSTYLE_ABANDONED,
    LOCAL_PLAYSTYLE_MARATHONER,
    LOCAL_PLAYSTYLE_RETURNER,
    LOCAL_PLAYSTYLE_ON_OFF,
    LOCAL_PLAYSTYLE_WEEKEND_WARRIOR
} local_playstyle_t;

typedef enum {
    GLOBAL_PLAYSTYLE_UNKNOWN,
    GLOBAL_PLAYSTYLE_CASUAL,
    GLOBAL_PLAYSTYLE_CORE_GAMER,
    GLOBAL_PLAYSTYLE_EXPLORER,
    GLOBAL_PLAYSTYLE_BINGER,
    GLOBAL_PLAYSTYLE_COMPLETIONIST,
    GLOBAL_PLAYSTYLE_POWER_USER,
    GLOBAL_PLAYSTYLE_COLLECTOR,
    GLOBAL_PLAYSTYLE_SPECIALIST,
    GLOBAL_PLAYSTYLE_NOMAD,
    GLOBAL_PLAYSTYLE_ROUTINE
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

    char longest_session[256];
    int longest_session_duration;

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
            return lang.MUXACTIVITY.STYLE.LOCAL.ONOFF;
        case LOCAL_PLAYSTYLE_WEEKEND_WARRIOR:
            return lang.MUXACTIVITY.STYLE.LOCAL.WEEKEND;
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
        default:
            return lang.GENERIC.UNKNOWN;
    }
}

static local_playstyle_t resolve_local_playstyle(int launches, int total_time) {
    if (launches <= 0) return LOCAL_PLAYSTYLE_UNKNOWN;
    if (launches == 1) return LOCAL_PLAYSTYLE_ONE_AND_DONE;

    if (total_time > 100 * 3600) return LOCAL_PLAYSTYLE_MARATHONER;

    if (launches <= 3 && total_time < 30 * 60) return LOCAL_PLAYSTYLE_ABANDONED;
    if (launches <= 5 && total_time > 20 * 3600) return LOCAL_PLAYSTYLE_COMPLETIONIST;

    int avg = total_time / launches;

    if (launches <= 5 && avg >= 3 * 3600) return LOCAL_PLAYSTYLE_WEEKEND_WARRIOR;
    if (launches <= 5 && avg >= 60 * 60) return LOCAL_PLAYSTYLE_LONG_SESSIONS;
    if (launches >= 10 && avg < 15 * 60) return LOCAL_PLAYSTYLE_SHORT_BURSTS;

    if (launches >= 5 && total_time < 3600) return LOCAL_PLAYSTYLE_SAMPLER;
    if (launches >= 15 && total_time < 10 * 3600) return LOCAL_PLAYSTYLE_RETURNER;

    if (launches >= 3 && launches <= 8 && avg < 30 * 60) return LOCAL_PLAYSTYLE_ON_OFF;

    return LOCAL_PLAYSTYLE_UNKNOWN;
}

static global_playstyle_t resolve_global_playstyle(const global_stats_t *gs) {
    if (gs->total_time <= 0) return GLOBAL_PLAYSTYLE_UNKNOWN;
    if (gs->total_time < 10 * 3600) return GLOBAL_PLAYSTYLE_CASUAL;

    if (gs->unique_titles >= 20 && gs->average_time < 45 * 60) return GLOBAL_PLAYSTYLE_EXPLORER;
    if (gs->unique_titles <= 5 && gs->total_time > 100 * 3600) return GLOBAL_PLAYSTYLE_COMPLETIONIST;

    if (gs->average_time > 2 * 3600) return GLOBAL_PLAYSTYLE_BINGER;

    if (gs->total_launches > 200 && gs->unique_cores >= 10) return GLOBAL_PLAYSTYLE_POWER_USER;
    if (gs->total_launches >= 100 && gs->average_time >= 30 * 60) return GLOBAL_PLAYSTYLE_CORE_GAMER;

    if (gs->unique_titles >= 50 && gs->average_time < 20 * 60) return GLOBAL_PLAYSTYLE_COLLECTOR;
    if (gs->unique_cores <= 2 && gs->total_time > 80 * 3600) return GLOBAL_PLAYSTYLE_SPECIALIST;

    if (gs->device_count >= 3) return GLOBAL_PLAYSTYLE_NOMAD;

    if (gs->total_launches >= 75 && gs->total_launches <= 200 &&
        gs->average_time >= 20 * 60 && gs->average_time <= 90 * 60)
        return GLOBAL_PLAYSTYLE_ROUTINE;

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

    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
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

    gs->top_time_value = -1;
    gs->top_launch_value = -1;
    gs->top_time[0] = '\0';
    gs->top_launch[0] = '\0';
    gs->first_content_time = INT_MAX;
    gs->longest_session_duration = 0;
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

        if (it->last_session > gs->longest_session_duration) {
            gs->longest_session_duration = it->last_session;
            snprintf(gs->longest_session, sizeof(gs->longest_session), "%s", it->name);
        }

        int found = 0;
        for (int j = 0; j < core_used; j++) {
            if (strcasecmp(core_map[j].key, it->core) == 0) {
                core_map[j].count += it->core_count;
                found = 1;
                break;
            }
        }

        if (!found && it->core[0] != '\0') {
            strncpy(core_map[core_used].key, it->core, sizeof(core_map[core_used].key));
            core_map[core_used].count = it->core_count;
            core_used++;
        }

        found = 0;
        for (int j = 0; j < device_used; j++) {
            if (strcasecmp(device_map[j].key, it->device) == 0) {
                device_map[j].count += it->device_count;
                found = 1;
                break;
            }
        }

        if (!found && it->device[0] != '\0') {
            strncpy(device_map[device_used].key, it->device, sizeof(device_map[device_used].key));
            device_map[device_used].count = it->device_count;
            device_used++;
        }

        found = 0;
        for (int j = 0; j < mode_used; j++) {
            if (strcasecmp(mode_map[j].key, it->mode) == 0) {
                mode_map[j].count += it->mode_count;
                found = 1;
                break;
            }
        }

        if (!found && it->mode[0] != '\0') {
            strncpy(mode_map[mode_used].key, it->mode, sizeof(mode_map[mode_used].key));
            mode_map[mode_used].count = it->mode_count;
            mode_used++;
        }
    }

    int max = -1;
    for (int i = 0; i < core_used; i++) {
        if (core_map[i].count > max) {
            max = core_map[i].count;
            strncpy(gs->core, core_map[i].key, sizeof(gs->core));
            gs->core_count = core_map[i].count;
        }
    }

    max = -1;
    for (int i = 0; i < device_used; i++) {
        if (device_map[i].count > max) {
            max = device_map[i].count;
            strncpy(gs->device, device_map[i].key, sizeof(gs->device));
            gs->device_count = device_map[i].count;
        }
    }

    max = -1;
    for (int i = 0; i < mode_used; i++) {
        if (mode_map[i].count > max) {
            max = mode_map[i].count;
            strncpy(gs->mode, mode_map[i].key, sizeof(gs->mode));
            gs->mode_count = mode_map[i].count;
        }
    }

    if (activity_count > 0) {
        gs->unique_cores = core_used;
        gs->global_playstyle = resolve_global_playstyle(gs);
        gs->average_time = gs->total_launches > 0 ? gs->total_time / gs->total_launches : 0;
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
    snprintf(current_activity_mode, sizeof(current_activity_mode), "%s: %s",
             lang.MUXACTIVITY.MODE, activity_display_mode ? lang.MUXACTIVITY.LAUNCH : lang.MUXACTIVITY.TIME);

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
                snprintf(global_label, sizeof(global_label), "First Game Played");
                snprintf(global_value, sizeof(global_value), "%s", gs.first_content);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_first");
                break;
            case GLOBAL_LONGEST_SESSION:
                snprintf(global_label, sizeof(global_label), "Longest Session");
                snprintf(global_value, sizeof(global_value), "%s", gs.longest_session);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_longest");
                break;
            case GLOBAL_PLAYSTYLE:
                snprintf(global_label, sizeof(global_label), "Overall Play Style");
                snprintf(global_value, sizeof(global_value), "%s", global_playstyle_name(gs.global_playstyle));
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_play");
                break;
            case GLOBAL_UNIQUE_TITLES:
                snprintf(global_label, sizeof(global_label), "Unique Content Played");
                snprintf(global_value, sizeof(global_value), "%zu", gs.unique_titles);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_uniqueplay");
                break;
            case GLOBAL_UNIQUE_CORES:
                snprintf(global_label, sizeof(global_label), "Unique Cores Used");
                snprintf(global_value, sizeof(global_value), "%d", gs.unique_cores);
                snprintf(global_glyph, sizeof(global_glyph), "%s", "global_uniquecore");
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
    if (msgbox_active || !ui_count || hold_call) return;
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
    hide_nav();

    overview_item_index = current_item_index;

    show_global_view();
    nav_moved = 1;
}

static void handle_y(void) {
    if (msgbox_active || !ui_count || hold_call || in_detail_view) return;

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
