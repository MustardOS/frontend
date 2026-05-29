#include "anim.h"
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include "log.h"

#define ANIM_MAX_FRAMES  64
#define ANIM_DEBOUNCE_MS 150
#define ANIM_STATE_FILE  "/run/muos/anim.state"

typedef struct {
    SDL_Surface *surface;
    SDL_Texture *texture;
} anim_frame_t;

static struct {
    anim_frame_t frames[ANIM_MAX_FRAMES];
    int frame_count;
    int frames_ready;
    int current_frame;
    int frame_delay_ms;
    int foreground;
    int position;
    int alpha;
    uint32_t last_tick;
    SDL_Texture *gradient_tex;
    SDL_mutex *mutex;
    SDL_Thread *loader;
    int stop;
    char base_path[PATH_MAX];
} anim;

static struct {
    char base_path[PATH_MAX];
    int frame_delay_ms;
    int foreground;
    int position;
    int alpha;
    int pending;
    uint32_t request_tick;
} pending;

static int64_t mono_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void save_state(void) {
    if (!anim.mutex || anim.frames_ready == 0) return;

    int64_t epoch_ms = mono_ms() - (int64_t) anim.current_frame * anim.frame_delay_ms;

    FILE *f = fopen(ANIM_STATE_FILE, "w");
    if (!f) {
        LOG_WARN("anim", "save_state: could not open %s: %s", ANIM_STATE_FILE, strerror(errno));
        return;
    }

    fprintf(f, "%s\n%d\n%lld\n%d\n", anim.base_path, anim.frame_delay_ms, (long long) epoch_ms, anim.frames_ready);
    fclose(f);

    LOG_INFO("anim", "save_state: path=%s delay=%d epoch=%lld frames=%d", anim.base_path, anim.frame_delay_ms, (long long) epoch_ms, anim.frames_ready);
}

static int restore_state(const char *base_path, int frame_delay_ms, int frame_count) {
    FILE *f = fopen(ANIM_STATE_FILE, "r");
    if (!f) return 0;

    char saved_path[PATH_MAX];
    int saved_delay, saved_frames;
    long long saved_epoch;

    int ok = fscanf(f, "%4095s\n%d\n%lld\n%d\n", saved_path, &saved_delay, &saved_epoch, &saved_frames) == 4;
    fclose(f);

    if (!ok || strcmp(saved_path, base_path) != 0 || saved_delay != frame_delay_ms) {
        LOG_INFO("anim", "restore_state: no matching state for %s", base_path);
        return 0;
    }

    if (frame_count <= 0) frame_count = saved_frames;
    if (frame_count <= 0) return 0;

    int64_t elapsed = mono_ms() - saved_epoch;
    int frame = (int) ((elapsed / frame_delay_ms) % frame_count);
    if (frame < 0) frame = 0;

    LOG_INFO("anim", "restore_state: elapsed=%lldms → frame %d / %d", (long long) elapsed, frame, frame_count);
    return frame;
}

static int loader_thread(void *unused) {
    (void) unused;

    LOG_INFO("anim", "Loader thread started, base path: %s", anim.base_path);

    int found = 0;

    for (int i = 0; i < ANIM_MAX_FRAMES; i++) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s.%d.png", anim.base_path, i);

        FILE *f = fopen(path, "r");
        if (!f) {
            LOG_INFO("anim", "No frame at index %d (%s) - stopping discovery", i, path);
            break;
        }
        fclose(f);

        LOG_INFO("anim", "Decoding frame %d: %s", i, path);

        SDL_Surface *surf = IMG_Load(path);
        if (!surf) {
            LOG_WARN("anim", "Failed to decode frame %d: %s", i, IMG_GetError());
            break;
        }

        LOG_INFO("anim", "Frame %d decoded (%dx%d)", i, surf->w, surf->h);

        SDL_LockMutex(anim.mutex);
        int should_stop = anim.stop;
        if (!should_stop) {
            anim.frames[anim.frame_count].surface = surf;
            anim.frame_count++;
            found++;
        }
        SDL_UnlockMutex(anim.mutex);

        if (should_stop) {
            LOG_INFO("anim", "Loader interrupted after %d frame(s)", found);
            SDL_FreeSurface(surf);
            break;
        }
    }

    LOG_INFO("anim", "Loader thread finished - %d frame(s) ready for upload", found);
    return 0;
}

void anim_unload(void) {
    if (!anim.mutex) return;

    LOG_INFO("anim", "Unloading animation (%d frame(s) loaded, %d uploaded)", anim.frame_count, anim.frames_ready);

    SDL_LockMutex(anim.mutex);
    anim.stop = 1;
    SDL_UnlockMutex(anim.mutex);

    if (anim.loader) {
        SDL_WaitThread(anim.loader, NULL);
        anim.loader = NULL;
    }

    SDL_LockMutex(anim.mutex);
    for (int i = 0; i < anim.frame_count; i++) {
        if (anim.frames[i].surface) {
            SDL_FreeSurface(anim.frames[i].surface);
            anim.frames[i].surface = NULL;
        }
    }
    SDL_UnlockMutex(anim.mutex);

    for (int i = 0; i < anim.frames_ready; i++) {
        if (anim.frames[i].texture) {
            SDL_DestroyTexture(anim.frames[i].texture);
            anim.frames[i].texture = NULL;
        }
    }

    SDL_DestroyMutex(anim.mutex);
    anim.mutex = NULL;

    if (anim.gradient_tex) {
        SDL_DestroyTexture(anim.gradient_tex);
        anim.gradient_tex = NULL;
    }

    anim.frame_count = 0;
    anim.frames_ready = 0;
    anim.current_frame = 0;
    anim.base_path[0] = '\0';

    LOG_INFO("anim", "Animation unloaded");
}

static void load_impl(const char *base_path, int frame_delay_ms, int foreground, int position, int alpha) {
    save_state();
    anim_unload();

    if (strlen(base_path) >= sizeof(anim.base_path)) {
        LOG_ERROR("anim", "Base path exceeds maximum length: %s", base_path);
        return;
    }

    snprintf(anim.base_path, sizeof(anim.base_path), "%s", base_path);

    anim.frame_delay_ms = frame_delay_ms > 0 ? frame_delay_ms : 100;
    anim.foreground = foreground;
    anim.position = position < 0 ? 0 : position > 8 ? 8 : position;
    anim.alpha = alpha < 0 ? 0 : alpha > 255 ? 255 : alpha;
    anim.frame_count = 0;
    anim.frames_ready = 0;
    anim.stop = 0;

    anim.current_frame = restore_state(base_path, anim.frame_delay_ms, 0);
    anim.last_tick = SDL_GetTicks();

    LOG_INFO("anim", "load_impl: base=%s delay=%dms foreground=%d position=%d start_frame=%d",
             anim.base_path, anim.frame_delay_ms, anim.foreground, anim.position, anim.current_frame);

    anim.mutex = SDL_CreateMutex();
    if (!anim.mutex) {
        LOG_ERROR("anim", "Failed to create mutex: %s", SDL_GetError());
        return;
    }

    anim.loader = SDL_CreateThread(loader_thread, "anim_loader", NULL);
    if (!anim.loader) {
        LOG_ERROR("anim", "Failed to create loader thread: %s", SDL_GetError());
        SDL_DestroyMutex(anim.mutex);
        anim.mutex = NULL;
        return;
    }

    LOG_INFO("anim", "Loader thread launched");
}

void anim_set_gradient(SDL_Texture *tex) {
    if (anim.gradient_tex) SDL_DestroyTexture(anim.gradient_tex);
    anim.gradient_tex = tex;
}

void anim_request(const char *embed_path, int frame_delay_ms, int foreground, int position, int alpha) {
    LOG_INFO("anim", "anim_request: embed_path=%s delay=%dms foreground=%d position=%d alpha=%d", embed_path, frame_delay_ms, foreground, position, alpha);
    const char *raw = (embed_path[0] == 'M' && embed_path[1] == ':') ? embed_path + 2 : embed_path;

    size_t len = strlen(raw);
    static const char suffix[] = ".0.png";
    const size_t suffix_len = sizeof(suffix) - 1;
    if (len > suffix_len && strcmp(raw + len - suffix_len, suffix) == 0) len -= suffix_len;

    if (len >= sizeof(pending.base_path)) {
        LOG_ERROR("anim", "anim_request: path too long: %s", raw);
        return;
    }

    memcpy(pending.base_path, raw, len);
    pending.base_path[len] = '\0';

    if (anim.mutex && strcmp(pending.base_path, anim.base_path) == 0) {
        LOG_INFO("anim", "anim_request: same path already active, skipping");
        pending.pending = 0;
        return;
    }

    pending.frame_delay_ms = frame_delay_ms > 0 ? frame_delay_ms : 100;
    pending.foreground = foreground;
    pending.position = position < 0 ? 0 : position > 8 ? 8 : position;
    pending.alpha = alpha < 0 ? 0 : alpha > 255 ? 255 : alpha;
    pending.pending = 1;
    pending.request_tick = SDL_GetTicks();

    LOG_INFO("anim", "anim_request: pending load for %s (debounce %dms)", pending.base_path, ANIM_DEBOUNCE_MS);
}

void anim_process(void) {
    if (!pending.pending) return;

    uint32_t elapsed = SDL_GetTicks() - pending.request_tick;
    if (anim.mutex && (int) elapsed < ANIM_DEBOUNCE_MS) return;

    pending.pending = 0;

    LOG_INFO("anim", "anim_process: %s (%ums), loading %s foreground=%d position=%d alpha=%d",
             anim.mutex ? "debounce elapsed" : "first load", elapsed, pending.base_path, pending.foreground, pending.position, pending.alpha);

    load_impl(pending.base_path, pending.frame_delay_ms, pending.foreground, pending.position, pending.alpha);
}

void anim_tick(SDL_Renderer *renderer) {
    SDL_LockMutex(anim.mutex);
    int count = anim.frame_count;
    SDL_UnlockMutex(anim.mutex);

    while (anim.frames_ready < count) {
        int i = anim.frames_ready;

        SDL_LockMutex(anim.mutex);
        SDL_Surface *surf = anim.frames[i].surface;
        anim.frames[i].surface = NULL;
        SDL_UnlockMutex(anim.mutex);

        if (surf) {
            anim.frames[i].texture = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_FreeSurface(surf);
            if (anim.frames[i].texture) {
                SDL_SetTextureBlendMode(anim.frames[i].texture, SDL_BLENDMODE_BLEND);
                SDL_SetTextureAlphaMod(anim.frames[i].texture, (uint8_t) anim.alpha);
                LOG_INFO("anim", "Frame %d uploaded to GPU", i);
            } else {
                LOG_WARN("anim", "Failed to upload frame %d to GPU: %s", i, SDL_GetError());
            }
        }

        anim.frames_ready++;
    }

    if (anim.frames_ready == 0) return;
    if (anim.current_frame >= anim.frames_ready) anim.current_frame = anim.current_frame % anim.frames_ready;

    uint32_t now = SDL_GetTicks();
    if ((int) (now - anim.last_tick) >= anim.frame_delay_ms) {
        anim.current_frame = (anim.current_frame + 1) % anim.frames_ready;
        anim.last_tick = now;
    }

    if (!anim.foreground && anim.gradient_tex)
        SDL_RenderCopy(renderer, anim.gradient_tex, NULL, NULL);

    SDL_Texture *tex = anim.frames[anim.current_frame].texture;
    if (tex) {
        int tw, th, rw, rh;
        SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
        SDL_GetRendererOutputSize(renderer, &rw, &rh);

        int col = anim.position % 3;
        int row = anim.position / 3;

        int x = col == 0 ? 0 : col == 1 ? (rw - tw) / 2 : rw - tw;
        int y = row == 0 ? 0 : row == 1 ? (rh - th) / 2 : rh - th;

        SDL_Rect dst = {x, y, tw, th};
        SDL_RenderCopy(renderer, tex, NULL, &dst);
    }
}

int anim_is_active(void) {
    return anim.mutex != NULL;
}

int anim_is_foreground(void) {
    return anim.foreground;
}

int anim_frames_ready(void) {
    return anim.frames_ready;
}
