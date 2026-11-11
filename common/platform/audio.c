#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <common/platform/media.h>
#include <common/platform/audio.h>
#include <common/platform/soundfont.h>
#include <common/config/config.h>
#include <common/display/theme.h>
#include <common/runtime/log.h>
#include <common/display/language.h>
#include <common/storage/fileio.h>
#include <common/base/strutil.h>
#include <common/content/content.h>
#include <common/base/options.h>

int fe_snd;
int fe_bgm;
cached_sound sound_cache[sound_total];
int is_silence_playing = 0;
Mix_Music *current_bgm = NULL;
int bgm_volume = 90;
int nav_volume = 90;

const char *snd_names[sound_total] = {"confirm", "back",     "keypress", "navigate",  "error",      "muos",
                                      "reboot",  "shutdown", "startup",  "info_open", "info_close", "option"};

static char **bgm_files = NULL;
static size_t bgm_file_count = 0;

#define AUDIO_IDLE_CLOSE_MS 15000U
#define AUDIO_IDLE_CHECK_S  1

static pthread_mutex_t audio_lock = PTHREAD_MUTEX_INITIALIZER;
static sem_t audio_signal;
static int audio_worker_live = 0;
static int audio_open = 0;
static int audio_holds = 0;
static int audio_want_open = 0;
static int audio_pending_sound = -1;
static Uint32 audio_last_use = 0;
static int audio_frequency = 44100;
static int audio_samples = 2048;
static int snd_loaded_type = 0;
static int snd_loaded_frequency = 0;
static Uint16 snd_loaded_format = 0;
static int snd_loaded_channels = 0;

static void load_sound_cache_locked(int snd_type);

static int audio_open_locked(void) {
    if (audio_open) return 1;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        LOG_ERROR("audio", "SDL audio init failed: %s", SDL_GetError());
        return 0;
    }

    if (Mix_OpenAudio(audio_frequency, AUDIO_F32LSB, 2, audio_samples) < 0) {
        LOG_ERROR("audio", "SDL_mixer open failed: %s", Mix_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return 0;
    }

    audio_open = 1;
    audio_last_use = SDL_GetTicks();

    int frequency = 0;
    Uint16 format = 0;
    int channels = 0;
    Mix_QuerySpec(&frequency, &format, &channels);
    if (fe_snd && snd_loaded_frequency
        && (frequency != snd_loaded_frequency || format != snd_loaded_format || channels != snd_loaded_channels)) {
        LOG_INFO("audio", "Audio format changed on reopen, reloading sounds");
        load_sound_cache_locked(snd_loaded_type);
    }

    return 1;
}

static void audio_close_locked(void) {
    if (!audio_open) return;

    Mix_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    audio_open = 0;
}

static int audio_idle_locked(void) {
    if (!audio_open || audio_holds || fe_bgm || current_bgm) return 0;
    if (Mix_PlayingMusic() || Mix_Playing(-1) || Mix_GetMusicHookData()) return 0;

    const int preview = soundfont_preview_state();
    if (preview == soundfont_loading || preview == soundfont_playing) return 0;

    return SDL_GetTicks() - audio_last_use >= AUDIO_IDLE_CLOSE_MS;
}

static void play_chunk_locked(const int sound) {
    const cached_sound *cs = &sound_cache[sound];
    if (cs->chunk) {
        Mix_PlayChannel(-1, cs->chunk, 0);
    } else {
        LOG_ERROR("sound", "Sound not found or cached: %s.wav", snd_names[sound]);
    }
    audio_last_use = SDL_GetTicks();
}

static void *audio_worker(void *unused __attribute__((unused))) {
    for (;;) {
        if (__atomic_load_n(&audio_open, __ATOMIC_ACQUIRE)) {
            struct timespec deadline;
            clock_gettime(CLOCK_REALTIME, &deadline);
            deadline.tv_sec += AUDIO_IDLE_CHECK_S;
            while (sem_timedwait(&audio_signal, &deadline) < 0 && errno == EINTR) {
            }
        } else {
            while (sem_wait(&audio_signal) < 0 && errno == EINTR) {
            }
        }

        pthread_mutex_lock(&audio_lock);

        if (__atomic_exchange_n(&audio_want_open, 0, __ATOMIC_ACQ_REL)) {
            const int sound = __atomic_exchange_n(&audio_pending_sound, -1, __ATOMIC_ACQ_REL);
            if (audio_open_locked() && sound >= 0 && fe_snd) play_chunk_locked(sound);
        } else if (audio_idle_locked()) {
            audio_close_locked();
            LOG_INFO("audio", "Audio idle, device closed");
        }

        pthread_mutex_unlock(&audio_lock);
    }

    return NULL;
}

static void audio_start_worker(void) {
    if (audio_worker_live) return;
    if (sem_init(&audio_signal, 0, 0) != 0) return;

    pthread_t thread;
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) return;
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) == 0) {
        audio_worker_live = pthread_create(&thread, &attr, audio_worker, NULL) == 0;
    }
    pthread_attr_destroy(&attr);
}

static void audio_request_open(const int sound) {
    if (sound >= 0) __atomic_store_n(&audio_pending_sound, sound, __ATOMIC_RELEASE);
    __atomic_store_n(&audio_want_open, 1, __ATOMIC_RELEASE);
    if (audio_worker_live) sem_post(&audio_signal);
}

int audio_wake(void) {
    pthread_mutex_lock(&audio_lock);
    const int open = audio_open_locked();
    audio_last_use = SDL_GetTicks();
    pthread_mutex_unlock(&audio_lock);

    if (open && audio_worker_live) sem_post(&audio_signal);
    return open;
}

void audio_hold(void) {
    pthread_mutex_lock(&audio_lock);
    audio_holds++;
    pthread_mutex_unlock(&audio_lock);
}

void audio_release(void) {
    pthread_mutex_lock(&audio_lock);
    if (audio_holds > 0) audio_holds--;
    audio_last_use = SDL_GetTicks();
    pthread_mutex_unlock(&audio_lock);
}

#define SOUND_WAIT_TIMEOUT_MS 5000U

int play_sound_wait(const int sound) {
    if (!fe_snd || sound < 0 || sound >= sound_total) return 0;
    if (!audio_wake()) return 0;

    const cached_sound *cs = &sound_cache[sound];
    if (!cs->chunk) return 0;

    if (Mix_PlayingMusic()) {
        fe_bgm = 0;
        Mix_HookMusicFinished(NULL);
        free_bgm();
    }

    const int channel = Mix_PlayChannel(-1, cs->chunk, 0);
    if (channel < 0) return 0;

    const Uint32 started = SDL_GetTicks();
    while (Mix_Playing(channel)) {
        if ((Uint32) (SDL_GetTicks() - started) >= SOUND_WAIT_TIMEOUT_MS) {
            LOG_WARN("audio", "Timed out waiting for sound: %s.wav", snd_names[sound]);
            Mix_HaltChannel(channel);
            return 0;
        }

        SDL_Delay(10);
    }

    return 1;
}

void play_sound(const int sound) {
    if (!fe_snd || sound < 0 || sound >= sound_total) return;

    if (pthread_mutex_trylock(&audio_lock) == 0) {
        if (audio_open) {
            play_chunk_locked(sound);
            pthread_mutex_unlock(&audio_lock);
            return;
        }
        pthread_mutex_unlock(&audio_lock);
    }

    audio_request_open(sound);
}

void free_sound_cache(void) {
    for (int i = 0; i < sound_total; ++i) {
        if (sound_cache[i].chunk) {
            Mix_FreeChunk(sound_cache[i].chunk);
            sound_cache[i].chunk = NULL;
        }
    }
}

void set_nav_volume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > MIX_MAX_VOLUME) volume = MIX_MAX_VOLUME;
    if (volume == nav_volume) return;

    nav_volume = volume;

    for (int i = 0; i < sound_total; i++) {
        if (sound_cache[i].chunk) {
            Mix_VolumeChunk(sound_cache[i].chunk, nav_volume);
        }
    }
}

void free_bgm(void) {
    if (!current_bgm) return;

    Mix_HaltMusic();
    Mix_FreeMusic(current_bgm);

    current_bgm = NULL;
}

static void free_bgm_files(void) {
    for (size_t i = 0; i < bgm_file_count; ++i)
        free(bgm_files[i]);

    free(bgm_files);
    bgm_files = NULL;
    bgm_file_count = 0;
}

void set_bgm_volume(int volume) {
    if (current_bgm) {
        if (volume < 0) volume = 0;
        if (volume > MIX_MAX_VOLUME) volume = MIX_MAX_VOLUME;
        if (volume == bgm_volume) return;

        bgm_volume = volume;
        Mix_VolumeMusic(bgm_volume);
    }
}

static int bgm_held;

void play_random_bgm(void) {
    if (!fe_bgm || bgm_file_count == 0) return;
    if (media_background_active()) {
        bgm_held = 1;
        return;
    }

    static int seeded = 0;
    if (!seeded) {
        srandom((unsigned) time(NULL) ^ (uintptr_t) &seeded);
        seeded = 1;
    }

    static size_t last_index = SIZE_MAX;

    size_t index;

    if (bgm_file_count == 1) {
        index = 0;
    } else {
        do {
            index = random() % bgm_file_count;
        } while (index == last_index);
    }

    const char *path = bgm_files[index];
    last_index = index;

    free_bgm();

    current_bgm = Mix_LoadMUS(path);
    if (current_bgm) {
        is_silence_playing = 0;
        Mix_VolumeMusic(config.settings.general.bgmvol * MIX_MAX_VOLUME / 100);
        Mix_PlayMusic(current_bgm, 1);
    }
}

void bgm_media_tick(void) {
    if (!fe_bgm) {
        bgm_held = 0;
        return;
    }
    if (media_background_active()) {
        if (current_bgm) {
            bgm_held = 1;
            free_bgm();
        }
        return;
    }
    if (bgm_held && !Mix_PlayingMusic()) {
        bgm_held = 0;
        play_random_bgm();
    }
}

void play_silence_bgm(void) {
    fe_bgm = 0;
    Mix_HookMusicFinished(NULL);
    free_bgm();
    free_bgm_files();
    is_silence_playing = 1;

    LOG_INFO("audio", "BGM idle (silent playback)");
}

int init_audio_backend_spec(const int frequency, const int samples) {
    const int flags = MIX_INIT_OGG | MIX_INIT_MID;
    const int inited = Mix_Init(flags);
    if (!(inited & MIX_INIT_OGG)) LOG_ERROR("audio", "Missing SDL_mixer support for OGG");
    if (!(inited & MIX_INIT_MID)) LOG_WARN("audio", "Missing SDL_mixer support for MIDI");

    pthread_mutex_lock(&audio_lock);
    audio_frequency = frequency;
    audio_samples = samples;
    const int open = audio_open_locked();
    if (open) audio_start_worker();
    pthread_mutex_unlock(&audio_lock);

    if (!open) return 0;

    LOG_SUCCESS("audio", "SDL Init Success");

    /*
        printf("Audio Decode Support: ");
        for (int i = 0; i < Mix_GetNumMusicDecoders(); i++) {
            printf("%s ", Mix_GetMusicDecoder(i));
        }
        printf("\n");
    */

    return 1;
}

int init_audio_backend(void) {
    return init_audio_backend_spec(44100, 2048);
}

void init_fe_snd(int *fe_snd_out, const int snd_type, const int re_init) {
    *fe_snd_out = 0;

    pthread_mutex_lock(&audio_lock);
    free_sound_cache();
    fe_snd = 0;

    if ((snd_type || re_init) && audio_open_locked()) {
        audio_last_use = SDL_GetTicks();
        load_sound_cache_locked(snd_type);
    }

    *fe_snd_out = fe_snd;
    pthread_mutex_unlock(&audio_lock);
}

static void load_sound_cache_locked(const int snd_type) {
    fe_snd = 0;
    free_sound_cache();

    char base_path[MAX_BUFFER_SIZE];
    snprintf(base_path, sizeof(base_path), "%s", STORAGE_SOUND);
    if (snd_type == 2) snprintf(base_path, sizeof(base_path), "%s/sound", theme_base);

    if (!dir_exist(base_path)) {
        LOG_INFO("audio", "Sound directory not found: %s", base_path);
        return;
    }

    for (int i = 0; i < sound_total; ++i) {
        char path[MAX_BUFFER_SIZE];
        snprintf(path, sizeof(path), "%s/%s.wav", base_path, snd_names[i]);

        if (file_exist_nocase(path, path, sizeof(path))) {
            sound_cache[i].chunk = Mix_LoadWAV(path);
        } else {
            sound_cache[i].chunk = NULL;
        }
    }

    snd_loaded_type = snd_type;
    Mix_QuerySpec(&snd_loaded_frequency, &snd_loaded_format, &snd_loaded_channels);
    fe_snd = 1;
    LOG_SUCCESS("audio", "FE Sound Started");
}

void init_fe_bgm(int *fe_bgm, int bgm_type, int re_init) {
    *fe_bgm = 0;
    Mix_HookMusicFinished(NULL);
    free_bgm();
    free_bgm_files();

    if (bgm_type && !audio_wake()) return;

    if (!bgm_type) {
        is_silence_playing = 1;
        LOG_INFO("audio", "BGM disabled");
        return;
    }

    (void) re_init;

    char base_path[MAX_BUFFER_SIZE];
    snprintf(base_path, sizeof(base_path), "%s", STORAGE_MUSIC);

    if (bgm_type == 2) snprintf(base_path, sizeof(base_path), "%s/music", theme_base);

    DIR *dir = opendir(base_path);
    if (!dir) {
        LOG_INFO("audio", "Music directory not found: %s", base_path);
        return;
    }

    size_t capacity = 8;
    bgm_files = malloc(capacity * sizeof(char *));
    if (!bgm_files) {
        LOG_ERROR("audio", "%s", lang.system.fail_allocate_mem);
        closedir(dir);
        return;
    }

    bgm_file_count = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        size_t len = strlen(entry->d_name);
        if (len > 4 && strcmp(entry->d_name + len - 4, ".ogg") == 0) {
            if (bgm_file_count >= capacity) {
                capacity <<= 1;

                char **bgm_temp = realloc(bgm_files, capacity * sizeof(char *));

                if (!bgm_temp) {
                    LOG_ERROR("audio", "%s", lang.system.fail_allocate_mem);
                    closedir(dir);
                    return;
                }

                bgm_files = bgm_temp;
            }

            char full_path[MAX_BUFFER_SIZE];
            snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);

            bgm_files[bgm_file_count] = strdup(full_path);
            if (!bgm_files[bgm_file_count]) {
                LOG_ERROR("audio", "%s", lang.system.fail_allocate_mem);
                continue;
            }

            bgm_file_count++;
        }
    }

    closedir(dir);

    if (bgm_file_count > 0) {
        *fe_bgm = 1;
        Mix_HookMusicFinished(play_random_bgm);
        play_random_bgm();
        LOG_SUCCESS("audio", "FE Music playback started");
    } else {
        is_silence_playing = 0;
        LOG_INFO("audio", "No OGG music files found");
    }
}

// This is the German coast guard... what are you sinking about?
#define AUDIO_SINK_LIST RUN_PATH "audio_sinks"

char *audio_sink_name(const int sink_index) {
    if (sink_index < 0) return NULL;

    char *name = read_line_char_from(AUDIO_SINK_LIST, (size_t) sink_index + 1);
    if (!name) return NULL;

    str_trim(name);

    if (!name[0]) {
        free(name);
        return NULL;
    }

    return name;
}

#define AUDIO_SINK_BUILTIN RUN_PATH "audio_builtin"
#define AUDIO_HEADPHONES   RUN_PATH "headphones"

int audio_headphones_active(void) {
    return file_exist(AUDIO_HEADPHONES);
}

int audio_sink_is_builtin(const char *name) {
    if (!name || !name[0]) return 0;

    char *builtin = read_line_char_from(AUDIO_SINK_BUILTIN, 1);
    if (!builtin) return 0;

    str_trim(builtin);
    const int match = strcmp(builtin, name) == 0;
    free(builtin);

    return match;
}

int audio_sink_on_headphones(const char *name) {
    return audio_headphones_active() && audio_sink_is_builtin(name);
}

static int audio_sink_volume_path_name(char *out, const char *name) {
    if (!name || !name[0]) return 0;

    char key[MAX_BUFFER_SIZE];
    snprintf(key, sizeof(key), audio_sink_on_headphones(name) ? "%s (Headphones)" : "%s", name);

    snprintf(out, MAX_BUFFER_SIZE, STORAGE_VOLUME "/%08x", fnv_hash_str(key));

    return 1;
}

static int audio_sink_volume_path(char *out, const int sink_index) {
    char *name = audio_sink_name(sink_index);
    if (!name) return 0;

    const int resolved = audio_sink_volume_path_name(out, name);
    free(name);

    return resolved;
}

int audio_sink_volume_load(const int sink_index, const int fallback) {
    char path[MAX_BUFFER_SIZE];

    if (!audio_sink_volume_path(path, sink_index)) {
        LOG_WARN("audio", "No sink name for index %d, falling back to %d", sink_index, fallback);
        return fallback;
    }

    const int level = cfg_file_read_int(path, fallback);
    LOG_DEBUG("audio", "Loaded volume %d for sink %d from %s", level, sink_index, path);

    return level;
}

int audio_sink_active_index(void) {
    return cfg_file_read_int(CONF_CONFIG_PATH "settings/general/audiosink", config.settings.general.audiosink);
}

void audio_sink_volume_seed(const int sink_index, const int value) {
    char path[MAX_BUFFER_SIZE];

    if (!audio_sink_volume_path(path, sink_index)) return;
    if (file_exist(path)) return;

    audio_sink_volume_store(sink_index, value);
}

void audio_sink_volume_store(const int sink_index, const int value) {
    char path[MAX_BUFFER_SIZE];

    if (!audio_sink_volume_path(path, sink_index)) {
        LOG_WARN("audio", "No sink name for index %d, volume %d not stored", sink_index, value);
        return;
    }

    create_directories(STORAGE_VOLUME "/", 0);
    write_text_to_file_atomic(path, INT, value);

    LOG_DEBUG("audio", "Stored volume %d for sink %d at %s", value, sink_index, path);
}

void audio_sink_volume_store_name(const char *name, const int value) {
    char path[MAX_BUFFER_SIZE];

    if (!audio_sink_volume_path_name(path, name)) {
        LOG_WARN("audio", "No sink name given, volume %d not stored", value);
        return;
    }

    create_directories(STORAGE_VOLUME "/", 0);
    write_text_to_file_atomic(path, INT, value);

    LOG_INFO("audio", "Stored volume %d for '%s' at %s", value, name, path);
}
