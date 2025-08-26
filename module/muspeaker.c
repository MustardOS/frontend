#include <SDL2/SDL.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

static volatile sig_atomic_t quit = 0;

static void on_sig() {
    quit = 1;
}

int main(void) {
    SDL_SetHint(SDL_HINT_AUDIO_DEVICE_STREAM_ROLE, "background");
    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_AudioSpec want, have;
    SDL_zero(want);

    want.freq = 48000;
    want.channels = 2;
    want.format = AUDIO_S16;
    want.samples = 16384;

    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, SDL_AUDIO_ALLOW_ANY_CHANGE);
    if (!audio_device) {
        fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError());
        return 1;
    }

    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);

    SDL_PauseAudioDevice(audio_device, 0);
    while (!quit) pause();

    SDL_CloseAudioDevice(audio_device);
    SDL_Quit();

    return 0;
}
