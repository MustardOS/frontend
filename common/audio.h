#pragma once

#include <SDL2/SDL_mixer.h>

enum {
    SOUND_TOTAL = 12
};

enum sound_type {
    SND_CONFIRM,
    SND_BACK,
    SND_KEYPRESS,
    SND_NAVIGATE,
    SND_ERROR,
    SND_MUOS,
    SND_REBOOT,
    SND_SHUTDOWN,
    SND_STARTUP,
    SND_INFO_OPEN,
    SND_INFO_CLOSE,
    SND_OPTION
};

typedef struct {
    Mix_Chunk *chunk;
} CachedSound;

extern CachedSound sound_cache[SOUND_TOTAL];
extern const char *snd_names[SOUND_TOTAL];

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
