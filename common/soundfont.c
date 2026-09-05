#include <dirent.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <strings.h>
#include <SDL2/SDL_mixer.h>
#include "soundfont.h"
#include "audio.h"
#include "device.h"
#include "fileio.h"
#include "log.h"
#include "options.h"
#include "strutil.h"

#define SOUNDFONT_DIR_MAX 4
#define PREVIEW_ALT_ODDS  32

static const char *sf_exts[] = {".sf2", ".sf3"};

static pthread_mutex_t sf_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t sf_thread;

static int sf_state = soundfont_idle;
static int sf_thread_live = 0;
static int sf_generation = 0;
static int sf_load_done = 0;
static int sf_bgm_yielded = 0;

static char sf_playing[MAX_BUFFER_SIZE];
static char sf_pending_path[PATH_MAX];
static char sf_pending_midi[PATH_MAX];

static Mix_Music *sf_music = NULL;
static Mix_Music *sf_loaded = NULL;

static void soundfont_preview_reset(int restore);

static size_t soundfont_dirs(char dirs[SOUNDFONT_DIR_MAX][PATH_MAX]) {
    const char *mounts[] = {device.storage.usb.mount, device.storage.sdcard.mount, device.storage.rom.mount};

    size_t count = 0;

    snprintf(dirs[count], PATH_MAX, "%s", STORAGE_SOUNDFONT);
    count++;

    for (size_t m = 0; m < A_SIZE(mounts) && count < SOUNDFONT_DIR_MAX; m++) {
        if (!mounts[m] || !*mounts[m]) continue;

        snprintf(dirs[count], PATH_MAX, "%s/%s", mounts[m], USER_SOUNDFONTS);
        remove_double_slashes(dirs[count]);
        count++;
    }

    return count;
}

static int soundfont_is_font(const char *file, size_t *stem_len) {
    const size_t len = strlen(file);

    for (size_t e = 0; e < A_SIZE(sf_exts); e++) {
        const size_t ext_len = strlen(sf_exts[e]);
        if (len > ext_len && !strcasecmp(file + len - ext_len, sf_exts[e])) {
            *stem_len = len - ext_len;
            return 1;
        }
    }

    return 0;
}

int soundfont_scan(char ***names, size_t *count) {
    char dirs[SOUNDFONT_DIR_MAX][PATH_MAX];
    const size_t dir_count = soundfont_dirs(dirs);

    char **list = NULL;
    size_t used = 0;
    size_t capacity = 0;

    for (size_t d = 0; d < dir_count; d++) {
        struct dirent **entries;
        const int n = scandir(dirs[d], &entries, NULL, alphasort);
        if (n < 0) continue;

        for (int i = 0; i < n; i++) {
            size_t stem_len = 0;

            if (soundfont_is_font(entries[i]->d_name, &stem_len)) {
                char name[MAX_BUFFER_SIZE];
                snprintf(name, sizeof(name), "%.*s", (int) stem_len, entries[i]->d_name);

                int duplicate = 0;
                for (size_t j = 0; j < used; j++) {
                    if (!strcasecmp(list[j], name)) {
                        duplicate = 1;
                        break;
                    }
                }

                if (!duplicate) {
                    if (used >= capacity) {
                        const size_t grown = capacity ? capacity * 2 : 8;
                        char **temp = realloc(list, grown * sizeof(char *));

                        if (!temp) {
                            free_array(list, used);
                            for (int k = i; k < n; k++)
                                free(entries[k]);
                            free(entries);
                            return -1;
                        }

                        list = temp;
                        capacity = grown;
                    }

                    list[used] = strdup(name);
                    if (list[used]) used++;
                }
            }

            free(entries[i]);
        }

        free(entries);
    }

    if (used > 1) qsort(list, used, sizeof(char *), str_compare);

    *names = list;
    *count = used;

    return 0;
}

int soundfont_resolve(const char *name, char *path, const size_t path_size) {
    if (!name || !*name) return 0;

    char dirs[SOUNDFONT_DIR_MAX][PATH_MAX];
    const size_t dir_count = soundfont_dirs(dirs);

    for (size_t d = 0; d < dir_count; d++) {
        for (size_t e = 0; e < A_SIZE(sf_exts); e++) {
            char candidate[PATH_MAX];
            snprintf(candidate, sizeof(candidate), "%s/%s%s", dirs[d], name, sf_exts[e]);

            if (file_exist_nocase(candidate, candidate, sizeof(candidate))) {
                snprintf(path, path_size, "%s", candidate);
                return 1;
            }
        }
    }

    return 0;
}

// Small self contained generator so the background music seeding is left alone
static int preview_alt_pick(void) {
    static uint32_t state = 0;

    if (!state) {
        state = (uint32_t) time(NULL) ^ (uint32_t) (uintptr_t) &state;
        if (!state) state = 1;
    }

    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;

    return state % PREVIEW_ALT_ODDS == 0;
}

static void soundfont_preview_midi(const char *font_path, char *midi) {
    if (preview_alt_pick() && file_exist(SOUNDFONT_PREVIEW_ALT)) {
        snprintf(midi, PATH_MAX, "%s", SOUNDFONT_PREVIEW_ALT);
        return;
    }

    char stem[PATH_MAX];
    snprintf(stem, sizeof(stem), "%s", font_path);

    char *dot = strrchr(stem, '.');
    if (dot) *dot = '\0';

    char candidate[PATH_MAX];
    snprintf(candidate, sizeof(candidate), "%s.mid", stem);

    if (file_exist_nocase(candidate, candidate, sizeof(candidate))) {
        snprintf(midi, PATH_MAX, "%s", candidate);
        return;
    }

    snprintf(midi, PATH_MAX, "%s", SOUNDFONT_PREVIEW);
}

static void *soundfont_load_thread(void *arg) {
    const int generation = (int) (intptr_t) arg;

    pthread_mutex_lock(&sf_lock);
    char font[PATH_MAX];
    char midi[PATH_MAX];
    snprintf(font, sizeof(font), "%s", sf_pending_path);
    snprintf(midi, sizeof(midi), "%s", sf_pending_midi);
    pthread_mutex_unlock(&sf_lock);

    Mix_SetSoundFonts(font);
    Mix_Music *music = Mix_LoadMUS(midi);

    pthread_mutex_lock(&sf_lock);
    if (generation != sf_generation) {
        if (music) Mix_FreeMusic(music);
    } else {
        sf_loaded = music;
        sf_load_done = 1;
    }
    pthread_mutex_unlock(&sf_lock);

    return NULL;
}

static void soundfont_preview_release(void) {
    if (sf_music) {
        Mix_FreeMusic(sf_music);
        sf_music = NULL;
    }

    if (sf_loaded) {
        Mix_FreeMusic(sf_loaded);
        sf_loaded = NULL;
    }
}

static void soundfont_restore_bgm(void) {
    if (!sf_bgm_yielded) return;

    sf_bgm_yielded = 0;

    Mix_HookMusicFinished(play_random_bgm);
    if (fe_bgm) play_random_bgm();
}

void soundfont_preview_start(const char *name) {
    soundfont_preview_reset(0);

    char font[PATH_MAX];
    if (!soundfont_resolve(name, font, sizeof(font))) {
        LOG_ERROR("soundfont", "Soundfont not found: %s", name ? name : "");
        sf_state = soundfont_failed;
        soundfont_restore_bgm();
        return;
    }

    char midi[PATH_MAX];
    soundfont_preview_midi(font, midi);

    if (!file_exist(midi)) {
        LOG_ERROR("soundfont", "Preview tune missing: %s", midi);
        sf_state = soundfont_failed;
        soundfont_restore_bgm();
        return;
    }

    Mix_HookMusicFinished(NULL);
    free_bgm();
    sf_bgm_yielded = 1;

    pthread_mutex_lock(&sf_lock);

    sf_generation++;
    sf_load_done = 0;

    snprintf(sf_pending_path, sizeof(sf_pending_path), "%s", font);
    snprintf(sf_pending_midi, sizeof(sf_pending_midi), "%s", midi);
    snprintf(sf_playing, sizeof(sf_playing), "%s", name);
    sf_state = soundfont_loading;

    const int generation = sf_generation;
    pthread_mutex_unlock(&sf_lock);

    if (pthread_create(&sf_thread, NULL, soundfont_load_thread, (void *) (intptr_t) generation) != 0) {
        LOG_ERROR("soundfont", "Preview thread failed to start");
        sf_state = soundfont_failed;
        soundfont_restore_bgm();
        return;
    }

    sf_thread_live = 1;
}

static void soundfont_preview_reset(const int restore) {
    pthread_mutex_lock(&sf_lock);
    sf_generation++;
    sf_load_done = 0;
    pthread_mutex_unlock(&sf_lock);

    if (sf_thread_live) {
        pthread_detach(sf_thread);
        sf_thread_live = 0;
    }

    Mix_HaltMusic();
    soundfont_preview_release();

    sf_state = soundfont_idle;
    sf_playing[0] = '\0';

    if (restore) soundfont_restore_bgm();
}

void soundfont_preview_stop(void) {
    soundfont_preview_reset(1);
}

void soundfont_preview_poll(void) {
    if (sf_state == soundfont_loading) {
        pthread_mutex_lock(&sf_lock);

        const int done = sf_load_done;
        Mix_Music *ready = sf_loaded;
        sf_loaded = NULL;

        pthread_mutex_unlock(&sf_lock);

        if (!done) return;

        if (sf_thread_live) {
            pthread_join(sf_thread, NULL);
            sf_thread_live = 0;
        }

        if (!ready) {
            LOG_ERROR("soundfont", "Preview load failed: %s", Mix_GetError());
            sf_state = soundfont_failed;
            sf_playing[0] = '\0';
            soundfont_restore_bgm();
            return;
        }

        sf_music = ready;
        Mix_VolumeMusic(MIX_MAX_VOLUME);

        if (Mix_PlayMusic(sf_music, 1) < 0) {
            LOG_ERROR("soundfont", "Preview playback failed: %s", Mix_GetError());
            soundfont_preview_release();
            sf_state = soundfont_failed;
            sf_playing[0] = '\0';
            soundfont_restore_bgm();
            return;
        }

        sf_state = soundfont_playing;
        return;
    }

    if (sf_state == soundfont_playing && !Mix_PlayingMusic()) {
        soundfont_preview_release();
        sf_state = soundfont_idle;
        sf_playing[0] = '\0';
        soundfont_restore_bgm();
    }
}

int soundfont_preview_state(void) {
    return sf_state;
}

const char *soundfont_preview_name(void) {
    return sf_playing;
}
