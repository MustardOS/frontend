#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <common/base/options.h>
#include <common/config/config.h>
#include <common/runtime/exec.h>
#include <common/saver/blank.h>
#include <common/saver/saver.h>

typedef struct {
    saver_state_t base;
    int blanked;
    int brightness;
} blank_module_t;

static blank_module_t mod;

static void apply_brightness(const int brightness) {
    char value[16];
    snprintf(value, sizeof(value), "%d", brightness);

    const char *args[] = {DEV_SCRIPT "bright.sh", value, "transient", NULL};
    run_exec(args, A_SIZE(args), 0, 0, NULL, NULL);
}

static void restore_backlight(void) {
    if (!mod.blanked) return;

    apply_brightness(mod.brightness);
    mod.blanked = 0;
}

static void on_idle_enter(void *user) {
    blank_module_t *blank = user;
    blank->brightness = config.settings.general.brightness;
    apply_brightness(0);
    blank->blanked = 1;
}

int blank_init(SDL_Renderer *renderer, const int screen_width, const int screen_height) {
    (void) renderer;

    restore_backlight();
    memset(&mod, 0, sizeof(mod));
    saver_init_base(&mod.base, screen_width, screen_height, "Blank", 0, 0, 0, NULL, on_idle_enter, &mod);
    return 1;
}

void blank_update(void) {
    saver_poll_idle(&mod.base, SDL_GetTicks());
}

void blank_render(SDL_Renderer *renderer) {
    if (!saver_active_base(&mod.base)) return;

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, NULL);
    saver_perf_note_draw_calls(1);
}

int blank_active(void) {
    return saver_active_base(&mod.base);
}

void blank_stop(void) {
    restore_backlight();
    saver_stop_base(&mod.base);
}

void blank_shutdown(void) {
    restore_backlight();
    saver_shutdown_base(&mod.base);
}
