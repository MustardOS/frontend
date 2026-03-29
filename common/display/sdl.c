#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "../log.h"
#include "../common.h"
#include "../device.h"
#include "../input.h"
#include "../config.h"
#include "../theme.h"
#include "sdl.h"
#include "dvd.h"

typedef struct {
    // Just the default base stuff we need
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    volatile bool refresh;

    // Cached render state
    SDL_Rect dest_rect;
    double angle;
    SDL_Point pivot;
    SDL_Point *pivot_ptr;

    // Track if we should clear
    bool needs_clear;
    bool force_clear;

    // Background image support
    bool background_solid;
    SDL_Texture *background_image;
    SDL_Color background_colour;
    char theme_name[256];
} monitor_t;

static monitor_t monitor;

static SDL_Rect pending_rect;
static bool pending_rect_valid = false;

int scale_width, scale_height, underscan;

static void update_blend_mode(void) {
    SDL_SetTextureBlendMode(monitor.texture, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawBlendMode(monitor.renderer, SDL_BLENDMODE_NONE);

    monitor.force_clear = true;
}

static SDL_Texture *load_png(SDL_Renderer *renderer, const char *path) {
    SDL_Surface *surf = IMG_Load(path);
    if (!surf) {
        LOG_ERROR("video", "PNG loading failure: %s", IMG_GetError());
        return NULL;
    }

    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);

    if (!tex) LOG_ERROR("video", "Texture creation failure: %s", SDL_GetError());
    return tex;
}

static void reload_background(const char *active_theme) {
    char back_image[MAX_BUFFER_SIZE];
    snprintf(back_image, sizeof(back_image), "%s/%simage/background.png", theme_base, mux_dim);

    if (!file_exist(back_image)) {
        snprintf(back_image, sizeof(back_image), "%s/image/background.png", theme_base);
        if (!file_exist(back_image)) {
            if (monitor.background_image) {
                SDL_DestroyTexture(monitor.background_image);
                monitor.background_image = NULL;
            }

            // If the theme background opacity is set to 0 we'll just revert
            // back to a solid black colour, otherwise we'll get some lovely
            // overlap graphic glitches!
            monitor.background_solid = true;
            monitor.background_colour = (SDL_Color) {theme.SDL.SOLID.R, theme.SDL.SOLID.G, theme.SDL.SOLID.B, 255};
            monitor.theme_name[0] = '\0';

            // The good news is that a failure to find an image fails gracefully!
            LOG_INFO("video", "No 'background.png' found for theme '%s'", active_theme);
            return;
        }
    }

    if (monitor.background_image && strcmp(active_theme, monitor.theme_name) == 0) return;

    if (monitor.background_image) {
        SDL_DestroyTexture(monitor.background_image);
        monitor.background_image = NULL;
    }

    monitor.background_image = load_png(monitor.renderer, back_image);

    if (!monitor.background_image) {
        monitor.theme_name[0] = '\0';
        return;
    }

    strncpy(monitor.theme_name, active_theme, sizeof(monitor.theme_name) - 1);
    monitor.theme_name[sizeof(monitor.theme_name) - 1] = '\0';

    LOG_INFO("video", "Loaded theme background: %s", back_image);
}

static void reload_screensaver() {
    dvd_shutdown();

    char saver_image[MAX_BUFFER_SIZE];
    snprintf(saver_image, sizeof(saver_image), "%s/%simage/screensaver.png", theme_base, mux_dim);

    if (!file_exist(saver_image)) {
        snprintf(saver_image, sizeof(saver_image), "%s/image/screensaver.png", theme_base);
        if (!file_exist(saver_image)) snprintf(saver_image, sizeof(saver_image), OPT_SHARE_PATH "media/logo.png");
    }

    dvd_init(monitor.renderer, saver_image, device.SCREEN.WIDTH, device.SCREEN.HEIGHT);
}

void check_theme_change(void) {
    const char *theme = config.THEME.ACTIVE;
    if (strncmp(theme, monitor.theme_name, sizeof(monitor.theme_name)) != 0) {
        LOG_DEBUG("video", "Theme change detected: %s -> %s", monitor.theme_name, theme);

        reload_background(theme);
        reload_screensaver();

        monitor.refresh = true;
    }
}

static inline void rect_union_xywh(SDL_Rect *dst, const SDL_Rect *src) {
    int x1 = (dst->x < src->x) ? dst->x : src->x;
    int y1 = (dst->y < src->y) ? dst->y : src->y;

    int x2 = ((dst->x + dst->w) > (src->x + src->w)) ? (dst->x + dst->w) : (src->x + src->w);
    int y2 = ((dst->y + dst->h) > (src->y + src->h)) ? (dst->y + dst->h) : (src->y + src->h);

    dst->x = x1;
    dst->y = y1;

    dst->w = x2 - x1;
    dst->h = y2 - y1;
}

static inline void accumulate_pending_rect(const SDL_Rect *upd) {
    if (upd->w <= 0 || upd->h <= 0) return;

    if (!pending_rect_valid) {
        pending_rect = *upd;
        pending_rect_valid = true;
        return;
    }

    rect_union_xywh(&pending_rect, upd);
}

static inline int scale_pixels(int px, float zoom) {
    return (int) ((float) px * zoom + 0.5f);
}

static inline int pct_offset(int screen, int render, float percent) {
    return ((screen - render) / 2) + (int) ((percent / 100.0f) * (float) screen);
}

static void update_render_state(void) {
    switch (config.SETTINGS.GENERAL.THEME_SCALING) {
        case 0: // No Scaling
            scale_width = device.MUX.WIDTH;
            scale_height = device.MUX.HEIGHT;
            LOG_INFO("video", "Scaling: Disabled");
            break;
        case 2: // Stretch to Screen
            if (device.SCREEN.ZOOM_WIDTH <= 0.0f || device.SCREEN.ZOOM_HEIGHT <= 0.0f) {
                scale_width = device.MUX.WIDTH;
                scale_height = device.MUX.HEIGHT;
            } else {
                scale_width = scale_pixels(device.MUX.WIDTH, device.SCREEN.ZOOM_WIDTH);
                scale_height = scale_pixels(device.MUX.HEIGHT, device.SCREEN.ZOOM_HEIGHT);
            }
            LOG_INFO("video", "Scaling: Stretch");
            break;
        default: // Scale with letterbox
            if (device.SCREEN.ZOOM <= 0.0f) {
                scale_width = device.MUX.WIDTH;
                scale_height = device.MUX.HEIGHT;
            } else {
                scale_width = scale_pixels(device.MUX.WIDTH, device.SCREEN.ZOOM);
                scale_height = scale_pixels(device.MUX.HEIGHT, device.SCREEN.ZOOM);
            }
            LOG_INFO("video", "Scaling: Scale");
            break;
    }

    LOG_INFO("video", "Device Scale: %dx%d", scale_width, scale_height);

    underscan = (config.BOOT.DEVICE_MODE && config.SETTINGS.HDMI.SCAN) ? 16 : 0;
    LOG_INFO("video", "Device Underscan: %d", underscan);

    int offset_render_x = pct_offset(device.SCREEN.WIDTH, scale_width, device.SCREEN.RENDER_OFFSET_X);
    int offset_render_y = pct_offset(device.SCREEN.HEIGHT, scale_height, device.SCREEN.RENDER_OFFSET_Y);

    int offset_theme_x = pct_offset(scale_width, scale_width, theme.SDL.RENDER.OFFSET_X);
    int offset_theme_y = pct_offset(scale_height, scale_height, theme.SDL.RENDER.OFFSET_Y);

    monitor.dest_rect = (SDL_Rect) {
            offset_render_x + offset_theme_x + underscan,
            offset_render_y + offset_theme_y + underscan,
            scale_width - (underscan * 2),
            scale_height - (underscan * 2)
    };

    if (monitor.dest_rect.w <= 0 || monitor.dest_rect.h <= 0) {
        monitor.dest_rect = (SDL_Rect) {0, 0, device.SCREEN.WIDTH, device.SCREEN.HEIGHT};
    }

    if (hdmi_mode) {
        monitor.angle = 0.0;
        monitor.pivot_ptr = NULL;
    } else {
        monitor.angle = (device.SCREEN.ROTATE <= 3) ? device.SCREEN.ROTATE * 90.0 : device.SCREEN.ROTATE;
        monitor.pivot = (SDL_Point) {device.SCREEN.ROTATE_PIVOT_X, device.SCREEN.ROTATE_PIVOT_Y};
        monitor.pivot_ptr = (monitor.pivot.x > 0 && monitor.pivot.y > 0) ? &monitor.pivot : NULL;
    }

    // So if we are not covering the whole window we'll clear it to avoid artifacts
    monitor.needs_clear =
            (monitor.dest_rect.x > 0) ||
            (monitor.dest_rect.y > 0) ||
            (monitor.dest_rect.w < device.SCREEN.WIDTH) ||
            (monitor.dest_rect.h < device.SCREEN.HEIGHT) ||
            (underscan > 0);
}

void sdl_init(void) {
    SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "1");
    SDL_SetHint(SDL_HINT_AUDIO_RESAMPLING_MODE, "1");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_SetHint(SDL_HINT_APP_NAME, MUX_CALLER);
    SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");

    if (hdmi_mode) {
        SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
    } else {
        SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) {
        LOG_ERROR("video", "SDL Init Failed: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    SDL_GameControllerEventState(SDL_ENABLE);
    SDL_JoystickEventState(SDL_ENABLE);

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        LOG_ERROR("video", "PNG Init Failed: %s", IMG_GetError());
    }

    SDL_ShowCursor(SDL_DISABLE);
    update_render_state();

    monitor.window = SDL_CreateWindow(MUX_CALLER, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                      device.SCREEN.WIDTH, device.SCREEN.HEIGHT,
                                      SDL_WINDOW_FULLSCREEN | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!monitor.window) {
        LOG_ERROR("video", "Window Creation Failed: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    monitor.renderer = SDL_CreateRenderer(monitor.window, -1,
                                          hdmi_mode ? SDL_RENDERER_ACCELERATED
                                                    : (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)
    );

    if (!monitor.renderer) {
        LOG_ERROR("video", "Renderer Creation Failed: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    int out_w = 0, out_h = 0;
    SDL_GetRendererOutputSize(monitor.renderer, &out_w, &out_h);

    monitor.texture = SDL_CreateTexture(monitor.renderer,
                                        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC,
                                        device.MUX.WIDTH, device.MUX.HEIGHT);

    if (!monitor.texture) {
        LOG_ERROR("video", "Texture Creation Failed: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    SDL_SetTextureScaleMode(monitor.texture, SDL_ScaleModeNearest);
    update_blend_mode();

    {
        const size_t tex_pixels = (size_t) device.MUX.WIDTH * (size_t) device.MUX.HEIGHT;
        const size_t tex_bytes = tex_pixels * sizeof(uint32_t);
        uint32_t * clear_buf = calloc(tex_pixels, sizeof(uint32_t));

        if (!clear_buf) {
            LOG_ERROR("video", "Texture clear buffer allocation failed");
            exit(EXIT_FAILURE);
        }

        if (SDL_UpdateTexture(monitor.texture, NULL, clear_buf, device.MUX.WIDTH * (int) sizeof(uint32_t)) != 0) {
            LOG_ERROR("video", "Initial texture clear failed: %s", SDL_GetError());
            free(clear_buf);
            exit(EXIT_FAILURE);
        }

        free(clear_buf);
        (void) tex_bytes;
    }

    pending_rect = (SDL_Rect) {0, 0, 0, 0};
    pending_rect_valid = false;
    monitor.refresh = true;
    monitor.force_clear = true;

    LOG_INFO("video", "SDL Video Initialised Successfully");

    SDL_RendererInfo info;
    SDL_GetRendererInfo(monitor.renderer, &info);

    LOG_INFO("video", "SDL Renderer: %s (%dx%d)", info.name, out_w, out_h);

    reload_background(config.THEME.ACTIVE);
    reload_screensaver();
}

void sdl_cleanup(void) {
    dvd_shutdown();

    if (monitor.texture) SDL_DestroyTexture(monitor.texture);
    if (monitor.renderer) SDL_DestroyRenderer(monitor.renderer);
    if (monitor.window) SDL_DestroyWindow(monitor.window);
    if (monitor.background_image) SDL_DestroyTexture(monitor.background_image);

    IMG_Quit();
    SDL_Quit();
}

void run_dvd_screensaver_loop(void) {
    mux_input_flush_all();

    SDL_Event ev;
    const uint32_t frame_ms = IDLE_MS;
    uint32_t next = SDL_GetTicks();

    while (dvd_active()) {
        uint32_t now = SDL_GetTicks();
        int timeout = (int) (next > now ? next - now : 0);

        if (SDL_WaitEventTimeout(&ev, timeout)) {
            do {
                switch (ev.type) {
                    case SDL_KEYDOWN:
                    case SDL_CONTROLLERBUTTONDOWN:
                    case SDL_JOYBUTTONDOWN:
                    case SDL_MOUSEBUTTONDOWN:
                    case SDL_QUIT:
                        dvd_stop();
                        break;
                    default:
                        break;
                }
            } while (SDL_PollEvent(&ev));
        }

        // The following looks really stupid but it works!
        if (!dvd_active()) break;
        dvd_update();
        if (!dvd_active()) break;

        SDL_SetRenderDrawColor(monitor.renderer, theme.SDL.SOLID.R, theme.SDL.SOLID.G, theme.SDL.SOLID.B, 255);
        SDL_RenderClear(monitor.renderer);
        dvd_render(monitor.renderer);
        SDL_RenderPresent(monitor.renderer);

        next += frame_ms;
    }

    monitor.force_clear = true;
    monitor.refresh = true;

    mux_input_resume();
}

void display_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    if (!monitor.texture || !monitor.renderer || !area || !color_p ||
        area->x2 < 0 || area->y2 < 0 ||
        area->x1 >= device.MUX.WIDTH || area->y1 >= device.MUX.HEIGHT) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    // Since we've got direct mode switched off we need to clamp to the screen
    // or we'll get lovely glitches galore all around the place
    int x1 = LV_CLAMP(0, area->x1, device.MUX.WIDTH - 1);
    int y1 = LV_CLAMP(0, area->y1, device.MUX.HEIGHT - 1);
    int x2 = LV_CLAMP(0, area->x2, device.MUX.WIDTH - 1);
    int y2 = LV_CLAMP(0, area->y2, device.MUX.HEIGHT - 1);

    if (x2 < x1 || y2 < y1) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    // Optimised full-screen flush...
    const int copy_w = x2 - x1 + 1;
    const int copy_h = y2 - y1 + 1;

    const int src_w = area->x2 - area->x1 + 1;
    const int src_x_ofs = x1 - area->x1;
    const int src_y_ofs = y1 - area->y1;

    SDL_Rect upd = {.x = x1, .y = y1, .w = copy_w, .h = copy_h};

    const uint8_t *src_base = (const uint8_t *) color_p;
    const uint8_t *src_ptr = src_base + ((size_t) (src_y_ofs * src_w + src_x_ofs) * sizeof(lv_color_t));

    const int src_pitch = src_w * (int) sizeof(lv_color_t);
    const int dst_pitch = copy_w * (int) sizeof(lv_color_t);

    uint8_t *stage = malloc((size_t) copy_h * dst_pitch);
    if (!stage) {
        LOG_ERROR("video", "Failed to allocate staging buffer for partial flush");
        lv_disp_flush_ready(disp_drv);
        return;
    }

    uint8_t *dst = stage;
    const uint8_t *src = src_ptr;

    for (int y = 0; y < copy_h; y++) {
        memcpy(dst, src, dst_pitch);
        dst += dst_pitch;
        src += src_pitch;
    }

    if (SDL_UpdateTexture(monitor.texture, &upd, stage, dst_pitch) != 0) {
        LOG_ERROR("video", "SDL_UpdateTexture staged upload failed: %s", SDL_GetError());
        free(stage);
        lv_disp_flush_ready(disp_drv);
        return;
    }

    free(stage);
    accumulate_pending_rect(&upd);

    if (!lv_disp_flush_is_last(disp_drv)) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    // Update our screensaver function, and if we update successfully
    // then we mark it as active and run the loop with a complete
    // separate input logic scheme that does not interrupt the muX frontend!
    if (config.SETTINGS.POWER.SCREENSAVER) dvd_update();
    if (config.SETTINGS.POWER.SCREENSAVER && dvd_active()) {
        pending_rect_valid = false;
        run_dvd_screensaver_loop();
    }

    if (monitor.needs_clear || monitor.force_clear) {
        if (monitor.background_image) {
            SDL_Rect full = {0, 0, device.SCREEN.WIDTH, device.SCREEN.HEIGHT};
            SDL_RenderCopy(monitor.renderer, monitor.background_image, NULL, &full);
        } else {
            SDL_SetRenderDrawColor(monitor.renderer, theme.SDL.SOLID.R, theme.SDL.SOLID.G, theme.SDL.SOLID.B, 255);
            SDL_RenderClear(monitor.renderer);
        }

        monitor.force_clear = false;
    }

    // LOG_DEBUG("sdl", "\tdest_rect: %d %d %d %d", dest_rect.x, dest_rect.y, dest_rect.w, dest_rect.h);

    // Simplify the rendering if we are not rotating as this saves a few cycles
    if (monitor.angle == 0.0) {
        SDL_RenderCopy(monitor.renderer, monitor.texture, NULL, &monitor.dest_rect);
    } else {
        SDL_RenderCopyEx(monitor.renderer, monitor.texture, NULL, &monitor.dest_rect,
                         monitor.angle, monitor.pivot_ptr, SDL_FLIP_NONE);
    }

    SDL_RenderPresent(monitor.renderer);

    pending_rect_valid = false;
    pending_rect = (SDL_Rect) {0, 0, 0, 0};

    lv_disp_flush_ready(disp_drv);
}
