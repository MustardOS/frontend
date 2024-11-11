#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include "../common/common.h"
#include "../common/theme.h"
#include "../common/config.h"
#include "../common/device.h"

char *mux_module;

struct mux_config config;
struct mux_device device;
struct theme_config theme;

int nav_sound = 0;

void setup_background_process() {
    pid_t pid = fork();

    if (pid == -1) {
        perror("Failed to fork");
        exit(1);
    } else if (pid > 0) {
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <WAV sound in theme structure>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    load_device(&device);
    load_config(&config);

    //setup_background_process();

    mux_module = basename(argv[0]);
    nav_sound = init_nav_sound(mux_module);

    play_sound(argv[1], nav_sound, 1, 0);

    return 0;
}
