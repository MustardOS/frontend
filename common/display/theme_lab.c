#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stddef.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <common/runtime/init.h>
#include <common/config/config.h>
#include <common/platform/device.h>
#include <common/display/theme_create.h>
#include <common/display/theme_lab.h>
#include <common/storage/fileio.h>
#include <common/ui/image.h>
#include <common/base/image_ext.h>
#include <common/display/language.h>
#include <common/content/catalogue.h>
#include <common/storage/union.h>
#include <common/ui/font.h>
#include <common/platform/audio.h>
#include <common/platform/display.h>
#include <common/display/theme.h>
#include <common/display/theme_json.h>
#include <json/json.h>
#include <common/storage/archive.h>
#include <common/runtime/exec.h>
#include <common/platform/rgb.h>

static const char *const lab_wall_slots[] = {
    "default", "muxlaunch", "muxplore", "muxcollect", "muxhistory", "muxapp",      "muxconfig",
    "muxinfo", "muxcharge", "bootlogo", "reboot",     "shutdown",   "screensaver", "kiosk",
};
static const int lab_wall_slot_count = (int) A_SIZE(lab_wall_slots);

static const char *const lab_image_exts[] = {"png", "jpg", "jpeg", "bmp", "gif", "webp", "svg"};

static int name_valid(const char *name) {
    return name && *name && name[0] != '.' && !strchr(name, '/');
}

static int has_ext(const char *name, const char *const *exts, const size_t count) {
    const char *dot = strrchr(name, '.');
    if (!dot) return 0;

    for (size_t i = 0; i < count; i++) {
        if (strcasecmp(dot + 1, exts[i]) == 0) return 1;
    }
    return 0;
}

static int is_video(const char *name) {
    static const char *const video[] = {"mp4"};
    return has_ext(name, video, A_SIZE(video));
}

static int lab_wall_slot_valid(const char *slot) {
    for (int i = 0; i < lab_wall_slot_count; i++) {
        if (slot && strcmp(slot, lab_wall_slots[i]) == 0) return 1;
    }
    return 0;
}

static void theme_file(char *out, const size_t size, const char *theme_folder, const char *relative) {
    snprintf(out, size, RUN_STORAGE_PATH "theme/%s/%s", theme_folder, relative);
}

#define LAB_DIMS_MAX 16
#define LAB_DIM_SIZE 16

static char lab_target[LAB_DIM_SIZE];
static char lab_dim[LAB_DIM_SIZE];
static int lab_spread;

void lab_set_target(const char *dim) {
    int width = 0;
    int height = 0;
    char tail = '\0';
    lab_target[0] = '\0';
    lab_spread = 0;
    if (dim && sscanf(dim, "%dx%d%c", &width, &height, &tail) == 2 && width > 0 && height > 0 && width < 10000
        && height < 10000)
        snprintf(lab_target, sizeof(lab_target), "%dx%d/", width, height);
}

static int theme_dims(const char *theme_folder, char (*dims)[LAB_DIM_SIZE], const int max) {
    char base[PATH_MAX];
    snprintf(base, sizeof(base), RUN_STORAGE_PATH "theme/%s", theme_folder);
    DIR *dir = opendir(base);
    if (!dir) return 0;

    int count = 0;
    const struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max) {
        int width = 0;
        int height = 0;
        char tail = '\0';
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", base, entry->d_name);
        if (sscanf(entry->d_name, "%dx%d%c", &width, &height, &tail) == 2 && width > 0 && height > 0 && dir_exist(path))
            snprintf(dims[count++], LAB_DIM_SIZE, "%dx%d/", width, height);
    }
    closedir(dir);
    return count;
}

static int write_dims(const char *theme_folder, char (*dims)[LAB_DIM_SIZE]) {
    if (lab_target[0]) {
        snprintf(dims[0], LAB_DIM_SIZE, "%s", lab_target);
        return 1;
    }
    const int count = theme_dims(theme_folder, dims, LAB_DIMS_MAX);
    if (count) return count;
    dims[0][0] = '\0';
    return 1;
}

static int clear_dims(const char *theme_folder, char (*dims)[LAB_DIM_SIZE], const int spread) {
    if (lab_target[0]) {
        snprintf(dims[0], LAB_DIM_SIZE, "%s", lab_target);
        return 1;
    }
    int count = spread ? theme_dims(theme_folder, dims, LAB_DIMS_MAX - 1) : 0;
    if (!spread && lab_dim[0]) snprintf(dims[count++], LAB_DIM_SIZE, "%s", lab_dim);
    dims[count++][0] = '\0';
    return count;
}

static void dim_size(const char *dim, int *width, int *height) {
    if (sscanf(dim, "%dx%d", width, height) == 2 && *width > 0 && *height > 0) return;
    *width = device.mux.width;
    *height = device.mux.height;
}

static void state_file(char *out, const size_t size, const char *theme_folder, const char *slot) {
    snprintf(out, size, RUN_STORAGE_PATH "theme/%s/.lab/wall/%s.txt", theme_folder, slot);
}

#define LAB_LANGUAGES_MAX  16
#define LAB_CANDIDATES_MAX 64

static int image_languages(const char *theme_folder, const char *dim, char (*languages)[64], const int max) {
    char base[PATH_MAX];
    snprintf(base, sizeof(base), RUN_STORAGE_PATH "theme/%s/%simage", theme_folder, dim);

    DIR *dir = opendir(base);
    if (!dir) return 0;

    int count = 0;
    const struct dirent *entry;
    while (count < max && (entry = readdir(dir)) != NULL) {
        if (!name_valid(entry->d_name)) continue;

        if (strcmp(entry->d_name, "wall") == 0 || strcmp(entry->d_name, "grid") == 0) continue;

        char folder[PATH_MAX];
        snprintf(folder, sizeof(folder), "%s/%s", base, entry->d_name);
        if (dir_exist(folder)) snprintf(languages[count++], 64, "%s", entry->d_name);
    }

    closedir(dir);
    return count;
}

static int screen_slot(const char *slot) {
    static const char *const screens[] = {"bootlogo", "reboot", "shutdown", "screensaver", "kiosk"};
    for (size_t i = 0; i < A_SIZE(screens); i++) {
        if (strcmp(slot, screens[i]) == 0) return 1;
    }
    return 0;
}

static int
wall_candidates(const char *theme_folder, const char *slot, char relative[][MAX_BUFFER_SIZE], const int max) {
    char dims[LAB_DIMS_MAX][LAB_DIM_SIZE];
    const int dim_total = clear_dims(theme_folder, dims, lab_spread);
    static const char *const slot_files[] = {"image/wall/%s.png", "image/wall/%s.svg", "image/wall/%s.mp4"};
    static const char *const every_files[] = {"image/background.mp4", "image/wall.png", "image/wall.svg"};

    int count = 0;
    if (screen_slot(slot)) {
        static const char *const exts[] = {"png", "bmp", "svg", "mp4"};
        for (int d = 0; d < dim_total; d++) {
            for (size_t e = 0; e < A_SIZE(exts) && count < max; e++)
                snprintf(relative[count++], MAX_BUFFER_SIZE, "%simage/%s.%s", dims[d], slot, exts[e]);

            char languages[LAB_LANGUAGES_MAX][64];
            const int language_count = image_languages(theme_folder, dims[d], languages, LAB_LANGUAGES_MAX);
            for (int l = 0; l < language_count && count < max; l++)
                snprintf(relative[count++], MAX_BUFFER_SIZE, "%simage/%s/%s.png", dims[d], languages[l], slot);
        }
        return count;
    }

    for (int d = 0; d < dim_total; d++) {
        for (size_t f = 0; f < A_SIZE(slot_files) && count < max; f++) {
            char leaf[MAX_BUFFER_SIZE];
            snprintf(leaf, sizeof(leaf), slot_files[f], slot);
            snprintf(relative[count++], MAX_BUFFER_SIZE, "%s%s", dims[d], leaf);
        }

        char languages[LAB_LANGUAGES_MAX][64];
        const int language_count = image_languages(theme_folder, dims[d], languages, LAB_LANGUAGES_MAX);
        for (int l = 0; l < language_count; l++) {
            static const char *const language_files[] = {"png", "svg"};
            for (size_t f = 0; f < A_SIZE(language_files) && count < max; f++) {
                snprintf(
                    relative[count++], MAX_BUFFER_SIZE, "%simage/%s/wall/%s.%s", dims[d], languages[l], slot,
                    language_files[f]
                );
            }
        }

        if (strcmp(slot, "default") != 0) continue;
        for (size_t f = 0; f < A_SIZE(every_files) && count < max; f++) {
            snprintf(relative[count++], MAX_BUFFER_SIZE, "%s%s", dims[d], every_files[f]);
        }
    }

    return count;
}

static int lab_written(const char *relative, const char *const *lab_paths) {
    for (size_t i = 0; lab_paths && lab_paths[i]; i++) {
        if (strcmp(relative, lab_paths[i]) == 0) return 1;
    }
    return 0;
}

static void
clear_files(const char *theme_folder, char relative[][MAX_BUFFER_SIZE], const int count, const char *const *lab_paths) {
    for (int i = 0; i < count; i++) {
        char path[PATH_MAX];
        theme_file(path, sizeof(path), theme_folder, relative[i]);
        if (!file_exist(path)) continue;

        char backup[PATH_MAX];
        snprintf(backup, sizeof(backup), RUN_STORAGE_PATH "theme/%s/.lab/original/%s", theme_folder, relative[i]);
        if (!lab_written(relative[i], lab_paths) && !file_exist(backup)) {
            create_directories(backup, 1);
            if (file_rename(path, backup) == 0) continue;
        }
        remove(path);
    }
}

static void restore_files(const char *theme_folder, char relative[][MAX_BUFFER_SIZE], const int count) {
    for (int i = 0; i < count; i++) {
        char backup[PATH_MAX];
        snprintf(backup, sizeof(backup), RUN_STORAGE_PATH "theme/%s/.lab/original/%s", theme_folder, relative[i]);
        if (!file_exist(backup)) continue;

        char path[PATH_MAX];
        theme_file(path, sizeof(path), theme_folder, relative[i]);
        create_directories(path, 1);
        file_rename(backup, path);
    }
}

static void clear_slot(const char *theme_folder, const char *slot, const char *const *lab_paths) {
    char relative[LAB_CANDIDATES_MAX][MAX_BUFFER_SIZE];
    clear_files(
        theme_folder, relative, wall_candidates(theme_folder, slot, relative, (int) A_SIZE(relative)), lab_paths
    );
}

static void restore_slot(const char *theme_folder, const char *slot) {
    char relative[LAB_CANDIDATES_MAX][MAX_BUFFER_SIZE];
    restore_files(theme_folder, relative, wall_candidates(theme_folder, slot, relative, (int) A_SIZE(relative)));
}

static int save_png(SDL_Surface *source, const SDL_Rect *crop, const char *to, const int width, const int height) {
    SDL_Surface *output = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    int ok = output && SDL_SoftStretchLinear(source, crop, output, NULL) == 0;

    char temporary[PATH_MAX];
    snprintf(temporary, sizeof(temporary), "%s.part", to);
    ok = ok && IMG_SavePNG(output, temporary) == 0 && file_rename(temporary, to) == 0;
    if (!ok) remove(temporary);

    SDL_FreeSurface(output);
    return ok;
}

static SDL_Surface *load_argb(const char *from) {
    SDL_Surface *loaded = IMG_Load(from);
    SDL_Surface *source = loaded ? SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_ARGB8888, 0) : NULL;
    SDL_FreeSurface(loaded);
    if (source && (source->w <= 0 || source->h <= 0)) {
        SDL_FreeSurface(source);
        return NULL;
    }
    return source;
}

static int save_cover_png(const char *from, const char *to, const int width, const int height) {
    SDL_Surface *source = load_argb(from);
    if (!source) return 0;

    const double scale_w = (double) width / source->w;
    const double scale_h = (double) height / source->h;
    const double scale = scale_w > scale_h ? scale_w : scale_h;
    const int crop_w = LV_MIN(source->w, (int) (width / scale + 0.5));
    const int crop_h = LV_MIN(source->h, (int) (height / scale + 0.5));
    const SDL_Rect crop = {(source->w - crop_w) / 2, (source->h - crop_h) / 2, crop_w, crop_h};

    const int ok = save_png(source, &crop, to, width, height);
    SDL_FreeSurface(source);
    return ok;
}

static int save_fit_png(const char *from, const char *to, const int limit) {
    SDL_Surface *source = load_argb(from);
    if (!source) return 0;

    const double scale = (double) limit / LV_MAX(source->w, source->h);
    const int width = LV_MAX(1, (int) (source->w * scale + 0.5));
    const int height = LV_MAX(1, (int) (source->h * scale + 0.5));

    const int ok = save_png(source, NULL, to, width, height);
    SDL_FreeSurface(source);
    return ok;
}

static int wall_apply_dim(const char *theme_folder, const char *slot, const char *source, const int had_state) {
    if (!theme_lab_own(theme_folder) || !lab_wall_slot_valid(slot)) return theme_create_name;

    char state[PATH_MAX];
    state_file(state, sizeof(state), theme_folder, slot);

    const int logo = strcmp(slot, "bootlogo") == 0;
    const int screen = screen_slot(slot);
    char picture[MAX_BUFFER_SIZE];
    char video[MAX_BUFFER_SIZE];
    snprintf(picture, sizeof(picture), screen ? "%simage/%s.png" : "%simage/wall/%s.png", lab_dim, slot);
    if (screen) {
        snprintf(video, sizeof(video), "%simage/%s.mp4", lab_dim, slot);
    } else if (strcmp(slot, "default") == 0) {
        snprintf(video, sizeof(video), "%simage/background.mp4", lab_dim);
    } else {
        snprintf(video, sizeof(video), "%simage/wall/%s.mp4", lab_dim, slot);
    }
    const char *const lab_paths[] = {picture, video, NULL};
    const char *const *written = had_state ? lab_paths : NULL;

    if (!source || !*source) {
        clear_slot(theme_folder, slot, written);
        restore_slot(theme_folder, slot);
        remove(state);
        if (logo) write_text_to_file(MUOS_BTL_LOAD, "w", INT, 1);
        wallpaper_forget(NULL);
        return theme_create_ok;
    }

    char from[PATH_MAX];
    snprintf(from, sizeof(from), LAB_DROP_PATH "/%s", source);
    if (!name_valid(source) || !file_exist(from)) return theme_create_name;

    const int is_movie = is_video(source);
    if (screen && is_movie && strcmp(slot, "screensaver") != 0) return theme_create_name;
    char target[PATH_MAX];
    theme_file(target, sizeof(target), theme_folder, is_movie ? video : picture);

    clear_slot(theme_folder, slot, written);
    create_directories(target, 1);

    int width = 0;
    int height = 0;
    dim_size(lab_dim, &width, &height);
    const int ok = is_movie ? copy_file(from, target) == 0 : save_cover_png(from, target, width, height);
    if (!ok) return theme_create_failed;

    if (is_movie && !screen && !config.visual.video_wallpaper) {
        config.visual.video_wallpaper = 1;
        write_text_to_file_atomic(CONF_CONFIG_PATH "visual/video_wallpaper", INT, 1);
    }

    create_directories(state, 1);
    write_text_to_file(state, "w", CHAR, source);
    if (logo) write_text_to_file(MUOS_BTL_LOAD, "w", INT, 1);
    wallpaper_forget(target);
    return theme_create_ok;
}

int lab_wall_apply(const char *theme_folder, const char *slot, const char *source) {
    char state[PATH_MAX];
    state_file(state, sizeof(state), theme_folder, slot);
    const int had_state = file_exist(state);

    char dims[LAB_DIMS_MAX][LAB_DIM_SIZE];
    int total = write_dims(theme_folder, dims);
    lab_spread = source && is_video(source) && !lab_target[0];
    if (lab_spread) {
        dims[0][0] = '\0';
        total = 1;
    }

    int result = theme_create_ok;
    for (int d = 0; d < total && result == theme_create_ok; d++) {
        snprintf(lab_dim, sizeof(lab_dim), "%s", dims[d]);
        result = wall_apply_dim(theme_folder, slot, source, had_state);
    }
    lab_spread = 0;
    return result;
}

static const struct {
    const char *folder;
    size_t offset;
} lab_areas[] = {
    {"header", offsetof(struct mux_lang, muxlabglyph.area.header)},
    {"footer", offsetof(struct mux_lang, muxlabglyph.area.footer)},
    {"bar", offsetof(struct mux_lang, muxlabglyph.area.bar)},
    {"section", offsetof(struct mux_lang, muxlabglyph.area.section)},
    {"muxoption", offsetof(struct mux_lang, muxlabglyph.area.option)},
    {"muxpicker", offsetof(struct mux_lang, muxlabglyph.area.picker)},
    {"muxaccess", offsetof(struct mux_lang, muxlabglyph.area.access)},
    {"muxnews", offsetof(struct mux_lang, muxlabglyph.area.news)},
    {"muxrgbzone", offsetof(struct mux_lang, muxlabglyph.area.rgbzone)},
    {"muxstorage", offsetof(struct mux_lang, muxstorage.title)},
};

static const char *lab_screen_title(const char *folder) {
    for (size_t i = 0; i < A_SIZE(lab_areas); i++) {
        if (strcmp(folder, lab_areas[i].folder) == 0) return (const char *) &lang + lab_areas[i].offset;
    }

    const char *title = lang_module_first(folder);
    return title && *title ? title : folder;
}

static int add_unique(char (*names)[LAB_GLYPH_SIZE], int count, const int max, const char *name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0) return count;
    }
    if (count < max) snprintf(names[count++], LAB_GLYPH_SIZE, "%s", name);
    return count;
}

static int add_entries(
    char (*names)[LAB_GLYPH_SIZE], int count, const int max, const char *folder, const int folders, const int strip
) {
    DIR *dir = opendir(folder);
    if (!dir) return count;

    const struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!name_valid(entry->d_name)) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", folder, entry->d_name);
        if (folders != !!dir_exist(path)) continue;

        char name[LAB_GLYPH_SIZE];
        snprintf(name, sizeof(name), "%s", entry->d_name);
        char *dot = strip ? strrchr(name, '.') : NULL;
        if (strip && !dot) continue;
        if (dot) *dot = '\0';

        count = add_unique(names, count, max, name);
    }

    closedir(dir);
    return count;
}

static int lab_glyph_screens(const char *theme_folder, char (*folders)[LAB_GLYPH_SIZE], const int max) {
    int count = add_entries(folders, 0, max, INTERNAL_THEME "/glyph", 1, 0);

    if (name_valid(theme_folder)) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), RUN_STORAGE_PATH "theme/%s/glyph", theme_folder);
        count = add_entries(folders, count, max, path, 1, 0);
        snprintf(path, sizeof(path), RUN_STORAGE_PATH "theme/%s/%sglyph", theme_folder, mux_dim);
        count = add_entries(folders, count, max, path, 1, 0);
    }

    return count;
}

static void
glyph_state_file(char *out, const size_t size, const char *theme_folder, const char *screen, const char *name) {
    snprintf(out, size, RUN_STORAGE_PATH "theme/%s/.lab/glyph/%s/%s.txt", theme_folder, screen, name);
}

static int glyph_candidates(
    const char *theme_folder, const char *screen, const char *name, char relative[][MAX_BUFFER_SIZE], const int max
) {
    char dims[LAB_DIMS_MAX][LAB_DIM_SIZE];
    const int dim_total = clear_dims(theme_folder, dims, lab_spread);
    int ext_count;
    const char **exts = image_ext_list(&ext_count);

    int count = 0;
    for (int d = 0; d < dim_total; d++) {

        for (int e = 0; e < ext_count && count < max; e++) {
            snprintf(relative[count++], MAX_BUFFER_SIZE, "%sglyph/%s/%s.%s", dims[d], screen, name, exts[e]);
        }

        char languages[LAB_LANGUAGES_MAX][64];
        const int language_count = image_languages(theme_folder, dims[d], languages, LAB_LANGUAGES_MAX);

        static const char *const grid_files[] = {"%s.svg", "%s.png", "%s_focused.svg", "%s_focused.png"};
        for (size_t g = 0; g < A_SIZE(grid_files); g++) {
            char leaf[MAX_BUFFER_SIZE];
            snprintf(leaf, sizeof(leaf), grid_files[g], name);
            if (count < max) snprintf(relative[count++], MAX_BUFFER_SIZE, "%simage/grid/%s/%s", dims[d], screen, leaf);

            for (int l = 0; l < language_count && count < max; l++) {
                snprintf(
                    relative[count++], MAX_BUFFER_SIZE, "%simage/%s/grid/%s/%s", dims[d], languages[l], screen, leaf
                );
            }
        }
    }

    return count;
}

int lab_glyph_apply(const char *theme_folder, const char *screen, const char *name, const char *source) {
    if (!theme_lab_own(theme_folder) || !name_valid(screen) || !name_valid(name)) return theme_create_name;
    snprintf(lab_dim, sizeof(lab_dim), "%s", lab_target);
    lab_spread = 1;

    char state[PATH_MAX];
    glyph_state_file(state, sizeof(state), theme_folder, screen, name);

    char vector_path[MAX_BUFFER_SIZE];
    char raster_path[MAX_BUFFER_SIZE];
    snprintf(vector_path, sizeof(vector_path), "%sglyph/%s/%s.svg", lab_dim, screen, name);
    snprintf(raster_path, sizeof(raster_path), "%sglyph/%s/%s.png", lab_dim, screen, name);
    const char *const lab_paths[] = {vector_path, raster_path, NULL};
    const char *const *written = file_exist(state) ? lab_paths : NULL;

    char relative[LAB_CANDIDATES_MAX * 2][MAX_BUFFER_SIZE];
    const int count = glyph_candidates(theme_folder, screen, name, relative, (int) A_SIZE(relative));

    if (!source || !*source) {
        clear_files(theme_folder, relative, count, written);
        restore_files(theme_folder, relative, count);
        remove(state);
        image_caches_reset();
        return theme_create_ok;
    }

    char from[PATH_MAX];
    snprintf(from, sizeof(from), LAB_GLYPH_PATH "/%s", source);
    if (!name_valid(source) || !file_exist(from)) return theme_create_name;

    static const char *const svg[] = {"svg"};
    const int vector = has_ext(source, svg, A_SIZE(svg));

    char target[PATH_MAX];
    snprintf(
        target, sizeof(target), RUN_STORAGE_PATH "theme/%s/%sglyph/%s/%s.%s", theme_folder, lab_dim, screen, name,
        vector ? "svg" : "png"
    );

    clear_files(theme_folder, relative, count, written);
    create_directories(target, 1);

    const int ok = vector ? copy_file(from, target) == 0 : save_fit_png(from, target, LAB_GLYPH_LIMIT);
    if (!ok) return theme_create_failed;

    create_directories(state, 1);
    write_text_to_file(state, "w", CHAR, source);
    image_caches_reset();
    return theme_create_ok;
}

void lab_export_screens(void) {
    static char written_language[MAX_BUFFER_SIZE] = "";
    if (!lang.muxlabglyph.title[0]) return;
    if (file_exist(LAB_SCREENS) && strcmp(written_language, config.settings.general.language) == 0) return;

    char folders[LAB_SCREENS_MAX][LAB_GLYPH_SIZE];
    const int count = lab_glyph_screens("", folders, LAB_SCREENS_MAX);

    FILE *file = fopen(LAB_SCREENS ".part", "w");
    if (!file) return;

    fprintf(file, "dim|%s\n", mux_dim);
    for (int i = 0; i < count; i++)
        fprintf(file, "%s|%s\n", folders[i], lab_screen_title(folders[i]));

    if (fclose(file) == 0 && file_rename(LAB_SCREENS ".part", LAB_SCREENS) == 0) {
        snprintf(written_language, sizeof(written_language), "%s", config.settings.general.language);
    }
}

static const char *const lab_art_kinds[] = {"Folder", "Application", "Collection"};

static int art_kind_valid(const char *kind) {
    for (size_t i = 0; i < A_SIZE(lab_art_kinds); i++) {
        if (kind && strcmp(kind, lab_art_kinds[i]) == 0) return 1;
    }
    return 0;
}

static int art_type_valid(const char *type) {
    return type && (strcmp(type, "grid") == 0 || strcmp(type, "box") == 0);
}

static void art_state_file(
    char *out, const size_t size, const char *theme_folder, const char *kind, const char *type, const char *name
) {
    snprintf(out, size, RUN_STORAGE_PATH "theme/%s/.lab/art/%s/%s/%s.txt", theme_folder, kind, type, name);
}

static int art_candidates(
    const char *theme_folder, const char *kind, const char *type, const char *name, char relative[][MAX_BUFFER_SIZE],
    const int max
) {
    char dims[LAB_DIMS_MAX][LAB_DIM_SIZE];
    const int dim_total = clear_dims(theme_folder, dims, 1);

    int ext_count;
    const char **exts = image_ext_list(&ext_count);
    static const char *const suffixes[] = {"", "_focused"};

    int count = 0;
    for (int d = 0; d < dim_total; d++) {
        char folder[LAB_DIM_SIZE + 1];
        snprintf(folder, sizeof(folder), "%s", dims[d]);
        for (size_t x = 0; x < A_SIZE(suffixes); x++) {
            for (int e = 0; e < ext_count && count < max; e++) {
                snprintf(
                    relative[count++], MAX_BUFFER_SIZE, "catalogue/%s/%s/%s%s%s.%s", kind, type, folder, name,
                    suffixes[x], exts[e]
                );
            }
        }
    }

    return count;
}

int lab_art_apply(const char *theme_folder, const char *kind, const char *type, const char *name, const char *source) {
    if (!theme_lab_own(theme_folder) || !art_kind_valid(kind) || !art_type_valid(type) || !name_valid(name)) {
        return theme_create_name;
    }

    char state[PATH_MAX];
    art_state_file(state, sizeof(state), theme_folder, kind, type, name);

    char vector_path[MAX_BUFFER_SIZE];
    char raster_path[MAX_BUFFER_SIZE];
    snprintf(vector_path, sizeof(vector_path), "catalogue/%s/%s/%s%s.svg", kind, type, lab_target, name);
    snprintf(raster_path, sizeof(raster_path), "catalogue/%s/%s/%s%s.png", kind, type, lab_target, name);
    const char *const lab_paths[] = {vector_path, raster_path, NULL};
    const char *const *written = file_exist(state) ? lab_paths : NULL;

    char relative[LAB_CANDIDATES_MAX][MAX_BUFFER_SIZE];
    const int count = art_candidates(theme_folder, kind, type, name, relative, (int) A_SIZE(relative));

    if (!source || !*source) {
        clear_files(theme_folder, relative, count, written);
        restore_files(theme_folder, relative, count);
        remove(state);
        invalidate_catalogue_cache();
        image_caches_reset();
        return theme_create_ok;
    }

    char from[PATH_MAX];
    snprintf(from, sizeof(from), LAB_ART_PATH "/%s", source);
    if (!name_valid(source) || !file_exist(from)) return theme_create_name;

    static const char *const svg[] = {"svg"};
    const int vector = has_ext(source, svg, A_SIZE(svg));

    char target[PATH_MAX];
    theme_file(target, sizeof(target), theme_folder, vector ? vector_path : raster_path);

    clear_files(theme_folder, relative, count, written);
    create_directories(target, 1);

    const int limit = strcmp(type, "box") == 0 ? LAB_BOX_LIMIT : LAB_GRID_LIMIT;
    const int ok = vector ? copy_file(from, target) == 0 : save_fit_png(from, target, limit);
    if (!ok) return theme_create_failed;

    create_directories(state, 1);
    write_text_to_file(state, "w", CHAR, source);
    invalidate_catalogue_cache();
    image_caches_reset();
    return theme_create_ok;
}

static const char *const lab_font_slots[] = {"default", "header", "footer", "panel"};
static const int lab_font_slot_count = (int) A_SIZE(lab_font_slots);

static const char *const lab_font_exts[] = {"ttf", "otf", "ttc", "pcf", "bdf"};

static int lab_font_slot_valid(const char *slot) {
    for (int i = 0; i < lab_font_slot_count; i++) {
        if (slot && strcmp(slot, lab_font_slots[i]) == 0) return 1;
    }
    return 0;
}

static void font_state_file(char *out, const size_t size, const char *theme_folder, const char *slot) {
    snprintf(out, size, RUN_STORAGE_PATH "theme/%s/.lab/font/%s.txt", theme_folder, slot);
}

static int font_file(const char *name) {
    static const char *const compiled[] = {"bin"};
    return has_ext(name, lab_font_exts, A_SIZE(lab_font_exts)) || has_ext(name, compiled, A_SIZE(compiled));
}

static int add_font_files(
    const char *theme_folder, const char *root, const char *folder, char relative[][MAX_BUFFER_SIZE], int count,
    const int max
) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), RUN_STORAGE_PATH "theme/%s/%s%s", theme_folder, root, folder);

    DIR *dir = opendir(path);
    if (!dir) return count;

    const struct dirent *entry;
    while (count < max && (entry = readdir(dir)) != NULL) {
        if (!name_valid(entry->d_name) || !font_file(entry->d_name)) continue;

        char candidate[MAX_BUFFER_SIZE];
        snprintf(candidate, sizeof(candidate), "%s%s", folder, entry->d_name);

        int seen = 0;
        for (int i = 0; i < count && !seen; i++)
            seen = strcmp(relative[i], candidate) == 0;
        if (!seen) snprintf(relative[count++], MAX_BUFFER_SIZE, "%s", candidate);
    }

    closedir(dir);
    return count;
}

static int font_languages(const char *theme_folder, const char *dim, char (*languages)[64], const int max) {
    char base[PATH_MAX];
    snprintf(base, sizeof(base), RUN_STORAGE_PATH "theme/%s/%sfont", theme_folder, dim);

    DIR *dir = opendir(base);
    if (!dir) return 0;

    int count = 0;
    const struct dirent *entry;
    while (count < max && (entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (!name_valid(name) || lab_font_slot_valid(name) || strcmp(name, "grid") == 0) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", base, name);
        if (dir_exist(path)) snprintf(languages[count++], 64, "%s", name);
    }

    closedir(dir);
    return count;
}

static int
font_candidates(const char *theme_folder, const char *slot, char relative[][MAX_BUFFER_SIZE], const int max) {
    char dims[LAB_DIMS_MAX][LAB_DIM_SIZE];
    const int dim_total = clear_dims(theme_folder, dims, 1);
    const char *roots[] = {"", ".lab/original/"};
    const int every = strcmp(slot, "default") == 0;

    int count = 0;
    for (int d = 0; d < dim_total; d++) {

        char languages[LAB_LANGUAGES_MAX][64];
        const int language_count = font_languages(theme_folder, dims[d], languages, LAB_LANGUAGES_MAX);

        for (int l = -1; l < language_count; l++) {
            for (int grid = 0; grid < 2; grid++) {
                char folder[MAX_BUFFER_SIZE];
                snprintf(
                    folder, sizeof(folder), "%sfont/%s%s%s%s%s", dims[d], l < 0 ? "" : languages[l], l < 0 ? "" : "/",
                    every ? "" : slot, every ? "" : "/", grid ? "grid/" : ""
                );

                for (size_t r = 0; r < A_SIZE(roots); r++)
                    count = add_font_files(theme_folder, roots[r], folder, relative, count, max);
            }
        }
    }

    return count;
}

int lab_font_apply(const char *theme_folder, const char *slot, const char *source) {
    if (!theme_lab_own(theme_folder) || !lab_font_slot_valid(slot)) return theme_create_name;

    char state[PATH_MAX];
    font_state_file(state, sizeof(state), theme_folder, slot);

    const int every = strcmp(slot, "default") == 0;
    char lab_files[A_SIZE(lab_font_exts)][MAX_BUFFER_SIZE];
    const char *lab_paths[A_SIZE(lab_font_exts) + 1];
    for (size_t i = 0; i < A_SIZE(lab_font_exts); i++) {
        snprintf(
            lab_files[i], sizeof(lab_files[i]), "%sfont/%s%sdefault.%s", lab_target, every ? "" : slot,
            every ? "" : "/", lab_font_exts[i]
        );
        lab_paths[i] = lab_files[i];
    }
    lab_paths[A_SIZE(lab_font_exts)] = NULL;
    const char *const *written = file_exist(state) ? lab_paths : NULL;

    char relative[LAB_CANDIDATES_MAX][MAX_BUFFER_SIZE];
    const int count = font_candidates(theme_folder, slot, relative, (int) A_SIZE(relative));

    if (!source || !*source) {
        clear_files(theme_folder, relative, count, written);
        restore_files(theme_folder, relative, count);
        remove(state);
        font_context_bump();
        return theme_create_ok;
    }

    char from[PATH_MAX];
    snprintf(from, sizeof(from), LAB_FONT_PATH "/%s", source);
    if (!name_valid(source) || !file_exist(from) || !has_ext(source, lab_font_exts, A_SIZE(lab_font_exts))) {
        return theme_create_name;
    }

    const char *dot = strrchr(source, '.');
    char target_relative[MAX_BUFFER_SIZE];
    snprintf(
        target_relative, sizeof(target_relative), "%sfont/%s%sdefault.%s", lab_target, every ? "" : slot,
        every ? "" : "/", dot + 1
    );
    for (char *c = strrchr(target_relative, '.'); c && *c; c++)
        *c = (char) tolower((unsigned char) *c);

    char target[PATH_MAX];
    theme_file(target, sizeof(target), theme_folder, target_relative);

    clear_files(theme_folder, relative, count, written);
    create_directories(target, 1);
    if (copy_file(from, target) != 0) return theme_create_failed;

    create_directories(state, 1);
    write_text_to_file(state, "w", CHAR, source);

    if (config.settings.advanced.font != 1) {
        config.settings.advanced.font = 1;
        write_text_to_file_atomic(CONF_CONFIG_PATH "settings/advanced/font", INT, 1);
    }

    font_context_bump();
    return theme_create_ok;
}

extern char **environ;

static int run_lab_script(const char *action, const char *source, const char *target) {
    char *const args[] = {(char *) "/bin/sh", (char *) LAB_SCRIPT, (char *) action,
                          (char *) source,    (char *) target,     NULL};

    pid_t pid;
    if (posix_spawn(&pid, args[0], NULL, NULL, args, environ) != 0) return -1;

    int status;
    if (waitpid(pid, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int lab_share_export(const char *theme_folder, char *out, const size_t out_size) {
    if (!theme_lab_own(theme_folder)) return theme_create_name;

    char package[PATH_MAX];
    snprintf(package, sizeof(package), LAB_SHARE_PATH "/%s.muxthm", theme_folder);

    const int code = run_lab_script("export", theme_folder, package);
    if (code != 0) return theme_create_failed;

    snprintf(out, out_size, "%s.muxthm", theme_folder);
    return theme_create_ok;
}

int lab_share_import(const char *package, char *out, const size_t out_size) {
    static const char *const muxthm[] = {"muxthm"};
    if (!name_valid(package) || !has_ext(package, muxthm, A_SIZE(muxthm))) return theme_create_name;

    char archive[PATH_MAX];
    snprintf(archive, sizeof(archive), LAB_SHARE_PATH "/%s", package);
    if (!file_exist(archive)) return theme_create_name;

    char base[LAB_NAME_SIZE];
    snprintf(base, sizeof(base), "%s", package);
    *strrchr(base, '.') = '\0';

    char folder[LAB_NAME_SIZE];
    char path[PATH_MAX];
    int found = 0;
    for (int copy = 1; copy < 100 && !found; copy++) {
        if (copy == 1) {
            snprintf(folder, sizeof(folder), "%s", base);
        } else {
            snprintf(folder, sizeof(folder), "%.200s %d", base, copy);
        }
        snprintf(path, sizeof(path), RUN_STORAGE_PATH "theme/%s", folder);
        found = !dir_exist(path) && strcasecmp(folder, "MustardOS") != 0;
    }
    if (!found) return theme_create_exists;

    const int code = run_lab_script("import", archive, folder);
    if (code == 5) return theme_create_unsafe;
    if (code != 0) return theme_create_failed;

    snprintf(out, out_size, "%s", folder);
    return theme_create_ok;
}

int lab_theme_set_credit(const char *theme_folder, const char *credit) {
    if (!theme_lab_own(theme_folder) || !credit) return theme_create_name;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), RUN_STORAGE_PATH "theme/%s/credits.txt", theme_folder);

    char *rest = NULL;
    FILE *file = fopen(path, "r");
    if (file) {
        char first[MAX_BUFFER_SIZE];
        if (fgets(first, sizeof(first), file)) {
            fseek(file, 0, SEEK_END);
            const long end = ftell(file);
            const long start = (long) strlen(first);
            if (end > start) {
                rest = malloc((size_t) (end - start) + 1);
                if (rest) {
                    fseek(file, start, SEEK_SET);
                    const size_t read = fread(rest, 1, (size_t) (end - start), file);
                    rest[read] = '\0';
                }
            }
        }
        fclose(file);
    }

    char line[MAX_BUFFER_SIZE];
    snprintf(line, sizeof(line), "%s", credit);
    line[strcspn(line, "\r\n")] = '\0';

    const size_t length = strlen(line) + 2 + (rest ? strlen(rest) : 0);
    char *text = malloc(length);
    if (!text) {
        free(rest);
        return theme_create_failed;
    }
    snprintf(text, length, "%s\n%s", line, rest ? rest : "");
    free(rest);

    const int ok = write_text_to_file_atomic(path, CHAR, text);
    free(text);
    return ok ? theme_create_ok : theme_create_failed;
}

static int media_kind(const char *name, const char *kind) {
    if (strcmp(kind, "music") == 0) return bgm_supported(name);

    const char *dot = strrchr(name, '.');
    return dot && strcasecmp(dot + 1, kind) == 0;
}

static void media_state_file(char *out, const size_t size, const char *theme_folder, const char *slot) {
    snprintf(out, size, RUN_STORAGE_PATH "theme/%s/.lab/media/%s.txt", theme_folder, slot);
}

static int media_slot_valid(const char *slot) {
    for (int i = 0; i < sound_total; i++) {
        if (slot && strcmp(slot, snd_names[i]) == 0) return 1;
    }
    return 0;
}

static int add_media_files(
    const char *theme_folder, const char *root, const char *folder, const char *match, const char *kind,
    char relative[][MAX_BUFFER_SIZE], int count, const int max
) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), RUN_STORAGE_PATH "theme/%s/%s%s", theme_folder, root, folder);

    DIR *dir = opendir(path);
    if (!dir) return count;

    const struct dirent *entry;
    while (count < max && (entry = readdir(dir)) != NULL) {
        if (!name_valid(entry->d_name)) continue;
        if (match ? strcasecmp(entry->d_name, match) != 0 : !media_kind(entry->d_name, kind)) continue;

        char candidate[MAX_BUFFER_SIZE];
        snprintf(candidate, sizeof(candidate), "%s%s", folder, entry->d_name);

        int seen = 0;
        for (int i = 0; i < count && !seen; i++)
            seen = strcmp(relative[i], candidate) == 0;
        if (!seen) snprintf(relative[count++], MAX_BUFFER_SIZE, "%s", candidate);
    }

    closedir(dir);
    return count;
}

static void lab_media_refresh(void) {
    init_fe_snd(&fe_snd, config.settings.general.sound, 1);
    init_fe_bgm(&fe_bgm, config.settings.general.bgm, 1);
    set_nav_volume(config.settings.general.soundvol);
    set_bgm_volume(config.settings.general.bgmvol);
}

int lab_media_apply(const char *theme_folder, const char *slot, const char *source) {
    if (!theme_lab_own(theme_folder) || !media_slot_valid(slot)) return theme_create_name;

    char state[PATH_MAX];
    media_state_file(state, sizeof(state), theme_folder, slot);

    char lab_file[MAX_BUFFER_SIZE];
    snprintf(lab_file, sizeof(lab_file), "sound/%s.wav", slot);
    const char *const lab_paths[] = {lab_file, NULL};
    const char *const *written = file_exist(state) ? lab_paths : NULL;

    char match[MAX_BUFFER_SIZE];
    snprintf(match, sizeof(match), "%s.wav", slot);

    char relative[LAB_CANDIDATES_MAX][MAX_BUFFER_SIZE];
    int count = 0;
    const char *roots[] = {"", ".lab/original/"};
    for (size_t r = 0; r < A_SIZE(roots); r++)
        count =
            add_media_files(theme_folder, roots[r], "sound/", match, "wav", relative, count, (int) A_SIZE(relative));

    if (!source || !*source) {
        clear_files(theme_folder, relative, count, written);
        restore_files(theme_folder, relative, count);
        remove(state);
        lab_media_refresh();
        return theme_create_ok;
    }

    char from[PATH_MAX];
    snprintf(from, sizeof(from), LAB_MEDIA_PATH "/%s", source);
    if (!name_valid(source) || !media_kind(source, "wav") || !file_exist(from)) return theme_create_name;

    char target[PATH_MAX];
    theme_file(target, sizeof(target), theme_folder, lab_file);

    clear_files(theme_folder, relative, count, written);
    create_directories(target, 1);
    if (copy_file(from, target) != 0) return theme_create_failed;

    create_directories(state, 1);
    write_text_to_file(state, "w", CHAR, source);

    if (config.settings.general.sound != 2) {
        config.settings.general.sound = 2;
        write_text_to_file_atomic(CONF_CONFIG_PATH "settings/general/sound", INT, 2);
    }

    lab_media_refresh();
    return theme_create_ok;
}

static int lab_music_tracks(const char *theme_folder, char (*tracks)[LAB_NAME_SIZE], const int max) {
    if (!name_valid(theme_folder)) return 0;

    char path[PATH_MAX];
    media_state_file(path, sizeof(path), theme_folder, "music");

    FILE *file = fopen(path, "r");
    if (!file) return 0;

    int count = 0;
    char line[LAB_NAME_SIZE];
    while (count < max && fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (name_valid(line)) snprintf(tracks[count++], LAB_NAME_SIZE, "%s", line);
    }

    fclose(file);
    return count;
}

int lab_music_apply(const char *theme_folder, char (*tracks)[LAB_NAME_SIZE], const int track_count) {
    if (!theme_lab_own(theme_folder)) return theme_create_name;

    for (int i = 0; i < track_count; i++) {
        char from[PATH_MAX];
        snprintf(from, sizeof(from), LAB_MEDIA_PATH "/%s", tracks[i]);
        if (!name_valid(tracks[i]) || !bgm_supported(tracks[i]) || !file_exist(from)) return theme_create_name;
    }

    char state[PATH_MAX];
    media_state_file(state, sizeof(state), theme_folder, "music");

    char previous[LAB_FILES_MAX][LAB_NAME_SIZE];
    const int previous_count = lab_music_tracks(theme_folder, previous, LAB_FILES_MAX);

    char lab_files[LAB_FILES_MAX][MAX_BUFFER_SIZE];
    const char *lab_paths[LAB_FILES_MAX + 1];
    for (int i = 0; i < previous_count; i++) {
        snprintf(lab_files[i], sizeof(lab_files[i]), "music/%s", previous[i]);
        lab_paths[i] = lab_files[i];
    }
    lab_paths[previous_count] = NULL;
    const char *const *written = file_exist(state) ? lab_paths : NULL;

    char relative[LAB_CANDIDATES_MAX][MAX_BUFFER_SIZE];
    int count = 0;
    const char *roots[] = {"", ".lab/original/"};
    for (size_t r = 0; r < A_SIZE(roots); r++)
        count =
            add_media_files(theme_folder, roots[r], "music/", NULL, "music", relative, count, (int) A_SIZE(relative));

    clear_files(theme_folder, relative, count, written);

    if (!track_count) {
        restore_files(theme_folder, relative, count);
        remove(state);
        lab_media_refresh();
        return theme_create_ok;
    }

    FILE *list = NULL;
    create_directories(state, 1);
    list = fopen(state, "w");
    if (!list) return theme_create_failed;

    int ok = 1;
    for (int i = 0; i < track_count && ok; i++) {
        char from[PATH_MAX];
        char target[PATH_MAX];
        char music_file[MAX_BUFFER_SIZE];
        snprintf(from, sizeof(from), LAB_MEDIA_PATH "/%s", tracks[i]);
        snprintf(music_file, sizeof(music_file), "music/%s", tracks[i]);
        theme_file(target, sizeof(target), theme_folder, music_file);

        create_directories(target, 1);
        ok = copy_file(from, target) == 0;
        if (ok) fprintf(list, "%s\n", tracks[i]);
    }
    fclose(list);
    if (!ok) return theme_create_failed;

    if (config.settings.general.bgm != 2) {
        config.settings.general.bgm = 2;
        write_text_to_file_atomic(CONF_CONFIG_PATH "settings/general/bgm", INT, 2);
    }

    lab_media_refresh();
    return theme_create_ok;
}

static const char *const lab_preview_names[] = {"preview", "preview.0", "preview.1"};

static void preview_state_file(char *out, const size_t size, const char *theme_folder, const int slot) {
    snprintf(out, size, RUN_STORAGE_PATH "theme/%s/.lab/preview/%d.txt", theme_folder, slot);
}

static int preview_source_path(const char *source, char *out, const size_t size) {
    const size_t prefix = strlen(LAB_SHOT_PREFIX);

    if (strcmp(source, LAB_SCREEN_SOURCE) == 0) {
        snprintf(out, size, "%s", LAB_SCREEN_CAPTURE);
    } else if (strncmp(source, LAB_SHOT_PREFIX, prefix) == 0) {
        if (!name_valid(source + prefix)) return 0;
        snprintf(out, size, STORAGE_SHOTS "/%s", source + prefix);
    } else {
        if (!name_valid(source) || is_video(source)) return 0;
        snprintf(out, size, LAB_DROP_PATH "/%s", source);
    }

    return file_exist(out);
}

static int preview_apply_dim(const char *theme_folder, const int slot, const char *source, const int had_state) {
    char state[PATH_MAX];
    preview_state_file(state, sizeof(state), theme_folder, slot);

    char dims[LAB_DIMS_MAX][LAB_DIM_SIZE];
    const int dim_total = clear_dims(theme_folder, dims, 0);
    char relative[LAB_DIMS_MAX][MAX_BUFFER_SIZE];
    for (int d = 0; d < dim_total; d++)
        snprintf(relative[d], MAX_BUFFER_SIZE, "%s%s.png", dims[d], lab_preview_names[slot]);

    const char *const lab_paths[] = {relative[0], NULL};
    const char *const *written = had_state ? lab_paths : NULL;

    if (!source || !*source) {
        clear_files(theme_folder, relative, dim_total, written);
        restore_files(theme_folder, relative, dim_total);
        remove(state);
        image_caches_reset();
        return theme_create_ok;
    }

    char from[PATH_MAX];
    if (!preview_source_path(source, from, sizeof(from))) return theme_create_name;

    char target[PATH_MAX];
    theme_file(target, sizeof(target), theme_folder, relative[0]);

    int width = 0;
    int height = 0;
    dim_size(lab_dim, &width, &height);
    clear_files(theme_folder, relative, dim_total, written);
    create_directories(target, 1);
    if (!save_cover_png(from, target, width, height)) return theme_create_failed;

    create_directories(state, 1);
    write_text_to_file(state, "w", CHAR, source);
    image_caches_reset();
    return theme_create_ok;
}

int lab_preview_apply(const char *theme_folder, const int slot, const char *source) {
    if (!theme_lab_own(theme_folder) || slot < 0 || slot >= LAB_PREVIEWS) return theme_create_name;

    char state[PATH_MAX];
    preview_state_file(state, sizeof(state), theme_folder, slot);
    const int had_state = file_exist(state);

    char dims[LAB_DIMS_MAX][LAB_DIM_SIZE];
    const int total = write_dims(theme_folder, dims);
    int result = theme_create_ok;
    for (int d = 0; d < total && result == theme_create_ok; d++) {
        snprintf(lab_dim, sizeof(lab_dim), "%s", dims[d]);
        result = preview_apply_dim(theme_folder, slot, source, had_state);
    }
    return result;
}

static const char *const lab_overlay_slots[] = {"screen", "game", "battery", "bright", "volume"};
static const int lab_overlay_slot_count = (int) A_SIZE(lab_overlay_slots);

#define LAB_OVERLAY_STEPS      10
#define LAB_OVERLAY_CANDIDATES 96

static int lab_overlay_slot_valid(const char *slot) {
    for (int i = 0; i < lab_overlay_slot_count; i++) {
        if (slot && strcmp(slot, lab_overlay_slots[i]) == 0) return 1;
    }
    return 0;
}

static int lab_overlay_indicator(const char *slot) {
    return slot && (strcmp(slot, "battery") == 0 || strcmp(slot, "bright") == 0 || strcmp(slot, "volume") == 0);
}

static int overlay_picture(const char *name) {
    return name_valid(name) && has_ext(name, lab_image_exts, A_SIZE(lab_image_exts));
}

static int set_name(const char *source, char *out, const size_t size) {
    const size_t length = source ? strlen(source) : 0;
    if (length < 2 || source[length - 1] != '/') return 0;

    snprintf(out, size, "%.*s", (int) (length - 1), source);
    return name_valid(out);
}

static int set_step_file(const char *set, const char *type, const char *step, char *out, const size_t size) {
    const int flat_ok = strcmp(step, "default") != 0;

    for (size_t e = 0; e < A_SIZE(lab_image_exts); e++) {
        snprintf(out, size, LAB_OVERLAY_PATH "/%s/%s/%s.%s", set, type, step, lab_image_exts[e]);
        if (file_exist(out)) return 1;

        if (!flat_ok) continue;
        snprintf(out, size, LAB_OVERLAY_PATH "/%s/%s.%s", set, step, lab_image_exts[e]);
        if (file_exist(out)) return 1;
    }
    return 0;
}

static int lab_overlay_set_steps(const char *set, const char *type) {
    if (!name_valid(set) || !lab_overlay_indicator(type)) return 0;

    int found = 0;
    for (int n = 0; n < LAB_OVERLAY_STEPS; n++) {
        char step[64];
        char path[PATH_MAX];
        snprintf(step, sizeof(step), "%s_%d", type, n);
        found += set_step_file(set, type, step, path, sizeof(path));
    }

    char path[PATH_MAX];
    found += set_step_file(set, type, "default", path, sizeof(path));
    return found;
}

static void overlay_state_file(char *out, const size_t size, const char *theme_folder, const char *slot) {
    snprintf(out, size, RUN_STORAGE_PATH "theme/%s/.lab/overlay/%s.txt", theme_folder, slot);
}

static int
overlay_candidates(const char *theme_folder, const char *slot, char relative[][MAX_BUFFER_SIZE], const int max) {
    int ext_count;
    const char **exts = overlay_ext_list(&ext_count);

    char dims[LAB_DIMS_MAX][LAB_DIM_SIZE];
    const int dim_total = clear_dims(theme_folder, dims, lab_overlay_indicator(slot));

    int count = 0;
    for (int d = 0; d < dim_total; d++) {
        for (int e = 0; e < ext_count; e++) {
            if (strcmp(slot, "screen") == 0) {
                if (count < max) snprintf(relative[count++], MAX_BUFFER_SIZE, "%simage/overlay.%s", dims[d], exts[e]);
            } else if (strcmp(slot, "game") == 0) {
                if (count < max) {
                    snprintf(relative[count++], MAX_BUFFER_SIZE, "%soverlay/base/default.%s", dims[d], exts[e]);
                }
            } else {
                for (int n = 0; n <= LAB_OVERLAY_STEPS && count < max; n++) {
                    if (n == LAB_OVERLAY_STEPS) {
                        snprintf(relative[count++], MAX_BUFFER_SIZE, "%soverlay/%s/default.%s", dims[d], slot, exts[e]);
                    } else {
                        snprintf(
                            relative[count++], MAX_BUFFER_SIZE, "%soverlay/%s/%s_%d.%s", dims[d], slot, slot, n, exts[e]
                        );
                    }
                }
            }
        }
    }
    return count;
}

static int save_same_png(const char *from, const char *to) {
    SDL_Surface *source = load_argb(from);
    if (!source) return 0;

    const int ok = save_png(source, NULL, to, source->w, source->h);
    SDL_FreeSurface(source);
    return ok;
}

static void lab_overlay_refresh(void) {
    wallpaper_forget(NULL);
    display_clear_theme_overlay();
    load_overlay_image_sdl();
}

static int overlay_apply_dim(const char *theme_folder, const char *slot, const char *source, const int had_state) {
    const int indicator = lab_overlay_indicator(slot);
    char state[PATH_MAX];
    overlay_state_file(state, sizeof(state), theme_folder, slot);

    char lab_files[LAB_OVERLAY_STEPS + 1][MAX_BUFFER_SIZE];
    const char *lab_paths[LAB_OVERLAY_STEPS + 2] = {NULL};
    int lab_count = 0;
    if (strcmp(slot, "screen") == 0) {
        snprintf(lab_files[lab_count++], MAX_BUFFER_SIZE, "%simage/overlay.png", lab_dim);
    } else if (strcmp(slot, "game") == 0) {
        snprintf(lab_files[lab_count++], MAX_BUFFER_SIZE, "%soverlay/base/default.png", lab_dim);
    } else {
        for (int n = 0; n < LAB_OVERLAY_STEPS; n++)
            snprintf(lab_files[lab_count++], MAX_BUFFER_SIZE, "%soverlay/%s/%s_%d.png", lab_dim, slot, slot, n);
        snprintf(lab_files[lab_count++], MAX_BUFFER_SIZE, "%soverlay/%s/default.png", lab_dim, slot);
    }
    for (int i = 0; i < lab_count; i++)
        lab_paths[i] = lab_files[i];
    const char *const *written = had_state ? lab_paths : NULL;

    char relative[LAB_OVERLAY_CANDIDATES][MAX_BUFFER_SIZE];
    const int count = overlay_candidates(theme_folder, slot, relative, LAB_OVERLAY_CANDIDATES);

    if (!source || !*source) {
        clear_files(theme_folder, relative, count, written);
        restore_files(theme_folder, relative, count);
        remove(state);
        lab_overlay_refresh();
        return theme_create_ok;
    }

    char set[LAB_NAME_SIZE];
    const int is_set = set_name(source, set, sizeof(set));
    if (is_set && (!indicator || !lab_overlay_set_steps(set, slot))) return theme_create_name;

    char from[PATH_MAX];
    snprintf(from, sizeof(from), LAB_OVERLAY_PATH "/%s", source);
    if (!is_set && (!overlay_picture(source) || !file_exist(from))) return theme_create_name;

    clear_files(theme_folder, relative, count, written);

    int ok = 1;
    char target[PATH_MAX];
    if (is_set) {
        char steps[LAB_OVERLAY_STEPS + 1][PATH_MAX];
        int have[LAB_OVERLAY_STEPS + 1] = {0};
        for (int n = 0; n <= LAB_OVERLAY_STEPS; n++) {
            char step[64];
            snprintf(step, sizeof(step), n == LAB_OVERLAY_STEPS ? "default" : "%s_%d", slot, n);
            have[n] = set_step_file(set, slot, step, steps[n], sizeof(steps[n]));
        }

        for (int n = 0; n <= LAB_OVERLAY_STEPS && ok; n++) {
            int use = n;
            for (int k = 1; n < LAB_OVERLAY_STEPS && !have[use] && k < LAB_OVERLAY_STEPS; k++) {
                if (n - k >= 0 && have[n - k])
                    use = n - k;
                else if (n + k < LAB_OVERLAY_STEPS && have[n + k])
                    use = n + k;
            }
            if (!have[use]) continue;

            char file[MAX_BUFFER_SIZE];
            if (n == LAB_OVERLAY_STEPS) {
                snprintf(file, sizeof(file), "%soverlay/%s/default.png", lab_dim, slot);
            } else {
                snprintf(file, sizeof(file), "%soverlay/%s/%s_%d.png", lab_dim, slot, slot, n);
            }
            theme_file(target, sizeof(target), theme_folder, file);
            create_directories(target, 1);
            ok = save_same_png(steps[use], target);
        }
    } else {
        theme_file(target, sizeof(target), theme_folder, lab_files[indicator ? LAB_OVERLAY_STEPS : 0]);
        create_directories(target, 1);
        int width = 0;
        int height = 0;
        dim_size(lab_dim, &width, &height);
        ok = indicator ? save_same_png(from, target) : save_cover_png(from, target, width, height);
    }
    if (!ok) return theme_create_failed;

    create_directories(state, 1);
    write_text_to_file(state, "w", CHAR, source);

    if (strcmp(slot, "screen") == 0 && config.visual.overlay_image != 1) {
        config.visual.overlay_image = 1;
        write_text_to_file_atomic(CONF_CONFIG_PATH "visual/overlayimage", INT, 1);
    }

    lab_overlay_refresh();
    return theme_create_ok;
}

int lab_overlay_apply(const char *theme_folder, const char *slot, const char *source) {
    if (!theme_lab_own(theme_folder) || !lab_overlay_slot_valid(slot)) return theme_create_name;

    char state[PATH_MAX];
    overlay_state_file(state, sizeof(state), theme_folder, slot);
    const int had_state = file_exist(state);

    if (lab_overlay_indicator(slot)) {
        snprintf(lab_dim, sizeof(lab_dim), "%s", lab_target);
        return overlay_apply_dim(theme_folder, slot, source, had_state);
    }

    char dims[LAB_DIMS_MAX][LAB_DIM_SIZE];
    const int total = write_dims(theme_folder, dims);
    int result = theme_create_ok;
    for (int d = 0; d < total && result == theme_create_ok; d++) {
        snprintf(lab_dim, sizeof(lab_dim), "%s", dims[d]);
        result = overlay_apply_dim(theme_folder, slot, source, had_state);
    }
    return result;
}

static int scheme_name_valid(const char *text, const int upper) {
    if (!text || !*text || strlen(text) > 64) return 0;
    for (const char *c = text; *c; c++) {
        if (!(upper ? isupper((unsigned char) *c) : islower((unsigned char) *c)) && !isdigit((unsigned char) *c)
            && *c != '_')
            return 0;
    }
    return 1;
}

static int scheme_screen_valid(const char *screen) {
    return !*screen || (strncmp(screen, "mux", 3) == 0 && scheme_name_valid(screen, 0));
}

static int scheme_targets(
    const char *theme_folder, char (*out)[MAX_BUFFER_SIZE], const int max, char *variant, const size_t variant_size,
    char *prefix
) {
    char base[PATH_MAX];
    char path[PATH_MAX];
    prefix[0] = '\0';
    snprintf(base, sizeof(base), RUN_STORAGE_PATH "theme/%s", theme_folder);
    variant[0] = '\0';

    char name[MAX_BUFFER_SIZE];
    if (get_active_alternate(name, sizeof(name)) && name_valid(name)) {
        snprintf(variant, variant_size, "%s", name);
        snprintf(path, sizeof(path), "%s/alternate/%s", base, name);
        if (!dir_exist(path) && !lab_target[0])
            return snprintf(out[0], MAX_BUFFER_SIZE, "alternate/%s.json", name) < MAX_BUFFER_SIZE;
        snprintf(prefix, MAX_BUFFER_SIZE, "alternate/%s/", name);
    }

    if (lab_target[0]) return snprintf(out[0], MAX_BUFFER_SIZE, "%s%stheme.json", prefix, lab_target) < MAX_BUFFER_SIZE;

    int count = 0;
    snprintf(out[count++], MAX_BUFFER_SIZE, "%stheme.json", prefix);

    char dims[LAB_DIMS_MAX][LAB_DIM_SIZE];
    const int dim_total = theme_dims(theme_folder, dims, LAB_DIMS_MAX);
    for (int d = 0; d < dim_total && count < max; d++) {
        snprintf(path, sizeof(path), "%s/%s%stheme.json", base, prefix, dims[d]);
        if (file_exist(path)) snprintf(out[count++], MAX_BUFFER_SIZE, "%s%stheme.json", prefix, dims[d]);
    }
    return count;
}

static int
lab_scheme_target(const char *theme_folder, char *out, const size_t size, char *variant, const size_t variant_size) {
    char targets[LAB_DIMS_MAX + 1][MAX_BUFFER_SIZE];
    char prefix[MAX_BUFFER_SIZE];
    const int count = scheme_targets(theme_folder, targets, LAB_DIMS_MAX + 1, variant, variant_size, prefix);
    if (count > 1) return snprintf(out, size, "%s and %d resolution files", targets[0], count - 1) < (int) size;
    return count && snprintf(out, size, "%s", targets[0]) < (int) size;
}

static char *json_splice(const char *data, const size_t start, const size_t end, const char *insert) {
    const size_t length = strlen(data);
    const size_t added = strlen(insert);
    char *out = malloc(length - (end - start) + added + 1);
    if (!out) return NULL;

    memcpy(out, data, start);
    memcpy(out + start, insert, added);
    memcpy(out + start + added, data + end, length - end + 1);
    return out;
}

static struct json json_path_get(const char *data, const char *const *path, const int depth) {
    struct json node = json_parse(data);
    for (int i = 0; i < depth && json_exists(node); i++)
        node = json_object_get(node, path[i]);
    return node;
}

static char *json_path_set(const char *data, const char *const *path, const int depth, const char *raw) {
    struct json object = json_parse(data);
    if (json_type(object) != JSON_OBJECT) return NULL;

    int found = 0;
    struct json leaf = {0};
    for (; found < depth; found++) {
        const struct json child = json_object_get(object, path[found]);
        if (!json_exists(child)) break;
        if (found == depth - 1) {
            leaf = child;
            continue;
        }
        if (json_type(child) != JSON_OBJECT) return NULL;
        object = child;
    }

    if (found == depth) {
        size_t start = (size_t) (json_raw(leaf) - data);
        size_t end = start + json_raw_length(leaf);
        if (raw) return json_splice(data, start, end, raw);

        while (start && isspace((unsigned char) data[start - 1]))
            start--;
        if (!start || data[start - 1] != ':') return NULL;
        start--;
        while (start && isspace((unsigned char) data[start - 1]))
            start--;
        if (!start || data[start - 1] != '"') return NULL;
        start--;
        while (start && data[start - 1] != '"')
            start--;
        if (!start) return NULL;
        start--;

        size_t after = end;
        while (isspace((unsigned char) data[after]))
            after++;
        if (data[after] == ',') {
            end = after + 1;
        } else {
            size_t before = start;
            while (before && isspace((unsigned char) data[before - 1]))
                before--;
            if (before && data[before - 1] == ',') start = before - 1;
        }
        return json_splice(data, start, end, "");
    }

    if (!raw) return strdup(data);

    char insert[MAX_BUFFER_SIZE * 2];
    size_t used = 0;
    for (int i = found; i < depth && used < sizeof(insert); i++)
        used +=
            (size_t) snprintf(insert + used, sizeof(insert) - used, i == depth - 1 ? "\"%s\":" : "\"%s\":{", path[i]);
    if (used < sizeof(insert)) used += (size_t) snprintf(insert + used, sizeof(insert) - used, "%s", raw);
    for (int i = found; i < depth - 1 && used < sizeof(insert); i++)
        used += (size_t) snprintf(insert + used, sizeof(insert) - used, "}");

    const char *open = json_raw(object);
    const char *next = open + 1;
    while (isspace((unsigned char) *next))
        next++;
    if (*next != '}' && used < sizeof(insert)) used += (size_t) snprintf(insert + used, sizeof(insert) - used, ",");
    if (used >= sizeof(insert)) return NULL;

    const size_t at = (size_t) (open - data) + 1;
    return json_splice(data, at, at, insert);
}

static int scheme_raw_value(const int index, const char *value, char *raw, const size_t size) {
    struct theme_field_info info;
    theme_field_info(index, &info);

    if (strcmp(info.type, "hex") == 0) {
        const char *hex = value[0] == '#' ? value + 1 : value;
        if (strlen(hex) != 6) return 0;
        for (int i = 0; i < 6; i++) {
            if (!isxdigit((unsigned char) hex[i])) return 0;
        }
        snprintf(raw, size, "\"#%.6s\"", hex);
        for (char *c = raw; *c; c++)
            *c = (char) toupper((unsigned char) *c);
        return 1;
    }

    if (strcmp(info.type, "str") == 0) {
        size_t used = (size_t) snprintf(raw, size, "\"");
        for (const char *c = value; *c; c++) {
            if ((unsigned char) *c < 32 || used + 4 >= size) return 0;
            if (*c == '"' || *c == '\\') raw[used++] = '\\';
            raw[used++] = *c;
        }
        snprintf(raw + used, size - used, "\"");
        return 1;
    }

    char *end = NULL;
    if (strcmp(info.type, "float") == 0) {
        const double number = strtod(value, &end);
        if (end == value || *end) return 0;
        snprintf(raw, size, "%g", number);
        return 1;
    }

    const long number = strtol(value, &end, 10);
    if (end == value || *end) return 0;
    if (strcmp(info.type, "uint") == 0 && (number < 0 || number > 65535)) return 0;
    if (strcmp(info.type, "int") == 0 && (number < -32768 || number > 32767)) return 0;
    if ((info.has_min && number < info.min) || (info.has_max && number > info.max)) return 0;
    snprintf(raw, size, "%ld", number);
    return 1;
}

static void scheme_state_file(char *out, const size_t size, const char *theme_folder, const char *screen) {
    snprintf(out, size, RUN_STORAGE_PATH "theme/%s/.lab/scheme/%s.txt", theme_folder, *screen ? screen : "every");
}

static int lab_scheme_changed(const char *theme_folder, const char *screen, const char *section, const char *key) {
    char path[PATH_MAX];
    scheme_state_file(path, sizeof(path), theme_folder, screen);

    FILE *file = fopen(path, "r");
    if (!file) return 0;

    char want[160];
    char line[160];
    snprintf(want, sizeof(want), "%s %s", section, key);
    int found = 0;
    while (!found && fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        found = strcmp(line, want) == 0;
    }
    fclose(file);
    return found;
}

static void scheme_state_update(const char *theme_folder, const char *screen, const char *entry, const int add) {
    char path[PATH_MAX];
    scheme_state_file(path, sizeof(path), theme_folder, screen);

    char *old = file_exist(path) ? read_all_char_from(path) : NULL;
    size_t size = (old ? strlen(old) : 0) + strlen(entry) + 2;
    char *text = calloc(1, size);
    if (!text) {
        free(old);
        return;
    }

    size_t used = 0;
    for (char *line = old ? strtok(old, "\n") : NULL; line; line = strtok(NULL, "\n")) {
        if (strcmp(line, entry) != 0 && *line) used += (size_t) snprintf(text + used, size - used, "%s\n", line);
    }
    if (add) snprintf(text + used, size - used, "%s\n", entry);
    free(old);

    if (*text) {
        create_directories(path, 1);
        write_text_to_file_atomic(path, CHAR, text);
    } else {
        remove(path);
    }
    free(text);
}

static int scheme_write(
    const char *theme_folder, const char *relative, const char *seed_relative, const char *const *path, const int depth,
    const char *value_raw
) {
    const int reset = value_raw == NULL;
    char raw[MAX_BUFFER_SIZE];
    snprintf(raw, sizeof(raw), "%s", reset ? "" : value_raw);
    const char *value = raw;

    char target[PATH_MAX];
    char backup[PATH_MAX];
    theme_file(target, sizeof(target), theme_folder, relative);
    snprintf(backup, sizeof(backup), RUN_STORAGE_PATH "theme/%s/.lab/original/%s", theme_folder, relative);

    char created[PATH_MAX + 8];
    snprintf(created, sizeof(created), "%s.none", backup);

    char *data = theme_json_read_file(target);
    if (!data && file_exist(target)) return theme_create_failed;
    if (!data && reset) return theme_create_ok;
    if (!data && !file_exist(backup)) {
        create_directories(created, 1);
        write_text_to_file(created, "w", CHAR, "");
    }
    if (data && !file_exist(backup) && !file_exist(created)) {
        create_directories(backup, 1);
        if (copy_file(target, backup) != 0) {
            free(data);
            return theme_create_failed;
        }
    }
    if (!data && seed_relative) {
        char seed[PATH_MAX];
        theme_file(seed, sizeof(seed), theme_folder, seed_relative);
        data = theme_json_read_file(seed);
    }
    if (!data) data = strdup("{}");
    if (!data) return theme_create_failed;

    if (reset && file_exist(backup)) {
        char *original = theme_json_read_file(backup);
        const struct json kept = original ? json_path_get(original, path, depth) : (struct json) {0};
        if (json_exists(kept) && json_raw_length(kept) < sizeof(raw)) {
            snprintf(raw, sizeof(raw), "%.*s", (int) json_raw_length(kept), json_raw(kept));
            value = raw;
        }
        free(original);
    }

    char *written = json_path_set(data, path, depth, *value ? raw : NULL);
    free(data);

    for (int level = depth - 1; written && !*value && level > 0; level--) {
        const struct json parent = json_path_get(written, path, level);
        if (json_type(parent) != JSON_OBJECT || json_exists(json_first(parent))) break;

        char *pruned = json_path_set(written, path, level, NULL);
        free(written);
        written = pruned;
    }
    if (!written) return theme_create_failed;

    int ok = json_valid(written);
    if (ok && file_exist(created) && !json_exists(json_first(json_parse(written)))) {
        remove(target);
    } else if (ok) {
        create_directories(target, 1);
        ok = write_text_to_file_atomic(target, CHAR, written);
    }
    free(written);
    if (!ok) return theme_create_failed;

    return theme_create_ok;
}

int lab_scheme_apply(
    const char *theme_folder, const char *screen, const char *section, const char *key, const char *value
) {
    if (!theme_lab_own(theme_folder) || !scheme_screen_valid(screen) || !scheme_name_valid(section, 0)
        || !scheme_name_valid(key, 1))
        return theme_create_name;

    const int index = theme_field_find(section, key);
    if (index < 0) return theme_create_name;

    const int reset = !*value;
    char raw[MAX_BUFFER_SIZE];
    if (*value && !scheme_raw_value(index, value, raw, sizeof(raw))) return theme_create_name;

    char targets[LAB_DIMS_MAX + 1][MAX_BUFFER_SIZE];
    char variant[MAX_BUFFER_SIZE];
    char prefix[MAX_BUFFER_SIZE];
    const int target_count = scheme_targets(theme_folder, targets, LAB_DIMS_MAX + 1, variant, sizeof(variant), prefix);
    if (!target_count) return theme_create_failed;

    char seed[MAX_BUFFER_SIZE + 16];
    snprintf(seed, sizeof(seed), "%stheme.json", prefix);
    const int seeded = lab_target[0] && strcmp(seed, targets[0]) != 0;

    const char *every[] = {"scheme", section, key};
    const char *module[] = {"modules", screen, "scheme", section, key};
    const char *const *path = *screen ? module : every;
    const int depth = *screen ? 5 : 3;

    for (int t = 0; t < target_count; t++) {
        const int result =
            scheme_write(theme_folder, targets[t], seeded ? seed : NULL, path, depth, reset ? NULL : raw);
        if (result != theme_create_ok) return result;
    }

    char entry[160];
    snprintf(entry, sizeof(entry), "%s %s", section, key);
    scheme_state_update(theme_folder, screen, entry, !reset);
    return theme_create_ok;
}

#define LAB_COLOUR_KEYS 20

typedef struct {
    const char *section;
    const char *key;
} lab_colour_key;

static const struct {
    const char *role;
    lab_colour_key alpha;
    lab_colour_key keys[LAB_COLOUR_KEYS];
} lab_colour_roles[] = {
    {"background",
     {NULL, NULL},
     {{"background", "BACKGROUND"},
      {"background", "BACKGROUND_GRADIENT_COLOR"},
      {"list", "LIST_DEFAULT_BACKGROUND"},
      {"grid", "BACKGROUND"},
      {"charging", "CHARGER_BACKGROUND"},
      {"verbose", "VERBOSE_BOOT_BACKGROUND"}}},
    {"text",
     {NULL, NULL},
     {{"list", "LIST_DEFAULT_TEXT"},
      {"charging", "CHARGER_TEXT"},
      {"verbose", "VERBOSE_BOOT_TEXT"},
      {"grid", "CELL_DEFAULT_TEXT"}}},
    {"text_shadow", {NULL, NULL}, {{"list", "LIST_DEFAULT_SHADOW_COLOUR"}}},
    {"glyphs",
     {"list", "LIST_DEFAULT_GLYPH_RECOLOUR_ALPHA"},
     {{"list", "LIST_DEFAULT_GLYPH_RECOLOUR"}, {"grid", "CELL_DEFAULT_IMAGE_RECOLOUR"}}},
    {"dialogue_background",
     {NULL, NULL},
     {{"dialogue", "DIALOGUE_BACKGROUND"}, {"notification", "MSG_BACKGROUND"}, {"bar", "BAR_BACKGROUND"}}},
    {"dialogue_text",
     {NULL, NULL},
     {{"dialogue", "DIALOGUE_CONTENT"}, {"dialogue", "DIALOGUE_TITLE"}, {"notification", "MSG_TEXT"}}},
    {"help_background", {NULL, NULL}, {{"help", "HELP_BACKGROUND"}}},
    {"help_text", {NULL, NULL}, {{"help", "HELP_CONTENT"}, {"help", "HELP_TITLE"}}},
    {"keyboard_background", {NULL, NULL}, {{"keyboard", "OSK_BACKGROUND"}, {"keyboard", "OSK_ITEM_BACKGROUND"}}},
    {"keyboard_text", {NULL, NULL}, {{"keyboard", "OSK_TEXT"}}},
    {"border",
     {NULL, NULL},
     {{"dialogue", "DIALOGUE_BORDER"},
      {"help", "HELP_BORDER"},
      {"notification", "MSG_BORDER"},
      {"keyboard", "OSK_BORDER"},
      {"keyboard", "OSK_ITEM_BORDER"},
      {"bar", "BAR_BORDER"},
      {"roll", "ROLL_BORDER_COLOUR"},
      {"counter", "COUNTER_BORDER_COLOUR"}}},
    {"highlight",
     {"list", "LIST_FOCUS_BACKGROUND_ALPHA"},
     {{"list", "LIST_FOCUS_BACKGROUND"},
      {"list", "LIST_FOCUS_INDICATOR"},
      {"dialogue", "DIALOGUE_SELECTION"},
      {"grid", "CELL_FOCUS_BACKGROUND"},
      {"grid", "CELL_FOCUS_BACKGROUND_GRADIENT_COLOR"},
      {"grid", "CELL_FOCUS_BORDER"},
      {"grid", "CELL_FOCUS_GLOW"},
      {"keyboard", "OSK_ITEM_BACKGROUND_FOCUS"},
      {"keyboard", "OSK_ITEM_BORDER_FOCUS"},
      {"roll", "ROLL_SELECT_BACKGROUND"},
      {"counter", "COUNTER_BACKGROUND"}}},
    {"highlight_text",
     {NULL, NULL},
     {{"list", "LIST_FOCUS_TEXT"},
      {"list", "LIST_FOCUS_GLYPH_RECOLOUR"},
      {"dialogue", "DIALOGUE_OPTION"},
      {"grid", "CELL_FOCUS_TEXT"},
      {"grid", "CELL_FOCUS_IMAGE_RECOLOUR"},
      {"keyboard", "OSK_TEXT_FOCUS"},
      {"roll", "ROLL_SELECT_TEXT"},
      {"counter", "COUNTER_TEXT"}}},
    {"highlight_shadow", {NULL, NULL}, {{"list", "LIST_FOCUS_SHADOW_COLOUR"}}},
    {"header_background", {"header", "HEADER_BACKGROUND_ALPHA"}, {{"header", "HEADER_BACKGROUND"}}},
    {"header_text", {NULL, NULL}, {{"header", "HEADER_TEXT"}, {"date", "DATETIME_TEXT"}}},
    {"footer_background", {"footer", "FOOTER_BACKGROUND_ALPHA"}, {{"footer", "FOOTER_BACKGROUND"}}},
    {"footer_text",
     {NULL, NULL},
     {{"footer", "FOOTER_TEXT"},
      {"navigation", "NAV_LR_GLYPH"},
      {"navigation", "NAV_LR_TEXT"},
      {"navigation", "NAV_UD_GLYPH"},
      {"navigation", "NAV_UD_TEXT"},
      {"navigation", "NAV_A_GLYPH"},
      {"navigation", "NAV_A_TEXT"},
      {"navigation", "NAV_B_GLYPH"},
      {"navigation", "NAV_B_TEXT"},
      {"navigation", "NAV_C_GLYPH"},
      {"navigation", "NAV_C_TEXT"},
      {"navigation", "NAV_X_GLYPH"},
      {"navigation", "NAV_X_TEXT"},
      {"navigation", "NAV_Y_GLYPH"},
      {"navigation", "NAV_Y_TEXT"},
      {"navigation", "NAV_Z_GLYPH"},
      {"navigation", "NAV_Z_TEXT"},
      {"navigation", "NAV_MENU_GLYPH"},
      {"navigation", "NAV_MENU_TEXT"}}},
    {"status_icons",
     {NULL, NULL},
     {{"battery", "BATTERY_NORMAL"}, {"network", "NETWORK_ACTIVE"}, {"bluetooth", "BLUETOOTH_ACTIVE"}}},
    {"progress_bar", {NULL, NULL}, {{"bar", "BAR_PROGRESS_ACTIVE_BACKGROUND"}}},
};

static int colour_shown_alpha(const lab_colour_key *alpha) {
    const int index = theme_field_find(alpha->section, alpha->key);
    if (index < 0) return 0;

    char value[MAX_BUFFER_SIZE];
    theme_field_snapshot("", &device);
    theme_field_value(index, value, sizeof(value));
    return atoi(value);
}

int lab_colour_apply(const char *theme_folder, const char *role, const char *value) {
    const int all = strcmp(role, "all") == 0;
    int found = 0;

    for (size_t r = 0; r < A_SIZE(lab_colour_roles); r++) {
        if (!all && strcmp(role, lab_colour_roles[r].role) != 0) continue;
        found = 1;

        for (int k = 0; k < LAB_COLOUR_KEYS && lab_colour_roles[r].keys[k].section; k++) {
            const int status = lab_scheme_apply(
                theme_folder, "", lab_colour_roles[r].keys[k].section, lab_colour_roles[r].keys[k].key, value
            );
            if (status != theme_create_ok) return status;
        }

        const lab_colour_key *alpha = &lab_colour_roles[r].alpha;
        if (!alpha->section) continue;
        if (!*value) {
            lab_scheme_apply(theme_folder, "", alpha->section, alpha->key, "");
        } else if (strcmp(lab_colour_roles[r].role, "highlight") != 0 || colour_shown_alpha(alpha) < 128) {
            lab_scheme_apply(theme_folder, "", alpha->section, alpha->key, "255");
        }
    }

    return found ? theme_create_ok : theme_create_name;
}

int lab_scheme_export(const char *theme_folder, const char *screen) {
    if (!name_valid(theme_folder) || !scheme_screen_valid(screen)) return theme_create_name;

    char relative[MAX_BUFFER_SIZE];
    char variant[MAX_BUFFER_SIZE];
    lab_scheme_target(theme_folder, relative, sizeof(relative), variant, sizeof(variant));
    char saved_dim[sizeof(mux_dim)];
    snprintf(saved_dim, sizeof(saved_dim), "%s", mux_dim);
    if (lab_target[0]) snprintf(mux_dim, sizeof(mux_dim), "%s", lab_target);
    theme_field_snapshot(screen, &device);
    snprintf(mux_dim, sizeof(mux_dim), "%s", saved_dim);

    const int count = theme_field_count();
    const size_t size = (size_t) count * 160 + 1024;
    char *out = malloc(size);
    if (!out) return theme_create_failed;

    size_t used = (size_t) snprintf(out, size, "{\"screen\":\"%s\",\"target\":\"%s\",\"variant\":\"", screen, relative);
    for (const char *c = variant; *c && used + 3 < size; c++) {
        if (*c == '"' || *c == '\\') out[used++] = '\\';
        out[used++] = *c;
    }
    used += (size_t) snprintf(out + used, size - used, "\",\"fields\":[");

    for (int i = 0; i < count && used < size; i++) {
        struct theme_field_info info;
        theme_field_info(i, &info);

        char value[MAX_BUFFER_SIZE];
        theme_field_value(i, value, sizeof(value));

        char min[16] = "null";
        char max[16] = "null";
        if (info.has_min) snprintf(min, sizeof(min), "%d", info.min);
        if (info.has_max) snprintf(max, sizeof(max), "%d", info.max);

        used += (size_t) snprintf(
            out + used, size - used, "%s[\"%s\",\"%s\",\"%s\",%s,%s,%s,%d]", i ? "," : "", info.section, info.key,
            info.type, min, max, value, lab_scheme_changed(theme_folder, screen, info.section, info.key)
        );
    }
    if (used < size) used += (size_t) snprintf(out + used, size - used, "]}");

    const int ok = used < size && json_valid(out) && write_text_to_file_atomic(LAB_SCHEME_EXPORT, CHAR, out);
    free(out);
    return ok ? theme_create_ok : theme_create_failed;
}

static const char *const lab_splash_roles[] = {"bootlogo", "reboot", "shutdown"};

static void original_paths(const char *theme_folder, const char *relative, char *path, char *backup, char *none) {
    theme_file(path, PATH_MAX, theme_folder, relative);
    snprintf(backup, PATH_MAX, RUN_STORAGE_PATH "theme/%s/.lab/original/%s", theme_folder, relative);
    snprintf(none, PATH_MAX + 8, "%s.none", backup);
}

static void keep_original(const char *theme_folder, const char *relative) {
    char path[PATH_MAX];
    char backup[PATH_MAX];
    char none[PATH_MAX + 8];
    original_paths(theme_folder, relative, path, backup, none);
    if (file_exist(backup) || file_exist(none)) return;

    create_directories(backup, 1);
    if (file_exist(path))
        copy_file(path, backup);
    else
        write_text_to_file(none, "w", CHAR, "");
}

static void put_back_original(const char *theme_folder, const char *relative) {
    char path[PATH_MAX];
    char backup[PATH_MAX];
    char none[PATH_MAX + 8];
    original_paths(theme_folder, relative, path, backup, none);

    if (file_exist(backup)) {
        create_directories(path, 1);
        file_rename(backup, path);
    } else if (file_exist(none)) {
        remove(path);
        remove(none);
    }
}

static int variant_exists(const char *theme_folder, const char *name) {
    static const char *const exts[] = {"json", "ini", "muxalt"};
    char path[PATH_MAX];
    char found[MAX_BUFFER_SIZE];
    for (size_t i = 0; i < A_SIZE(exts); i++) {
        char leaf[MAX_BUFFER_SIZE];
        snprintf(leaf, sizeof(leaf), "%s.%s", name, exts[i]);
        snprintf(path, sizeof(path), RUN_STORAGE_PATH "theme/%s/alternate/%s", theme_folder, leaf);
        if (theme_variant_file(leaf, found, sizeof(found)) && file_exist(path)) return 1;
    }
    return 0;
}

static void active_variant(const char *theme_folder, char *out, const size_t size) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), RUN_STORAGE_PATH "theme/%s/active.txt", theme_folder);
    char *text = file_exist(path) ? read_line_char_from(path, 1) : NULL;
    snprintf(out, size, "%s", text ? text : "");
    free(text);
    out[strcspn(out, "\r\n")] = '\0';
    if (!name_valid(out) || !variant_exists(theme_folder, out)) out[0] = '\0';
}

static void lab_rgb_restore(void) {
    if (config.settings.rgb.mode != RGB_MODE_THEME_SUPPLIED) return;
    const char *argv[2] = {RGBLED_BIN, "restore"};
    run_exec(argv, 2, 0, 0, NULL, NULL);
}

static int use_variant(const char *theme_folder, const char *name) {
    if (*name && (!name_valid(name) || !variant_exists(theme_folder, name))) return theme_create_name;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), RUN_STORAGE_PATH "theme/%s/active.txt", theme_folder);
    if (!write_text_to_file(path, "w", CHAR, name)) return theme_create_failed;
    reset_active_alternate();

    char archive[PATH_MAX];
    char base[PATH_MAX];
    snprintf(archive, sizeof(archive), RUN_STORAGE_PATH "theme/%s/alternate/%s.muxalt", theme_folder, name);
    snprintf(base, sizeof(base), RUN_STORAGE_PATH "theme/%s", theme_folder);
    if (*name && file_exist(archive) && extract_zip_to_dir(archive, base) != MUX_EXTRACT_OK) return theme_create_failed;

    write_text_to_file(MUOS_BTL_LOAD, "w", INT, 1);
    lab_rgb_restore();
    return theme_create_ok;
}

static int copy_variant_file(const char *theme_folder, const char *from, const char *to) {
    char source[PATH_MAX];
    char target[PATH_MAX];
    theme_file(source, sizeof(source), theme_folder, from);
    theme_file(target, sizeof(target), theme_folder, to);
    if (!file_exist(source)) return 1;
    create_directories(target, 1);
    return copy_file(source, target) == 0;
}

static int create_variant(const char *theme_folder, const char *name, const int copy) {
    if (!name_valid(name) || strlen(name) > 64 || variant_exists(theme_folder, name)) return theme_create_exists;

    char current[MAX_BUFFER_SIZE];
    active_variant(theme_folder, current, sizeof(current));

    char json[MAX_BUFFER_SIZE];
    snprintf(json, sizeof(json), "alternate/%s.json", name);
    char path[PATH_MAX];
    theme_file(path, sizeof(path), theme_folder, json);
    create_directories(path, 1);
    if (!write_text_to_file(path, "w", CHAR, "{}\n")) return theme_create_failed;
    if (!copy || !current[0]) return theme_create_ok;

    char from[MAX_BUFFER_SIZE];
    char to[MAX_BUFFER_SIZE];
    int ok = 1;
    const char *leaves[] = {".ini", ".json"};
    for (size_t i = 0; i < A_SIZE(leaves) && ok; i++) {
        snprintf(from, sizeof(from), "alternate/%s%s", current, leaves[i]);
        snprintf(to, sizeof(to), "alternate/%s%s", name, leaves[i]);
        ok = copy_variant_file(theme_folder, from, to);
    }
    for (size_t i = 0; i < A_SIZE(lab_splash_roles) && ok; i++) {
        snprintf(from, sizeof(from), "alternate/%s_%s.json", current, lab_splash_roles[i]);
        snprintf(to, sizeof(to), "alternate/%s_%s.json", name, lab_splash_roles[i]);
        ok = copy_variant_file(theme_folder, from, to);
    }
    const char *lights[] = {"colour_l", "colour_r", "bright", "mode", "rgbconf.sh"};
    for (size_t i = 0; i < A_SIZE(lights) && ok; i++) {
        snprintf(from, sizeof(from), "alternate/rgb/%s/%s", current, lights[i]);
        snprintf(to, sizeof(to), "alternate/rgb/%s/%s", name, lights[i]);
        ok = copy_variant_file(theme_folder, from, to);
    }
    return ok ? theme_create_ok : theme_create_failed;
}

static int delete_variant(const char *theme_folder, const char *name) {
    if (!name_valid(name) || !variant_exists(theme_folder, name)) return theme_create_name;

    char current[MAX_BUFFER_SIZE];
    active_variant(theme_folder, current, sizeof(current));

    char path[PATH_MAX];
    const char *files[] = {"alternate/%s.ini",           "alternate/%s.json",        "alternate/%s.muxalt",
                           "alternate/%s_bootlogo.json", "alternate/%s_reboot.json", "alternate/%s_shutdown.json"};
    for (size_t i = 0; i < A_SIZE(files); i++) {
        char relative[MAX_BUFFER_SIZE];
        snprintf(relative, sizeof(relative), files[i], name);
        theme_file(path, sizeof(path), theme_folder, relative);
        remove(path);
    }
    for (size_t i = 0; i < A_SIZE(files); i++) {
        char relative[MAX_BUFFER_SIZE];
        char backup[PATH_MAX];
        char none[PATH_MAX + 8];
        snprintf(relative, sizeof(relative), files[i], name);
        original_paths(theme_folder, relative, path, backup, none);
        remove(backup);
        remove(none);
    }

    const char *folders[] = {
        "alternate/%s", "alternate/rgb/%s", ".lab/original/alternate/%s", ".lab/original/alternate/rgb/%s"
    };
    for (size_t i = 0; i < A_SIZE(folders); i++) {
        char relative[MAX_BUFFER_SIZE];
        snprintf(relative, sizeof(relative), folders[i], name);
        theme_file(path, sizeof(path), theme_folder, relative);
        if (dir_exist(path)) remove_directory_recursive(path);
    }

    return strcmp(current, name) == 0 ? use_variant(theme_folder, "") : theme_create_ok;
}

int lab_variant_apply(const char *theme_folder, const char *op, const char *name, const char *source) {
    if (!theme_lab_own(theme_folder) || !op) return theme_create_name;
    if (strcmp(op, "use") == 0) return use_variant(theme_folder, name ? name : "");
    if (strcmp(op, "create") == 0) return create_variant(theme_folder, name, source && strcmp(source, "copy") == 0);
    if (strcmp(op, "delete") == 0) return delete_variant(theme_folder, name);
    return theme_create_name;
}

static void light_folder(const char *theme_folder, char *out, const size_t size) {
    char current[MAX_BUFFER_SIZE];
    active_variant(theme_folder, current, sizeof(current));
    if (current[0])
        snprintf(out, size, "alternate/rgb/%s/", current);
    else
        snprintf(out, size, "rgb/");
}

static int valid_hex(const char *value) {
    const char *hex = value[0] == '#' ? value + 1 : value;
    if (strlen(hex) != 6) return 0;
    for (int i = 0; i < 6; i++) {
        if (!isxdigit((unsigned char) hex[i])) return 0;
    }
    return 1;
}

static void read_light(const char *theme_folder, const char *folder, const char *leaf, char *out, const size_t size) {
    char relative[MAX_BUFFER_SIZE];
    char path[PATH_MAX];
    snprintf(relative, sizeof(relative), "%s%s", folder, leaf);
    theme_file(path, sizeof(path), theme_folder, relative);
    char *text = file_exist(path) ? read_line_char_from(path, 1) : NULL;
    snprintf(out, size, "%s", text ? text : "");
    free(text);
    out[strcspn(out, "\r\n")] = '\0';
}

int lab_light_apply(const char *theme_folder, const char *side, const char *value) {
    if (!theme_lab_own(theme_folder) || !side) return theme_create_name;

    char folder[MAX_BUFFER_SIZE];
    light_folder(theme_folder, folder, sizeof(folder));

    char relative[3][MAX_BUFFER_SIZE];
    snprintf(relative[0], MAX_BUFFER_SIZE, "%scolour_l", folder);
    snprintf(relative[1], MAX_BUFFER_SIZE, "%scolour_r", folder);
    snprintf(relative[2], MAX_BUFFER_SIZE, "%srgbconf.sh", folder);

    if (!value || !*value) {
        for (int i = 0; i < 3; i++)
            put_back_original(theme_folder, relative[i]);
        lab_rgb_restore();
        return theme_create_ok;
    }

    const int left = strcmp(side, "l") == 0;
    if ((!left && strcmp(side, "r") != 0) || !valid_hex(value)) return theme_create_name;

    char colours[2][8];
    read_light(theme_folder, folder, "colour_l", colours[0], sizeof(colours[0]));
    read_light(theme_folder, folder, "colour_r", colours[1], sizeof(colours[1]));
    snprintf(colours[left ? 0 : 1], sizeof(colours[0]), "%.6s", value[0] == '#' ? value + 1 : value);
    for (int c = 0; c < 2; c++) {
        if (!valid_hex(colours[c])) snprintf(colours[c], sizeof(colours[c]), "%s", colours[left ? 0 : 1]);
        for (char *p = colours[c]; *p; p++)
            *p = (char) toupper((unsigned char) *p);
    }

    char bright[16];
    read_light(theme_folder, folder, "bright", bright, sizeof(bright));
    const int level = atoi(bright) > 0 ? atoi(bright) : 76;

    for (int i = 0; i < 3; i++)
        keep_original(theme_folder, relative[i]);
    char path[PATH_MAX];
    int ok = 1;
    for (int c = 0; c < 2 && ok; c++) {
        theme_file(path, sizeof(path), theme_folder, relative[c]);
        create_directories(path, 1);
        ok = write_text_to_file(path, "w", CHAR, colours[c]);
    }

    unsigned l = 0;
    unsigned r = 0;
    sscanf(colours[0], "%6x", &l);
    sscanf(colours[1], "%6x", &r);
    char script[512];
    snprintf(
        script, sizeof(script),
        "#!/bin/sh\n\n. /opt/muos/script/var/config.sh\n\n[ \"$(GET_VAR \"device\" \"led/rgb\")\" -eq 0 ] && return 0\n"
        "\"$MUOS_RGB_BIN\" -b AUTO 1 %d %u %u %u %u %u %u\n",
        level, l >> 16, (l >> 8) & 0xFF, l & 0xFF, r >> 16, (r >> 8) & 0xFF, r & 0xFF
    );
    theme_file(path, sizeof(path), theme_folder, relative[2]);
    ok = ok && write_text_to_file(path, "w", CHAR, script);
    if (!ok) return theme_create_failed;

    lab_rgb_restore();
    return theme_create_ok;
}

static void splash_file(const char *theme_folder, const char *role, char *out, const size_t size) {
    char current[MAX_BUFFER_SIZE];
    active_variant(theme_folder, current, sizeof(current));
    if (current[0])
        snprintf(out, size, "alternate/%s_%s.json", current, role);
    else
        snprintf(out, size, "%s.json", role);
}

int lab_splash_apply(const char *theme_folder, const char *field, const char *value) {
    static const char *const fields[][2] = {
        {"background", "background_colour"},
        {"gradient", "background_gradient_colour"},
        {"tint", "png_recolour"},
        {"tintalpha", "png_recolour_alpha"},
    };
    if (!theme_lab_own(theme_folder) || !field) return theme_create_name;

    char relative[A_SIZE(lab_splash_roles)][MAX_BUFFER_SIZE];
    for (size_t i = 0; i < A_SIZE(lab_splash_roles); i++)
        splash_file(theme_folder, lab_splash_roles[i], relative[i], sizeof(relative[i]));

    if (!value || !*value) {
        for (size_t i = 0; i < A_SIZE(relative); i++)
            put_back_original(theme_folder, relative[i]);
        write_text_to_file(MUOS_BTL_LOAD, "w", INT, 1);
        return theme_create_ok;
    }

    const char *key = NULL;
    for (size_t i = 0; i < A_SIZE(fields); i++) {
        if (strcmp(field, fields[i][0]) == 0) key = fields[i][1];
    }
    if (!key) return theme_create_name;

    char raw[32];
    if (strcmp(field, "tintalpha") == 0) {
        char *end = NULL;
        const long alpha = strtol(value, &end, 10);
        if (end == value || *end || alpha < 0 || alpha > 255) return theme_create_name;
        snprintf(raw, sizeof(raw), "%ld", alpha);
    } else {
        if (!valid_hex(value)) return theme_create_name;
        snprintf(raw, sizeof(raw), "\"%.6s\"", value[0] == '#' ? value + 1 : value);
        for (char *p = raw; *p; p++)
            *p = (char) toupper((unsigned char) *p);
    }

    for (size_t i = 0; i < A_SIZE(relative); i++) {
        char path[PATH_MAX];
        keep_original(theme_folder, relative[i]);
        theme_file(path, sizeof(path), theme_folder, relative[i]);

        char *data = theme_json_read_file(path);
        if (!data) data = strdup("{}");
        if (!data) return theme_create_failed;
        const char *keys[] = {key};
        char *written = json_path_set(data, keys, 1, raw);
        free(data);
        const int ok = written && json_valid(written) && write_text_to_file_atomic(path, CHAR, written);
        free(written);
        if (!ok) return theme_create_failed;
    }

    write_text_to_file(MUOS_BTL_LOAD, "w", INT, 1);
    return theme_create_ok;
}

static int scheme_raw_any(const char *section, const char *key, const char *value, char *raw, const size_t size) {
    const int index = theme_field_find(section, key);
    char *end = NULL;
    if (index >= 0) {
        struct theme_field_info info;
        theme_field_info(index, &info);

        if (strcmp(info.type, "hex") == 0) {
            const char *digits = value[0] == '#' ? value + 1 : value;
            if (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) digits += 2;
            const unsigned long colour = strtoul(digits, &end, 16);
            if (end != digits && !*end) return snprintf(raw, size, "\"#%06lX\"", colour & 0xFFFFFF) < (int) size;
        } else if (strcmp(info.type, "float") == 0) {
            const double number = strtod(value, &end);
            if (end != value && !*end) return snprintf(raw, size, "%g", number) < (int) size;
        } else if (strcmp(info.type, "str") != 0) {
            const long number = strtol(value, &end, 10);
            if (end != value && !*end) return snprintf(raw, size, "%ld", number) < (int) size;
        }
    } else {
        const long number = strtol(value, &end, 10);
        if (end != value && !*end) return snprintf(raw, size, "%ld", number) < (int) size;
    }

    size_t used = (size_t) snprintf(raw, size, "\"");
    for (const char *c = value; *c; c++) {
        if ((unsigned char) *c < 32 || used + 4 >= size) return 0;
        if (*c == '"' || *c == '\\') raw[used++] = '\\';
        raw[used++] = *c;
    }
    snprintf(raw + used, size - used, "\"");
    return 1;
}

static char *trim_text(char *text) {
    while (isspace((unsigned char) *text))
        text++;
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char) end[-1]))
        *--end = '\0';
    return text;
}

static int convert_ini(char **data, const char *path, const char **prefix, const int depth) {
    char *text = file_exist(path) ? read_all_char_from(path) : NULL;
    if (!text) return 0;

    char section[64] = "";
    for (char *line = strtok(text, "\n"); line && *data; line = strtok(NULL, "\n")) {
        char *clean = trim_text(line);
        if (!*clean || *clean == ';' || *clean == '#') continue;

        const size_t length = strlen(clean);
        if (*clean == '[' && clean[length - 1] == ']') {
            clean[length - 1] = '\0';
            snprintf(section, sizeof(section), "%s", trim_text(clean + 1));
            continue;
        }

        char *equals = strchr(clean, '=');
        if (!section[0] || !equals) continue;
        *equals = '\0';
        char *key = trim_text(clean);
        char *value = trim_text(equals + 1);
        while (*value == '"')
            value++;
        for (size_t end = strlen(value); end && value[end - 1] == '"'; end--)
            value[end - 1] = '\0';

        const struct json scheme = json_path_get(*data, prefix, depth);
        if (json_exists(theme_json_lookup(scheme, section, key))) continue;

        char raw[MAX_BUFFER_SIZE];
        if (!*key || !scheme_raw_any(section, key, value, raw, sizeof(raw))) continue;

        const char *path_parts[8];
        for (int i = 0; i < depth; i++)
            path_parts[i] = prefix[i];
        path_parts[depth] = section;
        path_parts[depth + 1] = key;

        char *next = json_path_set(*data, path_parts, depth + 2, raw);
        free(*data);
        *data = next;
    }

    free(text);
    return 1;
}

static int is_resolution(const char *name) {
    int width = 0;
    int height = 0;
    char tail = '\0';
    return sscanf(name, "%dx%d%c", &width, &height, &tail) == 2 && width > 0 && height > 0;
}

static void scheme_ini_path(const char *base_path, const char *dim, const char *name, char *out, const size_t size) {
    snprintf(out, size, "%s/%sscheme/%s.ini", base_path, dim, name);
    if (*dim && !file_exist(out)) snprintf(out, size, "%s/scheme/%s.ini", base_path, name);
}

static int
scheme_ini_names(const char *base_path, const char *dim, char (*names)[NAME_MAX + 1], int count, const int max) {
    char folder[PATH_MAX];
    snprintf(folder, sizeof(folder), "%s/%sscheme", base_path, dim);
    DIR *dir = opendir(folder);
    if (!dir) return count;

    const struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max) {
        const char *dot = strrchr(entry->d_name, '.');
        if (entry->d_name[0] == '.' || !dot || strcasecmp(dot, ".ini") != 0) continue;

        char name[NAME_MAX + 1];
        snprintf(name, sizeof(name), "%.*s", (int) (dot - entry->d_name), entry->d_name);
        int seen = strcmp(name, "global") == 0 || strcmp(name, "default") == 0;
        for (int i = 0; i < count && !seen; i++)
            seen = strcmp(names[i], name) == 0;
        if (!seen) snprintf(names[count++], NAME_MAX + 1, "%s", name);
    }
    closedir(dir);
    return count;
}

static int remove_scheme_inis(const char *folder) {
    DIR *dir = opendir(folder);
    if (!dir) return 0;

    int removed = 0;
    const struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *dot = strrchr(entry->d_name, '.');
        if (entry->d_name[0] == '.' || !dot || strcasecmp(dot, ".ini") != 0) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", folder, entry->d_name);
        removed += remove(path) == 0;
    }
    closedir(dir);
    rmdir(folder);
    return removed;
}

static int convert_scheme_base(const char *base_path) {
    char dims[LAB_LANGUAGES_MAX + 1][64] = {""};
    int dim_count = 1;
    DIR *dir = opendir(base_path);
    if (dir) {
        const struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && dim_count < (int) A_SIZE(dims)) {
            char folder[PATH_MAX];
            snprintf(folder, sizeof(folder), "%s/%s/scheme", base_path, entry->d_name);
            if (is_resolution(entry->d_name) && dir_exist(folder))
                snprintf(dims[dim_count++], sizeof(dims[0]), "%s/", entry->d_name);
        }
        closedir(dir);
    }

    char root_json[PATH_MAX];
    snprintf(root_json, sizeof(root_json), "%s/theme.json", base_path);
    char *root_before = theme_json_read_file(root_json);

    int ok = 1;
    for (int d = 0; d < dim_count && ok; d++) {
        char target[PATH_MAX];
        snprintf(target, sizeof(target), "%s/%stheme.json", base_path, dims[d]);
        char *data = d ? theme_json_read_file(target) : (root_before ? strdup(root_before) : NULL);
        if (!data) data = strdup(d && root_before ? root_before : "{}");
        if (!data) {
            ok = 0;
            break;
        }

        int found = 0;
        const char *scheme_prefix[] = {"scheme"};
        const char *bases[] = {"default", "global"};
        for (size_t b = 0; b < A_SIZE(bases); b++) {
            char ini[PATH_MAX];
            scheme_ini_path(base_path, dims[d], bases[b], ini, sizeof(ini));
            found |= convert_ini(&data, ini, scheme_prefix, 1);
        }

        static char names[LAB_SCREENS_MAX][NAME_MAX + 1];
        int count = scheme_ini_names(base_path, dims[d], names, 0, LAB_SCREENS_MAX);
        if (d) count = scheme_ini_names(base_path, "", names, count, LAB_SCREENS_MAX);
        for (int n = 0; n < count && data; n++) {
            char ini[PATH_MAX];
            scheme_ini_path(base_path, dims[d], names[n], ini, sizeof(ini));
            const char *module_prefix[] = {"modules", names[n], "scheme"};
            found |= convert_ini(&data, ini, module_prefix, 3);
        }

        if (!data) {
            ok = 0;
        } else if (found) {
            ok = json_valid(data) && write_text_to_file_atomic(target, CHAR, data);
        }
        free(data);
    }
    free(root_before);
    if (!ok) return -1;

    int removed = 0;
    for (int d = 0; d < dim_count; d++) {
        char folder[PATH_MAX];
        snprintf(folder, sizeof(folder), "%s/%sscheme", base_path, dims[d]);
        removed += remove_scheme_inis(folder);
    }
    return removed;
}

int lab_scheme_to_json(const char *theme_folder) {
    if (!name_valid(theme_folder)) return -1;

    char base[PATH_MAX];
    snprintf(base, sizeof(base), RUN_STORAGE_PATH "theme/%s", theme_folder);
    int converted = convert_scheme_base(base);
    if (converted < 0) return -1;

    char alternates[PATH_MAX];
    snprintf(alternates, sizeof(alternates), "%s/alternate", base);
    DIR *dir = opendir(alternates);
    if (!dir) return converted;

    const struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' || strcmp(entry->d_name, "rgb") == 0) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", alternates, entry->d_name);
        if (dir_exist(path)) {
            const int more = convert_scheme_base(path);
            if (more > 0) converted += more;
            continue;
        }

        const char *dot = strrchr(entry->d_name, '.');
        if (!dot || strcasecmp(dot, ".ini") != 0) continue;

        char target[PATH_MAX];
        snprintf(target, sizeof(target), "%s/%.*s.json", alternates, (int) (dot - entry->d_name), entry->d_name);
        char *data = theme_json_read_file(target);
        if (!data) data = strdup("{}");
        const char *prefix[] = {"scheme"};
        if (data && convert_ini(&data, path, prefix, 1) && data && json_valid(data)
            && write_text_to_file_atomic(target, CHAR, data)) {
            converted += remove(path) == 0;
        }
        free(data);
    }
    closedir(dir);
    return converted;
}

static const char *const lab_resolutions[] = {"320x240",  "640x480",  "720x480",  "720x576",  "720x720",
                                              "1024x768", "1280x720", "1366x768", "1920x1080"};

int lab_resolution_apply(const char *theme_folder, const char *op, const char *dim) {
    if (!theme_lab_own(theme_folder) || !op || !dim) return theme_create_name;

    int known = 0;
    for (size_t i = 0; i < A_SIZE(lab_resolutions); i++)
        known |= strcmp(dim, lab_resolutions[i]) == 0;
    if (!known) return theme_create_name;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), RUN_STORAGE_PATH "theme/%s/%s", theme_folder, dim);
    if (strcmp(op, "add") == 0) {
        if (dir_exist(path)) return theme_create_exists;
        return mkdir(path, 0755) == 0 ? theme_create_ok : theme_create_failed;
    }
    if (strcmp(op, "remove") == 0) {
        if (!dir_exist(path)) return theme_create_name;
        return remove_directory_recursive(path) == 0 ? theme_create_ok : theme_create_failed;
    }
    return theme_create_name;
}
