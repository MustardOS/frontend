#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "../../common/log.h"
#include "../common/common.h"
#include "notif.h"

/* ---------------------------------------------------------------------------
 * Stage Overlay Notification file format
 * File location at: /run/muos/overlay.notif
 *
 * Key and value pairs (spaces around '=' are trimmed), then a line containing
 * exactly '-' as the separator.  Everything after the separator is treated
 * as raw notification text: newlines are preserved and blank lines create
 * visual gaps between paragraphs.
 *
 *   position      = 4         0-8 grid (0=top left  4=centre  8=bottom right)
 *   font_size     = 24        8-96 (be reasonable!)
 *   font_align    = 0         0=auto  1=right  2=centre  3=left
 *   font_colour   = FFFFFF    hex RGB, no '#' character!
 *   font_alpha    = 255       0-255
 *   box_colour    = 000000    hex RGB
 *   box_alpha     = 210       0-255
 *   border_colour = 444444    hex RGB
 *   border_alpha  = 180       0-255
 *   dim_colour    = 000000    hex RGB
 *   dim_alpha     = 100       0-255  (0 disables the dim layer entirely)
 *   -
 *   Line one of the notification
 *   Second line
 *
 *   Blank line above creates a visual gap
 *
 * Unknown keys are silently ignored. A '#' at the start of a line is a comment
 * but only in the key/value section.  After '-' every line is literal text.
 *
 * You can source the `notif.sh` script at `/opt/muos/script/var` for
 * various functions that can be used to make this process easier!
 * -------------------------------------------------------------------------*/

#define WW_MAX_LINES 256
#define WW_LINE_LEN  NOTIF_TEXT_LINE_LEN

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

static Uint8 parse_alpha(const char *s) {
    int v = safe_atoi(s);

    if (v < 0) v = 0;
    if (v > 255) v = 255;

    return (Uint8) v;
}

static char *trim(char *s) {
    while (*s && isspace((unsigned char) *s)) s++;

    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char) *(end - 1))) end--;

    *end = '\0';
    return s;
}

static int parse_notif_file(void) {
    FILE *f = fopen(NOTIF_PATH, "r");
    if (!f) return 0;

    notif_cfg.font_size = 24;
    notif_cfg.font_align = 0;
    notif_cfg.font_colour.r = 255;
    notif_cfg.font_colour.g = 255;
    notif_cfg.font_colour.b = 255;
    notif_cfg.font_alpha = 255;

    notif_cfg.box_colour.r = 0;
    notif_cfg.box_colour.g = 0;
    notif_cfg.box_colour.b = 0;
    notif_cfg.box_alpha = 210;

    notif_cfg.border_colour.r = 68;
    notif_cfg.border_colour.g = 68;
    notif_cfg.border_colour.b = 68;
    notif_cfg.border_alpha = 180;

    notif_cfg.dim_colour.r = 0;
    notif_cfg.dim_colour.g = 0;
    notif_cfg.dim_colour.b = 0;
    notif_cfg.dim_alpha = 100;

    notif_cfg.position = 4;
    notif_cfg.text_line_count = 0;

    char line[NOTIF_TEXT_LINE_LEN + 16];
    int in_text = 0;

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') { line[--len] = '\0'; }
        if (len > 0 && line[len - 1] == '\r') { line[--len] = '\0'; }

        if (!in_text) {
            char *trimmed = trim(line);
            if (strcmp(trimmed, "-") == 0) {
                in_text = 1;
                continue;
            }
            if (trimmed[0] == '\0' || trimmed[0] == '#') continue;

            char *eq = strchr(trimmed, '=');
            if (!eq) continue;

            *eq = '\0';
            char *key = trim(trimmed);
            char *val = trim(eq + 1);

            if (strcmp(key, "position") == 0) {
                int v = safe_atoi(val);
                if (v < 0) v = 0;
                if (v > 8) v = 8;
                notif_cfg.position = (notif_pos_t) v;
            } else if (strcmp(key, "font_size") == 0) {
                notif_cfg.font_size = safe_atoi(val);
                if (notif_cfg.font_size < 8) notif_cfg.font_size = 8;
                if (notif_cfg.font_size > 96) notif_cfg.font_size = 96;
            } else if (strcmp(key, "font_align") == 0) {
                int v = safe_atoi(val);
                if (v < 0) v = 0;
                if (v > 3) v = 3;
                notif_cfg.font_align = (notif_align_t) v;
            } else if (strcmp(key, "font_colour") == 0) {
                parse_hex_colour(val, &notif_cfg.font_colour);
            } else if (strcmp(key, "font_alpha") == 0) {
                notif_cfg.font_alpha = parse_alpha(val);
            } else if (strcmp(key, "box_colour") == 0) {
                parse_hex_colour(val, &notif_cfg.box_colour);
            } else if (strcmp(key, "box_alpha") == 0) {
                notif_cfg.box_alpha = parse_alpha(val);
            } else if (strcmp(key, "border_colour") == 0) {
                parse_hex_colour(val, &notif_cfg.border_colour);
            } else if (strcmp(key, "border_alpha") == 0) {
                notif_cfg.border_alpha = parse_alpha(val);
            } else if (strcmp(key, "dim_colour") == 0) {
                parse_hex_colour(val, &notif_cfg.dim_colour);
            } else if (strcmp(key, "dim_alpha") == 0) {
                notif_cfg.dim_alpha = parse_alpha(val);
            }
        } else {
            if (notif_cfg.text_line_count < NOTIF_MAX_TEXT_LINES) {
                strncpy(notif_cfg.text_lines[notif_cfg.text_line_count], line, NOTIF_TEXT_LINE_LEN - 1);
                notif_cfg.text_lines[notif_cfg.text_line_count][NOTIF_TEXT_LINE_LEN - 1] = '\0';
                notif_cfg.text_line_count++;
            }
        }
    }

    while (notif_cfg.text_line_count > 0 && notif_cfg.text_lines[notif_cfg.text_line_count - 1][0] == '\0') {
        notif_cfg.text_line_count--;
    }

    fclose(f);
    return (in_text && notif_cfg.text_line_count > 0);
}

static int word_wrap(TTF_Font *font, const char *text, int max_px, char lines[][WW_LINE_LEN], int max_lines) {
    int line_count = 0;
    char cur[WW_LINE_LEN];
    cur[0] = '\0';

    const char *src = text;

    while (*src && line_count < max_lines) {
        char word[WW_LINE_LEN];
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
                strncpy(lines[line_count++], cur, WW_LINE_LEN - 1);
                cur[0] = '\0';
            }
            continue;
        }

        char trial[WW_LINE_LEN];
        if (cur[0]) snprintf(trial, sizeof(trial), "%s %s", cur, word);
        else snprintf(trial, sizeof(trial), "%s", word);

        int tw = 0, th = 0;
        TTF_SizeUTF8(font, trial, &tw, &th);

        if (tw > max_px && cur[0]) {
            strncpy(lines[line_count++], cur, WW_LINE_LEN - 1);
            strncpy(cur, word, sizeof(cur) - 1);
        } else {
            strncpy(cur, trial, sizeof(cur) - 1);
        }

        if (at_newline) {
            strncpy(lines[line_count++], cur, WW_LINE_LEN - 1);
            cur[0] = '\0';
        }
    }

    if (line_count < max_lines) strncpy(lines[line_count++], cur, WW_LINE_LEN - 1);
    return line_count;
}

static int text_line_x(notif_align_t align, int surf_w, int txt_w, int border, int pad, int total_lines) {
    notif_align_t effective = align;
    if (effective == NOTIF_ALIGN_AUTO) effective = (total_lines == 1) ? NOTIF_ALIGN_LEFT : NOTIF_ALIGN_CENTRE;
    switch (effective) {
        case NOTIF_ALIGN_RIGHT:
            return surf_w - border - pad - txt_w;
        case NOTIF_ALIGN_CENTRE:
            return (surf_w - txt_w) / 2;
        case NOTIF_ALIGN_LEFT:
        default:
            return border + pad;
    }
}

static void calc_box_position(int pos, int fb_w, int fb_h, int box_w, int box_h, int margin, int *out_bx, int *out_by) {
    int bx, by;
    switch (pos % 3) {
        case 0:
            bx = margin;
            break;
        case 1:
            bx = (fb_w - box_w) / 2;
            break;
        default:
            bx = fb_w - box_w - margin;
            break;
    }

    switch (pos / 3) {
        case 0:
            by = margin;
            break;
        case 1:
            by = (fb_h - box_h) / 2;
            break;
        default:
            by = fb_h - box_h - margin;
            break;
    }

    *out_bx = bx < 0 ? 0 : bx;
    *out_by = by < 0 ? 0 : by;
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

    static char ww_lines[WW_MAX_LINES][WW_LINE_LEN];
    int total_lines = 0;

    for (int p = 0; p < notif_cfg.text_line_count && total_lines < WW_MAX_LINES; p++) {
        const char *para = notif_cfg.text_lines[p];
        if (para[0] == '\0') {
            ww_lines[total_lines][0] = '\0';
            total_lines++;
            continue;
        }
        int wrapped = word_wrap(font, para, max_text_w, &ww_lines[total_lines], WW_MAX_LINES - total_lines);
        total_lines += wrapped;
    }

    if (total_lines == 0) {
        TTF_CloseFont(font);
        return NULL;
    }

    int max_line_px = 0;
    for (int i = 0; i < total_lines; i++) {
        if (!ww_lines[i][0]) continue;
        int tw = 0, th = 0;
        TTF_SizeUTF8(font, ww_lines[i], &tw, &th);
        if (tw > max_line_px) max_line_px = tw;
    }

    int content_text_w = max_line_px < max_text_w ? max_line_px : max_text_w;
    int clipped_box_w = content_text_w + 2 * (pad + border);
    if (clipped_box_w > max_box_w) clipped_box_w = max_box_w;

    const int total_content_h = total_lines * line_h + 2 * (pad + border);
    const int screen_margin = 2 * pad;

    const int max_visible_h = fb_h - 2 * screen_margin;
    const int max_vis_lines = (max_visible_h - 2 * (pad + border)) / line_h;

    notif_scroll_mode_t scroll_mode = NOTIF_SCROLL_NONE;
    int scroll_total = 0;

    if (total_lines == 1 && max_line_px > max_text_w) {
        scroll_mode = NOTIF_SCROLL_MARQUEE;
        scroll_total = max_line_px + NOTIF_MARQUEE_GAP;
    } else if (total_lines > max_vis_lines) {
        scroll_mode = NOTIF_SCROLL_VERTICAL;
        scroll_total = total_content_h - (max_vis_lines * line_h + 2 * (pad + border));
    }

    const int visible_lines = (scroll_mode == NOTIF_SCROLL_NONE) ? total_lines : (total_lines < max_vis_lines ? total_lines : max_vis_lines);
    const int box_h = visible_lines * line_h + 2 * (pad + border);

    int bx = 0, by = 0;
    calc_box_position((int) notif_cfg.position, fb_w, fb_h, clipped_box_w, box_h, screen_margin, &bx, &by);

    const int surf_h = (scroll_mode == NOTIF_SCROLL_VERTICAL) ? total_content_h : box_h;
    const int surf_w = (scroll_mode == NOTIF_SCROLL_MARQUEE) ? (max_line_px + NOTIF_MARQUEE_GAP) : clipped_box_w;

    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, surf_w, surf_h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surf) {
        TTF_CloseFont(font);
        return NULL;
    }

    SDL_SetSurfaceBlendMode(surf, SDL_BLENDMODE_BLEND);
    SDL_FillRect(surf, NULL, SDL_MapRGBA(surf->format, 0, 0, 0, 0));

    const SDL_Color *bdc = &notif_cfg.border_colour;
    if (scroll_mode == NOTIF_SCROLL_MARQUEE) {
        SDL_Rect top_strip = {0, 0, surf_w, border};
        SDL_Rect bot_strip = {0, surf_h - border, surf_w, border};
        Uint32 b_col = SDL_MapRGBA(surf->format, bdc->r, bdc->g, bdc->b, notif_cfg.border_alpha);
        SDL_FillRect(surf, &top_strip, b_col);
        SDL_FillRect(surf, &bot_strip, b_col);
    } else {
        SDL_FillRect(surf, NULL, SDL_MapRGBA(surf->format, bdc->r, bdc->g, bdc->b, notif_cfg.border_alpha));
    }

    const SDL_Color *bxc = &notif_cfg.box_colour;
    {
        SDL_Rect inner;
        if (scroll_mode == NOTIF_SCROLL_MARQUEE) {
            inner.x = 0;
            inner.y = border;
            inner.w = surf_w;
            inner.h = surf_h - 2 * border;
        } else {
            inner.x = border;
            inner.y = border;
            inner.w = surf_w - 2 * border;
            inner.h = surf_h - 2 * border;
        }
        SDL_FillRect(surf, &inner, SDL_MapRGBA(surf->format, bxc->r, bxc->g, bxc->b, notif_cfg.box_alpha));
    }

    SDL_Color fg = notif_cfg.font_colour;
    fg.a = notif_cfg.font_alpha;

    for (int i = 0; i < total_lines; i++) {
        if (!ww_lines[i][0]) continue;

        SDL_Surface *txt = TTF_RenderUTF8_Blended(font, ww_lines[i], fg);
        if (!txt) continue;

        int lx, ly;
        if (scroll_mode == NOTIF_SCROLL_MARQUEE) {
            lx = 0;
            ly = border + pad;
        } else {
            lx = text_line_x(notif_cfg.font_align, surf_w, txt->w, border, pad, total_lines);
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
        int tail_w = cw - offset;
        if (tail_w > notif_box_w) tail_w = notif_box_w;

        SDL_Rect src1 = {offset, 0, tail_w, notif_box_h};
        SDL_Rect dst1 = {0, 0, tail_w, notif_box_h};
        SDL_BlitSurface(notif_content_surf, &src1, frame, &dst1);

        int wrapped = notif_box_w - tail_w;
        if (wrapped > 0) {
            SDL_Rect src2 = {0, 0, wrapped, notif_box_h};
            SDL_Rect dst2 = {tail_w, 0, wrapped, notif_box_h};
            SDL_BlitSurface(notif_content_surf, &src2, frame, &dst2);
        }
    } else {
        int offset = (int) notif_scroll_pos;
        SDL_Rect src = {0, offset, notif_box_w, notif_box_h};
        SDL_BlitSurface(notif_content_surf, &src, frame, NULL);
    }

    return frame;
}

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

    if (notif_cfg.dim_alpha > 0) {
        const SDL_Color *dc = &notif_cfg.dim_colour;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, dc->r, dc->g, dc->b, notif_cfg.dim_alpha);
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
    return notif_visible && (notif_cfg.dim_alpha > 0);
}
