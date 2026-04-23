#include <sys/stat.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "../../common/log.h"
#include "../common/common.h"
#include "notif.h"

#define MAX_LINES 128
#define MAX_LINE_CHARS 512

int notif_visible = 0;
notif_cfg_t notif_cfg;

static struct stat notif_stat_last;

typedef enum {
    NOTIF_SCROLL_NONE,
    NOTIF_SCROLL_MARQUEE,
    NOTIF_SCROLL_VERTICAL
} notif_scroll_mode_t;

static SDL_Surface *notif_content_surf = NULL;
static int notif_content_w = 0;

static int notif_box_w = 0;
static int notif_box_h = 0;

static int notif_bx = 0;
static int notif_by = 0;

static notif_scroll_mode_t notif_scroll_mode = NOTIF_SCROLL_NONE;
static float notif_scroll_pos = 0.0f;
static uint64_t notif_scroll_start = 0;
static int notif_scroll_total = 0;

static SDL_Texture *notif_sdl_tex = NULL;

static int notif_sdl_w = 0;
static int notif_sdl_h = 0;
static int notif_sdl_bx = 0;
static int notif_sdl_by = 0;

static GLuint notif_gles_tex = 0;
static int notif_gles_w = 0;
static int notif_gles_h = 0;
static gl_vtx_t vtx_notif[4];

/* File parsing:
 * /run/muos/overlay.notif format (one field per line):
 *   Line 1 – font size (integer, e.g. 24)
 *   Line 2 – foreground colour, hex without '#' (e.g. FFFFFF)
 *   Line 3 – border colour, hex without '#' (e.g. 000000)
 *   Line 4 – position: top | middle | bottom
 *   Line 5 – dim screen: 0 or 1
 *   Line 6 – notification text (word-wrapped automatically, can be long!)
 */
static int parse_notif_file(void) {
    char fs_buf[16] = "24";
    char fg_buf[16] = "FFFFFF";
    char bdr_buf[16] = "000000";
    char pos_buf[16] = "middle";
    char dim_buf[4] = "1";
    char text_buf[1024];
    text_buf[0] = '\0';

    if (!read_line_from_file(NOTIF_PATH, 1, fs_buf, sizeof(fs_buf))) return 0;
    if (!read_line_from_file(NOTIF_PATH, 2, fg_buf, sizeof(fg_buf))) return 0;
    if (!read_line_from_file(NOTIF_PATH, 3, bdr_buf, sizeof(bdr_buf))) return 0;
    if (!read_line_from_file(NOTIF_PATH, 4, pos_buf, sizeof(pos_buf))) return 0;
    if (!read_line_from_file(NOTIF_PATH, 5, dim_buf, sizeof(dim_buf))) return 0;
    if (!read_line_from_file(NOTIF_PATH, 6, text_buf, sizeof(text_buf))) return 0;

    if (!text_buf[0]) return 0;

    notif_cfg.font_size = safe_atoi(fs_buf);
    if (notif_cfg.font_size < 8) notif_cfg.font_size = 8;
    if (notif_cfg.font_size > 96) notif_cfg.font_size = 96;

    notif_cfg.fg.a = 255;
    notif_cfg.border.a = 255;

    if (!parse_hex_colour(fg_buf, &notif_cfg.fg)) {
        notif_cfg.fg.r = 255;
        notif_cfg.fg.g = 255;
        notif_cfg.fg.b = 255;
    }
    if (!parse_hex_colour(bdr_buf, &notif_cfg.border)) {
        notif_cfg.border.r = 0;
        notif_cfg.border.g = 0;
        notif_cfg.border.b = 0;
    }

    if (strcmp(pos_buf, "top") == 0) {
        notif_cfg.position = NOTIF_POS_TOP;
    } else if (strcmp(pos_buf, "bottom") == 0) {
        notif_cfg.position = NOTIF_POS_BOTTOM;
    } else {
        notif_cfg.position = NOTIF_POS_MIDDLE;
    }

    notif_cfg.dim = (safe_atoi(dim_buf) != 0);

    strncpy(notif_cfg.text, text_buf, sizeof(notif_cfg.text) - 1);
    notif_cfg.text[sizeof(notif_cfg.text) - 1] = '\0';

    return 1;
}

static int word_wrap(TTF_Font *font, const char *text, int max_px, char lines[][MAX_LINE_CHARS]) {
    int line_count = 0;
    char cur[MAX_LINE_CHARS];
    cur[0] = '\0';

    const char *src = text;

    while (*src && line_count < MAX_LINES) {
        char word[MAX_LINE_CHARS];
        int wi = 0;

        while (*src && *src != ' ' && *src != '\n') {
            if (wi < (int) sizeof(word) - 1) word[wi++] = *src;
            src++;
        }
        word[wi] = '\0';

        int at_newline = (*src == '\n');
        if (*src == ' ' || *src == '\n') src++;

        if (wi == 0) {
            if (at_newline) {
                strncpy(lines[line_count++], cur, MAX_LINE_CHARS - 1);
                cur[0] = '\0';
            }
            continue;
        }

        char trial[MAX_LINE_CHARS];
        if (cur[0]) snprintf(trial, sizeof(trial), "%s %s", cur, word);
        else snprintf(trial, sizeof(trial), "%s", word);

        int tw = 0, th = 0;
        TTF_SizeUTF8(font, trial, &tw, &th);

        if (tw > max_px && cur[0]) {
            strncpy(lines[line_count++], cur, MAX_LINE_CHARS - 1);
            strncpy(cur, word, sizeof(cur) - 1);
        } else {
            strncpy(cur, trial, sizeof(cur) - 1);
        }

        if (at_newline) {
            strncpy(lines[line_count++], cur, MAX_LINE_CHARS - 1);
            cur[0] = '\0';
        }
    }

    if (cur[0] && line_count < MAX_LINES)
        strncpy(lines[line_count++], cur, MAX_LINE_CHARS - 1);

    return line_count;
}

static SDL_Surface *build_notif_content(int fb_w, int fb_h, int *out_bx, int *out_by, int *out_box_w, int *out_box_h,
                                        notif_scroll_mode_t *out_scroll_mode, int *out_scroll_total) {
    if (!TTF_WasInit() && TTF_Init() != 0) {
        LOG_ERROR("notif", "TTF_Init: %s", TTF_GetError());
        return NULL;
    }

    TTF_Font *font = TTF_OpenFont(NOTIF_FONT, notif_cfg.font_size);
    if (!font) {
        LOG_ERROR("notif", "TTF_OpenFont: %s", TTF_GetError());
        return NULL;
    }

    const int pad = NOTIF_PADDING;
    const int border = NOTIF_BORDER;
    const int line_h = TTF_FontLineSkip(font);

    const int max_box_w = (int) ((float) fb_w * NOTIF_MAX_BOX_FRAC);
    const int max_text_w = max_box_w - 2 * (pad + border);

    static char lines[MAX_LINES][MAX_LINE_CHARS];
    int line_count = word_wrap(font, notif_cfg.text, max_text_w, lines);

    if (line_count == 0) {
        TTF_CloseFont(font);
        return NULL;
    }

    int max_line_px = 0;
    for (int i = 0; i < line_count; i++) {
        int tw = 0, th = 0;

        if (lines[i][0]) TTF_SizeUTF8(font, lines[i], &tw, &th);
        if (tw > max_line_px) max_line_px = tw;
    }

    int content_text_w = max_line_px;
    if (content_text_w > max_text_w) content_text_w = max_text_w;

    const int box_inner_w = content_text_w + 2 * pad;
    const int box_w = box_inner_w + 2 * border;

    const int clipped_box_w = box_w < max_box_w ? box_w : max_box_w;
    const int total_content_h = line_count * line_h + 2 * (pad + border);

    const int screen_margin = 2 * pad;
    const int max_visible_h = fb_h - 2 * screen_margin;

    const int max_vis_lines = (max_visible_h - 2 * (pad + border)) / line_h;

    notif_scroll_mode_t scroll_mode = NOTIF_SCROLL_NONE;
    int scroll_total = 0;

    if (line_count == 1 && max_line_px > max_text_w) {
        scroll_mode = NOTIF_SCROLL_MARQUEE;
        scroll_total = max_line_px + NOTIF_MARQUEE_GAP;
    } else if (line_count > max_vis_lines) {
        scroll_mode = NOTIF_SCROLL_VERTICAL;
        scroll_total = total_content_h - (max_vis_lines * line_h + 2 * (pad + border));
    }

    const int visible_lines = (scroll_mode == NOTIF_SCROLL_NONE) ? line_count : (line_count < max_vis_lines ? line_count : max_vis_lines);
    const int box_h = visible_lines * line_h + 2 * (pad + border);

    int bx = (fb_w - clipped_box_w) / 2;
    int by = 0;
    switch (notif_cfg.position) {
        case NOTIF_POS_TOP:
            by = screen_margin;
            break;
        case NOTIF_POS_BOTTOM:
            by = fb_h - box_h - screen_margin;
            break;
        default:
            by = (fb_h - box_h) / 2;
            break;
    }

    if (bx < 0) bx = 0;
    if (by < 0) by = 0;

    const int surf_h = (scroll_mode == NOTIF_SCROLL_VERTICAL) ? total_content_h : box_h;
    const int surf_w = (scroll_mode == NOTIF_SCROLL_MARQUEE) ? (max_line_px + NOTIF_MARQUEE_GAP) : clipped_box_w;

    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, surf_w, surf_h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surf) {
        TTF_CloseFont(font);
        return NULL;
    }

    SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_BLEND);

    const SDL_Color *bc = &notif_cfg.border;
    SDL_FillRect(surf, NULL, SDL_MapRGBA(surf->format, bc->r, bc->g, bc->b, 255));

    if (scroll_mode == NOTIF_SCROLL_MARQUEE) {
        SDL_Rect inner = {0, border, surf_w, surf_h - 2 * border};
        SDL_FillRect(surf, &inner, SDL_MapRGBA(surf->format, 0, 0, 0, 210));
    } else {
        SDL_Rect inner = {border, border, surf_w - 2 * border, surf_h - 2 * border};
        SDL_FillRect(surf, &inner, SDL_MapRGBA(surf->format, 0, 0, 0, 210));
    }

    for (int i = 0; i < line_count; i++) {
        if (!lines[i][0]) continue;

        SDL_Surface *txt = TTF_RenderUTF8_Blended(font, lines[i], notif_cfg.fg);
        if (!txt) continue;

        int lx, ly;
        if (scroll_mode == NOTIF_SCROLL_MARQUEE) {
            lx = 0;
            ly = border + pad;
        } else {
            lx = (surf_w - txt->w) / 2;
            ly = border + pad + i * line_h;
        }

        SDL_Rect dst_rect = {lx, ly, txt->w, txt->h};
        SDL_BlitSurface(txt, NULL, surf, &dst_rect);
        SDL_FreeSurface(txt);
    }

    TTF_CloseFont(font);

    *out_bx = bx;
    *out_by = by;

    *out_box_w = clipped_box_w;
    *out_box_h = box_h;

    *out_scroll_mode = scroll_mode;
    *out_scroll_total = scroll_total;

    return surf;
}

static SDL_Surface *compose_frame(void) {
    if (!notif_content_surf) return NULL;

    if (notif_scroll_mode != NOTIF_SCROLL_NONE) {
        uint64_t now = now_ms();
        int64_t elapsed = (int64_t) (now - notif_scroll_start) - NOTIF_SCROLL_DELAY;

        if (elapsed > 0) {
            float px = (float) elapsed * NOTIF_SCROLL_PPS / 1000.0f;

            if (notif_scroll_mode == NOTIF_SCROLL_MARQUEE) {
                notif_scroll_pos = px - (float) ((int) (px / (float) notif_scroll_total) * notif_scroll_total);
            } else {
                notif_scroll_pos = px < (float) notif_scroll_total ? px : (float) notif_scroll_total;
            }
        } else {
            notif_scroll_pos = 0.0f;
        }
    }

    SDL_Surface *frame = SDL_CreateRGBSurfaceWithFormat(0, notif_box_w, notif_box_h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!frame) return NULL;

    SDL_SetSurfaceBlendMode(frame, SDL_BLENDMODE_BLEND);

    if (notif_scroll_mode == NOTIF_SCROLL_NONE) {
        SDL_BlitSurface(notif_content_surf, NULL, frame, NULL);
    } else if (notif_scroll_mode == NOTIF_SCROLL_MARQUEE) {
        int offset = (int) notif_scroll_pos;
        int cw = notif_content_w;

        SDL_Rect src1 = {offset, 0, cw - offset, notif_box_h};
        SDL_Rect dst1 = {0, 0, cw - offset, notif_box_h};

        if (dst1.w > notif_box_w) dst1.w = notif_box_w;
        if (src1.w > notif_box_w) src1.w = notif_box_w;

        SDL_BlitSurface(notif_content_surf, &src1, frame, &dst1);

        int wrapped = notif_box_w - dst1.w;
        if (wrapped > 0) {
            SDL_Rect src2 = {0, 0, wrapped, notif_box_h};
            SDL_Rect dst2 = {dst1.w, 0, wrapped, notif_box_h};
            SDL_BlitSurface(notif_content_surf, &src2, frame, &dst2);
        }

    } else {
        int offset = (int) notif_scroll_pos;
        SDL_Rect src = {0, offset, notif_box_w, notif_box_h};
        SDL_BlitSurface(notif_content_surf, &src, frame, NULL);
    }

    return frame;
}

/* ---------------------------------------------------------------------------
 * Internal texture/surface cleanup
 * -------------------------------------------------------------------------*/
static void free_content(void) {
    if (notif_content_surf) {
        SDL_FreeSurface(notif_content_surf);
        notif_content_surf = NULL;
    }

    notif_content_w = 0;

    notif_box_w = 0;
    notif_box_h = 0;

    notif_bx = 0;
    notif_by = 0;

    notif_scroll_mode = NOTIF_SCROLL_NONE;
    notif_scroll_pos = 0.0f;
    notif_scroll_start = 0;
    notif_scroll_total = 0;
}

static void notif_free_textures(void) {
    if (notif_sdl_tex) {
        SDL_DestroyTexture(notif_sdl_tex);
        notif_sdl_tex = NULL;
    }

    notif_sdl_w = 0;
    notif_sdl_h = 0;
    notif_sdl_bx = 0;
    notif_sdl_by = 0;

    if (notif_gles_tex) {
        glDeleteTextures(1, &notif_gles_tex);
        notif_gles_tex = 0;
    }

    notif_gles_w = 0;
    notif_gles_h = 0;
}

void notif_update(void) {
    struct stat st;

    if (stat(NOTIF_PATH, &st) != 0) {
        if (notif_visible) {
            notif_visible = 0;
            notif_free_textures();
            free_content();
        }
        return;
    }

    if (notif_visible && st.st_mtime == notif_stat_last.st_mtime) return;

    notif_stat_last = st;
    notif_free_textures();

    free_content();
    notif_visible = 0;

    if (!parse_notif_file()) return;
    notif_visible = 1;
}

int notif_is_visible(void) {
    return notif_visible;
}

void sdl_notif_free(void) {
    if (notif_sdl_tex) {
        SDL_DestroyTexture(notif_sdl_tex);
        notif_sdl_tex = NULL;
    }

    notif_sdl_w = 0;
    notif_sdl_h = 0;
    notif_sdl_bx = 0;
    notif_sdl_by = 0;
}

void sdl_notif_draw(SDL_Renderer *renderer, int fb_w, int fb_h) {
    if (!notif_visible || !renderer) return;

    if (!notif_content_surf) {
        notif_content_surf = build_notif_content(fb_w, fb_h, &notif_bx, &notif_by, &notif_box_w, &notif_box_h,
                                                 &notif_scroll_mode, &notif_scroll_total);

        if (!notif_content_surf) {
            notif_visible = 0;
            return;
        }

        notif_content_w = notif_content_surf->w;

        notif_scroll_start = now_ms();
        notif_scroll_pos = 0.0f;
    }

    const int is_scrolling = (notif_scroll_mode != NOTIF_SCROLL_NONE);

    if (is_scrolling || !notif_sdl_tex) {
        if (notif_sdl_tex) {
            SDL_DestroyTexture(notif_sdl_tex);
            notif_sdl_tex = NULL;
        }

        SDL_Surface *frame = compose_frame();
        if (!frame) return;

        SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, frame);
        SDL_FreeSurface(frame);
        if (!tex) return;

        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_QueryTexture(tex, NULL, NULL, &notif_sdl_w, &notif_sdl_h);

        notif_sdl_tex = tex;
        notif_sdl_bx = notif_bx;
        notif_sdl_by = notif_by;
    }

    if (notif_cfg.dim) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, NOTIF_ALPHA);
        SDL_RenderFillRect(renderer, NULL);
    }

    SDL_Rect dst = {notif_sdl_bx, notif_sdl_by, notif_sdl_w, notif_sdl_h};
    SDL_SetTextureColorMod(notif_sdl_tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(notif_sdl_tex, 255);
    SDL_RenderCopy(renderer, notif_sdl_tex, NULL, &dst);
}

void gl_notif_free(void) {
    if (notif_gles_tex) {
        glDeleteTextures(1, &notif_gles_tex);
        notif_gles_tex = 0;
    }

    notif_gles_w = 0;
    notif_gles_h = 0;
}

static void build_notif_vtx(int fb_w, int fb_h, int bx, int by) {
    const float x0 = (float) bx / (float) fb_w * 2.0f - 1.0f;
    const float x1 = (float) (bx + notif_gles_w) / (float) fb_w * 2.0f - 1.0f;
    const float y0 = -(float) by / (float) fb_h * 2.0f + 1.0f;
    const float y1 = -(float) (by + notif_gles_h) / (float) fb_h * 2.0f + 1.0f;

    vtx_notif[0].x = x0;
    vtx_notif[0].y = y0;
    vtx_notif[0].u = 0.0f;
    vtx_notif[0].v = 0.0f;

    vtx_notif[1].x = x0;
    vtx_notif[1].y = y1;
    vtx_notif[1].u = 0.0f;
    vtx_notif[1].v = 1.0f;

    vtx_notif[2].x = x1;
    vtx_notif[2].y = y0;
    vtx_notif[2].u = 1.0f;
    vtx_notif[2].v = 0.0f;

    vtx_notif[3].x = x1;
    vtx_notif[3].y = y1;
    vtx_notif[3].u = 1.0f;
    vtx_notif[3].v = 1.0f;
}

int gl_notif_prepare(int fb_w, int fb_h) {
    if (!notif_visible) return 0;

    /* Build content surface lazily */
    if (!notif_content_surf) {
        notif_content_surf = build_notif_content(fb_w, fb_h, &notif_bx, &notif_by, &notif_box_w, &notif_box_h,
                                                 &notif_scroll_mode, &notif_scroll_total);

        if (!notif_content_surf) {
            notif_visible = 0;
            return 0;
        }

        notif_content_w = notif_content_surf->w;
        notif_scroll_start = now_ms();
        notif_scroll_pos = 0.0f;
    }

    const int is_scrolling = (notif_scroll_mode != NOTIF_SCROLL_NONE);
    if (is_scrolling && notif_gles_tex) {
        glDeleteTextures(1, &notif_gles_tex);
        notif_gles_tex = 0;
        notif_gles_w = 0;
        notif_gles_h = 0;
    }

    if (!notif_gles_tex) {
        SDL_Surface *frame = compose_frame();
        if (!frame) return 0;

        SDL_Surface *rgba = frame;
        if (frame->format->format != SDL_PIXELFORMAT_RGBA32) {
            rgba = SDL_ConvertSurfaceFormat(frame, SDL_PIXELFORMAT_RGBA32, 0);
            SDL_FreeSurface(frame);
            if (!rgba) return 0;
            frame = NULL;
        }

        notif_gles_w = rgba->w;
        notif_gles_h = rgba->h;

        upload_texture_rgba(rgba, &notif_gles_tex);
        SDL_FreeSurface(rgba);

        if (!notif_gles_tex) {
            notif_gles_w = 0;
            notif_gles_h = 0;
            return 0;
        }

        build_notif_vtx(fb_w, fb_h, notif_bx, notif_by);
    }

    return 1;
}

GLuint gl_notif_get_tex(void) {
    return notif_gles_tex;
}

const gl_vtx_t *gl_notif_get_vtx(void) {
    return vtx_notif;
}

int gl_notif_needs_dim(void) {
    return notif_visible && notif_cfg.dim;
}
