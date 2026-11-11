#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common/base/options.h>
#include <common/display/theme.h>
#include <common/storage/fileio.h>
#include <common/display/theme_json.h>
#include <common/runtime/init.h>
#include <common/ui/nav/menu_style.h>
#include <common/ui/nav/style/tweak.h>
#include <json/json.h>

#define TWEAK_PATH RUN_STORAGE_PATH "theme/override/layout"

const char *const style_tweak_keys[TWEAK_FIELD_COUNT] = {
    "dx", "dy", "dw", "dh", "angle", "tilt_x", "tilt_y", "depth", "opacity", "glyph",
};

_Static_assert(NUMBER_COUNT <= 32, "numbers outgrew the set mask");

const char *const style_number_keys[NUMBER_COUNT] = {
    "size_pct",   "spacing_pct", "tilt",         "swell_pct", "text_pct",   "focus_width", "slice_gap",
    "slant",      "text_bg",     "text_x",       "text_y",    "text_angle", "name_x",      "name_y",
    "name_angle", "name_bg",     "mascot_scale", "glyph_pct", "name_w",     "radius_pct",
};

const char *const style_cascade_keys[CASCADE_FIELD_COUNT] = {"size", "spacing", "angle", "tilt", "fade"};

static const char *const item_keys[TWEAK_ITEMS] = {"explore", "collection", "history", "apps",
                                                   "info",    "config",     "reboot",  "shutdown"};

static style_tweaks tweaks;
static style_tweaks package;
static int has_package = 0;
static int loaded_source = tweak_target_override;

static const char *const motions[MOTION_COUNT] = {"default", "ease", "swing", "orbit"};

style_tweaks *style_tweaks_current(void) {
    return &tweaks;
}

static void tweaks_reset(style_tweaks *target) {
    memset(target, 0, sizeof(*target));
    target->swell = -1;
}

void style_tweaks_clear(void) {
    tweaks_reset(&tweaks);
}

int style_tweak_get(const style_tweak *tweak, const int field, const int fallback) {
    return tweak->mask & (1u << field) ? tweak->value[field] : fallback;
}

int style_tweaks_remove_all(void) {
    style_tweaks_clear();
    return !dir_exist(TWEAK_PATH) || remove_directory_recursive(TWEAK_PATH) == 0;
}

int style_tweaks_source(void) {
    return loaded_source;
}

const style_tweaks *style_tweaks_package(void) {
    return has_package ? &package : NULL;
}

void style_tweak_unset(style_tweak *tweak, const int field) {
    tweak->mask &= ~(1u << field);
}

void style_tweak_set(style_tweak *tweak, const int field, const int value) {
    tweak->mask |= 1u << field;
    tweak->value[field] = value;
}

static const char *theme_name(void) {
    const char *slash = theme_base ? strrchr(theme_base, '/') : NULL;
    return slash ? slash + 1 : theme_base ? theme_base : "";
}

static void tweak_path(char *path, const size_t size, const int style, const int global) {
    snprintf(path, size, TWEAK_PATH "/%s/%s.json", global ? "global" : theme_name(), menu_style_key(style));
}

static int read_document(const char *path, style_tweaks *out);

static void legacy_path(char *path, const size_t size, const int style, const int global) {
    snprintf(path, size, TWEAK_PATH "/%s/%d.json", global ? "global" : theme_name(), style);
}

static int read_saved(const int style, const int global, style_tweaks *out) {
    char path[MAX_BUFFER_SIZE];
    tweak_path(path, sizeof(path), style, global);
    if (read_document(path, out)) return 1;

    legacy_path(path, sizeof(path), style, global);
    return read_document(path, out);
}

int style_tweaks_saved(const int style) {
    char path[MAX_BUFFER_SIZE];
    for (int global = 0; global < 2; global++) {
        tweak_path(path, sizeof(path), style, global);
        if (file_exist(path)) return 1;
        legacy_path(path, sizeof(path), style, global);
        if (file_exist(path)) return 1;
    }
    return 0;
}

static void remove_saved(const int style, const int global) {
    char path[MAX_BUFFER_SIZE];
    legacy_path(path, sizeof(path), style, global);
    if (file_exist(path)) remove(path);
    tweak_path(path, sizeof(path), style, global);
    if (file_exist(path)) remove(path);
}

void style_tweaks_remove(const int style) {
    remove_saved(style, 0);
    remove_saved(style, 1);
}

static void read_entry(const struct json entry, style_tweak *tweak) {
    for (int field = 0; field < TWEAK_FIELD_COUNT; field++) {
        const struct json value = json_object_get(entry, style_tweak_keys[field]);
        if (json_exists(value)) style_tweak_set(tweak, field, json_int(value));
    }
}

static void read_tweaks(const struct json root, style_tweaks *out) {
    tweaks_reset(out);

    const struct json cascade = json_object_get(root, "cascade");
    for (int field = 0; field < CASCADE_FIELD_COUNT; field++) {
        const struct json value = json_object_get(cascade, style_cascade_keys[field]);
        if (json_exists(value)) out->cascade[field] = json_int(value);
    }

    const struct json motion = json_object_get(root, "motion");
    for (int i = 1; i < MOTION_COUNT; i++) {
        if (json_string_compare(motion, motions[i]) == 0) out->motion = i;
    }

    const struct json time = json_object_get(root, "time");
    if (json_exists(time)) out->time = json_int(time);

    const struct json swell = json_object_get(root, "swell");
    if (json_exists(swell)) out->swell = json_int(swell);

    json_string_copy(json_object_get(root, "options"), out->options, sizeof(out->options));

    const struct json height = json_object_get(root, "bounce_height");
    const struct json speed = json_object_get(root, "bounce_speed");
    out->bounce_set = (json_exists(height) ? 1 : 0) | (json_exists(speed) ? 2 : 0);
    if (json_exists(height)) out->bounce_height = json_int(height);
    if (json_exists(speed)) out->bounce_speed = json_int(speed);

    const struct json nudge_x = json_object_get(root, "mascot_x");
    const struct json nudge_y = json_object_get(root, "mascot_y");
    if (json_exists(nudge_x)) out->mascot_x = json_int(nudge_x);
    if (json_exists(nudge_y)) out->mascot_y = json_int(nudge_y);

    for (int id = 0; id < NUMBER_COUNT; id++) {
        const struct json value = json_object_get(root, style_number_keys[id]);
        if (!json_exists(value)) continue;
        out->number[id] = json_int(value);
        out->number_set |= 1u << id;
    }

    const struct json slots = json_object_get(root, "slots");
    for (size_t i = 0; i < json_array_count(slots) && i < TWEAK_SLOTS; i++) {
        read_entry(json_array_get(slots, i), &out->slots[i]);
    }

    const struct json items = json_object_get(root, "items");
    for (int i = 0; i < TWEAK_ITEMS; i++)
        read_entry(json_object_get(items, item_keys[i]), &out->items[i]);
}

static int read_document(const char *path, style_tweaks *out) {
    char *data = theme_json_read_file(path);
    if (!data) return 0;

    read_tweaks(json_parse(data), out);
    free(data);
    return 1;
}

static void package_path(char *path, const size_t size, const int dim) {
    snprintf(path, size, "%s/%stheme.json", theme_base ? theme_base : "", dim ? mux_dim : "");
}

static struct json package_entry(const struct json root, const int style) {
    return json_object_get(theme_json_module(root, "muxlaunch", "styles"), menu_style_key(style));
}

static int read_package(const int style) {
    for (int dim = 1; dim >= 0; dim--) {
        char path[MAX_BUFFER_SIZE];
        package_path(path, sizeof(path), dim);
        char *data = theme_json_read_file(path);
        if (!data) continue;

        const struct json entry = package_entry(json_parse(data), style);
        const int found = json_type(entry) == JSON_OBJECT;
        if (found) read_tweaks(entry, &package);
        free(data);
        if (found) return 1;
    }

    return 0;
}

void style_tweaks_load(const int style) {
    style_tweaks_clear();
    has_package = read_package(style);

    loaded_source = tweak_target_override;
    if (read_saved(style, 0, &tweaks)) return;

    if (has_package) {
        tweaks = package;
        loaded_source = tweak_target_package;
        return;
    }

    if (read_saved(style, 1, &tweaks)) loaded_source = tweak_target_global;
}

static size_t write_entry(char *out, const size_t size, const style_tweak *tweak) {
    size_t used = (size_t) snprintf(out, size, "{");
    int first = 1;
    for (int field = 0; field < TWEAK_FIELD_COUNT && used < size; field++) {
        if (!(tweak->mask & (1u << field))) continue;
        used += (size_t) snprintf(
            out + used, size - used, "%s\"%s\":%d", first ? "" : ",", style_tweak_keys[field], tweak->value[field]
        );
        first = 0;
    }
    if (used < size) used += (size_t) snprintf(out + used, size - used, "}");
    return used;
}

static size_t build_document(char *out, const size_t size) {
    size_t used = (size_t) snprintf(out, size, "{\"cascade\":{");
    for (int field = 0; field < CASCADE_FIELD_COUNT; field++) {
        used += (size_t) snprintf(
            out + used, size - used, "%s\"%s\":%d", field ? "," : "", style_cascade_keys[field], tweaks.cascade[field]
        );
    }

    used += (size_t) snprintf(
        out + used, size - used, "},\"motion\":\"%s\",\"time\":%d,\"swell\":%d,", motions[tweaks.motion], tweaks.time,
        tweaks.swell
    );
    for (int id = 0; id < NUMBER_COUNT; id++) {
        if (!(tweaks.number_set & (1u << id))) continue;
        used += (size_t) snprintf(out + used, size - used, "\"%s\":%d,", style_number_keys[id], tweaks.number[id]);
    }
    if (tweaks.bounce_set & 1) {
        used += (size_t) snprintf(out + used, size - used, "\"bounce_height\":%d,", tweaks.bounce_height);
    }
    if (tweaks.bounce_set & 2) {
        used += (size_t) snprintf(out + used, size - used, "\"bounce_speed\":%d,", tweaks.bounce_speed);
    }
    if (tweaks.mascot_x) used += (size_t) snprintf(out + used, size - used, "\"mascot_x\":%d,", tweaks.mascot_x);
    if (tweaks.mascot_y) used += (size_t) snprintf(out + used, size - used, "\"mascot_y\":%d,", tweaks.mascot_y);
    if (tweaks.options[0]) used += (size_t) snprintf(out + used, size - used, "\"options\":\"%s\",", tweaks.options);

    used += (size_t) snprintf(out + used, size - used, "\"slots\":[");
    for (int i = 0; i < TWEAK_SLOTS && used < size; i++) {
        if (i) used += (size_t) snprintf(out + used, size - used, ",");
        used += write_entry(out + used, size - used, &tweaks.slots[i]);
    }
    used += (size_t) snprintf(out + used, size - used, "],\"items\":{");
    for (int i = 0; i < TWEAK_ITEMS && used < size; i++) {
        used += (size_t) snprintf(out + used, size - used, "%s\"%s\":", i ? "," : "", item_keys[i]);
        used += write_entry(out + used, size - used, &tweaks.items[i]);
    }
    if (used < size) used += (size_t) snprintf(out + used, size - used, "}}");
    return used;
}

static int object_empty(const char *open) {
    const char *next = open + 1;
    while (*next == ' ' || *next == '\n' || *next == '\r' || *next == '\t')
        next++;
    return *next == '}';
}

static int save_package(const int style, const char *document) {
    char path[MAX_BUFFER_SIZE];
    package_path(path, sizeof(path), 0);

    char *data = theme_json_read_file(path);
    if (!data && file_exist(path)) return 0;
    if (!data) data = strdup("{}");
    if (!data) return 0;

    const struct json root = json_parse(data);
    const struct json modules = json_object_get(root, "modules");
    const struct json launch = json_object_get(modules, "muxlaunch");
    const struct json styles = json_object_get(launch, "styles");
    const struct json entry = package_entry(root, style);
    const char *key = menu_style_key(style);

    static char insert[20480];
    size_t start, end;
    if (json_exists(entry)) {
        start = (size_t) (json_raw(entry) - data);
        end = start + json_raw_length(entry);
        snprintf(insert, sizeof(insert), "%s", document);
    } else {
        const struct json parent = json_type(styles) == JSON_OBJECT    ? styles
                                   : json_type(launch) == JSON_OBJECT  ? launch
                                   : json_type(modules) == JSON_OBJECT ? modules
                                                                       : root;
        const char *open = json_raw(parent);
        start = end = (size_t) (open - data) + 1;

        const char *comma = object_empty(open) ? "" : ",";
        if (json_type(styles) == JSON_OBJECT) {
            snprintf(insert, sizeof(insert), "\"%s\":%s%s", key, document, comma);
        } else if (json_type(launch) == JSON_OBJECT) {
            snprintf(insert, sizeof(insert), "\"styles\":{\"%s\":%s}%s", key, document, comma);
        } else if (json_type(modules) == JSON_OBJECT) {
            snprintf(insert, sizeof(insert), "\"muxlaunch\":{\"styles\":{\"%s\":%s}}%s", key, document, comma);
        } else {
            snprintf(
                insert, sizeof(insert), "\"modules\":{\"muxlaunch\":{\"styles\":{\"%s\":%s}}}%s", key, document, comma
            );
        }
    }

    const size_t length = strlen(data);
    const size_t total = length - (end - start) + strlen(insert);
    char *written = malloc(total + 1);
    if (!written) {
        free(data);
        return 0;
    }

    memcpy(written, data, start);
    memcpy(written + start, insert, strlen(insert));
    memcpy(written + start + strlen(insert), data + end, length - end + 1);
    free(data);

    const int saved = json_valid(written) && write_text_to_file_atomic(path, CHAR, written);
    free(written);
    return saved;
}

int style_tweaks_save(const int style, const int target) {
    static char out[16384];
    if (build_document(out, sizeof(out)) >= sizeof(out)) return 0;

    char path[MAX_BUFFER_SIZE];
    if (target != tweak_target_override) remove_saved(style, 0);

    if (target == tweak_target_package) {
        if (!save_package(style, out)) return 0;
        package = tweaks;
        has_package = 1;
        loaded_source = tweak_target_package;
        return 1;
    }

    const int global = target == tweak_target_global;
    legacy_path(path, sizeof(path), style, global);
    if (file_exist(path)) remove(path);
    tweak_path(path, sizeof(path), style, global);
    create_directories(path, 1);
    loaded_source = target;
    return write_text_to_file_atomic(path, CHAR, out);
}
