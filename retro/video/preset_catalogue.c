#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <json/json.h>
#include <common/config/config.h>
#include <common/display/language.h>
#include <common/platform/audio.h>
#include <common/platform/device.h>
#include <common/platform/sysinfo.h>
#include <common/runtime/init.h>
#include <common/runtime/log.h>
#include <common/storage/download.h>
#include <common/storage/fileio.h>
#include <common/ui/nav.h>
#include "colour.h"
#include "preset_catalogue.h"
#include "overlay_bridge.h"
#include "overlay_library.h"
#include "../core/muxretro.h"
#include "../core/paths.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

#define CATALOGUE_MANIFEST_MAX MAX_MANIFEST_BYTES

enum catalogue_state { catalogue_idle = 0, catalogue_manifest, catalogue_browsing, catalogue_package };

enum catalogue_item_state { catalogue_item_available = 0, catalogue_item_installed, catalogue_item_update };

#define CATALOGUE_VARIANT_MAX 8
#define CATALOGUE_VARIANT_URL 512

typedef struct {
    char resolution[12];
    char url[CATALOGUE_VARIANT_URL];
    char sha256[SHA256_DIGEST_LENGTH * 2 + 1];
} catalogue_variant;

typedef struct {
    char name[64];
    char author[64];
    char author_directory[64];
    char key[64];
    char url[MAX_BUFFER_SIZE];
    char version[32];
    char sha256[SHA256_DIGEST_LENGTH * 2 + 1];
    char target[PATH_MAX];
    catalogue_variant variants[CATALOGUE_VARIANT_MAX];
    int variant_count;
    enum catalogue_item_state state;
} catalogue_item;

typedef struct {
    char name[64];
    int first;
    int count;
} catalogue_author;

static catalogue_item *items;
static catalogue_item **ordered_items;
static const char **labels;
static const char **glyphs;
static catalogue_author *authors;
static const char **author_labels;
static const char **author_glyphs;
static int item_count;
static int author_count;
static int active_author = -1;
static int package_index = -1;
static int package_variant = -1;
static enum preset_catalogue_kind active_kind;
static enum catalogue_state state;
static char manifest_path[PATH_MAX];
static char package_path[PATH_MAX];
static submenu author_menu;
static submenu item_menu;
static submenu_def author_definition;
static submenu_def item_definition;
static int initialised;

static size_t kind_size_limit(void);

static int regular_file_bounded(const char *path, const off_t limit) {
    struct stat status;
    return path && lstat(path, &status) == 0 && S_ISREG(status.st_mode) && status.st_size > 0
           && status.st_size <= limit;
}

static int https_url(const char *url) {
    return url && strncasecmp(url, "https://", 8) == 0 && url[8] != '\0';
}

static int sha256_text_valid(const char *text) {
    if (!text || strlen(text) != SHA256_DIGEST_LENGTH * 2) return 0;
    for (size_t i = 0; i < SHA256_DIGEST_LENGTH * 2; i++)
        if (!isxdigit((unsigned char) text[i])) return 0;
    return 1;
}

static int file_sha256(const char *path, const off_t size_limit, char output[SHA256_DIGEST_LENGTH * 2 + 1]) {
    const int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return 0;

    struct stat status;
    if (fstat(fd, &status) != 0 || !S_ISREG(status.st_mode) || status.st_size <= 0 || status.st_size > size_limit) {
        close(fd);
        return 0;
    }

    EVP_MD_CTX *context = EVP_MD_CTX_new();
    int okay = context && EVP_DigestInit_ex(context, EVP_sha256(), NULL) == 1;
    unsigned char buffer[4096];

    while (okay) {
        const ssize_t count = read(fd, buffer, sizeof(buffer));
        if (count > 0) {
            okay = EVP_DigestUpdate(context, buffer, (size_t) count) == 1;
            continue;
        }
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) okay = 0;
        break;
    }

    unsigned char digest[SHA256_DIGEST_LENGTH];
    unsigned digest_size = 0;
    if (okay) okay = EVP_DigestFinal_ex(context, digest, &digest_size) == 1 && digest_size == sizeof(digest);

    EVP_MD_CTX_free(context);
    close(fd);
    if (!okay) return 0;

    static const char alphabet[] = "0123456789abcdef";
    for (size_t i = 0; i < sizeof(digest); i++) {
        output[i * 2] = alphabet[digest[i] >> 4];
        output[i * 2 + 1] = alphabet[digest[i] & 15];
    }
    output[SHA256_DIGEST_LENGTH * 2] = '\0';
    return 1;
}

static enum catalogue_item_state item_state(const catalogue_item *item) {
    char path[PATH_MAX];
    const char *installed = item->target;
    if (active_kind == preset_catalogue_filter) {
        if (colour_filter_path(item->key, path, sizeof(path))) installed = path;
    } else if (active_kind == preset_catalogue_shader) {
        if (colour_shader_path(item->key, path, sizeof(path))) installed = path;
    } else {
        const int index = overlay_library_index(item->key);
        if (index >= 0 && overlay_library_path(index, path, sizeof(path))) installed = path;
    }

    char digest[SHA256_DIGEST_LENGTH * 2 + 1];
    if (!file_sha256(installed, (off_t) kind_size_limit(), digest)) return catalogue_item_available;
    return strcasecmp(digest, item->sha256) == 0 ? catalogue_item_installed : catalogue_item_update;
}

static int copy_json_string(const struct json object, const char *key, char *output, const size_t size) {
    const struct json value = json_object_get(object, key);
    if (json_type(value) != JSON_STRING) return 0;
    const size_t length = json_string_copy(value, output, size);
    return length > 0 && length < size;
}

static int catalogue_label_valid(const char *text) {
    if (!text || !text[0] || isspace((unsigned char) text[0])) return 0;

    size_t length = 0;
    for (const unsigned char *p = (const unsigned char *) text; *p; p++, length++)
        if (*p < 0x20 || *p == 0x7f) return 0;

    return length > 0 && !isspace((unsigned char) text[length - 1]);
}

static int safe_stem(const char *name, char *output, const size_t size) {
    size_t used = 0;
    int separator = 0;

    for (const unsigned char *p = (const unsigned char *) name; *p && used + 1 < size; p++) {
        if (isalnum(*p) || *p == '-' || *p == '_') {
            output[used++] = (char) *p;
            separator = 0;
        } else if (isspace(*p) || *p >= 0x80) {
            if (used && !separator) output[used++] = ' ';
            separator = 1;
        }
    }

    while (used && output[used - 1] == ' ')
        used--;
    output[used] = '\0';
    return used > 0;
}

static int catalogue_item_compare(const void *left, const void *right) {
    const catalogue_item *a = *(catalogue_item *const *) left;
    const catalogue_item *b = *(catalogue_item *const *) right;
    int folded = strcasecmp(a->author, b->author);
    if (folded) return folded;
    folded = strcasecmp(a->name, b->name);
    return folded ? folded : strcmp(a->name, b->name);
}

static catalogue_item *catalogue_item_at(const int index) {
    return index >= 0 && index < item_count ? ordered_items[index] : NULL;
}

static int selected_item_index(const int index) {
    if (active_author < 0 || active_author >= author_count || index < 0 || index >= authors[active_author].count)
        return -1;
    return authors[active_author].first + index;
}

static catalogue_item *selected_item_at(const int index) {
    return catalogue_item_at(selected_item_index(index));
}

static const char *kind_directory(void) {
    static char overlay_directory[PATH_MAX];

    switch (active_kind) {
        case preset_catalogue_filter:
            return COLOUR_FILTER_USER_DIR;
        case preset_catalogue_overlay:
            return overlay_library_dir(overlay_directory, sizeof(overlay_directory)) ? overlay_directory : "";
        default:
            return COLOUR_SHADER_USER_DIR;
    }
}

static const char *kind_extension(void) {
    switch (active_kind) {
        case preset_catalogue_filter:
            return ".ini";
        case preset_catalogue_overlay:
            return ".png";
        default:
            return ".frag";
    }
}

static const char *kind_glyph(void) {
    switch (active_kind) {
        case preset_catalogue_filter:
            return "filter";
        case preset_catalogue_overlay:
            return "overlay";
        default:
            return "shader";
    }
}

static const char *kind_temporary(void) {
    switch (active_kind) {
        case preset_catalogue_filter:
            return ".filter-download";
        case preset_catalogue_overlay:
            return ".overlay-download";
        default:
            return ".shader-download";
    }
}

static size_t kind_size_limit(void) {
    return active_kind == preset_catalogue_overlay ? OVERLAY_IMAGE_MAX : COLOUR_PRESET_FILE_MAX;
}

static int kind_file_valid(const char *path) {
    switch (active_kind) {
        case preset_catalogue_filter:
            return colour_filter_file_valid(path);
        case preset_catalogue_overlay:
            return overlay_library_file_valid(path);
        default:
            return colour_shader_file_valid(path);
    }
}

static int url_stem(const char *url, char *output, const size_t size) {
    const char *slash = strrchr(url, '/');
    const char *base = slash ? slash + 1 : url;

    char trimmed[128];
    snprintf(trimmed, sizeof(trimmed), "%s", base);

    char *dot = strrchr(trimmed, '.');
    if (dot) *dot = '\0';

    return safe_stem(trimmed, output, size);
}

static void device_resolution(char *out, const size_t out_size) {
    snprintf(out, out_size, "%dx%d", device.screen.width, device.screen.height);
}

static int
variant_target(const char *resolution, const char *author, const char *stem, char *out, const size_t out_size) {
    return (size_t) snprintf(out, out_size, OVERLAY_IMAGE_ROOT "%s/%s/%s.png", resolution, author, stem) < out_size;
}

static int read_variants(const struct json node, catalogue_item *item, char *stem, const size_t stem_size) {
    const struct json images = json_object_get(node, "images");
    if (json_type(images) != JSON_ARRAY) return 0;

    const size_t count = json_array_count(images);
    char wanted[16];
    device_resolution(wanted, sizeof(wanted));

    int matched = 0;
    stem[0] = '\0';
    item->variant_count = 0;

    for (size_t index = 0; index < count && item->variant_count < CATALOGUE_VARIANT_MAX; index++) {
        const struct json image = json_array_get(images, index);
        catalogue_variant *variant = &item->variants[item->variant_count];

        if (json_type(image) != JSON_OBJECT
            || !copy_json_string(image, "resolution", variant->resolution, sizeof(variant->resolution))
            || !copy_json_string(image, "url", variant->url, sizeof(variant->url))
            || !copy_json_string(image, "sha256", variant->sha256, sizeof(variant->sha256)) || !https_url(variant->url)
            || !sha256_text_valid(variant->sha256))
            return 0;

        // Every image of an overlay is published under one name, taken from the repo
        if (!stem[0] && (!url_stem(variant->url, stem, stem_size) || strcasecmp(stem, "none") == 0)) return 0;

        if ((size_t) snprintf(item->key, sizeof(item->key), "%s/%s", item->author_directory, stem) >= sizeof(item->key))
            return 0;

        char target[PATH_MAX];
        if (!variant_target(variant->resolution, item->author_directory, stem, target, sizeof(target))) return 0;

        if (strcmp(variant->resolution, wanted) == 0) {
            snprintf(item->url, sizeof(item->url), "%s", variant->url);
            snprintf(item->sha256, sizeof(item->sha256), "%s", variant->sha256);
            snprintf(item->target, sizeof(item->target), "%s", target);
            matched = 1;
        }

        item->variant_count++;
    }

    return matched;
}

static int parse_manifest(void) {
    if (!regular_file_bounded(manifest_path, CATALOGUE_MANIFEST_MAX)) return 0;

    char *raw = read_all_char_from(manifest_path);
    if (!raw || !*raw || !json_valid(raw)) {
        free(raw);
        return 0;
    }

    const struct json root = json_parse(raw);
    const size_t count = json_type(root) == JSON_ARRAY ? json_array_count(root) : SIZE_MAX;
    if (count > INT_MAX || count > SIZE_MAX / sizeof(*items)) {
        free(raw);
        return 0;
    }

    catalogue_item *next_items = count ? calloc(count, sizeof(*next_items)) : NULL;
    catalogue_item **next_ordered_items = count ? calloc(count, sizeof(*next_ordered_items)) : NULL;
    const char **next_labels = count ? calloc(count, sizeof(*next_labels)) : NULL;
    const char **next_glyphs = count ? calloc(count, sizeof(*next_glyphs)) : NULL;
    catalogue_author *next_authors = count ? calloc(count, sizeof(*next_authors)) : NULL;
    const char **next_author_labels = count ? calloc(count, sizeof(*next_author_labels)) : NULL;
    const char **next_author_glyphs = count ? calloc(count, sizeof(*next_author_glyphs)) : NULL;
    if (count
        && (!next_items || !next_ordered_items || !next_labels || !next_glyphs || !next_authors || !next_author_labels
            || !next_author_glyphs)) {
        free(next_items);
        free(next_ordered_items);
        free(next_labels);
        free(next_glyphs);
        free(next_authors);
        free(next_author_labels);
        free(next_author_glyphs);
        free(raw);
        return 0;
    }

    free(items);
    free(ordered_items);
    free(labels);
    free(glyphs);
    free(authors);
    free(author_labels);
    free(author_glyphs);
    items = next_items;
    ordered_items = next_ordered_items;
    labels = next_labels;
    glyphs = next_glyphs;
    authors = next_authors;
    author_labels = next_author_labels;
    author_glyphs = next_author_glyphs;

    item_count = 0;
    author_count = 0;
    active_author = -1;
    for (size_t index = 0; index < count; index++) {
        const struct json node = json_array_get(root, index);
        catalogue_item *item = &items[item_count];
        char stem[56];

        if (json_type(node) != JSON_OBJECT || !copy_json_string(node, "name", item->name, sizeof(item->name))
            || !copy_json_string(node, "author", item->author, sizeof(item->author))
            || !catalogue_label_valid(item->name) || !catalogue_label_valid(item->author)
            || !safe_stem(item->author, item->author_directory, sizeof(item->author_directory))
            || !copy_json_string(node, "version", item->version, sizeof(item->version))) {
            item_count = 0;
            free(raw);
            return 0;
        }

        if (active_kind == preset_catalogue_overlay) {
            if (!read_variants(node, item, stem, sizeof(stem))) continue;
        } else {
            item->variant_count = 0;

            if (!copy_json_string(node, "url", item->url, sizeof(item->url))
                || !copy_json_string(node, "sha256", item->sha256, sizeof(item->sha256)) || !https_url(item->url)
                || !sha256_text_valid(item->sha256) || !url_stem(item->url, stem, sizeof(stem))
                || strcasecmp(stem, "none") == 0) {
                item_count = 0;
                free(raw);
                return 0;
            }

            if ((size_t) snprintf(item->key, sizeof(item->key), "%s/%s", item->author_directory, stem)
                    >= sizeof(item->key)
                || (size_t) snprintf(
                       item->target, sizeof(item->target), "%s%s%s", kind_directory(), item->key, kind_extension()
                   ) >= sizeof(item->target)) {
                item_count = 0;
                free(raw);
                return 0;
            }
        }

        for (int prior = 0; prior < item_count; prior++) {
            if (strcmp(items[prior].target, item->target) == 0) {
                item_count = 0;
                free(raw);
                return 0;
            }
        }

        item->state = item_state(item);
        ordered_items[item_count] = item;
        item_count++;
    }

    free(raw);
    if (item_count > 1) qsort(ordered_items, (size_t) item_count, sizeof(*ordered_items), catalogue_item_compare);
    for (int index = 0; index < item_count; index++) {
        if (author_count == 0 || strcasecmp(authors[author_count - 1].name, ordered_items[index]->author) != 0) {
            catalogue_author *author = &authors[author_count];
            snprintf(author->name, sizeof(author->name), "%s", ordered_items[index]->author);
            author->first = index;
            author_labels[author_count] = author->name;
            author_glyphs[author_count] = "folder";
            author_count++;
        }

        authors[author_count - 1].count++;
        labels[index] = ordered_items[index]->name;
        glyphs[index] = kind_glyph();
    }
    author_definition.labels = author_labels;
    author_definition.glyphs = author_glyphs;
    return 1;
}

static void catalogue_value(const int index, char *buffer, const size_t length) {
    const catalogue_item *item = selected_item_at(index);
    if (!item) return;

    const char *status = lang.muxretro.catalogue_screen.not_installed;
    if (item->state == catalogue_item_installed) status = lang.muxretro.catalogue_screen.installed;
    if (item->state == catalogue_item_update) status = lang.muxretro.catalogue_screen.update;
    snprintf(buffer, length, "%s", status);
}

static int catalogue_action_row(const int index) {
    return selected_item_index(index) >= 0;
}

static const char *catalogue_action_label(const int index) {
    const catalogue_item *item = selected_item_at(index);
    if (!item) return lang.generic.download;
    if (item->state == catalogue_item_installed) return lang.muxretro.catalogue_screen.reinstall;
    if (item->state == catalogue_item_update) return lang.muxretro.catalogue_screen.update;
    return lang.generic.download;
}

static void author_menu_closed(void) {
    state = catalogue_idle;
    active_author = -1;
    if (active_kind == preset_catalogue_filter) {
        colfilter_menu_reopen_download();
    } else if (active_kind == preset_catalogue_overlay) {
        overlay_bridge_set_suppressed(0);
        overlay_bridge_apply();
        overlay_image_menu_reopen_download();
    } else {
        shader_menu_reopen_download();
    }
}

static void item_menu_closed(void) {
    submenu_reopen_at(&author_menu, active_author);
}

static void refresh_item_states(void) {
    for (int index = 0; index < item_count; index++)
        items[index].state = item_state(&items[index]);
}

static int publish_package(void) {
    catalogue_item *item = catalogue_item_at(package_index);
    if (!item) return 0;

    const char *expected = item->sha256;
    if (package_variant >= 0 && package_variant < item->variant_count)
        expected = item->variants[package_variant].sha256;

    char digest[SHA256_DIGEST_LENGTH * 2 + 1];
    if (!file_sha256(package_path, (off_t) kind_size_limit(), digest) || strcasecmp(digest, expected) != 0) return 0;

    const int valid = kind_file_valid(package_path);
    if (!valid) return 0;

    char destination[PATH_MAX];
    snprintf(destination, sizeof(destination), "%s", item->target);

    if (package_variant >= 0 && package_variant < item->variant_count) {
        char stem[56];
        if (!url_stem(item->variants[package_variant].url, stem, sizeof(stem))
            || !variant_target(
                item->variants[package_variant].resolution, item->author_directory, stem, destination,
                sizeof(destination)
            ))
            return 0;
    }

    create_directories(destination, 1);
    if (rename(package_path, destination) != 0) return 0;

    char directory[PATH_MAX];
    snprintf(directory, sizeof(directory), "%s", destination);
    char *slash = strrchr(directory, '/');
    if (slash) *slash = '\0';
    const int fd = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd >= 0) {
        fsync(fd);
        close(fd);
    }
    return 1;
}

static int start_download(void);

static void package_complete(const int result) {
    state = catalogue_browsing;
    const int valid = result == 0 && publish_package();
    if (!valid) unlink(package_path);

    const int run_of_images = package_index >= 0 && package_variant >= 0;
    const catalogue_item *item = catalogue_item_at(package_index);
    const int more_to_fetch = valid && run_of_images && item && package_variant + 1 < item->variant_count;

    if (run_of_images && !more_to_fetch) hide_progress_bar();

    if (more_to_fetch) {
        package_variant++;
        if (start_download()) return;
        hide_progress_bar();
    }

    if (active_kind == preset_catalogue_overlay) {
        char active_overlay[64];
        snprintf(active_overlay, sizeof(active_overlay), "%s", overlay_library_key(session_settings.overlay_image));
        set_download_progress_span(0, 1);
        overlay_library_refresh();
        const int active_overlay_index = overlay_library_index(active_overlay);
        session_settings_set_overlay_image(active_overlay_index >= 0 ? active_overlay_index : 0);
    }

    if (result != 0) {
        pause_menu_show_toast(lang.muxretro.catalogue_screen.download_failed);
    } else if (!valid) {
        pause_menu_show_toast(lang.muxretro.catalogue_screen.integrity_failed);
    } else {
        if (active_kind != preset_catalogue_overlay) submenu_stack_reload_colour_presets();
        pause_menu_show_toast(lang.muxretro.catalogue_screen.installed_done);
    }

    refresh_item_states();
    submenu_refresh_values(&item_menu);
    submenu_refresh_nav(&item_menu);
    package_index = -1;
    package_variant = -1;
}

static int start_download(void) {
    const catalogue_item *item = catalogue_item_at(package_index);
    if (!item) return 0;
    const char *url = item->url;
    int own_bar = 1;

    if (package_variant >= 0 && package_variant < item->variant_count) {
        url = item->variants[package_variant].url;

        set_download_progress_span(package_variant, item->variant_count);
        own_bar = 0;
    }

    snprintf(package_path, sizeof(package_path), "%s%s", kind_directory(), kind_temporary());

    set_download_callbacks(package_complete);
    if (initiate_download_limited(
            url, package_path, kind_size_limit(), own_bar, lang.muxretro.catalogue_screen.downloading
        )
        != 0)
        return 0;

    state = catalogue_package;
    return 1;
}

static void catalogue_action(const int index) {
    const int ordered_index = selected_item_index(index);
    const catalogue_item *item = catalogue_item_at(ordered_index);
    if (!item || atomic_load(&download_in_progress)) return;

    package_index = ordered_index;
    package_variant = item->variant_count > 0 ? 0 : -1;

    if (package_variant >= 0) show_progress_bar(lang.muxretro.catalogue_screen.downloading);

    if (!start_download()) {
        if (package_variant >= 0) hide_progress_bar();
        set_download_progress_span(0, 1);
        package_index = -1;
        package_variant = -1;
        pause_menu_show_toast(lang.muxretro.catalogue_screen.download_failed);
    }
}

static int author_action_row(const int index) {
    return index >= 0 && index < author_count;
}

static void author_action(const int index) {
    if (!author_action_row(index)) return;

    active_author = index;
    item_definition.labels = labels + authors[index].first;
    item_definition.glyphs = glyphs + authors[index].first;
    item_definition.row_count = authors[index].count;
    submenu_open(&item_menu);
}

static int author_child_tick(void) {
    if (!submenu_is_active(&item_menu)) return 0;
    submenu_tick(&item_menu);
    return 1;
}

static void manifest_complete(const int result) {
    if (result != 0 || !parse_manifest()) {
        state = catalogue_idle;
        pause_menu_show_toast(lang.muxretro.catalogue_screen.manifest_failed);
        return;
    }
    if (item_count == 0) {
        state = catalogue_idle;
        pause_menu_show_toast(lang.muxretro.catalogue_screen.empty);
        return;
    }

    author_definition.row_count = author_count;
    state = catalogue_browsing;
    submenu_open(&author_menu);
}

void preset_catalogue_init(void) {
    if (initialised) return;

    author_definition = (submenu_def) {
        .labels = author_labels,
        .glyphs = author_glyphs,
        .row_count = 0,
        .row_is_action = author_action_row,
        .action = author_action,
        .action_without_save_guard = 1,
        .child_tick = author_child_tick,
        .closed = author_menu_closed,
        .save_title = NULL,
        .save_desc = NULL,
    };
    item_definition = (submenu_def) {
        .labels = labels,
        .glyphs = glyphs,
        .row_count = 0,
        .value_text = catalogue_value,
        .row_is_action = catalogue_action_row,
        .action_label = catalogue_action_label,
        .action = catalogue_action,
        .action_without_save_guard = 1,
        .closed = item_menu_closed,
        .save_title = lang.muxretro.save.display_title,
        .save_desc = lang.muxretro.save.display_desc,
    };
    submenu_init(&author_menu, &author_definition);
    submenu_init(&item_menu, &item_definition);
    initialised = 1;
}

void preset_catalogue_open(const enum preset_catalogue_kind kind) {
    if (state != catalogue_idle || atomic_load(&download_in_progress)) {
        pause_menu_show_toast(lang.muxretro.catalogue_screen.download_failed);
        return;
    }
    if (!device.board.has_network || !is_network_connected()) {
        pause_menu_show_toast(lang.generic.need_connect);
        return;
    }

    active_kind = kind;

    if (kind == preset_catalogue_overlay) overlay_bridge_set_suppressed(1);

    const char *url = config.extra.shader.data;
    const char *filename = "shaders.json";
    if (kind == preset_catalogue_filter) {
        url = config.extra.filter.data;
        filename = "filters.json";
    } else if (kind == preset_catalogue_overlay) {
        url = config.extra.overlay.data;
        filename = "overlays.json";
    }

    if (!https_url(url)
        || (size_t) snprintf(manifest_path, sizeof(manifest_path), "%s/%s", RETRO_CAT_PATH, filename)
               >= sizeof(manifest_path)) {
        pause_menu_show_toast(lang.muxretro.catalogue_screen.manifest_failed);
        return;
    }

    set_download_callbacks(manifest_complete);
    if (initiate_download_limited(
            url, manifest_path, CATALOGUE_MANIFEST_MAX, 1, lang.muxretro.catalogue_screen.downloading
        )
        != 0) {
        pause_menu_show_toast(lang.muxretro.catalogue_screen.manifest_failed);
        return;
    }
    state = catalogue_manifest;
}

int preset_catalogue_tick(void) {
    if (state == catalogue_idle) return 0;
    download_poll();
    if (state == catalogue_browsing) submenu_tick(&author_menu);
    return 1;
}
