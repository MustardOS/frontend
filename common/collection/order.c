#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "order.h"
#include "../options.h"
#include "../strutil.h"
#include "../fileio.h"
#include "../config.h"
#include "../content.h"
#include "../language.h"
#include "../json/json.h"

#define ORDER_CONFIG   CONF_CONFIG_PATH "sort_order"
#define ORDER_SEED     "/tmp/order_seed"
#define ORDER_DIR_PATH CONF_CONFIG_PATH "sort_order.d"

order_method order_active = order_natural;
int order_scope_directory = 0;
int order_variants[order_count];

static const struct {
    order_method method;
    const char *key;
} method_keys[order_count] = {
    {order_natural, "natural"},         {order_play_time, "play_time"},           {order_times_played, "times_played"},
    {order_last_played, "last_played"}, {order_recently_added, "recently_added"}, {order_title_length, "title_length"},
    {order_file_size, "file_size"},     {order_feeling_lucky, "feeling_lucky"},
};

const char *order_method_name(const order_method method) {
    switch (method) {
        case order_play_time:
            return lang.muxorder.method.play_time;
        case order_times_played:
            return lang.muxorder.method.times_played;
        case order_last_played:
            return lang.muxorder.method.last_played;
        case order_recently_added:
            return lang.muxorder.method.recently_added;
        case order_title_length:
            return lang.muxorder.method.title_length;
        case order_file_size:
            return lang.muxorder.method.file_size;
        case order_feeling_lucky:
            return lang.muxorder.method.feeling_lucky;
        case order_natural:
        default:
            return lang.muxorder.method.natural;
    }
}

const char *order_method_glyph(const order_method method) {
    for (int i = 0; i < order_count; i++) {
        if (method_keys[i].method == method) return method_keys[i].key;
    }

    return method_keys[0].key;
}

int order_variant_count(const order_method method) {
    return method == order_feeling_lucky ? ORDER_MAX_VARIANTS : 2;
}

const char *order_variant_name(const order_method method, const int variant) {
    const int a = variant == 0;

    switch (method) {
        case order_play_time:
            return a ? lang.muxorder.direction.longest : lang.muxorder.direction.shortest;
        case order_times_played:
            return a ? lang.muxorder.direction.most : lang.muxorder.direction.least;
        case order_last_played:
        case order_recently_added:
            return a ? lang.muxorder.direction.recent : lang.muxorder.direction.oldest;
        case order_title_length:
            return a ? lang.muxorder.direction.shortest : lang.muxorder.direction.longest;
        case order_file_size:
            return a ? lang.muxorder.direction.largest : lang.muxorder.direction.smallest;
        case order_feeling_lucky:
            switch (variant) {
                case 1:
                    return lang.muxorder.lucky.backwards;
                case 2:
                    return lang.muxorder.lucky.vowels;
                case 3:
                    return lang.muxorder.lucky.wordy;
                case 4:
                    return lang.muxorder.lucky.sequels;
                case 5:
                    return lang.muxorder.lucky.shouty;
                case 0:
                default:
                    return lang.muxorder.lucky.random;
            }
        case order_natural:
        default:
            return a ? lang.muxorder.direction.a_to_z : lang.muxorder.direction.z_to_a;
    }
}

int order_is_default(void) {
    return order_active == order_natural && order_variants[order_natural] == 0;
}

void order_reset_defaults(void) {
    // Alphasort is what users expect most from a fresh install!
    order_active = order_natural;

    for (int i = 0; i < order_count; i++) {
        order_variants[i] = 0;
    }
}

static int method_from_key(const char *key, order_method *method) {
    for (int i = 0; i < order_count; i++) {
        if (strcasecmp(method_keys[i].key, key) == 0) {
            *method = method_keys[i].method;
            return 1;
        }
    }

    return 0;
}

static void dir_config_path(char *out, const char *dir) {
    snprintf(out, MAX_BUFFER_SIZE, "%s/%08X", ORDER_DIR_PATH, fnv_hash_str(dir));
}

static int parse_config(const char *path) {
    if (!file_exist(path)) return 0;

    char *raw = read_all_char_from(path);
    if (!raw || !*raw) {
        free(raw);
        return 0;
    }

    int have_active = 0;

    for (const char *entry = strtok(raw, ","); entry; entry = strtok(NULL, ",")) {
        char key[32];
        int variant = 0;
        int active = 0;

        if (sscanf(entry, "%31[^:]:%d:%d", key, &variant, &active) != 3) continue;

        order_method method;
        if (!method_from_key(str_trim(key), &method)) continue;

        order_variants[method] = variant >= 0 && variant < order_variant_count(method) ? variant : 0;

        if (active && !have_active) {
            order_active = method;
            have_active = 1;
        }
    }

    free(raw);

    return have_active;
}

void order_load(const char *dir) {
    order_reset_defaults();
    order_scope_directory = 0;

    parse_config(ORDER_CONFIG);

    if (dir && *dir) {
        char path[MAX_BUFFER_SIZE];
        dir_config_path(path, dir);

        if (parse_config(path)) order_scope_directory = 1;
    }
}

void order_save(const char *dir, const int directory_scope) {
    char text[MAX_BUFFER_SIZE];
    size_t used = 0;

    for (int i = 0; i < order_count; i++) {
        const order_method method = method_keys[i].method;

        const int written = snprintf(
            text + used, sizeof(text) - used, "%s%s:%d:%d", used ? "," : "", method_keys[i].key, order_variants[method],
            method == order_active
        );

        if (written < 0 || (size_t) written >= sizeof(text) - used) break;
        used += (size_t) written;
    }

    char path[MAX_BUFFER_SIZE];

    if (directory_scope && dir && *dir) {
        create_directories(ORDER_DIR_PATH "/", 0);
        dir_config_path(path, dir);
        write_text_to_file_atomic(path, CHAR, text);

        order_scope_directory = 1;
        return;
    }

    create_directories(CONF_CONFIG_PATH, 0);
    write_text_to_file_atomic(ORDER_CONFIG, CHAR, text);

    if (dir && *dir) {
        dir_config_path(path, dir);
        remove(path);
    }

    order_scope_directory = 0;
}

static unsigned int session_seed(void) {
    if (file_exist(ORDER_SEED)) {
        const int stored = read_line_int_from(ORDER_SEED, 1);
        if (stored != 0) return (unsigned int) stored;
    }

    const unsigned int seed = (unsigned int) time(NULL) | 1U;
    write_text_to_file(ORDER_SEED, "w", INT, (int) seed);

    return seed;
}

static void title_curiosities(order_key *key, const char *title) {
    int in_word = 0;
    const size_t title_len = strlen(title);

    for (const char *c = title; *c; c++) {
        const unsigned char ch = (unsigned char) *c;

        if (strchr("aeiouAEIOU", *c)) key->vowels++;
        if (isupper(ch)) key->capitals++;
        if (isdigit(ch)) key->sequel = 1;

        if (isspace(ch)) {
            in_word = 0;
        } else if (!in_word) {
            in_word = 1;
            key->words++;
        }
    }

    static const char *numerals[] = {" II", " III", " IV", " V", " VI", " VII", " VIII", " IX", " X"};
    for (size_t i = 0; i < A_SIZE(numerals); i++) {
        const size_t len = strlen(numerals[i]);

        if (title_len >= len && strncasecmp(title + title_len - len, numerals[i], len) == 0) key->sequel = 1;
    }
}

void order_prepare(content_item *content_items, const size_t count, const char *base_dir) {
    if (!content_items || count == 0) return;

    const int wants_file_meta = order_active == order_recently_added || order_active == order_file_size;
    const int wants_playtime =
        order_active == order_play_time || order_active == order_times_played || order_active == order_last_played;
    const int wants_title_length = order_active == order_title_length;
    const int lucky_variant = order_active == order_feeling_lucky ? order_variants[order_feeling_lucky] : -1;
    const int wants_shuffle = lucky_variant == 0;
    const int wants_curiosities = lucky_variant >= 2;

    if (!wants_file_meta && !wants_playtime && !wants_title_length && !wants_shuffle && !wants_curiosities) return;

    const unsigned int seed = wants_shuffle ? session_seed() : 0;

    char playtime_path[MAX_BUFFER_SIZE];
    snprintf(playtime_path, sizeof(playtime_path), INFO_ACT_PATH "/" PLAYTIME_DATA);

    char *playtime_raw = NULL;
    struct json playtime_root = {0};
    int playtime_valid = 0;

    if (wants_playtime && file_exist(playtime_path)) {
        playtime_raw = read_all_char_from(playtime_path);
        if (playtime_raw && json_valid(playtime_raw)) {
            playtime_root = json_parse(playtime_raw);
            playtime_valid = 1;
        }
    }

    for (size_t i = 0; i < count; i++) {
        content_item *it = &content_items[i];
        order_key *key = &it->order;

        *key = (order_key) {0};

        if (wants_title_length) key->title_length = it->display_name ? strlen(it->display_name) : 0;
        if (wants_shuffle) key->shuffle = fnv_hash_str(it->name ? it->name : "") ^ seed;

        if (wants_curiosities && it->display_name) title_curiosities(key, it->display_name);

        if (!wants_file_meta && !playtime_valid) continue;

        const char *path = it->extra_data;
        char full_path[PATH_MAX];

        if ((!path || *path != '/') && base_dir) {
            snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, it->name ? it->name : "");
            path = full_path;
        }

        if (!path || !*path) continue;

        if (wants_file_meta) {
            struct stat st;

            if (stat(path, &st) == 0) {
                key->added = st.st_mtime;
                key->file_size = st.st_size;
            }
        }

        if (!playtime_valid) continue;

        const struct json entry = json_object_get(playtime_root, path);
        if (!json_exists(entry)) continue;

        key->play_time = (size_t) json_int(json_object_get(entry, "total_time"));
        key->times_played = (size_t) json_int(json_object_get(entry, "launches"));
        key->last_played = (long) json_int(json_object_get(entry, "start_time"));
    }

    free(playtime_raw);
}

static int compare_size(const size_t a, const size_t b, const int variant) {
    if (a == b) return 0;
    return variant == 0 ? (a < b ? 1 : -1) : a < b ? -1 : 1;
}

static int compare_long(const long a, const long b, const int variant) {
    if (a == b) return 0;
    return variant == 0 ? (a < b ? 1 : -1) : a < b ? -1 : 1;
}

static int compare_missing(const long a, const long b) {
    if (a > 0 && b > 0) return 0;
    if (a > 0) return -1;
    if (b > 0) return 1;

    return 0;
}

static int compare_rule(const content_item *a, const content_item *b, const order_method method, const int variant) {
    const char *name_a = a->sort_name ? a->sort_name : "";
    const char *name_b = b->sort_name ? b->sort_name : "";

    switch (method) {
        case order_play_time:
            return compare_size(a->order.play_time, b->order.play_time, variant);
        case order_times_played:
            return compare_size(a->order.times_played, b->order.times_played, variant);
        case order_last_played: {
            const int missing = compare_missing(a->order.last_played, b->order.last_played);
            return missing ? missing : compare_long(a->order.last_played, b->order.last_played, variant);
        }
        case order_recently_added:
            return compare_long(a->order.added, b->order.added, variant);
        case order_title_length:
            return compare_size(b->order.title_length, a->order.title_length, variant);
        case order_file_size: {
            const int missing = compare_missing(a->order.file_size, b->order.file_size);
            return missing ? missing : compare_long(a->order.file_size, b->order.file_size, variant);
        }
        case order_feeling_lucky:
            switch (variant) {
                case 1:
                    return -str_compare(&name_a, &name_b);
                case 2:
                    return compare_size(a->order.vowels, b->order.vowels, 0);
                case 3:
                    return compare_size(a->order.words, b->order.words, 0);
                case 4:
                    if (a->order.sequel != b->order.sequel) return a->order.sequel ? -1 : 1;
                    return 0;
                case 5:
                    return compare_size(a->order.capitals, b->order.capitals, 0);
                case 0:
                default:
                    if (a->order.shuffle == b->order.shuffle) return 0;
                    return a->order.shuffle < b->order.shuffle ? -1 : 1;
            }
        case order_natural: {
            const int result = str_compare(&name_a, &name_b);
            return variant == 0 ? result : -result;
        }
        case order_count:
        default:
            return 0;
    }
}

static int order_compare(const void *a, const void *b) {
    const content_item *item_a = a;
    const content_item *item_b = b;

    if (!config.visual.mixed_content && item_a->content_type != item_b->content_type)
        return item_a->content_type == content_type_folder ? -1 : 1;

    if (item_a->sort_bucket != item_b->sort_bucket) return item_b->sort_bucket - item_a->sort_bucket;

    const int result = compare_rule(item_a, item_b, order_active, order_variants[order_active]);
    if (result != 0) return result;

    const char *name_a = item_a->sort_name ? item_a->sort_name : "";
    const char *name_b = item_b->sort_name ? item_b->sort_name : "";

    return str_compare(&name_a, &name_b);
}

void order_apply(content_item *content_items, const size_t count) {
    if (!content_items || count < 2U) return;

    qsort(content_items, count, sizeof(content_item), order_compare);
}
