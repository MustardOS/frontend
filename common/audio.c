#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "audio.h"
#include "config.h"
#include "theme.h"
#include "log.h"
#include "language.h"
#include "fileio.h"

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

int play_sound_wait(const int sound) {
    if (!fe_snd || sound < 0 || sound >= sound_total) return 0;

    const cached_sound *cs = &sound_cache[sound];
    if (!cs->chunk) return 0;

    if (Mix_PlayingMusic()) {
        Mix_HaltMusic();
        current_bgm = NULL;
    }

    const int channel = Mix_PlayChannel(-1, cs->chunk, 0);
    if (channel < 0) return 0;

    while (Mix_Playing(channel))
        SDL_Delay(10);

    return 1;
}

void play_sound(const int sound) {
    if (!fe_snd || sound < 0 || sound >= sound_total) return;

    const cached_sound *cs = &sound_cache[sound];
    if (cs->chunk) {
        Mix_PlayChannel(-1, cs->chunk, 0);
    } else {
        LOG_ERROR("sound", "Sound not found or cached: %s.wav", snd_names[sound]);
    }
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

void set_bgm_volume(int volume) {
    if (current_bgm) {
        if (volume < 0) volume = 0;
        if (volume > MIX_MAX_VOLUME) volume = MIX_MAX_VOLUME;
        if (volume == bgm_volume) return;

        bgm_volume = volume;
        Mix_VolumeMusic(bgm_volume);
    }
}

void play_random_bgm(void) {
    if (bgm_file_count == 0) return;

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

void play_silence_bgm(void) {
    free_bgm();
    is_silence_playing = 0;

    LOG_INFO("audio", "BGM idle (silent playback)");
}

int init_audio_backend(void) {
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        LOG_ERROR("audio", "SDL Init Failed");
        return 0;
    }

    const int flags = MIX_INIT_OGG;
    const int inited = Mix_Init(flags);
    if ((inited & flags) != flags) {
        LOG_ERROR("audio", "Missing SDL_mixer support for OGG");
    }

    if (Mix_OpenAudio(44100, AUDIO_F32LSB, 2, 2048) < 0) {
        LOG_ERROR("audio", "SDL_mixer open failed: %s", Mix_GetError());
        return 0;
    }

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

void init_fe_snd(int *fe_snd, const int snd_type, const int re_init) {
    *fe_snd = 0;
    free_sound_cache();

    if (!snd_type && !re_init) return;

    char base_path[MAX_BUFFER_SIZE];
    snprintf(base_path, sizeof(base_path), "%s", STORAGE_SOUND);
    if (snd_type == 2) snprintf(base_path, sizeof(base_path), "%s/sound", theme_base);

    DIR *dir = opendir(base_path);
    if (!dir) {
        LOG_INFO("audio", "Sound directory not found: %s", base_path);
        return;
    }

    for (int i = 0; i < sound_total; ++i) {
        char path[MAX_BUFFER_SIZE];
        snprintf(path, sizeof(path), "%s/%s.wav", base_path, snd_names[i]);

        if (file_exist(path)) {
            sound_cache[i].chunk = Mix_LoadWAV(path);
        } else {
            sound_cache[i].chunk = NULL;
        }
    }

    *fe_snd = 1;
    LOG_SUCCESS("audio", "FE Sound Started");
}

void init_fe_bgm(int *fe_bgm, int bgm_type, int re_init) {
    free_bgm();
    *fe_bgm = 0;

    if (!bgm_type && !re_init) {
        is_silence_playing = 0;
        LOG_INFO("audio", "BGM disabled");
        return;
    }

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
        Mix_HookMusicFinished(play_random_bgm);
        play_random_bgm();
        *fe_bgm = 1;
        LOG_SUCCESS("audio", "FE Music playback started");
    } else {
        is_silence_playing = 0;
        LOG_INFO("audio", "No OGG music files found");
    }
}
