#include "catalogue.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include <json/json.h>
#include <module/muxshare.h>
#include <common/content/manifest.h>
#include <common/platform/sysinfo.h>
#include <common/storage/download.h>
#include <common/storage/fileio.h>

#include "../core/paths.h"

#define CATALOGUE_PRESET_MAX  (128 * 1024)
#define CATALOGUE_OVERLAY_MAX (8 * 1024 * 1024)
#define CATALOGUE_VARIANT_MAX 16

typedef enum { catalogue_idle, catalogue_manifest, catalogue_authors, catalogue_items, catalogue_package } catalogue_state;
typedef enum { catalogue_available, catalogue_installed, catalogue_update } catalogue_item_state;

typedef struct {
    char resolution[16];
    char url[MAX_BUFFER_SIZE];
    char sha256[SHA256_DIGEST_LENGTH * 2 + 1];
    char target[PATH_MAX];
} catalogue_variant;

typedef struct {
    char name[64];
    char author[64];
    char author_directory[64];
    char key[128];
    char url[MAX_BUFFER_SIZE];
    char sha256[SHA256_DIGEST_LENGTH * 2 + 1];
    char target[PATH_MAX];
    catalogue_variant variants[CATALOGUE_VARIANT_MAX];
    int variant_count;
    catalogue_item_state state;
} catalogue_item;

typedef struct {
    char name[64];
    int first;
    int count;
} catalogue_author;

static catalogue_item *entries;
static catalogue_item **ordered;
static catalogue_author *authors;
static int entry_count;
static int author_count;
static int active_author = -1;
static int package_index = -1;
static int package_variant = -1;
static wasabi_asset_kind active_kind;
static catalogue_state state;
static char manifest_path[PATH_MAX];
static char package_path[PATH_MAX];
static unsigned revision;

static size_t package_limit(void) {
    return active_kind == wasabi_asset_overlay ? CATALOGUE_OVERLAY_MAX : CATALOGUE_PRESET_MAX;
}

static int file_sha256(const char *path, char output[SHA256_DIGEST_LENGTH * 2 + 1]) {
    const int descriptor = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0) return 0;
    struct stat status;
    if (fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode) || status.st_size <= 0
        || (uintmax_t) status.st_size > package_limit()) {
        close(descriptor);
        return 0;
    }
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    int okay = context && EVP_DigestInit_ex(context, EVP_sha256(), NULL) == 1;
    unsigned char buffer[4096];
    while (okay) {
        const ssize_t count = read(descriptor, buffer, sizeof(buffer));
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
    close(descriptor);
    if (!okay) return 0;
    static const char alphabet[] = "0123456789abcdef";
    for (size_t index = 0; index < sizeof(digest); index++) {
        output[index * 2] = alphabet[digest[index] >> 4];
        output[index * 2 + 1] = alphabet[digest[index] & 15];
    }
    output[SHA256_DIGEST_LENGTH * 2] = '\0';
    return 1;
}

static int filter_valid(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) return 0;
    char line[160];
    int matrix = 0;
    int rows = 0;
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0] || line[0] == '#') continue;
        if (line[0] == '[') {
            matrix = strcmp(line, "[matrix]") == 0;
            continue;
        }
        if (matrix && rows < 3) {
            float a, b, c;
            char tail;
            if (sscanf(line, " %f %f %f %c", &a, &b, &c, &tail) != 3) {
                fclose(file);
                return 0;
            }
            rows++;
        }
    }
    fclose(file);
    return rows == 3;
}

static int shader_valid(const char *path) {
    struct stat status;
    if (lstat(path, &status) != 0 || !S_ISREG(status.st_mode) || status.st_size <= 0
        || status.st_size > CATALOGUE_PRESET_MAX)
        return 0;
    char *source = read_all_char_from(path);
    if (!source) return 0;
    const int valid = strstr(source, "void main") != NULL;
    free(source);
    return valid;
}

static int overlay_valid(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    unsigned char signature[8];
    const int valid = fread(signature, 1, sizeof(signature), file) == sizeof(signature)
                      && memcmp(signature, "\x89PNG\r\n\x1a\n", sizeof(signature)) == 0;
    fclose(file);
    return valid;
}

static catalogue_item_state installed_state(const catalogue_item *item) {
    const char *installed = item->target;
    const int asset_count = wasabi_assets_count(active_kind);
    for (int index = 1; index < asset_count; index++) {
        if (strcasecmp(wasabi_asset_key(active_kind, index), item->key) != 0) continue;
        installed = wasabi_asset_path(active_kind, index);
        break;
    }
    char digest[SHA256_DIGEST_LENGTH * 2 + 1];
    if (!file_sha256(installed, digest)) return catalogue_available;
    return strcasecmp(digest, item->sha256) == 0 ? catalogue_installed : catalogue_update;
}

static int compare_items(const void *left, const void *right) {
    const catalogue_item *a = *(catalogue_item *const *) left;
    const catalogue_item *b = *(catalogue_item *const *) right;
    int result = strcasecmp(a->author, b->author);
    if (!result) result = strcasecmp(a->name, b->name);
    return result ? result : strcmp(a->name, b->name);
}

static void clear_catalogue(void) {
    free(entries);
    free(ordered);
    free(authors);
    entries = NULL;
    ordered = NULL;
    authors = NULL;
    entry_count = author_count = 0;
    active_author = -1;
}

static int parse_manifest(void) {
    struct stat status;
    if (lstat(manifest_path, &status) != 0 || !S_ISREG(status.st_mode) || status.st_size <= 0
        || status.st_size > MAX_MANIFEST_BYTES)
        return 0;
    char *raw = read_all_char_from(manifest_path);
    if (!raw || !json_valid(raw)) {
        free(raw);
        return 0;
    }
    const struct json root = json_parse(raw);
    const size_t count = json_type(root) == JSON_ARRAY ? json_array_count(root) : SIZE_MAX;
    if (count > INT_MAX || count > SIZE_MAX / sizeof(*entries)) {
        free(raw);
        return 0;
    }
    clear_catalogue();
    entries = count ? calloc(count, sizeof(*entries)) : NULL;
    ordered = count ? calloc(count, sizeof(*ordered)) : NULL;
    authors = count ? calloc(count, sizeof(*authors)) : NULL;
    if (count && (!entries || !ordered || !authors)) {
        free(raw);
        clear_catalogue();
        return 0;
    }
    char overlay_root[PATH_MAX];
    snprintf(overlay_root, sizeof(overlay_root), WASABI_OVERLAY_PATH "%dx%d/", device.screen.width,
             device.screen.height);
    const char *root_path = active_kind == wasabi_asset_filter ? WASABI_FILTER_PATH
                            : active_kind == wasabi_asset_shader ? WASABI_SHADER_PATH
                                                                 : overlay_root;
    const char *suffix = active_kind == wasabi_asset_filter ? ".ini"
                         : active_kind == wasabi_asset_shader ? ".frag"
                                                              : ".png";
    wasabi_assets_refresh(active_kind);
    for (size_t index = 0; index < count; index++) {
        const struct json node = json_array_get(root, index);
        catalogue_item *item = &entries[entry_count];
        char version[32];
        char stem[64];
        if (json_type(node) != JSON_OBJECT || !manifest_json_string(node, "name", item->name, sizeof(item->name))
            || !manifest_json_string(node, "author", item->author, sizeof(item->author))
            || !manifest_json_string(node, "version", version, sizeof(version))
            || !manifest_label_valid(item->name) || !manifest_label_valid(item->author)
            || !manifest_safe_stem(item->author, item->author_directory, sizeof(item->author_directory)))
            goto failed;
        if (active_kind == wasabi_asset_overlay) {
            const struct json images = json_object_get(node, "images");
            const size_t image_count = json_type(images) == JSON_ARRAY ? json_array_count(images) : 0;
            char wanted[32];
            snprintf(wanted, sizeof(wanted), "%dx%d", device.screen.width, device.screen.height);
            int found = 0;
            for (size_t image = 0; image < image_count && item->variant_count < CATALOGUE_VARIANT_MAX; image++) {
                const struct json candidate = json_array_get(images, image);
                catalogue_variant *variant = &item->variants[item->variant_count];
                if (json_type(candidate) == JSON_OBJECT
                    && manifest_json_string(candidate, "resolution", variant->resolution, sizeof(variant->resolution))
                    && manifest_json_string(candidate, "url", variant->url, sizeof(variant->url))
                    && manifest_json_string(candidate, "sha256", variant->sha256, sizeof(variant->sha256))
                    && manifest_https_url(variant->url) && manifest_sha256_valid(variant->sha256)) {
                    item->variant_count++;
                    if (strcmp(variant->resolution, wanted) == 0) {
                        snprintf(item->url, sizeof(item->url), "%s", variant->url);
                        snprintf(item->sha256, sizeof(item->sha256), "%s", variant->sha256);
                        found = 1;
                    }
                } else {
                    goto failed;
                }
            }
            if (!found) continue;
        } else if (!manifest_json_string(node, "url", item->url, sizeof(item->url))
                   || !manifest_json_string(node, "sha256", item->sha256, sizeof(item->sha256))) {
            goto failed;
        }
        if (!manifest_url_stem(item->url, stem, sizeof(stem)) || !manifest_https_url(item->url)
            || !manifest_sha256_valid(item->sha256) || strcasecmp(stem, "none") == 0)
            goto failed;
        if (active_kind == wasabi_asset_overlay) {
            for (int variant = 0; variant < item->variant_count; variant++) {
                if ((size_t) snprintf(item->variants[variant].target, sizeof(item->variants[variant].target),
                                      WASABI_OVERLAY_PATH "%s/%s/%s.png", item->variants[variant].resolution,
                                      item->author_directory, stem)
                    >= sizeof(item->variants[variant].target))
                    goto failed;
            }
        }
        if ((size_t) snprintf(item->key, sizeof(item->key), "%s/%s", item->author_directory, stem)
                >= sizeof(item->key)
            || (size_t) snprintf(item->target, sizeof(item->target), "%s%s%s", root_path, item->key, suffix)
                   >= sizeof(item->target))
            goto failed;
        for (int prior = 0; prior < entry_count; prior++)
            if (strcmp(entries[prior].target, item->target) == 0) goto failed;
        item->state = installed_state(item);
        ordered[entry_count] = item;
        entry_count++;
    }
    free(raw);
    if (entry_count > 1) qsort(ordered, (size_t) entry_count, sizeof(*ordered), compare_items);
    for (int index = 0; index < entry_count; index++) {
        if (!author_count || strcasecmp(authors[author_count - 1].name, ordered[index]->author) != 0) {
            snprintf(authors[author_count].name, sizeof(authors[author_count].name), "%s", ordered[index]->author);
            authors[author_count].first = index;
            author_count++;
        }
        authors[author_count - 1].count++;
    }
    return 1;

failed:
    free(raw);
    clear_catalogue();
    return 0;
}

static int selected_item_index(const int row) {
    if (state != catalogue_items && state != catalogue_package) return -1;
    if (active_author < 0 || active_author >= author_count || row < 0 || row >= authors[active_author].count) return -1;
    return authors[active_author].first + row;
}

static catalogue_item *selected_item(const int row) {
    const int index = selected_item_index(row);
    return index >= 0 && index < entry_count ? ordered[index] : NULL;
}

static void manifest_complete(const int result) {
    if (result != download_result_ok || !parse_manifest()) {
        state = catalogue_idle;
        toast_message(lang.muxretro.catalogue_screen.manifest_failed, tst_wait_m);
    } else if (!entry_count) {
        state = catalogue_idle;
        toast_message(lang.muxretro.catalogue_screen.empty, tst_wait_m);
    } else {
        state = catalogue_authors;
    }
    revision++;
}

static void package_complete(const int result) {
    catalogue_item *item = package_index >= 0 && package_index < entry_count ? ordered[package_index] : NULL;
    int valid = result == download_result_ok && item;
    const char *expected = item ? item->sha256 : "";
    const char *target = item ? item->target : "";
    if (item && package_variant >= 0 && package_variant < item->variant_count) {
        expected = item->variants[package_variant].sha256;
        target = item->variants[package_variant].target;
    }
    if (valid) {
        char digest[SHA256_DIGEST_LENGTH * 2 + 1];
        valid = file_sha256(package_path, digest) && strcasecmp(digest, expected) == 0
                && (active_kind == wasabi_asset_filter ? filter_valid(package_path)
                    : active_kind == wasabi_asset_shader ? shader_valid(package_path)
                                                         : overlay_valid(package_path));
    }
    if (valid) {
        create_directories(target, 1);
        valid = rename(package_path, target) == 0;
        if (valid) {
            char directory[PATH_MAX];
            snprintf(directory, sizeof(directory), "%s", target);
            char *separator = strrchr(directory, '/');
            if (separator) *separator = '\0';
            const int descriptor = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
            if (descriptor >= 0) {
                fsync(descriptor);
                close(descriptor);
            }
            char name_path[PATH_MAX];
            if ((size_t) snprintf(name_path, sizeof(name_path), "%s.name", target) < sizeof(name_path))
                write_text_to_file_atomic(name_path, CHAR, item->name);
        }
    }
    if (!valid) unlink(package_path);
    const int more = valid && item && package_variant >= 0 && package_variant + 1 < item->variant_count;
    if (more) {
        package_variant++;
        const catalogue_variant *variant = &item->variants[package_variant];
        set_download_progress_span(package_variant, item->variant_count);
        set_download_callbacks(package_complete);
        if (initiate_download_limited(
                variant->url, package_path, package_limit(), 0, lang.muxretro.catalogue_screen.downloading
            ) == 0)
            return;
        valid = 0;
    }
    if (package_variant >= 0) {
        hide_progress_bar();
        set_download_progress_span(0, 1);
    }
    if (result != download_result_ok)
        toast_message(lang.muxretro.catalogue_screen.download_failed, tst_wait_m);
    else if (!valid)
        toast_message(lang.muxretro.catalogue_screen.integrity_failed, tst_wait_m);
    else
        toast_message(lang.muxretro.catalogue_screen.installed_done, tst_wait_m);
    wasabi_assets_refresh(active_kind);
    for (int index = 0; index < entry_count; index++) ordered[index]->state = installed_state(ordered[index]);
    package_index = -1;
    package_variant = -1;
    state = catalogue_items;
    revision++;
}

static int start_package(const int row) {
    catalogue_item *item = selected_item(row);
    if (!item || atomic_load(&download_in_progress)) return 0;
    package_index = selected_item_index(row);
    package_variant = item->variant_count > 0 ? 0 : -1;
    snprintf(package_path, sizeof(package_path), "%s.download",
             active_kind == wasabi_asset_filter ? WASABI_FILTER_PATH
             : active_kind == wasabi_asset_shader ? WASABI_SHADER_PATH
             : WASABI_OVERLAY_PATH);
    create_directories(package_path, 1);
    set_download_callbacks(package_complete);
    const char *url = package_variant >= 0 ? item->variants[package_variant].url : item->url;
    const int own_progress = package_variant < 0;
    if (!own_progress) {
        show_progress_bar(lang.muxretro.catalogue_screen.downloading);
        set_download_progress_span(0, item->variant_count);
    }
    if (initiate_download_limited(
            url, package_path, package_limit(), own_progress, lang.muxretro.catalogue_screen.downloading
        ) != 0) {
        if (!own_progress) {
            hide_progress_bar();
            set_download_progress_span(0, 1);
        }
        package_index = -1;
        package_variant = -1;
        toast_message(lang.muxretro.catalogue_screen.download_failed, tst_wait_m);
        return 0;
    }
    state = catalogue_package;
    return 1;
}

int wasabi_catalogue_open(const wasabi_asset_kind kind) {
    if ((kind < wasabi_asset_filter || kind >= wasabi_asset_kind_count) || state != catalogue_idle
        || atomic_load(&download_in_progress))
        return 0;
    if (!device.board.has_network || !is_network_connected()) {
        toast_message(lang.generic.need_connect, tst_wait_m);
        return 0;
    }
    const char *url = kind == wasabi_asset_filter ? config.extra.filter.data
                      : kind == wasabi_asset_shader ? config.extra.shader.data
                                                    : config.extra.overlay.data;
    const char *filename = kind == wasabi_asset_filter ? "filters.json"
                           : kind == wasabi_asset_shader ? "shaders.json"
                                                         : "overlays.json";
    if (!manifest_https_url(url)
        || (size_t) snprintf(manifest_path, sizeof(manifest_path), WASABI_SHARE_PATH "catalogue/%s", filename)
               >= sizeof(manifest_path)) {
        toast_message(lang.muxretro.catalogue_screen.manifest_failed, tst_wait_m);
        return 0;
    }
    create_directories(manifest_path, 1);
    active_kind = kind;
    set_download_callbacks(manifest_complete);
    if (initiate_download_limited(
            url, manifest_path, MAX_MANIFEST_BYTES, 1, lang.muxretro.catalogue_screen.downloading
        ) != 0) {
        toast_message(lang.muxretro.catalogue_screen.manifest_failed, tst_wait_m);
        return 0;
    }
    state = catalogue_manifest;
    return 1;
}

int wasabi_catalogue_active(void) {
    return state == catalogue_authors || state == catalogue_items || state == catalogue_package;
}

int wasabi_catalogue_count(void) {
    if (state == catalogue_authors) return author_count;
    if ((state == catalogue_items || state == catalogue_package) && active_author >= 0 && active_author < author_count)
        return authors[active_author].count;
    return 0;
}

const char *wasabi_catalogue_label(const int row) {
    if (state == catalogue_authors) return row >= 0 && row < author_count ? authors[row].name : "";
    catalogue_item *item = selected_item(row);
    return item ? item->name : "";
}

const char *wasabi_catalogue_value(const int row) {
    catalogue_item *item = selected_item(row);
    if (!item) return "";
    if (item->state == catalogue_installed) return lang.muxretro.catalogue_screen.installed;
    if (item->state == catalogue_update) return lang.muxretro.catalogue_screen.update;
    return lang.muxretro.catalogue_screen.not_installed;
}

const char *wasabi_catalogue_glyph(const int row __attribute__((unused))) {
    if (state == catalogue_authors) return "folder";
    return active_kind == wasabi_asset_filter ? "filter"
           : active_kind == wasabi_asset_shader ? "shader"
                                                 : "overlay";
}

const char *wasabi_catalogue_action(const int row) {
    catalogue_item *item = selected_item(row);
    if (!item) return lang.generic.select;
    if (item->state == catalogue_installed) return lang.muxretro.catalogue_screen.reinstall;
    if (item->state == catalogue_update) return lang.muxretro.catalogue_screen.update;
    return lang.generic.download;
}

int wasabi_catalogue_confirm(const int row) {
    if (state == catalogue_authors) {
        if (row < 0 || row >= author_count) return 0;
        active_author = row;
        state = catalogue_items;
        revision++;
        return 1;
    }
    return start_package(row);
}

int wasabi_catalogue_back(void) {
    if (state == catalogue_items) {
        state = catalogue_authors;
        revision++;
        return 1;
    }
    if (state == catalogue_package) return 1;
    if (state == catalogue_authors) {
        state = catalogue_idle;
        active_author = -1;
        revision++;
    }
    return 0;
}

unsigned wasabi_catalogue_revision(void) {
    return revision;
}

void wasabi_catalogue_tick(void) {
    if (state == catalogue_manifest || state == catalogue_package) download_poll();
}
