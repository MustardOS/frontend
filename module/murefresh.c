#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "../common/common.h"
#include "../common/language.h"
#include "../common/theme.h"
#include "../common/config.h"
#include "../common/device.h"
#include "../common/kiosk.h"

char *mux_module;
char *osd_message;
int msgbox_active = 0;
int nav_sound = 0;

struct mux_lang lang;
struct mux_config config;
struct mux_device device;
struct mux_kiosk kiosk;
struct theme_config theme;

int progress_onscreen = -1;
int ui_count = 0;
int current_item_index = 0;

lv_obj_t *msgbox_element = NULL;

// Stubs to appease the compiler!
void list_nav_prev(void) {}

void list_nav_next(void) {}

void setup_background_process() {
    pid_t pid = fork();

    if (pid == -1) {
        perror(lang.SYSTEM.FAIL_FORK);
        exit(1);
    } else if (pid > 0) {
        exit(0);
    }
}

int main() {
    setup_background_process();

    load_device(&device);
    load_config(&config);
    load_lang(&lang);

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        SDL_Log("error %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window *window;
    SDL_Renderer *renderer;

    struct screen_dimension dims = get_device_dimensions();

    if (SDL_CreateWindowAndRenderer(dims.WIDTH, dims.HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer) != 0) {
        SDL_Log("error %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    int safe_quit = 0;
    while (true) {
        if (safe_quit > device.SCREEN.WAIT) break;
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
        usleep(device.SCREEN.WAIT);
        safe_quit++;
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    write_text_to_file("/tmp/hdmi_out", "w", CHAR, "hack");

    return 0;
}
