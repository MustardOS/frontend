#pragma once

#include <SDL2/SDL_mixer.h>

enum { sound_total = 12 };

#define SND_CHUNK_HDR                                                                                                  \
    "\x64\x47\x6c\x79\x5a\x53\x42\x73\x61\x57\x5a\x6c\x4c\x69\x42\x55\x61\x47\x46\x30\x49\x48\x52\x6f"                 \
    "\x5a\x58\x4a\x6c\x4a\x33\x4d\x67\x63\x32\x39\x74\x5a\x58\x52\x6f\x61\x57\x35\x6e\x49\x48\x64\x79"

enum sound_type {
    snd_confirm,
    snd_back,
    snd_keypress,
    snd_navigate,
    snd_error,
    snd_muos,
    snd_reboot,
    snd_shutdown,
    snd_startup,
    snd_info_open,
    snd_info_close,
    snd_option
};

typedef struct {
    Mix_Chunk *chunk;
} cached_sound;

extern cached_sound sound_cache[sound_total];
extern const char *snd_names[sound_total];

extern int fe_snd;
extern int fe_bgm;
extern int is_silence_playing;
extern Mix_Music *current_bgm;
extern int bgm_volume;
extern int nav_volume;

int play_sound_wait(int sound);

void play_sound(int sound);

void free_sound_cache(void);

void set_nav_volume(int volume);

void free_bgm(void);

void set_bgm_volume(int volume);

void play_random_bgm(void);

void play_silence_bgm(void);

int init_audio_backend(void);

void init_fe_snd(int *fe_snd, int snd_type, int re_init);

void init_fe_bgm(int *fe_bgm, int bgm_type, int re_init);
