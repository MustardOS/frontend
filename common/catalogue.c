#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "init.h"
#include "log.h"
#include "config.h"
#include "theme.h"
#include "fileio.h"
#include "image.h"

#define CAT_DIR_CACHE_MAX 256

typedef struct cat_dir_entry {
    char path[MAX_BUFFER_SIZE];
    char **files;
    int count;
} cat_dir_entry;

static cat_dir_entry cat_dir_cache[CAT_DIR_CACHE_MAX];
static int cat_dir_cache_n;
static char cat_theme_path_snapshot[MAX_BUFFER_SIZE];

static int cat_file_cmp(const void *a, const void *b) {
    return strcmp(*(const char **) a, *(const char **) b);
}

static void cat_dir_free_all(void) {
    for (int i = 0; i < cat_dir_cache_n; i++) {
        for (int j = 0; j < cat_dir_cache[i].count; j++) {
            free(cat_dir_cache[i].files[j]);
        }

        free(cat_dir_cache[i].files);

        cat_dir_cache[i].files = NULL;
        cat_dir_cache[i].count = 0;
    }

    cat_dir_cache_n = 0;
}

void invalidate_catalogue_cache(void) {
    cat_dir_free_all();
    cat_theme_path_snapshot[0] = '\0';
}

static cat_dir_entry *cat_dir_lookup(const char *dir_path) {
    if (strcmp(config.theme.theme_cat_path, cat_theme_path_snapshot) != 0) {
        cat_dir_free_all();
        snprintf(cat_theme_path_snapshot, sizeof(cat_theme_path_snapshot), "%s", config.theme.theme_cat_path);
    }

    for (int i = 0; i < cat_dir_cache_n; i++) {
        if (strcmp(cat_dir_cache[i].path, dir_path) == 0) return &cat_dir_cache[i];
    }

    if (cat_dir_cache_n >= CAT_DIR_CACHE_MAX) return NULL;

    cat_dir_entry *e = &cat_dir_cache[cat_dir_cache_n++];
    snprintf(e->path, sizeof(e->path), "%s", dir_path);

    e->files = NULL;
    e->count = 0;

    DIR *d = opendir(dir_path);
    if (!d) return e;

    int cap = 64;
    e->files = malloc((size_t) cap * sizeof(char *));
    if (!e->files) {
        closedir(d);
        return e;
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        const char *dot = strrchr(ent->d_name, '.');

        if (!dot) continue;
        const char *ext = dot + 1;

        if (!is_image_ext(ext)) continue;

        if (e->count >= cap) {
            const int new_cap = cap * 2;
            char **tmp = realloc(e->files, (size_t) new_cap * sizeof(char *));
            if (!tmp) break;

            e->files = tmp;
            cap = new_cap;
        }

        e->files[e->count] = strdup(ent->d_name);
        if (e->files[e->count]) e->count++;
    }
    closedir(d);

    if (e->count > 0) qsort(e->files, (size_t) e->count, sizeof(char *), cat_file_cmp);

    return e;
}

static int cat_dir_has_file(cat_dir_entry *e, const char *filename) {
    if (!e || e->count == 0) return 0;
    return bsearch(&filename, e->files, (size_t) e->count, sizeof(char *), cat_file_cmp) != NULL;
}

void load_splash_image_fallback(const char *mux_dim, char *image, const size_t image_size) {
    if (snprintf(image, image_size, "%s/splash.png", INFO_CAT_PATH) >= 0 && file_exist(image)) return;

    if (snprintf(image, image_size, "%s/%simage/splash.png", theme_base, mux_dim) >= 0 && file_exist(image)) return;

    snprintf(image, image_size, "%s/image/splash.png", theme_base);
}

int is_supported_theme_catalogue(const char *catalogue_name, const char *image_type) {
    return (strcmp(catalogue_name, "Application") == 0 && strcmp(image_type, "box") == 0)
           || (strcmp(catalogue_name, "Application") == 0 && strcmp(image_type, "grid") == 0)
           || (strcmp(catalogue_name, "Collection") == 0 && strcmp(image_type, "box") == 0)
           || (strcmp(catalogue_name, "Collection") == 0 && strcmp(image_type, "grid") == 0)
           || (strcmp(catalogue_name, "Folder") == 0 && strcmp(image_type, "box") == 0)
           || (strcmp(catalogue_name, "Folder") == 0 && strcmp(image_type, "grid") == 0);
}

int load_image_catalogue(
    const char *catalogue_name, const char *program, const char *program_alt, const char *program_default,
    const char *mux_dim, const char *image_type, char *image_path, const size_t path_size
) {
    enum catalogue_kind { cat_theme, cat_info };

    const struct {
        enum catalogue_kind kind;
        const char *catalogue_path;
        const char *dimension;
        const char *program;
    } args[] = {
        {cat_theme, config.theme.theme_cat_path, mux_dim, program},
        {cat_theme, config.theme.theme_cat_path, mux_dim, program_alt},
        {cat_theme, config.theme.theme_cat_path, "", program},
        {cat_theme, config.theme.theme_cat_path, "", program_alt},

        {cat_info, INFO_CAT_PATH, mux_dim, program},
        {cat_info, INFO_CAT_PATH, mux_dim, program_alt},
        {cat_info, INFO_CAT_PATH, "", program},
        {cat_info, INFO_CAT_PATH, "", program_alt},

        {cat_theme, config.theme.theme_cat_path, mux_dim, program_default},
        {cat_theme, config.theme.theme_cat_path, "", program_default},

        {cat_info, INFO_CAT_PATH, mux_dim, program_default},
        {cat_info, INFO_CAT_PATH, "", program_default},
    };

    int ext_count;
    const char **extensions = image_ext_list(&ext_count);
    const int skip_theme = !is_supported_theme_catalogue(catalogue_name, image_type);

    if (image_path && path_size > 0) image_path[0] = '\0';

    if (!catalogue_name || !catalogue_name[0] || !image_type || !image_type[0] || !image_path || path_size == 0) {
        return 0;
    }

    for (int j = 0; j < ext_count; j++) {
        for (size_t i = 0; i < A_SIZE(args); i++) {
            if ((args[i].kind == cat_theme && skip_theme) || !args[i].catalogue_path || !args[i].catalogue_path[0]
                || !args[i].program || !args[i].program[0]) {
                continue;
            }

            char base_dir[MAX_BUFFER_SIZE];
            const int bw =
                snprintf(base_dir, sizeof(base_dir), "%s/%s/%s", args[i].catalogue_path, catalogue_name, image_type);
            if (bw < 0 || (size_t) bw >= sizeof(base_dir)) continue;

            char dim_clean[MAX_BUFFER_SIZE];
            dim_clean[0] = '\0';

            if (args[i].dimension && args[i].dimension[0]) {
                snprintf(dim_clean, sizeof(dim_clean), "%s", args[i].dimension);

                size_t len = strlen(dim_clean);
                while (len > 0 && dim_clean[len - 1] == '/') {
                    dim_clean[--len] = '\0';
                }
            }

            char dir[MAX_BUFFER_SIZE];
            const int dw = dim_clean[0] ? snprintf(dir, sizeof(dir), "%s/%s", base_dir, dim_clean)
                                        : snprintf(dir, sizeof(dir), "%s", base_dir);

            if (dw < 0 || (size_t) dw >= sizeof(dir)) continue;

            char filename[MAX_BUFFER_SIZE];
            const int fw = snprintf(filename, sizeof(filename), "%s.%s", args[i].program, extensions[j]);
            if (fw < 0 || (size_t) fw >= sizeof(filename)) continue;

            char full_path[MAX_BUFFER_SIZE];
            const int pw = snprintf(full_path, sizeof(full_path), "%s/%s", dir, filename);
            if (pw < 0 || (size_t) pw >= sizeof(full_path)) continue;

            cat_dir_entry *e = cat_dir_lookup(dir);
            if (e) {
                if (!cat_dir_has_file(e, filename)) continue;
            } else if (!file_exist(full_path)) {
                continue;
            }

            snprintf(image_path, path_size, "%s", full_path);
            LOG_DEBUG(mux_module, "Catalogue image found: %s", image_path);
            return 1;
        }
    }

    return 0;
}

int load_manual_catalogue(
    const char *catalogue_name, const char *program, const char *program_alt, char *manual_path,
    const size_t path_size
) {
    const char *programs[] = {program, program_alt};

    if (manual_path && path_size > 0) manual_path[0] = '\0';

    if (!catalogue_name || !catalogue_name[0] || !manual_path || path_size == 0) return 0;

    for (size_t i = 0; i < A_SIZE(programs); i++) {
        if (!programs[i] || programs[i][0] == '\0') continue;

        char dir[MAX_BUFFER_SIZE];
        const int dw = snprintf(dir, sizeof(dir), "%s/%s/manual", INFO_CAT_PATH, catalogue_name);
        if (dw < 0 || (size_t) dw >= sizeof(dir)) continue;

        char filename[MAX_BUFFER_SIZE];
        const int fw = snprintf(filename, sizeof(filename), "%s.txt", programs[i]);
        if (fw < 0 || (size_t) fw >= sizeof(filename)) continue;

        char full_path[MAX_BUFFER_SIZE];
        const int pw = snprintf(full_path, sizeof(full_path), "%s/%s", dir, filename);
        if (pw < 0 || (size_t) pw >= sizeof(full_path)) continue;

        if (!file_exist(full_path)) continue;

        snprintf(manual_path, path_size, "%s", full_path);
        LOG_DEBUG(mux_module, "Catalogue manual found: %s", manual_path);
        return 1;
    }

    return 0;
}

int load_video_catalogue(
    const char *catalogue_name, const char *program, const char *program_alt, const char *mux_dim, char *video_path,
    const size_t path_size
) {
    const struct {
        const char *dimension;
        const char *program;
    } args[] = {
        {mux_dim, program},
        {mux_dim, program_alt},
        {"", program},
        {"", program_alt},
    };

    const char *extensions[] = {"mp4"}; // Just MP4 for now...

    for (size_t j = 0; j < A_SIZE(extensions); j++) {
        for (size_t i = 0; i < A_SIZE(args); i++) {
            if (!args[i].program || args[i].program[0] == '\0') continue;

            char dir[MAX_BUFFER_SIZE];
            const int dw = snprintf(dir, sizeof(dir), "%s/%s/video", INFO_CAT_PATH, catalogue_name);
            if (dw < 0 || (size_t) dw >= sizeof(dir)) continue;

            char filename[MAX_BUFFER_SIZE];
            const int fw =
                snprintf(filename, sizeof(filename), "%s%s.%s", args[i].dimension, args[i].program, extensions[j]);
            if (fw < 0 || (size_t) fw >= sizeof(filename)) continue;

            const int pw = snprintf(video_path, path_size, "%s/%s", dir, filename);
            if (pw >= 0 && (size_t) pw < path_size && file_exist(video_path)) return 1;
        }
    }

    return 0;
}
