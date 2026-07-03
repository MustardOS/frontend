#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "../log.h"
#include "../saver.h"
#include "../options.h"
#include "bsod.h"

#define BSOD_FONT_FILE    OPT_PATH "share/font/muterm.ttf"
#define BSOD_MESSAGE_FILE OPT_SHARE_PATH "message.txt"

#define BSOD_BG_R 0
#define BSOD_BG_G 0
#define BSOD_BG_B 170

#define BSOD_TITLE_BG_R 192
#define BSOD_TITLE_BG_G 192
#define BSOD_TITLE_BG_B 192

#define BSOD_MIN_HOLD_MS 3000

#define BSOD_CRASH_MAX_HOLD_MS  12000
#define BSOD_REBOOT_MAX_HOLD_MS 4000

#define BSOD_CURSOR_BLINK_MS 500

#define BSOD_TEXT_BUF_SIZE 2048

typedef enum {
    bsod_state_crash = 0,
    bsod_state_reboot,
} bsod_state_t;

typedef struct {
    saver_state_t base;

    SDL_Renderer *renderer;
    TTF_Font *font;

    bsod_state_t state;
    uint32_t phase_start;
    uint32_t hold_ms;

    int cursor_on;
    uint32_t last_cursor_toggle;

    SDL_Texture *tex_title;
    int title_w, title_h;
    int title_x, title_y, title_box_w, title_box_h;

    SDL_Texture *tex_body;
    int body_w, body_h;
    int body_x, body_y;

    SDL_Texture *tex_footer;
    int footer_w, footer_h;
    int footer_y;
    int footer_visible;

    SDL_Texture *tex_reboot;
    int reboot_w, reboot_h;
    int reboot_x, reboot_y;

    SDL_Texture *tex_cursor;
    int cursor_w, cursor_h;
    int cursor_x, cursor_y;
} bsod_module_t;

static bsod_module_t mod = {0};

static const char *bsod_vxd_names[] = {
    "VMM(01)",    "VFAT(04)", "VCACHE(01)",   "IOS",         "CDFS(03)",   "VNETSUP(01)",
    "VDD(01)",    "VTDI(01)", "NDIS",         "VPOWERD(01)", "VCOMM(04)",  "VJOYD(02)",
    "VSHARE(01)", "VXDLDR",   "VFBACKUP(01)", "VREDIR(02)",  "VMOUSE(01)", "IFSMGR(04)",
};

#define BSOD_VXD_COUNT (int) (sizeof(bsod_vxd_names) / sizeof(bsod_vxd_names[0]))

static const char *bsod_exceptions[] = {"02", "04", "05", "06", "0C", "0D", "0E", "10"};

#define BSOD_EXC_COUNT (int) (sizeof(bsod_exceptions) / sizeof(bsod_exceptions[0]))

static uint32_t bsod_rand_addr(void) {
    return (uint32_t) saver_rand_range(0x10000) << 16 | (uint32_t) saver_rand_range(0x10000);
}

static uint32_t bsod_random_hold_ms(const uint32_t max_at_cruise_ms) {
    const int speed = mod.base.speed > 0 ? mod.base.speed : SAVER_SPEED_DEFAULT;

    uint32_t max_ms = (uint32_t) ((float) max_at_cruise_ms * ((float) SAVER_SPEED_DEFAULT / (float) speed));

    const uint32_t cap = max_at_cruise_ms * 3;
    if (max_ms > cap) max_ms = cap;
    if (max_ms <= BSOD_MIN_HOLD_MS) return BSOD_MIN_HOLD_MS;

    return BSOD_MIN_HOLD_MS + (uint32_t) saver_rand_range((int32_t) (max_ms - BSOD_MIN_HOLD_MS));
}

static char *bsod_pick_message_line(void) {
    FILE *f = fopen(BSOD_MESSAGE_FILE, "r");
    if (!f) return NULL;

    char buf[512];
    int eligible = 0;
    int line_no = 0;

    while (fgets(buf, sizeof(buf), f)) {
        line_no++;
        if (line_no == 1) continue;

        char *p = buf;
        while (*p)
            ++p;
        while (p > buf && (p[-1] == '\n' || p[-1] == '\r'))
            --p;
        *p = '\0';

        if (buf[0] == '\0') continue;
        eligible++;
    }

    if (eligible == 0) {
        fclose(f);
        return NULL;
    }

    const int pick = saver_rand_range(eligible);
    rewind(f);

    line_no = 0;
    int seen = 0;
    char *result = NULL;

    while (fgets(buf, sizeof(buf), f)) {
        line_no++;
        if (line_no == 1) continue;

        char *p = buf;
        while (*p)
            ++p;
        while (p > buf && (p[-1] == '\n' || p[-1] == '\r'))
            --p;
        *p = '\0';

        if (buf[0] == '\0') continue;

        if (seen == pick) {
            result = strdup(buf);
            break;
        }

        seen++;
    }

    fclose(f);
    return result;
}

static void bsod_generate_text(char *out) {
    const char *exc = bsod_exceptions[saver_rand_range(BSOD_EXC_COUNT)];
    const char *vxd1 = bsod_vxd_names[saver_rand_range(BSOD_VXD_COUNT)];
    const char *vxd2 = bsod_vxd_names[saver_rand_range(BSOD_VXD_COUNT)];

    int len = snprintf(
        out, BSOD_TEXT_BUF_SIZE,
        "An exception %s has occurred at 0028:%08X in %s\nThis was called from 0028:%08X in %s + %08X.", exc,
        bsod_rand_addr(), vxd1, bsod_rand_addr(), vxd2, bsod_rand_addr()
    );

    if (len > 0 && (size_t) len < BSOD_TEXT_BUF_SIZE && saver_rand_range(2)) {
        const char *vxd3 = bsod_vxd_names[saver_rand_range(BSOD_VXD_COUNT)];

        len += snprintf(
            out + len, BSOD_TEXT_BUF_SIZE - (size_t) len, " This was interuppted by 0028:%08X in %s + %08X.",
            bsod_rand_addr(), vxd3, bsod_rand_addr()
        );
    }

    if (len > 0 && (size_t) len < BSOD_TEXT_BUF_SIZE) {
        len += snprintf(out + len, BSOD_TEXT_BUF_SIZE - (size_t) len, "\n\n* Press any button to attempt to continue");
    }

    char *msg = bsod_pick_message_line();
    if (msg) {
        if (len > 0 && (size_t) len < BSOD_TEXT_BUF_SIZE)
            snprintf(out + len, BSOD_TEXT_BUF_SIZE - (size_t) len, "\n* %s", msg);
        free(msg);
    }
}

static void bsod_build_crash_content(SDL_Renderer *renderer) {
    if (mod.tex_body) {
        SDL_DestroyTexture(mod.tex_body);
        mod.tex_body = NULL;
    }

    char text[BSOD_TEXT_BUF_SIZE];
    bsod_generate_text(text);

    const int margin_x = (int) ((float) mod.base.screen_w * 0.045f);
    int wrap_w = mod.base.screen_w - margin_x * 2;
    if (wrap_w < 32) wrap_w = 32;

    TTF_SetFontWrappedAlign(mod.font, TTF_WRAPPED_ALIGN_LEFT);

    const SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *surf = TTF_RenderUTF8_Blended_Wrapped(mod.font, text, white, wrap_w);
    if (surf) {
        mod.tex_body = SDL_CreateTextureFromSurface(renderer, surf);
        mod.body_w = surf->w;
        mod.body_h = surf->h;
        SDL_FreeSurface(surf);
    } else {
        mod.body_w = 0;
        mod.body_h = 0;
    }

    const int gap = mod.font ? TTF_FontLineSkip(mod.font) : 8;

    mod.body_x = margin_x;
    mod.body_y = mod.title_y + mod.title_box_h + gap;

    mod.footer_y = mod.body_y + mod.body_h + gap;

    const int bottom_margin = (int) ((float) mod.base.screen_h * 0.03f);
    mod.footer_visible = mod.tex_footer && mod.footer_y + mod.footer_h <= mod.base.screen_h - bottom_margin;
}

static void bsod_build_title(SDL_Renderer *renderer) {
    const SDL_Color black = {0, 0, 0, 255};
    SDL_Surface *surf = TTF_RenderUTF8_Blended(mod.font, "MustardOS", black);
    if (surf) {
        mod.tex_title = SDL_CreateTextureFromSurface(renderer, surf);
        mod.title_w = surf->w;
        mod.title_h = surf->h;
        SDL_FreeSurface(surf);
    }

    int pad_x = mod.title_w / 3;
    if (pad_x < 8) pad_x = 8;

    int pad_y = mod.title_h / 2;
    if (pad_y < 4) pad_y = 4;

    mod.title_box_w = mod.title_w + pad_x * 2;
    mod.title_box_h = mod.title_h + pad_y * 2;
    mod.title_x = (mod.base.screen_w - mod.title_box_w) / 2;
    mod.title_y = (int) ((float) mod.base.screen_h * 0.08f);
}

static void bsod_build_footer(SDL_Renderer *renderer) {
    const SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *surf = TTF_RenderUTF8_Blended(mod.font, "Press any button to continue", white);
    if (surf) {
        mod.tex_footer = SDL_CreateTextureFromSurface(renderer, surf);
        mod.footer_w = surf->w;
        mod.footer_h = surf->h;
        SDL_FreeSurface(surf);
    }
}

static void bsod_build_reboot(SDL_Renderer *renderer) {
    const SDL_Color white = {255, 255, 255, 255};

    SDL_Surface *surf = TTF_RenderUTF8_Blended(mod.font, "Reloading MustardOS...", white);
    if (surf) {
        mod.tex_reboot = SDL_CreateTextureFromSurface(renderer, surf);
        mod.reboot_w = surf->w;
        mod.reboot_h = surf->h;
        SDL_FreeSurface(surf);
    }

    surf = TTF_RenderUTF8_Blended(mod.font, "_", white);
    if (surf) {
        mod.tex_cursor = SDL_CreateTextureFromSurface(renderer, surf);
        mod.cursor_w = surf->w;
        mod.cursor_h = surf->h;
        SDL_FreeSurface(surf);
    }

    mod.reboot_x = (mod.base.screen_w - (mod.reboot_w + mod.cursor_w)) / 2;
    mod.reboot_y = (mod.base.screen_h - mod.reboot_h) / 2;

    mod.cursor_x = mod.reboot_x + mod.reboot_w;
    mod.cursor_y = mod.reboot_y + (mod.reboot_h - mod.cursor_h) / 2;
}

static void bsod_on_speed_changed(void *user) {
    (void) user;
}

static void bsod_on_idle_enter(void *user) {
    (void) user;

    mod.state = bsod_state_crash;
    mod.phase_start = SDL_GetTicks();
    mod.hold_ms = bsod_random_hold_ms(BSOD_CRASH_MAX_HOLD_MS);
    bsod_build_crash_content(mod.renderer);
}

int bsod_init(SDL_Renderer *renderer, const int screen_w, const int screen_h) {
    if (!TTF_WasInit() && TTF_Init() != 0) {
        LOG_ERROR("saver", "Blue Screen: TTF_Init failed: %s", TTF_GetError());
        return 0;
    }

    int font_size = screen_h / 22;
    if (font_size < 12) font_size = 12;

    mod.font = TTF_OpenFont(BSOD_FONT_FILE, font_size);
    if (!mod.font) {
        LOG_ERROR("saver", "Blue Screen: failed to open font %s: %s", BSOD_FONT_FILE, TTF_GetError());
        return 0;
    }

    mod.renderer = renderer;

    saver_init_base(
        &mod.base, screen_w, screen_h, "Blue Screen", 255, 255, 255, bsod_on_speed_changed, bsod_on_idle_enter, &mod
    );

    bsod_build_title(renderer);
    bsod_build_footer(renderer);
    bsod_build_reboot(renderer);

    mod.state = bsod_state_crash;
    mod.phase_start = SDL_GetTicks();
    mod.tex_body = NULL;

    LOG_INFO("saver", "Blue Screen Initialised (%dx%d)", screen_w, screen_h);

    return 1;
}

void bsod_update(void) {
    const uint32_t now = SDL_GetTicks();
    if (!saver_poll_idle(&mod.base, now)) return;

    const uint32_t elapsed = now - mod.phase_start;

    if (mod.state == bsod_state_crash) {
        if (elapsed >= mod.hold_ms) {
            mod.state = bsod_state_reboot;
            mod.phase_start = now;
            mod.hold_ms = bsod_random_hold_ms(BSOD_REBOOT_MAX_HOLD_MS);
            mod.cursor_on = 1;
            mod.last_cursor_toggle = now;
        }
        return;
    }

    if (now - mod.last_cursor_toggle >= BSOD_CURSOR_BLINK_MS) {
        mod.cursor_on = !mod.cursor_on;
        mod.last_cursor_toggle = now;
    }

    if (elapsed >= mod.hold_ms) {
        mod.state = bsod_state_crash;
        mod.phase_start = now;
        mod.hold_ms = bsod_random_hold_ms(BSOD_CRASH_MAX_HOLD_MS);
        bsod_build_crash_content(mod.renderer);
    }
}

void bsod_render(SDL_Renderer *renderer) {
    if (!mod.base.enabled || !mod.base.idle_active || !mod.font) return;

    if (mod.state == bsod_state_crash) {
        SDL_SetRenderDrawColor(renderer, BSOD_BG_R, BSOD_BG_G, BSOD_BG_B, 255);
        SDL_RenderFillRect(renderer, NULL);

        const SDL_Rect title_rect = {mod.title_x, mod.title_y, mod.title_box_w, mod.title_box_h};
        SDL_SetRenderDrawColor(renderer, BSOD_TITLE_BG_R, BSOD_TITLE_BG_G, BSOD_TITLE_BG_B, 255);
        SDL_RenderFillRect(renderer, &title_rect);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderDrawRect(renderer, &title_rect);

        if (mod.tex_title) {
            const SDL_Rect dst = {
                mod.title_x + (mod.title_box_w - mod.title_w) / 2, mod.title_y + (mod.title_box_h - mod.title_h) / 2,
                mod.title_w, mod.title_h
            };
            SDL_RenderCopy(renderer, mod.tex_title, NULL, &dst);
        }

        if (mod.tex_body) {
            const SDL_Rect dst = {mod.body_x, mod.body_y, mod.body_w, mod.body_h};
            SDL_RenderCopy(renderer, mod.tex_body, NULL, &dst);
        }

        if (mod.footer_visible) {
            const SDL_Rect dst = {(mod.base.screen_w - mod.footer_w) / 2, mod.footer_y, mod.footer_w, mod.footer_h};
            SDL_RenderCopy(renderer, mod.tex_footer, NULL, &dst);
        }
    } else {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, NULL);

        if (mod.tex_reboot) {
            const SDL_Rect dst = {mod.reboot_x, mod.reboot_y, mod.reboot_w, mod.reboot_h};
            SDL_RenderCopy(renderer, mod.tex_reboot, NULL, &dst);
        }

        if (mod.cursor_on && mod.tex_cursor) {
            const SDL_Rect dst = {mod.cursor_x, mod.cursor_y, mod.cursor_w, mod.cursor_h};
            SDL_RenderCopy(renderer, mod.tex_cursor, NULL, &dst);
        }
    }
}

int bsod_active(void) {
    return saver_active_base(&mod.base);
}

void bsod_stop(void) {
    saver_stop_base(&mod.base);
}

void bsod_shutdown(void) {
    if (mod.tex_title) {
        SDL_DestroyTexture(mod.tex_title);
        mod.tex_title = NULL;
    }

    if (mod.tex_body) {
        SDL_DestroyTexture(mod.tex_body);
        mod.tex_body = NULL;
    }

    if (mod.tex_footer) {
        SDL_DestroyTexture(mod.tex_footer);
        mod.tex_footer = NULL;
    }

    if (mod.tex_reboot) {
        SDL_DestroyTexture(mod.tex_reboot);
        mod.tex_reboot = NULL;
    }

    if (mod.tex_cursor) {
        SDL_DestroyTexture(mod.tex_cursor);
        mod.tex_cursor = NULL;
    }

    if (mod.font) {
        TTF_CloseFont(mod.font);
        mod.font = NULL;
    }

    saver_shutdown_base(&mod.base);
}
