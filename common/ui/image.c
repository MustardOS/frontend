#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <SDL2/SDL_image.h>
#if defined(__aarch64__)
#include <arm_neon.h>
#endif
#include <common/runtime/init.h>
#include <common/runtime/perf.h>
#include <common/ui/common.h>
#include <common/ui/image.h>
#include <common/ui/cache.h>
#include <common/platform/video.h>
#include <common/config/config.h>
#include <common/platform/device.h>
#include <common/display/theme.h>
#include <module/muxshare.h>
#include <lvgl/src/draw/sdl/lv_draw_sdl_texture_cache.h>

char current_wall[MAX_BUFFER_SIZE];

static void read_image_dims(const char *path, int *w, int *h);
static void free_scaled_raster(lv_obj_t *ui_img_obj);

#define IMAGE_JOB_CAPACITY 16
#define IMAGE_CACHE_COUNT  6
#define IMAGE_CACHE_BUDGET (24u * 1024u * 1024u)

typedef struct {
    lv_obj_t *obj;
    uint64_t generation;
    char path[MAX_BUFFER_SIZE];
    int tw, th, align, pad_l, pad_r, pad_t, pad_b;
} image_job_t;

typedef struct {
    image_job_t job;
    uint8_t *pixels;
    size_t bytes;
    double work_ms;
} image_result_t;

typedef struct {
    lv_obj_t *obj;
    uint64_t generation;
} image_generation_t;

typedef struct {
    char path[MAX_BUFFER_SIZE];
    int tw, th;
    off_t file_size;
    time_t mtime;
    uint8_t *pixels;
    size_t bytes;
    uint64_t used;
} image_cache_t;

static pthread_mutex_t image_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t image_cond = PTHREAD_COND_INITIALIZER;
static pthread_t image_thread;
static int image_thread_started;
static int image_thread_stop;
static image_job_t image_jobs[IMAGE_JOB_CAPACITY];
static int image_job_count;
static image_result_t image_results[IMAGE_JOB_CAPACITY];
static int image_result_count;
static image_generation_t image_generations[IMAGE_JOB_CAPACITY * 2];
static unsigned image_generation_replace;
static image_cache_t image_cache[IMAGE_CACHE_COUNT];
static size_t image_cache_bytes;
static uint64_t image_cache_serial;

static uint64_t image_next_generation(lv_obj_t *obj) {
    uint64_t generation = 1;
    pthread_mutex_lock(&image_lock);
    int slot = -1;
    for (size_t i = 0; i < A_SIZE(image_generations); i++) {
        if (image_generations[i].obj == obj) {
            slot = (int) i;
            break;
        }
        if (slot < 0 && !image_generations[i].obj) slot = (int) i;
    }
    if (slot < 0) slot = (int) (image_generation_replace++ % A_SIZE(image_generations));
    image_generations[slot].obj = obj;
    generation = ++image_generations[slot].generation;
    pthread_mutex_unlock(&image_lock);
    return generation;
}

static int image_generation_current(lv_obj_t *obj, const uint64_t generation) {
    int current = 0;
    pthread_mutex_lock(&image_lock);
    for (size_t i = 0; i < A_SIZE(image_generations); i++) {
        if (image_generations[i].obj == obj) {
            current = image_generations[i].generation == generation;
            break;
        }
    }
    pthread_mutex_unlock(&image_lock);
    return current;
}

static uint8_t *image_scale_bgra(const image_job_t *job, size_t *out_bytes) {
    struct stat st;
    if (stat(job->path, &st) != 0 || st.st_size <= 0 || st.st_size > 32 * 1024 * 1024) return NULL;
    if (job->tw <= 0 || job->th <= 0 || job->tw > 1920 || job->th > 1920 || (int64_t) job->tw * job->th > 1920 * 1080)
        return NULL;

    for (int i = 0; i < IMAGE_CACHE_COUNT; i++) {
        image_cache_t *cache = &image_cache[i];
        if (cache->pixels && cache->tw == job->tw && cache->th == job->th && cache->file_size == st.st_size
            && cache->mtime == st.st_mtime && strcmp(cache->path, job->path) == 0) {
            uint8_t *copy = malloc(cache->bytes);
            if (!copy) return NULL;
            memcpy(copy, cache->pixels, cache->bytes);
            cache->used = ++image_cache_serial;
            *out_bytes = cache->bytes;
            return copy;
        }
    }

    SDL_Surface *loaded = IMG_Load(job->path);
    if (!loaded || loaded->w <= 0 || loaded->h <= 0 || loaded->w > 8192 || loaded->h > 8192
        || (int64_t) loaded->w * loaded->h > 16 * 1024 * 1024) {
        if (loaded) SDL_FreeSurface(loaded);
        return NULL;
    }
    SDL_Surface *surface = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_BGRA32, 0);
    SDL_FreeSurface(loaded);
    if (!surface) return NULL;

    const size_t bytes = (size_t) job->tw * (size_t) job->th * 4u;
    uint8_t *dst = malloc(bytes);
    int *sx0 = malloc(sizeof(*sx0) * (size_t) job->tw);
    int *sx1 = malloc(sizeof(*sx1) * (size_t) job->tw);
    int *sy0 = malloc(sizeof(*sy0) * (size_t) job->th);
    int *sy1 = malloc(sizeof(*sy1) * (size_t) job->th);
    if (!dst || !sx0 || !sx1 || !sy0 || !sy1) {
        free(dst);
        free(sx0);
        free(sx1);
        free(sy0);
        free(sy1);
        SDL_FreeSurface(surface);
        return NULL;
    }

    for (int x = 0; x < job->tw; x++) {
        sx0[x] = x * surface->w / job->tw;
        sx1[x] = (x + 1) * surface->w / job->tw;
        if (sx1[x] <= sx0[x]) sx1[x] = sx0[x] + 1;
    }
    for (int y = 0; y < job->th; y++) {
        sy0[y] = y * surface->h / job->th;
        sy1[y] = (y + 1) * surface->h / job->th;
        if (sy1[y] <= sy0[y]) sy1[y] = sy0[y] + 1;
    }

    for (int y = 0; y < job->th; y++) {
        uint8_t *out = dst + (size_t) y * (size_t) job->tw * 4u;
        for (int x = 0; x < job->tw; x++) {
            uint32_t sum[4] = {0};
            for (int sy = sy0[y]; sy < sy1[y]; sy++) {
                const uint8_t *row = (const uint8_t *) surface->pixels + (size_t) sy * (size_t) surface->pitch;
                int sx = sx0[x];
#if defined(__aarch64__)
                for (; sx + 8 <= sx1[x]; sx += 8) {
                    const uint8x8x4_t pixels = vld4_u8(row + (size_t) sx * 4u);
                    sum[0] += vaddlv_u8(pixels.val[0]);
                    sum[1] += vaddlv_u8(pixels.val[1]);
                    sum[2] += vaddlv_u8(pixels.val[2]);
                    sum[3] += vaddlv_u8(pixels.val[3]);
                }
#endif
                for (; sx < sx1[x]; sx++) {
                    const uint8_t *p = row + (size_t) sx * 4u;
                    sum[0] += p[0];
                    sum[1] += p[1];
                    sum[2] += p[2];
                    sum[3] += p[3];
                }
            }
            const uint32_t count = (uint32_t) (sx1[x] - sx0[x]) * (uint32_t) (sy1[y] - sy0[y]);
            out[0] = (uint8_t) (sum[0] / count);
            out[1] = (uint8_t) (sum[1] / count);
            out[2] = (uint8_t) (sum[2] / count);
            out[3] = (uint8_t) (sum[3] / count);
            out += 4;
        }
    }
    free(sx0);
    free(sx1);
    free(sy0);
    free(sy1);
    SDL_FreeSurface(surface);

    if (bytes <= IMAGE_CACHE_BUDGET / 2u) {
        int slot = -1;
        for (int i = 0; i < IMAGE_CACHE_COUNT; i++) {
            if (!image_cache[i].pixels) {
                slot = i;
                break;
            }
            if (slot < 0 || image_cache[i].used < image_cache[slot].used) slot = i;
        }
        while (slot >= 0 && image_cache_bytes + bytes > IMAGE_CACHE_BUDGET) {
            image_cache_bytes -= image_cache[slot].bytes;
            free(image_cache[slot].pixels);
            memset(&image_cache[slot], 0, sizeof(image_cache[slot]));
            slot = -1;
            for (int i = 0; i < IMAGE_CACHE_COUNT; i++) {
                if (!image_cache[i].pixels) {
                    slot = i;
                    break;
                }
                if (slot < 0 || image_cache[i].used < image_cache[slot].used) slot = i;
            }
        }
        if (slot >= 0) {
            uint8_t *copy = malloc(bytes);
            if (copy) {
                if (image_cache[slot].pixels) image_cache_bytes -= image_cache[slot].bytes;
                free(image_cache[slot].pixels);
                memcpy(copy, dst, bytes);
                image_cache[slot] = (image_cache_t) {.tw = job->tw,
                                                     .th = job->th,
                                                     .file_size = st.st_size,
                                                     .mtime = st.st_mtime,
                                                     .pixels = copy,
                                                     .bytes = bytes,
                                                     .used = ++image_cache_serial};
                snprintf(image_cache[slot].path, sizeof(image_cache[slot].path), "%s", job->path);
                image_cache_bytes += bytes;
            }
        }
    }
    *out_bytes = bytes;
    return dst;
}

static void *image_worker(void *unused) {
    (void) unused;
    for (;;) {
        pthread_mutex_lock(&image_lock);
        while (!image_thread_stop && image_job_count == 0)
            pthread_cond_wait(&image_cond, &image_lock);
        if (image_thread_stop) {
            pthread_mutex_unlock(&image_lock);
            break;
        }
        const image_job_t job = image_jobs[0];
        memmove(image_jobs, image_jobs + 1, (size_t) (--image_job_count) * sizeof(*image_jobs));
        pthread_mutex_unlock(&image_lock);

        const uint64_t start = SDL_GetPerformanceCounter();
        size_t bytes = 0;
        uint8_t *pixels = image_scale_bgra(&job, &bytes);
        const double work_ms =
            (double) (SDL_GetPerformanceCounter() - start) * 1000.0 / (double) SDL_GetPerformanceFrequency();

        pthread_mutex_lock(&image_lock);
        if (pixels) {
            if (image_result_count == IMAGE_JOB_CAPACITY) {
                free(image_results[0].pixels);
                memmove(image_results, image_results + 1, (IMAGE_JOB_CAPACITY - 1u) * sizeof(*image_results));
                image_result_count--;
            }
            image_results[image_result_count++] =
                (image_result_t) {.job = job, .pixels = pixels, .bytes = bytes, .work_ms = work_ms};
        }
        pthread_mutex_unlock(&image_lock);
    }
    return NULL;
}

static int image_async_enqueue(const image_job_t *job) {
    pthread_mutex_lock(&image_lock);
    if (!image_thread_started) {
        image_thread_stop = 0;
        if (pthread_create(&image_thread, NULL, image_worker, NULL) == 0) image_thread_started = 1;
    }
    if (!image_thread_started) {
        pthread_mutex_unlock(&image_lock);
        return 0;
    }
    for (int i = image_job_count - 1; i >= 0; i--) {
        if (image_jobs[i].obj != job->obj) continue;
        memmove(&image_jobs[i], &image_jobs[i + 1], (size_t) (image_job_count - i - 1) * sizeof(*image_jobs));
        image_job_count--;
    }
    if (image_job_count == IMAGE_JOB_CAPACITY) {
        memmove(image_jobs, image_jobs + 1, (IMAGE_JOB_CAPACITY - 1u) * sizeof(*image_jobs));
        image_job_count--;
    }
    image_jobs[image_job_count++] = *job;
    pthread_cond_signal(&image_cond);
    pthread_mutex_unlock(&image_lock);
    return 1;
}

void image_async_tick(void) {
    for (;;) {
        pthread_mutex_lock(&image_lock);
        if (!image_result_count) {
            pthread_mutex_unlock(&image_lock);
            break;
        }
        image_result_t result = image_results[0];
        memmove(image_results, image_results + 1, (size_t) (--image_result_count) * sizeof(*image_results));
        pthread_mutex_unlock(&image_lock);

        fe_perf_record(fe_perf_stage_image, result.work_ms);
        if (!image_generation_current(result.job.obj, result.job.generation) || !lv_obj_is_valid(result.job.obj)) {
            free(result.pixels);
            continue;
        }

        lv_img_dsc_t *dsc = lv_img_buf_alloc(result.job.tw, result.job.th, LV_IMG_CF_TRUE_COLOR_ALPHA);
        if (!dsc || result.bytes != (size_t) result.job.tw * (size_t) result.job.th * LV_IMG_PX_SIZE_ALPHA_BYTE) {
            if (dsc) lv_img_buf_free(dsc);
            free(result.pixels);
            continue;
        }
        memcpy((void *) dsc->data, result.pixels, result.bytes);
        free(result.pixels);

        free_scaled_raster(result.job.obj);
        lv_obj_set_user_data(result.job.obj, dsc);
        lv_img_set_size_mode(result.job.obj, LV_IMG_SIZE_MODE_VIRTUAL);
        lv_img_set_zoom(result.job.obj, LV_IMG_ZOOM_NONE);
        if (result.job.align >= 0) lv_obj_set_align(result.job.obj, result.job.align);
        lv_obj_set_style_pad_left(result.job.obj, result.job.pad_l, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_right(result.job.obj, result.job.pad_r, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_top(result.job.obj, result.job.pad_t, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_bottom(result.job.obj, result.job.pad_b, MU_OBJ_MAIN_DEFAULT);
        lv_img_set_src(result.job.obj, dsc);
        lv_obj_move_foreground(result.job.obj);
    }
}

void image_async_shutdown(void) {
    pthread_mutex_lock(&image_lock);
    const int join = image_thread_started;
    image_thread_stop = 1;
    pthread_cond_signal(&image_cond);
    pthread_mutex_unlock(&image_lock);

    if (join) pthread_join(image_thread, NULL);

    pthread_mutex_lock(&image_lock);
    for (int i = 0; i < image_result_count; i++)
        free(image_results[i].pixels);
    for (int i = 0; i < IMAGE_CACHE_COUNT; i++)
        free(image_cache[i].pixels);
    memset(image_results, 0, sizeof(image_results));
    memset(image_jobs, 0, sizeof(image_jobs));
    memset(image_cache, 0, sizeof(image_cache));
    memset(image_generations, 0, sizeof(image_generations));
    image_result_count = 0;
    image_job_count = 0;
    image_cache_bytes = 0;
    image_thread_started = 0;
    pthread_mutex_unlock(&image_lock);
}

int load_element_image_specifics(
    const char *mux_dim, const char *program, const char *image_type, const char *element, const char *element_fallback,
    const char *image_extension, char *image_path, const size_t path_size
) {
    const char *curr_lang = config.settings.general.language;

    char cache_key[MAX_BUFFER_SIZE];
    snprintf(
        cache_key, sizeof(cache_key), "img_elem:%s/%s/%s/%s/%s/%s/%s", mux_dim, curr_lang, program, image_type, element,
        element_fallback, image_extension
    );

    const int cached = asset_cache_get(cache_key, image_path, path_size);
    if (cached >= 0) return cached;

    const char *dims[] = {mux_dim, ""};
    const char *elements[] = {element, element_fallback};

    int found = 0;
    for (size_t i = 0; i < A_SIZE(dims) && !found; ++i) {
        const char *paths[] = {"%s/%simage/%s/%s/%s/%s.%s", "%s/%simage/%s/%s/%s.%s"};
        for (size_t j = 0; j < A_SIZE(paths) && !found; ++j) {
            for (size_t k = 0; k < A_SIZE(elements) && !found; ++k) {
                int written;

                switch (j) {
                    case 0:
                        written = snprintf(
                            image_path, path_size, paths[j], theme_base, dims[i], curr_lang, image_type, program,
                            elements[k], image_extension
                        );
                        break;
                    case 1:
                    default:
                        written = snprintf(
                            image_path, path_size, paths[j], theme_base, dims[i], image_type, program, elements[k],
                            image_extension
                        );
                        break;
                }

                if (written >= 0 && file_exist_nocase(image_path, image_path, path_size)) found = 1;
            }
        }
    }

    asset_cache_put(cache_key, image_path, found);
    return found;
}

int load_image_specifics(
    const char *mux_dim, const char *program, const char *image_type, const char *image_extension, char *image_path,
    const size_t path_size
) {
    const char *curr_lang = config.settings.general.language;

    char cache_key[MAX_BUFFER_SIZE];
    snprintf(
        cache_key, sizeof(cache_key), "img:%s/%s/%s/%s/%s", mux_dim, curr_lang, program, image_type, image_extension
    );

    const int cached = asset_cache_get(cache_key, image_path, path_size);
    if (cached >= 0) return cached;

    const char *paths[] = {
        "%s/%simage/%s.%s", "%s/%simage/%s/%s/%s.%s", "%s/%simage/%s/%s.%s", "%s/%simage/%s/%s/default.%s",
        "%s/%simage/%s/default.%s"
    };

    int found = 0;
    for (size_t i = 0; i < A_SIZE(paths) && !found; ++i) {
        int written;

        switch (i) {
            case 0:
                written = snprintf(image_path, path_size, paths[i], theme_base, mux_dim, image_type, image_extension);
                break;
            case 1:
                written = snprintf(
                    image_path, path_size, paths[i], theme_base, mux_dim, curr_lang, image_type, program,
                    image_extension
                );
                break;
            case 2:
                written = snprintf(
                    image_path, path_size, paths[i], theme_base, mux_dim, image_type, program, image_extension
                );
                break;
            case 3:
                written = snprintf(
                    image_path, path_size, paths[i], theme_base, mux_dim, curr_lang, image_type, image_extension
                );
                break;
            case 4:
            default:
                written = snprintf(image_path, path_size, paths[i], theme_base, mux_dim, image_type, image_extension);
                break;
        }

        if (written >= 0 && file_exist_nocase(image_path, image_path, path_size)) found = 1;
    }

    asset_cache_put(cache_key, image_path, found);
    return found;
}

char *get_wallpaper_path(lv_obj_t *ui_screen, lv_group_t *ui_group, const int wall_type) {
    const char *program = lv_obj_get_user_data(ui_screen);

    static char wall_image_path[MAX_BUFFER_SIZE];
    static char wall_image_embed[MAX_BUFFER_SIZE];

    const char *element = "";
    if (ui_group != NULL && lv_group_get_obj_count(ui_group) > 0) {
        struct _lv_obj_t *e_focused = lv_group_get_focused(ui_group);
        if (e_focused != NULL) {
            const char *ud = lv_obj_get_user_data(e_focused);
            if (ud) element = ud;
        }
    }

    static char cached_theme_base[MAX_BUFFER_SIZE];
    static int cached_wall_type = -1;
    static int cached_video_wallpaper = -1;
    static char cached_program[MAX_BUFFER_SIZE];
    static char cached_element[MAX_BUFFER_SIZE];

    if (strcmp(theme_base, cached_theme_base) == 0 && wall_type == cached_wall_type
        && config.visual.video_wallpaper == cached_video_wallpaper && strcmp(program, cached_program) == 0
        && strcmp(element, cached_element) == 0) {
        return wall_image_embed;
    }

    snprintf(cached_theme_base, sizeof(cached_theme_base), "%s", theme_base);
    cached_wall_type = wall_type;
    cached_video_wallpaper = config.visual.video_wallpaper;
    snprintf(cached_program, sizeof(cached_program), "%s", program);
    snprintf(cached_element, sizeof(cached_element), "%s", element);
    wall_image_embed[0] = '\0';

#define TRY_EMBED(path_buf)                                                                                            \
    do {                                                                                                               \
        int embed_len = snprintf(wall_image_embed, sizeof(wall_image_embed), "M:%s", (path_buf));                      \
        if (embed_len < 0 || (size_t) embed_len >= sizeof(wall_image_embed)) wall_image_embed[0] = '\0';               \
    } while (0)

    if (config.visual.video_wallpaper) {
        const char *ad_dims[] = {mux_dim, ""};
        for (size_t dim_idx = 0; dim_idx < 2; dim_idx++) {
            int mp4_w;
            if (ui_group != NULL && lv_group_get_obj_count(ui_group) > 0) {
                mp4_w = snprintf(
                    wall_image_path, sizeof(wall_image_path), "%s/%simage/wall/%s.mp4", theme_base, ad_dims[dim_idx],
                    program
                );
                if (mp4_w > 0 && (size_t) mp4_w < sizeof(wall_image_path)
                    && file_exist_nocase(wall_image_path, wall_image_path, sizeof(wall_image_path))) {
                    TRY_EMBED(wall_image_path);
                    return wall_image_embed;
                }
            }
            mp4_w = snprintf(
                wall_image_path, sizeof(wall_image_path), "%s/%simage/background.mp4", theme_base, ad_dims[dim_idx]
            );
            if (mp4_w > 0 && (size_t) mp4_w < sizeof(wall_image_path)
                && file_exist_nocase(wall_image_path, wall_image_path, sizeof(wall_image_path))) {
                TRY_EMBED(wall_image_path);
                return wall_image_embed;
            }
        }
    }

    if (ui_group != NULL && lv_group_get_obj_count(ui_group) > 0) {
        const char *catalogue = NULL;
        switch (wall_type) {
            case wall_application:
                catalogue = "Application";
                break;
            case wall_archive:
                catalogue = "Archive";
                break;
            case wall_task:
                catalogue = "Task";
                break;
            default:
                break;
        }
        if (catalogue
            && load_image_catalogue(
                catalogue, element, "", "default", mux_dim, "wall", wall_image_path, sizeof(wall_image_path)
            )) {
            TRY_EMBED(wall_image_path);
            return wall_image_embed;
        }

        if (load_element_image_specifics(
                mux_dim, program, "wall", strcmp(program, "muxlaunch") == 0 ? element : "default", "default", "png",
                wall_image_path, sizeof(wall_image_path)
            )) {
            TRY_EMBED(wall_image_path);
            return wall_image_embed;
        }
    }

    if (load_image_specifics(mux_dim, program, "wall", "png", wall_image_path, sizeof(wall_image_path))
        || load_image_specifics("", program, "wall", "png", wall_image_path, sizeof(wall_image_path))) {
        TRY_EMBED(wall_image_path);
    } else {
        if (load_image_specifics(mux_dim, program, "wall", "svg", wall_image_path, sizeof(wall_image_path))
            || load_image_specifics("", program, "wall", "svg", wall_image_path, sizeof(wall_image_path))) {
            TRY_EMBED(wall_image_path);
        }
    }

#undef TRY_EMBED

    return wall_image_embed;
}

void load_wallpaper(lv_obj_t *ui_screen, lv_group_t *ui_group, lv_obj_t *ui_img_wall, const int wall_type) {
    static char new_wall[MAX_BUFFER_SIZE];
    snprintf(new_wall, sizeof(new_wall), "%s", get_wallpaper_path(ui_screen, ui_group, wall_type));

    if (strcasecmp(new_wall, current_wall) != 0) {
        snprintf(current_wall, sizeof(current_wall), "%s", new_wall);
        if (strlen(new_wall) > 3) {
            const size_t wall_len = strlen(new_wall);
            const int wall_is_mp4 = wall_len > 6 && strcasecmp(new_wall + wall_len - 4, ".mp4") == 0;
            if (wall_is_mp4) {
                const char *mp4_path = new_wall + 2;
                LOG_DEBUG(mux_module, "Wallpaper video chosen: %s", mp4_path);
                video_wallpaper_play(mp4_path);
                lv_img_set_src(ui_img_wall, &ui_img_blank);
                lv_obj_set_style_bg_opa(ui_screen_container, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
            } else {
                if (video_wallpaper_active()) {
                    video_wallpaper_stop();
                    set_gradient_visible(1);
                }
                const size_t wlen = strlen(new_wall);
                if (wlen > 4 && strcmp(new_wall + wlen - 4, ".svg") == 0) {
                    char svg_wall[MAX_BUFFER_SIZE];
                    snprintf(svg_wall, sizeof(svg_wall), "%s?%dx%d", new_wall, device.mux.width, device.mux.height);
                    lv_img_set_src(ui_img_wall, svg_wall);
                } else {
                    lv_img_set_zoom(ui_img_wall, LV_IMG_ZOOM_NONE);
                    lv_img_set_src(ui_img_wall, new_wall);
                    if (config.visual.background_scale > 0) {
                        int iw = 0, ih = 0;
                        read_image_dims(new_wall + 2, &iw, &ih);
                        if (iw > 0 && ih > 0) {
                            const float wr = (float) device.mux.width / (float) iw;
                            const float hr = (float) device.mux.height / (float) ih;
                            const float zr = config.visual.background_scale == 2 ? (wr > hr ? wr : hr)
                                             : wr < hr                           ? wr
                                                                                 : hr;
                            uint16_t zoom = (uint16_t) (zr * (float) LV_IMG_ZOOM_NONE);
                            if (zoom < 1) zoom = 1;
                            lv_img_set_zoom(ui_img_wall, zoom);
                            lv_img_set_pivot(ui_img_wall, iw / 2, ih / 2);
                            lv_obj_align(ui_img_wall, LV_ALIGN_CENTER, 0, 0);
                        }
                    }
                }
            }
        } else {
            if (video_wallpaper_active()) {
                video_wallpaper_stop();
                set_gradient_visible(1);
            }
            lv_img_set_src(ui_img_wall, &ui_img_blank);
        }
    }
}

char *load_static_image(lv_obj_t *ui_screen, lv_group_t *ui_group, const int wall_type) {
    const char *program = lv_obj_get_user_data(ui_screen);

    static char static_image_path[MAX_BUFFER_SIZE];
    static char static_image_embed[MAX_BUFFER_SIZE];

    if (lv_group_get_obj_count(ui_group) > 0) {
        const char *element = lv_obj_get_user_data(lv_group_get_focused(ui_group));
        switch (wall_type) {
            case wall_application:
                if (grid_mode_enabled && config.visual.box_art_hide) {
                    return "";
                }
                if (load_image_catalogue(
                        "Application", element, "", "default", mux_dim, "box", static_image_path,
                        sizeof(static_image_path)
                    )) {
                    const int written =
                        snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);
                    if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return "";
                    return static_image_embed;
                }
                break;
            case wall_archive:
                if (load_image_catalogue(
                        "Archive", element, "", "default", mux_dim, "box", static_image_path, sizeof(static_image_path)
                    )) {
                    const int written =
                        snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);
                    if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return "";
                    return static_image_embed;
                }
                break;
            case wall_task:
                if (load_image_catalogue(
                        "Task", element, "", "default", mux_dim, "box", static_image_path, sizeof(static_image_path)
                    )) {
                    const int written =
                        snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);
                    if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return "";
                    return static_image_embed;
                }
                break;
            case wall_general:
            default:
                if (load_element_image_specifics(
                        mux_dim, program, "static", strcmp(program, "muxlaunch") == 0 ? element : "default", "default",
                        "png", static_image_path, sizeof(static_image_path)
                    )) {

                    const int written =
                        snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);
                    if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return "";
                    return static_image_embed;
                }
        }
    }

    return "";
}

int resolve_overlay_pattern_image(const int value, char *path, const size_t path_size) {
    snprintf(path, path_size, "%s/%s%d.png", STORAGE_OVERLAY, mux_dim, value);
    if (!file_exist(path)) {
        snprintf(path, path_size, "%s/standard/%d.png", STORAGE_OVERLAY, value);
    }
    return file_exist(path);
}

void load_overlay_image(lv_obj_t *ui_screen, lv_obj_t *overlay_image) {
    if (config.visual.overlay_image == 0) return;

    const char *program = lv_obj_get_user_data(ui_screen);

    static char static_image_path[MAX_BUFFER_SIZE];
    static char static_image_embed[MAX_BUFFER_SIZE];

    switch (config.visual.overlay_image) {
        case 1:
            if (load_image_specifics(mux_dim, program, "overlay", "png", static_image_path, sizeof(static_image_path))
                || load_image_specifics("", program, "overlay", "png", static_image_path, sizeof(static_image_path))) {
                const int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);
                if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return;
            } else {
                return;
            }
            break;
        default: {
            resolve_overlay_pattern_image(config.visual.overlay_image, static_image_path, sizeof(static_image_path));
            const int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);
            if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return;
            break;
        }
    }

    if (!file_exist(static_image_path)) return;

    lv_obj_set_size(overlay_image, device.screen.width, device.screen.height);
    lv_obj_set_pos(overlay_image, 0, 0);
    lv_obj_set_style_bg_opa(overlay_image, LV_OPA_TRANSP, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_border_width(overlay_image, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_shadow_width(overlay_image, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_all(overlay_image, 0, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_img_src(overlay_image, static_image_embed, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_img_tiled(overlay_image, 1, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_bg_img_opa(overlay_image, config.visual.overlay_transparency, MU_OBJ_MAIN_DEFAULT);
    mu_img_no_shadow(overlay_image);
    lv_obj_move_foreground(overlay_image);
}

void load_overlay_image_sdl(void) {
    if (config.visual.overlay_image == 0) {
        display_clear_theme_overlay();
        return;
    }

    const char *program = lv_obj_get_user_data(ui_screen);
    char image_path[MAX_BUFFER_SIZE];

    switch (config.visual.overlay_image) {
        case 1:
            if (!load_image_specifics(mux_dim, program, "overlay", "png", image_path, sizeof(image_path))
                && !load_image_specifics("", program, "overlay", "png", image_path, sizeof(image_path))) {
                return;
            }
            break;
        default:
            resolve_overlay_pattern_image(config.visual.overlay_image, image_path, sizeof(image_path));
            break;
    }

    if (!file_exist(image_path)) return;

    SDL_Texture *tex = display_load_png_texture(image_path);
    if (tex) display_set_theme_overlay(tex, (uint8_t) config.visual.overlay_transparency);
}

void load_kiosk_image(lv_obj_t *ui_screen, lv_obj_t *kiosk_image) {
    const char *program = lv_obj_get_user_data(ui_screen);

    static char static_image_path[MAX_BUFFER_SIZE];
    static char static_image_embed[MAX_BUFFER_SIZE];

    if (load_image_specifics(mux_dim, program, "kiosk", "png", static_image_path, sizeof(static_image_path))
        || load_image_specifics("", program, "kiosk", "png", static_image_path, sizeof(static_image_path))) {

        const int written = snprintf(static_image_embed, sizeof(static_image_embed), "M:%s", static_image_path);
        if (written < 0 || (size_t) written >= sizeof(static_image_embed)) return;

        lv_img_set_src(kiosk_image, static_image_embed);
        mu_img_no_shadow(kiosk_image);
        lv_obj_move_foreground(kiosk_image);
    }
}

int load_terminal_resource(const char *resource, const char *extension, char *buffer, const size_t size) {
    const char *dims[] = {mux_dim, ""};

    for (size_t i = 0; i < 2; i++) {
        snprintf(buffer, size, "%s/%s%s/muterm.%s", theme_base, dims[i], resource, extension);
        if (file_exist_nocase(buffer, buffer, size)) return 1;
    }

    return 0;
}

void unload_image_animation(void) {
    if (video_wallpaper_active()) {
        video_wallpaper_stop();
        set_gradient_visible(1);
    }
    current_wall[0] = '\0';
}

static void read_image_dims(const char *path, int *w, int *h) {
    *w = 0;
    *h = 0;
    FILE *f = fopen(path, "rb");
    if (!f) return;

    unsigned char buf[25];
    const size_t n = fread(buf, 1, sizeof(buf), f);

    if (n >= 24 && buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G') {
        *w = buf[16] << 24 | buf[17] << 16 | buf[18] << 8 | buf[19];
        *h = buf[20] << 24 | buf[21] << 16 | buf[22] << 8 | buf[23];
    } else if (n >= 2 && buf[0] == 0xFF && buf[1] == 0xD8) {
        unsigned char mk[2];
        fseek(f, 2, SEEK_SET);
        while (!(fread(mk, 1, 2, f) != 2 || mk[0] != 0xFF)) {

            if (mk[1] == 0xD9 || mk[1] == 0xDA) break;
            if (mk[1] == 0x01 || (mk[1] >= 0xD0 && mk[1] <= 0xD9)) continue;
            unsigned char lb[2];

            if (fread(lb, 1, 2, f) != 2) break;
            const int seg = lb[0] << 8 | lb[1];

            if (mk[1] >= 0xC0 && mk[1] <= 0xC3) {
                unsigned char sof[5];
                if (fread(sof, 1, 5, f) == 5) {
                    *h = sof[1] << 8 | sof[2];
                    *w = sof[3] << 8 | sof[4];
                }
                break;
            }

            if (seg < 2 || fseek(f, seg - 2, SEEK_CUR) != 0) break;
        }
    }

    fclose(f);

    if (*w <= 0 || *h <= 0) {
        char source[MAX_BUFFER_SIZE];
        lv_img_header_t header;
        const int written = snprintf(source, sizeof(source), "M:%s", path);
        if (written > 0 && (size_t) written < sizeof(source) && lv_img_decoder_get_info(source, &header) == LV_RES_OK) {
            *w = header.w;
            *h = header.h;
        }
    }
}

static void free_scaled_raster(lv_obj_t *ui_img_obj) {
    lv_img_dsc_t *old_dsc = lv_obj_get_user_data(ui_img_obj);
    if (old_dsc) {
        lv_img_cache_invalidate_src(old_dsc);

        const lv_disp_t *disp = lv_disp_get_default();
        if (disp && disp->driver && disp->driver->draw_ctx) {
            lv_draw_sdl_texture_cache_remove_src((lv_draw_sdl_ctx_t *) disp->driver->draw_ctx, old_dsc);
        }

        lv_img_buf_free(old_dsc);
        lv_obj_set_user_data(ui_img_obj, NULL);
    }
}

void clear_image(lv_obj_t *ui_img_obj) {
    if (!ui_img_obj) return;
    image_next_generation(ui_img_obj);
    lv_img_set_src(ui_img_obj, &ui_img_blank);
    free_scaled_raster(ui_img_obj);
}

static void scale_and_set_raster(
    lv_obj_t *ui_img_obj, const char *image_path, const int tw, const int th, const int align, const int pad_l,
    const int pad_r, const int pad_t, const int pad_b
) {
    char lvgl_path[MAX_BUFFER_SIZE];
    snprintf(lvgl_path, sizeof(lvgl_path), "M:%s", image_path);

    lv_img_decoder_dsc_t decode_dsc = {0};
    if (lv_img_decoder_open(&decode_dsc, lvgl_path, lv_color_white(), 0) != LV_RES_OK) return;

    const int sw = (int) decode_dsc.header.w;
    const int sh = (int) decode_dsc.header.h;
    if (sw <= 0 || sh <= 0) {
        lv_img_decoder_close(&decode_dsc);
        return;
    }

    uint8_t *src_buf = NULL;
    int src_allocated = 0;
    if (decode_dsc.img_data) {
        src_buf = (uint8_t *) decode_dsc.img_data;
    } else {
        src_buf = lv_mem_alloc((size_t) sw * sh * LV_IMG_PX_SIZE_ALPHA_BYTE);
        if (!src_buf) {
            lv_img_decoder_close(&decode_dsc);
            return;
        }

        src_allocated = 1;
        for (int y = 0; y < sh; y++) {
            if (lv_img_decoder_read_line(&decode_dsc, 0, y, sw, src_buf + (size_t) y * sw * LV_IMG_PX_SIZE_ALPHA_BYTE)
                != LV_RES_OK) {
                lv_mem_free(src_buf);
                lv_img_decoder_close(&decode_dsc);
                return;
            }
        }
    }

    lv_img_dsc_t *scaled_dsc = lv_img_buf_alloc(tw, th, LV_IMG_CF_TRUE_COLOR_ALPHA);
    if (!scaled_dsc) {
        if (src_allocated) lv_mem_free(src_buf);
        lv_img_decoder_close(&decode_dsc);
        return;
    }

    int *sx0 = lv_mem_alloc(sizeof(int) * (size_t) tw);
    int *sx1 = lv_mem_alloc(sizeof(int) * (size_t) tw);
    int *sy0 = lv_mem_alloc(sizeof(int) * (size_t) th);
    int *sy1 = lv_mem_alloc(sizeof(int) * (size_t) th);

    if (!sx0 || !sx1 || !sy0 || !sy1) {
        lv_mem_free(sx0);
        lv_mem_free(sx1);
        lv_mem_free(sy0);
        lv_mem_free(sy1);
        if (src_allocated) lv_mem_free(src_buf);
        lv_img_buf_free(scaled_dsc);
        lv_img_decoder_close(&decode_dsc);
        return;
    }

    for (int dx = 0; dx < tw; dx++) {
        sx0[dx] = dx * sw / tw;
        sx1[dx] = (dx + 1) * sw / tw;
        if (sx1[dx] <= sx0[dx]) sx1[dx] = sx0[dx] + 1;
    }
    for (int dy = 0; dy < th; dy++) {
        sy0[dy] = dy * sh / th;
        sy1[dy] = (dy + 1) * sh / th;
        if (sy1[dy] <= sy0[dy]) sy1[dy] = sy0[dy] + 1;
    }

    uint8_t *dst = (uint8_t *) scaled_dsc->data;
    for (int dy = 0; dy < th; dy++) {
        uint8_t *out_row = dst + (size_t) dy * tw * LV_IMG_PX_SIZE_ALPHA_BYTE;

        for (int dx = 0; dx < tw; dx++) {
            uint32_t acc0 = 0, acc1 = 0, acc2 = 0, acc3 = 0;

            for (int sy = sy0[dy]; sy < sy1[dy]; sy++) {
                const uint8_t *row = src_buf + (size_t) sy * sw * LV_IMG_PX_SIZE_ALPHA_BYTE;

                for (int sx = sx0[dx]; sx < sx1[dx]; sx++) {
                    const uint8_t *px = row + (size_t) sx * LV_IMG_PX_SIZE_ALPHA_BYTE;

                    acc0 += px[0];
                    acc1 += px[1];
                    acc2 += px[2];
                    acc3 += px[3];
                }
            }

            const uint32_t n = (uint32_t) (sx1[dx] - sx0[dx]) * (uint32_t) (sy1[dy] - sy0[dy]);
            uint8_t *out = out_row + (size_t) dx * LV_IMG_PX_SIZE_ALPHA_BYTE;

            out[0] = (uint8_t) (acc0 / n);
            out[1] = (uint8_t) (acc1 / n);
            out[2] = (uint8_t) (acc2 / n);
            out[3] = (uint8_t) (acc3 / n);
        }
    }

    lv_mem_free(sx0);
    lv_mem_free(sx1);
    lv_mem_free(sy0);
    lv_mem_free(sy1);

    if (src_allocated) lv_mem_free(src_buf);
    lv_img_decoder_close(&decode_dsc);

    lv_obj_set_user_data(ui_img_obj, scaled_dsc);
    lv_img_set_size_mode(ui_img_obj, LV_IMG_SIZE_MODE_VIRTUAL);
    lv_img_set_zoom(ui_img_obj, LV_IMG_ZOOM_NONE);

    if (align >= 0) lv_obj_set_align(ui_img_obj, align);

    lv_obj_set_style_pad_left(ui_img_obj, pad_l, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_right(ui_img_obj, pad_r, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_top(ui_img_obj, pad_t, MU_OBJ_MAIN_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_img_obj, pad_b, MU_OBJ_MAIN_DEFAULT);

    lv_img_set_src(ui_img_obj, scaled_dsc);
    lv_obj_move_foreground(ui_img_obj);
}

static void update_image_inner(lv_obj_t *ui_img_obj, const struct image_settings image_settings) {
    clear_image(ui_img_obj);

    if (file_exist(image_settings.image_path)) {
        const size_t plen = strlen(image_settings.image_path);
        const int is_svg = plen > 4 && strcasecmp(image_settings.image_path + plen - 4, ".svg") == 0;

        if (!is_svg && image_settings.max_width > 0 && image_settings.max_height > 0) {
            int iw = 0, ih = 0;
            read_image_dims(image_settings.image_path, &iw, &ih);
            if (iw > 0 && ih > 0 && iw <= 8192 && ih <= 8192 && (int64_t) iw * ih <= 16 * 1024 * 1024) {
                const float wr = (float) image_settings.max_width / (float) iw;
                const float hr = (float) image_settings.max_height / (float) ih;

                const float zr = wr < hr ? wr : hr;

                const int tw = (int) ((float) iw * zr);
                const int th = (int) ((float) ih * zr);

                if (tw > 0 && th > 0) {
                    image_job_t job = {
                        .obj = ui_img_obj,
                        .generation = image_next_generation(ui_img_obj),
                        .tw = tw,
                        .th = th,
                        .align = image_settings.align,
                        .pad_l = image_settings.pad_left,
                        .pad_r = image_settings.pad_right,
                        .pad_t = image_settings.pad_top,
                        .pad_b = image_settings.pad_bottom,
                    };
                    snprintf(job.path, sizeof(job.path), "%s", image_settings.image_path);
                    if (!image_async_enqueue(&job)) {
                        scale_and_set_raster(
                            ui_img_obj, image_settings.image_path, tw, th, image_settings.align,
                            image_settings.pad_left, image_settings.pad_right, image_settings.pad_top,
                            image_settings.pad_bottom
                        );
                    }
                    return;
                }
            }
        }

        char image_path[MAX_BUFFER_SIZE];
        if (is_svg && image_settings.max_width > 0 && image_settings.max_height > 0) {
            build_embed_path(
                image_path, sizeof(image_path), image_settings.image_path, image_settings.max_width,
                image_settings.max_height
            );
        } else {
            snprintf(image_path, sizeof(image_path), "M:%s", image_settings.image_path);
        }

        lv_img_set_size_mode(ui_img_obj, LV_IMG_SIZE_MODE_VIRTUAL);
        lv_img_set_zoom(ui_img_obj, LV_IMG_ZOOM_NONE);

        if (image_settings.align >= 0) lv_obj_set_align(ui_img_obj, image_settings.align);
        lv_obj_set_style_pad_left(ui_img_obj, image_settings.pad_left, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_right(ui_img_obj, image_settings.pad_right, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_top(ui_img_obj, image_settings.pad_top, MU_OBJ_MAIN_DEFAULT);
        lv_obj_set_style_pad_bottom(ui_img_obj, image_settings.pad_bottom, MU_OBJ_MAIN_DEFAULT);

        lv_img_set_src(ui_img_obj, image_path);
        lv_obj_move_foreground(ui_img_obj);
    }
}

void update_image(lv_obj_t *ui_img_obj, const struct image_settings image_settings) {
    const uint64_t image_start = fe_perf_begin();
    update_image_inner(ui_img_obj, image_settings);
    fe_perf_end(fe_perf_stage_image, image_start);
}
