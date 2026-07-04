#include <math.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include "muxshare.h"
#include "text.h"

#define SHARE_DIR "/opt/muos/share/"
#define MEDIA_DIR SHARE_DIR "media"
#define FONT_FILE SHARE_DIR "font/mucredits.ttf"

#define REF_H 480
#define REF_W 640

#define PAGE_PAD_FRAC   0.06f
#define WARN_TIME_SCALE 2.25f

#define WARN_FADE_IN_S  (1.25f * WARN_TIME_SCALE)
#define WARN_HOLD_S     (9.75f * WARN_TIME_SCALE)
#define WARN_FADE_OUT_S (1.25f * WARN_TIME_SCALE)
#define WARN_TOTAL_S    (WARN_FADE_IN_S + WARN_HOLD_S + WARN_FADE_OUT_S)

#define KB_ZOOM 0.18f
#define KB_PAN  0.06f

#define COL_TITLE_R 247
#define COL_TITLE_G 227
#define COL_TITLE_B 24

#define COL_BODY_R 221
#define COL_BODY_G 162
#define COL_BODY_B 0

#define BG_DARKEN_ALPHA 45

#define FONT_BIG_PT 30
#define FONT_MED_PT 22

#define FONT_MED_MIN_PT 10

#define LANG_TITLE "MustardOS Disclaimer"

#define LANG_BODY                                                                                                      \
    "MustardOS is built with respect for developers, artists, and "                                                    \
    "content creators. The system and tools provided are intended for use "                                            \
    "with legally obtained files only.\n\n"                                                                            \
    "MustardOS does not condone, support, or facilitate the use of "                                                   \
    "unauthorised content in any form. Sourcing, distributing, or using "                                              \
    "such content is solely the responsibility of the user and is done "                                               \
    "against the spirit of this open source and free to use project.\n\n"                                              \
    "By installing and using MustardOS, you agree to respect the work of "                                             \
    "those who make the content we all enjoy."

static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;

static int g_screen_w = 0;
static int g_screen_h = 0;
static float g_scale = 1.0f;
static float g_font_scale = 1.0f;

static TTF_Font *g_font_big = NULL;
static TTF_Font *g_font_med = NULL;

typedef struct {
    SDL_Texture *tex;
    int w, h;
} bg_image_t;

static bg_image_t g_bg = {NULL, 0, 0};
static float g_bg_kb_t = 0.0f;

static SDL_Texture *g_tex_title = NULL;
static int g_tex_title_w = 0, g_tex_title_h = 0;

static SDL_Texture *g_tex_body = NULL;
static int g_tex_body_w = 0, g_tex_body_h = 0;

static int g_title_x = 0, g_title_y = 0;
static int g_body_x = 0, g_body_y = 0;
static int g_div_x = 0, g_div_y = 0, g_div_w = 0, g_div_h = 0;
static int g_glow_x = 0, g_glow_y = 0, g_glow_w = 0, g_glow_h = 0;
static int g_bar_x = 0, g_bar_y = 0, g_bar_w = 0, g_bar_h = 0;
static int g_bar_radius = 0;

static int g_running = 1;
static float g_elapsed = 0.0f;

static int sx(const int v) {
    return (int) lroundf((float) v * g_scale);
}

static void load_background(void) {
    static const char *exts[] = {"png", "jpg", "jpeg", NULL};
    for (int i = 0; exts[i]; ++i) {
        char path[512];
        snprintf(path, sizeof(path), "%s/warn.%s", MEDIA_DIR, exts[i]);
        SDL_Surface *surf = IMG_Load(path);
        if (surf) {
            g_bg.w = surf->w;
            g_bg.h = surf->h;
            g_bg.tex = SDL_CreateTextureFromSurface(g_renderer, surf);
            if (g_bg.tex) {
                SDL_SetTextureScaleMode(g_bg.tex, SDL_ScaleModeLinear);
                SDL_SetTextureBlendMode(g_bg.tex, SDL_BLENDMODE_BLEND);
            }
            SDL_FreeSurface(surf);
            return;
        }
    }

    LOG_INFO(mux_module, "no background image found at %s/warn.{png,jpg,jpeg}", MEDIA_DIR);
}

static void bg_render(const Uint8 alpha) {
    const float img_w = (float) g_bg.w;
    const float img_h = (float) g_bg.h;

    const float screen_aspect = (float) g_screen_w / (float) g_screen_h;
    const float img_aspect = img_w / img_h;

    float crop_w = img_w;
    float crop_h = img_h;

    if (img_aspect > screen_aspect) {
        crop_w = img_h * screen_aspect;
    } else {
        crop_h = img_w / screen_aspect;
    }

    SDL_Rect src;

    src.w = (int) lroundf(crop_w);
    src.h = (int) lroundf(crop_h);

    if (src.w < 1) src.w = 1;
    if (src.h < 1) src.h = 1;

    if (src.w > g_bg.w) src.w = g_bg.w;
    if (src.h > g_bg.h) src.h = g_bg.h;

    src.x = (g_bg.w - src.w) / 2;
    src.y = (g_bg.h - src.h) / 2;

    const float t = sdl_smoothstep01(g_bg_kb_t);

    const float zoom = 1.0f + KB_ZOOM * t;

    const float dw = (float) g_screen_w * zoom;
    const float dh = (float) g_screen_h * zoom;

    const float slack_x = dw - (float) g_screen_w;
    const float slack_y = dh - (float) g_screen_h;

    const float pan_x = (t - 0.5f) * 2.0f * KB_PAN * slack_x * 0.5f;
    const float pan_y = (t - 0.5f) * 2.0f * KB_PAN * slack_y * 0.5f * 0.6f;

    SDL_FRect dst;

    dst.x = -slack_x * 0.5f + pan_x;
    dst.y = -slack_y * 0.5f + pan_y;

    dst.w = dw;
    dst.h = dh;

    SDL_SetTextureAlphaMod(g_bg.tex, alpha);
    SDL_RenderCopyF(g_renderer, g_bg.tex, &src, &dst);
}

static int text_shadow_offset(void) {
    const int v = sx(1);
    return v < 1 ? 1 : v;
}

static SDL_Texture *render_text(TTF_Font *font, const char *text, const SDL_Color col, int *out_w, int *out_h) {
    return sdl_render_text(g_renderer, font, text, col, text_shadow_offset(), out_w, out_h);
}

static SDL_Texture *
render_text_wrapped(TTF_Font *font, const char *text, const SDL_Color col, const int wrap_w, int *out_w, int *out_h) {
    return sdl_render_text_wrapped(g_renderer, font, text, col, wrap_w, text_shadow_offset(), out_w, out_h);
}

static int layout_title_gap(void) {
    return sx(14);
}

static int layout_bar_gap(void) {
    return sx(28);
}

static int layout_bar_h(void) {
    return sx(10);
}

static int layout_v_margin(void) {
    return sx(20);
}

static int layout_chrome_h(void) {
    return g_tex_title_h + layout_title_gap() + layout_bar_gap() + layout_bar_h();
}

static int layout_avail_h(void) {
    return g_screen_h - 2 * layout_v_margin();
}

static void build_textures(void) {
    const SDL_Color c_title = {COL_TITLE_R, COL_TITLE_G, COL_TITLE_B, 255};
    const SDL_Color c_body = {COL_BODY_R, COL_BODY_G, COL_BODY_B, 255};

    const int page_pad = (int) ((float) g_screen_w * PAGE_PAD_FRAC);
    const int wrap = g_screen_w - 2 * page_pad;

    g_tex_title = render_text(g_font_big, LANG_TITLE, c_title, &g_tex_title_w, &g_tex_title_h);
    if (g_tex_title && g_tex_title_w > wrap) {
        SDL_DestroyTexture(g_tex_title);
        g_tex_title = render_text_wrapped(g_font_big, LANG_TITLE, c_title, wrap, &g_tex_title_w, &g_tex_title_h);
    }

    g_tex_body = render_text_wrapped(g_font_med, LANG_BODY, c_body, wrap, &g_tex_body_w, &g_tex_body_h);

    if (!g_tex_title || !g_tex_body) sdl_text_die("build_textures");

    int body_pt = (int) lroundf((float) FONT_MED_PT * g_font_scale);
    int min_pt = (int) lroundf((float) FONT_MED_MIN_PT * g_font_scale);
    if (min_pt < FONT_MED_MIN_PT) min_pt = FONT_MED_MIN_PT;
    const int avail_h = layout_avail_h();

    while (body_pt > min_pt) {
        if (g_tex_body_h + layout_chrome_h() <= avail_h) break;

        body_pt -= 1;

        TTF_CloseFont(g_font_med);
        g_font_med = TTF_OpenFont(FONT_FILE, body_pt);
        if (!g_font_med) sdl_text_die("TTF_OpenFont (shrink)");

        SDL_DestroyTexture(g_tex_body);
        g_tex_body = render_text_wrapped(g_font_med, LANG_BODY, c_body, wrap, &g_tex_body_w, &g_tex_body_h);
        if (!g_tex_body) sdl_text_die("build_textures (shrink)");
    }
}

static void build_layout(void) {
    const int title_gap = layout_title_gap();
    const int bar_gap = layout_bar_gap();

    g_bar_h = layout_bar_h();
    g_bar_w = (int) ((float) g_screen_w * (1.0f - PAGE_PAD_FRAC * 2.0f) * 0.70f);
    g_bar_radius = g_bar_h / 2;

    const int content_h = g_tex_title_h + title_gap + g_tex_body_h + bar_gap + g_bar_h;

    int start_y = (g_screen_h - content_h) / 2;
    const int v_margin = layout_v_margin();
    if (start_y < v_margin) start_y = v_margin;

    g_title_x = (g_screen_w - g_tex_title_w) / 2;
    g_title_y = start_y;

    g_body_y = start_y + g_tex_title_h + title_gap;
    g_body_x = (g_screen_w - g_tex_body_w) / 2;

    g_div_w = (int) ((float) g_screen_w * 0.40f);
    g_div_h = sx(1) < 1 ? 1 : sx(1);
    g_div_x = (g_screen_w - g_div_w) / 2;
    g_div_y = g_body_y - sx(8);

    g_glow_x = g_div_x - sx(4);
    g_glow_y = g_div_y - sx(2);
    g_glow_w = g_div_w + sx(8);
    g_glow_h = g_div_h + sx(4);

    g_bar_x = (g_screen_w - g_bar_w) / 2;
    g_bar_y = g_body_y + g_tex_body_h + bar_gap;
}

static void
fill_rounded_rect(const SDL_Rect r, const int radius, const Uint8 cr, const Uint8 cg, const Uint8 cb, const Uint8 ca) {
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, cr, cg, cb, ca);

    if (radius <= 0 || radius * 2 > r.w || radius * 2 > r.h) {
        SDL_RenderFillRect(g_renderer, &r);
        return;
    }

    const SDL_Rect centre = {r.x + radius, r.y, r.w - radius * 2, r.h};
    const SDL_Rect left = {r.x, r.y + radius, radius, r.h - radius * 2};
    const SDL_Rect right = {r.x + r.w - radius, r.y + radius, radius, r.h - radius * 2};
    SDL_RenderFillRect(g_renderer, &centre);
    SDL_RenderFillRect(g_renderer, &left);
    SDL_RenderFillRect(g_renderer, &right);

    for (int cy = 0; cy < radius; cy++) {
        for (int cx = 0; cx < radius; cx++) {
            const int dx = radius - cx - 1;
            const int dy = radius - cy - 1;
            if (dx * dx + dy * dy <= radius * radius) {
                SDL_RenderDrawPoint(g_renderer, r.x + cx, r.y + cy);
                SDL_RenderDrawPoint(g_renderer, r.x + r.w - 1 - cx, r.y + cy);
                SDL_RenderDrawPoint(g_renderer, r.x + cx, r.y + r.h - 1 - cy);
                SDL_RenderDrawPoint(g_renderer, r.x + r.w - 1 - cx, r.y + r.h - 1 - cy);
            }
        }
    }
}

static void render_background(const float dt, const Uint8 master_alpha) {
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);

    g_bg_kb_t += dt / WARN_TOTAL_S;
    if (g_bg_kb_t > 1.0f) g_bg_kb_t = 1.0f;

    if (g_bg.tex) {
        bg_render(master_alpha);

        const Uint8 dim_a = (Uint8) lroundf((float) BG_DARKEN_ALPHA * ((float) master_alpha / 255.0f));
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, dim_a);
        const SDL_Rect full = {0, 0, g_screen_w, g_screen_h};
        SDL_RenderFillRect(g_renderer, &full);
    }
}

static void render_content(const Uint8 master_alpha, const float bar_pct) {
    SDL_SetTextureAlphaMod(g_tex_title, master_alpha);
    const SDL_Rect title_dst = {g_title_x, g_title_y, g_tex_title_w, g_tex_title_h};
    SDL_RenderCopy(g_renderer, g_tex_title, NULL, &title_dst);

    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(
        g_renderer, COL_TITLE_R, COL_TITLE_G, COL_TITLE_B, (Uint8) lroundf((float) master_alpha * 0.20f)
    );
    const SDL_Rect glow = {g_glow_x, g_glow_y, g_glow_w, g_glow_h};
    SDL_RenderFillRect(g_renderer, &glow);

    SDL_SetRenderDrawColor(g_renderer, COL_TITLE_R, COL_TITLE_G, COL_TITLE_B, master_alpha);
    const SDL_Rect line = {g_div_x, g_div_y, g_div_w, g_div_h};
    SDL_RenderFillRect(g_renderer, &line);

    SDL_SetTextureAlphaMod(g_tex_body, master_alpha);
    const SDL_Rect body_dst = {g_body_x, g_body_y, g_tex_body_w, g_tex_body_h};
    SDL_RenderCopy(g_renderer, g_tex_body, NULL, &body_dst);

    const SDL_Rect track = {g_bar_x, g_bar_y, g_bar_w, g_bar_h};
    fill_rounded_rect(track, g_bar_radius, 0x22, 0x22, 0x22, (Uint8) lroundf((float) master_alpha * 0.55f));

    int fill_w = (int) lroundf((float) g_bar_w * bar_pct);
    if (fill_w > 0) {
        if (fill_w < g_bar_radius * 2) fill_w = g_bar_radius * 2;
        const SDL_Rect fill = {g_bar_x, g_bar_y, fill_w, g_bar_h};
        fill_rounded_rect(fill, g_bar_radius, 0xFF, 0xFF, 0xFF, master_alpha);
    }
}

static void handle_event(const SDL_Event *ev) {
    if (ev->type == SDL_QUIT) g_running = 0;
}

static void init_sdl(void) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) sdl_text_die("SDL_Init");
    if (TTF_Init() < 0) sdl_text_die("TTF_Init");

    const int img_flags = IMG_INIT_PNG | IMG_INIT_JPG;
    if ((IMG_Init(img_flags) & img_flags) != img_flags) LOG_WARN(mux_module, "IMG_Init partial: %s", IMG_GetError());

    g_window = SDL_CreateWindow(
        mux_module, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0,
        SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS
    );

    if (!g_window) sdl_text_die("SDL_CreateWindow");

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    if (!g_renderer) sdl_text_die("SDL_CreateRenderer");

    SDL_GetRendererOutputSize(g_renderer, &g_screen_w, &g_screen_h);
    g_scale = (float) g_screen_h / (float) REF_H;

    const float scale_w = (float) g_screen_w / (float) REF_W;
    const float scale_h = (float) g_screen_h / (float) REF_H;
    g_font_scale = scale_w < scale_h ? scale_w : scale_h;

    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_ShowCursor(SDL_DISABLE);
}

static void load_fonts(void) {
    const int s_big = (int) lroundf((float) FONT_BIG_PT * g_font_scale);
    const int s_med = (int) lroundf((float) FONT_MED_PT * g_font_scale);

    g_font_big = TTF_OpenFont(FONT_FILE, s_big);
    g_font_med = TTF_OpenFont(FONT_FILE, s_med);

    if (!g_font_big || !g_font_med) sdl_text_die("TTF_OpenFont");
}

static void main_loop(void) {
    Uint64 prev = SDL_GetPerformanceCounter();
    const double freq = (double) SDL_GetPerformanceFrequency();

    while (1) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
            handle_event(&ev);

        if (!g_running) break;

        const Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float) ((double) (now - prev) / freq);
        prev = now;
        if (dt > 0.1f) dt = 0.1f;

        g_elapsed += dt;
        if (g_elapsed >= WARN_TOTAL_S) break;

        float alpha_f;
        if (g_elapsed < WARN_FADE_IN_S) {
            alpha_f = g_elapsed / WARN_FADE_IN_S;
        } else if (g_elapsed < WARN_FADE_IN_S + WARN_HOLD_S) {
            alpha_f = 1.0f;
        } else {
            const float fo = g_elapsed - WARN_FADE_IN_S - WARN_HOLD_S;
            alpha_f = 1.0f - fo / WARN_FADE_OUT_S;
        }
        alpha_f = sdl_smoothstep01(alpha_f);
        const Uint8 master_alpha = (Uint8) lroundf(alpha_f * 255.0f);

        float bar_pct = 0.0f;
        if (g_elapsed >= WARN_FADE_IN_S) {
            float hold_elapsed = g_elapsed - WARN_FADE_IN_S;
            if (hold_elapsed > WARN_HOLD_S) hold_elapsed = WARN_HOLD_S;
            bar_pct = hold_elapsed / WARN_HOLD_S;
        }

        render_background(dt, master_alpha);
        render_content(master_alpha, bar_pct);
        SDL_RenderPresent(g_renderer);
    }
}

static void cleanup(void) {
    if (g_bg.tex) SDL_DestroyTexture(g_bg.tex);

    if (g_tex_title) SDL_DestroyTexture(g_tex_title);
    if (g_tex_body) SDL_DestroyTexture(g_tex_body);

    if (g_font_big) TTF_CloseFont(g_font_big);
    if (g_font_med) TTF_CloseFont(g_font_med);

    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);

    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}

int main(void) {
    init_module("muwarn");
    load_device(&device);
    load_config(&config);

    init_sdl();
    load_fonts();
    load_background();
    build_textures();
    build_layout();

    main_loop();

    cleanup();
    return 0;
}
