#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "render.h"
#include "vt.h"
#include "sixel.h"

static TTF_Font *g_fonts[8] = {NULL};

static int g_cell_w = 8;
static int g_cell_h = 16;

static SDL_Color g_def_fg = {255, 255, 255, 255};
static SDL_Color g_def_bg = {0, 0, 0, 255};

typedef struct GlyphEntry {
    Uint32 codepoint;

    SDL_Color fg;
    Uint8 style;

    SDL_Texture *tex;

    int w, h;
    struct GlyphEntry *next;
} GlyphEntry;

#define GLYPH_BUCKETS     1024u
#define GLYPH_MAX_ENTRIES 8192u

static GlyphEntry *glyph_table[GLYPH_BUCKETS];
static unsigned glyph_entries = 0;

static GlyphEntry *evict_ring[GLYPH_MAX_ENTRIES];
static unsigned evict_head = 0;
static unsigned evict_tail = 0;

static Uint32 glyph_hash(Uint32 cp, SDL_Color fg, Uint8 style) {
    Uint32 h = cp * 2654435761u;

    h ^= ((Uint32) fg.r << 24) ^ ((Uint32) fg.g << 16) ^ ((Uint32) fg.b << 8) ^ (Uint32) fg.a;
    h ^= (Uint32) style * 97531u;
    h ^= h >> 16;
    h *= 2246822519u;
    h ^= h >> 13;

    return h;
}

static inline int colour_equal(SDL_Color a, SDL_Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

void render_glyph_cache_clear(void) {
    for (unsigned i = 0; i < GLYPH_BUCKETS; i++) {
        GlyphEntry *e = glyph_table[i];

        while (e) {
            GlyphEntry *n = e->next;
            if (e->tex) SDL_DestroyTexture(e->tex);
            free(e);
            e = n;
        }

        glyph_table[i] = NULL;
    }

    glyph_entries = 0;

    evict_head = 0;
    evict_tail = 0;
}

static int utf8_encode(char out[8], Uint32 cp) {
    if (cp <= 0x7F) {
        out[0] = (char) cp;
        out[1] = '\0';

        return 1;
    } else if (cp <= 0x7FF) {
        out[0] = (char) (0xC0 | ((cp >> 6) & 0x1F));
        out[1] = (char) (0x80 | (cp & 0x3F));
        out[2] = '\0';

        return 2;
    } else if (cp <= 0xFFFF) {
        out[0] = (char) (0xE0 | ((cp >> 12) & 0x0F));
        out[1] = (char) (0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char) (0x80 | (cp & 0x3F));
        out[3] = '\0';

        return 3;
    } else if (cp <= 0x10FFFF) {
        out[0] = (char) (0xF0 | ((cp >> 18) & 0x07));
        out[1] = (char) (0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char) (0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char) (0x80 | (cp & 0x3F));
        out[4] = '\0';

        return 4;
    }

    out[0] = (char) 0xEF;
    out[1] = (char) 0xBF;
    out[2] = (char) 0xBD;
    out[3] = '\0';

    return 3;
}

static inline int is_vt_box_char(Uint32 cp) {
    return cp >= 0x2500 && cp <= 0x257F;
}

static inline int is_vt_block_char(Uint32 cp) {
    return cp >= 0x2580 && cp <= 0x259F;
}

static inline int is_vt_geom_char(Uint32 cp) {
    return cp == 0x25A0 || cp == 0x25CF;
}

static inline int is_vt_scanline_char(Uint32 cp) {
    return cp >= 0x23BA && cp <= 0x23BD;
}

static inline int is_vt_control_picture(Uint32 cp) {
    return cp == 0x2409 || cp == 0x240A || cp == 0x240B || cp == 0x240C || cp == 0x240D || cp == 0x2424;
}

static inline int is_vt_soft_char(Uint32 cp) {
    return is_vt_box_char(cp) || is_vt_block_char(cp) || is_vt_geom_char(cp) || is_vt_scanline_char(cp)
           || is_vt_control_picture(cp) || (cp >= 0x2800 && cp <= 0x28FF) || cp == 0x00B7;
}

static int vt_line_thickness(int cell_h, int heavy) {
    int t = cell_h / 8;

    if (t < 1) t = 1;
    if (heavy && t < 2) t = 2;

    return t;
}

static void fill_rect_px(SDL_Surface *s, Uint32 col, int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;

    SDL_Rect r = {x, y, w, h};
    SDL_FillRect(s, &r, col);
}

static void draw_hline_mid(SDL_Surface *s, Uint32 col, int ch, int fx, int tx, int t) {
    fill_rect_px(s, col, fx, (ch - t) / 2, tx - fx, t);
}

static void draw_vline_mid(SDL_Surface *s, Uint32 col, int cw, int fy, int ty, int t) {
    fill_rect_px(s, col, (cw - t) / 2, fy, t, ty - fy);
}

static void draw_hline_px(SDL_Surface *s, Uint32 col, int x0, int x1, int y, int t) {
    fill_rect_px(s, col, x0, y, x1 - x0, t);
}

static void draw_vline_px(SDL_Surface *s, Uint32 col, int x, int y0, int y1, int t) {
    fill_rect_px(s, col, x, y0, t, y1 - y0);
}

static void draw_frame_px(SDL_Surface *s, Uint32 col, int x, int y, int w, int h, int t) {
    draw_hline_px(s, col, x, x + w, y, t);
    draw_hline_px(s, col, x, x + w, y + h - t, t);
    draw_vline_px(s, col, x, y, y + h, t);
    draw_vline_px(s, col, x + w - t, y, y + h, t);
}

static void draw_arrow_right_px(SDL_Surface *s, Uint32 col, int x0, int x1, int y, int t, int head) {
    int shaft_end = x1 - head;
    if (shaft_end < x0) shaft_end = x0;

    draw_hline_px(s, col, x0, shaft_end, y, t);

    for (int i = 0; i < head; i++) {
        fill_rect_px(s, col, shaft_end + i, y - i, 1, i * 2 + t);
    }
}

static void draw_arrow_left_px(SDL_Surface *s, Uint32 col, int x0, int x1, int y, int t, int head) {
    int shaft_start = x0 + head;
    if (shaft_start > x1) shaft_start = x1;

    draw_hline_px(s, col, shaft_start, x1, y, t);

    for (int i = 0; i < head; i++) {
        fill_rect_px(s, col, x0 + i, y - (head - 1 - i), 1, (head - 1 - i) * 2 + t);
    }
}

static void draw_arrow_down_px(SDL_Surface *s, Uint32 col, int x, int y0, int y1, int t, int head) {
    int shaft_end = y1 - head;
    if (shaft_end < y0) shaft_end = y0;

    draw_vline_px(s, col, x, y0, shaft_end, t);

    for (int i = 0; i < head; i++) {
        fill_rect_px(s, col, x - i, shaft_end + i, i * 2 + t, 1);
    }
}

static void draw_arrow_up_px(SDL_Surface *s, Uint32 col, int x, int y0, int y1, int t, int head) {
    int shaft_start = y0 + head;
    if (shaft_start > y1) shaft_start = y1;

    draw_vline_px(s, col, x, shaft_start, y1, t);

    for (int i = 0; i < head; i++) {
        fill_rect_px(s, col, x - (head - 1 - i), y0 + i, (head - 1 - i) * 2 + t, 1);
    }
}

static SDL_Surface *render_vt_soft_glyph(Uint32 cp, SDL_Color fg, int cw, int ch) {
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormat(0, cw, ch, 32, SDL_PIXELFORMAT_RGBA8888);

    if (!surf) return NULL;

    SDL_FillRect(surf, NULL, 0);
    Uint32 col = SDL_MapRGBA(surf->format, fg.r, fg.g, fg.b, 255);

    int cx = cw / 2;
    int cy = ch / 2;

    int light = vt_line_thickness(ch, 0);
    int heavy = vt_line_thickness(ch, 1);

    if (is_vt_box_char(cp)) {
        int l = 0, r = 0, u = 0, d = 0, t = light;
        switch (cp) {
            case 0x2500:
                l = 1;
                r = 1;
                break;
            case 0x2501:
                l = 1;
                r = 1;
                t = heavy;
                break;
            case 0x2502:
                u = 1;
                d = 1;
                break;
            case 0x2503:
                u = 1;
                d = 1;
                t = heavy;
                break;
            case 0x250C:
            case 0x250D:
            case 0x250E:
            case 0x250F:
                r = 1;
                d = 1;
                break;
            case 0x2510:
            case 0x2511:
            case 0x2512:
            case 0x2513:
                l = 1;
                d = 1;
                break;
            case 0x2514:
            case 0x2515:
            case 0x2516:
            case 0x2517:
                r = 1;
                u = 1;
                break;
            case 0x2518:
            case 0x2519:
            case 0x251A:
            case 0x251B:
                l = 1;
                u = 1;
                break;
            case 0x251C:
            case 0x251D:
            case 0x251E:
            case 0x251F:
            case 0x2520:
            case 0x2521:
            case 0x2522:
            case 0x2523:
                u = 1;
                d = 1;
                r = 1;
                break;
            case 0x2524:
            case 0x2525:
            case 0x2526:
            case 0x2527:
            case 0x2528:
            case 0x2529:
            case 0x252A:
            case 0x252B:
                u = 1;
                d = 1;
                l = 1;
                break;
            case 0x252C:
            case 0x252D:
            case 0x252E:
            case 0x252F:
            case 0x2530:
            case 0x2531:
            case 0x2532:
            case 0x2533:
                l = 1;
                r = 1;
                d = 1;
                break;
            case 0x2534:
            case 0x2535:
            case 0x2536:
            case 0x2537:
            case 0x2538:
            case 0x2539:
            case 0x253A:
            case 0x253B:
                l = 1;
                r = 1;
                u = 1;
                break;
            case 0x253C:
            case 0x253D:
            case 0x253E:
            case 0x253F:
            case 0x2540:
            case 0x2541:
            case 0x2542:
            case 0x2543:
            case 0x2544:
            case 0x2545:
            case 0x2546:
            case 0x2547:
            case 0x2548:
            case 0x2549:
            case 0x254A:
            case 0x254B:
                l = 1;
                r = 1;
                u = 1;
                d = 1;
                break;
            case 0x254C:
            case 0x254D:
            case 0x254E:
            case 0x254F:
                l = 1;
                r = 1;
                break;
            case 0x2550:
                l = 1;
                r = 1;
                t = heavy;
                break;
            case 0x2551:
                u = 1;
                d = 1;
                t = heavy;
                break;
            case 0x2554:
                r = 1;
                d = 1;
                t = heavy;
                break;
            case 0x2557:
                l = 1;
                d = 1;
                t = heavy;
                break;
            case 0x255A:
                r = 1;
                u = 1;
                t = heavy;
                break;
            case 0x255D:
                l = 1;
                u = 1;
                t = heavy;
                break;
            case 0x2560:
                u = 1;
                d = 1;
                r = 1;
                t = heavy;
                break;
            case 0x2563:
                u = 1;
                d = 1;
                l = 1;
                t = heavy;
                break;
            case 0x2566:
                l = 1;
                r = 1;
                d = 1;
                t = heavy;
                break;
            case 0x2569:
                l = 1;
                r = 1;
                u = 1;
                t = heavy;
                break;
            case 0x256C:
                l = 1;
                r = 1;
                u = 1;
                d = 1;
                t = heavy;
                break;
            default:
                l = 1;
                r = 1;
                u = 1;
                d = 1;
                break;
        }

        if (l) draw_hline_mid(surf, col, ch, 0, cx, t);
        if (r) draw_hline_mid(surf, col, ch, cx, cw, t);

        if (u) draw_vline_mid(surf, col, cw, 0, cy, t);
        if (d) draw_vline_mid(surf, col, cw, cy, ch, t);

        return surf;
    }

    if (is_vt_block_char(cp)) {
        switch (cp) {
            case 0x2581:
                fill_rect_px(surf, col, 0, ch - ch / 8, cw, ch / 8);
                break;
            case 0x2582:
                fill_rect_px(surf, col, 0, ch - ch / 4, cw, ch / 4);
                break;
            case 0x2583:
                fill_rect_px(surf, col, 0, ch - 3 * ch / 8, cw, 3 * ch / 8);
                break;
            case 0x2584:
                fill_rect_px(surf, col, 0, ch / 2, cw, ch - ch / 2);
                break;
            case 0x2585:
                fill_rect_px(surf, col, 0, ch - 5 * ch / 8, cw, 5 * ch / 8);
                break;
            case 0x2586:
                fill_rect_px(surf, col, 0, ch - 3 * ch / 4, cw, 3 * ch / 4);
                break;
            case 0x2587:
                fill_rect_px(surf, col, 0, ch - 7 * ch / 8, cw, 7 * ch / 8);
                break;
            case 0x2588:
                fill_rect_px(surf, col, 0, 0, cw, ch);
                break;
            case 0x2580:
                fill_rect_px(surf, col, 0, 0, cw, ch / 2);
                break;
            case 0x2594:
                fill_rect_px(surf, col, 0, 0, cw, ch / 8);
                break;
            case 0x2595:
                fill_rect_px(surf, col, cw - cw / 8, 0, cw / 8, ch);
                break;
            case 0x258F:
                fill_rect_px(surf, col, 0, 0, cw / 8, ch);
                break;
            case 0x258E:
                fill_rect_px(surf, col, 0, 0, cw / 4, ch);
                break;
            case 0x258D:
                fill_rect_px(surf, col, 0, 0, 3 * cw / 8, ch);
                break;
            case 0x258C:
                fill_rect_px(surf, col, 0, 0, cw / 2, ch);
                break;
            case 0x258B:
                fill_rect_px(surf, col, 0, 0, 5 * cw / 8, ch);
                break;
            case 0x258A:
                fill_rect_px(surf, col, 0, 0, 3 * cw / 4, ch);
                break;
            case 0x2589:
                fill_rect_px(surf, col, 0, 0, 7 * cw / 8, ch);
                break;
            case 0x2590:
                fill_rect_px(surf, col, cw / 2, 0, cw - cw / 2, ch);
                break;
            case 0x2591: {
                Uint32 *px = (Uint32 *) surf->pixels;
                int p4 = surf->pitch / 4;
                for (int y = 0; y < ch; y++) {
                    for (int x = 0; x < cw; x++) {
                        if (((x + y * 2) & 3) == 0) px[y * p4 + x] = col;
                    }
                }
                break;
            }
            case 0x2592: {
                Uint32 *px = (Uint32 *) surf->pixels;
                int p4 = surf->pitch / 4;
                for (int y = 0; y < ch; y++) {
                    for (int x = 0; x < cw; x++) {
                        if (((x + y) & 1) == 0) px[y * p4 + x] = col;
                    }
                }
                break;
            }
            case 0x2593: {
                Uint32 *px = (Uint32 *) surf->pixels;
                int p4 = surf->pitch / 4;
                for (int y = 0; y < ch; y++) {
                    for (int x = 0; x < cw; x++) {
                        if (((x + y) & 1) != 0 || ((x & 1) == 0 && (y & 1) == 0)) px[y * p4 + x] = col;
                    }
                }
                break;
            }
            case 0x2596:
                fill_rect_px(surf, col, 0, ch / 2, cw / 2, ch - ch / 2);
                break;
            case 0x2597:
                fill_rect_px(surf, col, cw / 2, ch / 2, cw - cw / 2, ch - ch / 2);
                break;
            case 0x2598:
                fill_rect_px(surf, col, 0, 0, cw / 2, ch / 2);
                break;
            case 0x259D:
                fill_rect_px(surf, col, cw / 2, 0, cw - cw / 2, ch / 2);
                break;
            case 0x2599:
                fill_rect_px(surf, col, 0, ch / 2, cw / 2, ch - ch / 2);
                fill_rect_px(surf, col, cw / 2, ch / 2, cw - cw / 2, ch - ch / 2);
                fill_rect_px(surf, col, 0, 0, cw / 2, ch / 2);
                break;
            case 0x259A:
                fill_rect_px(surf, col, 0, 0, cw / 2, ch / 2);
                fill_rect_px(surf, col, cw / 2, ch / 2, cw - cw / 2, ch - ch / 2);
                break;
            case 0x259B:
                fill_rect_px(surf, col, 0, 0, cw / 2, ch / 2);
                fill_rect_px(surf, col, cw / 2, 0, cw - cw / 2, ch / 2);
                fill_rect_px(surf, col, 0, ch / 2, cw / 2, ch - ch / 2);
                break;
            case 0x259C:
                fill_rect_px(surf, col, 0, 0, cw / 2, ch / 2);
                fill_rect_px(surf, col, cw / 2, 0, cw - cw / 2, ch / 2);
                fill_rect_px(surf, col, cw / 2, ch / 2, cw - cw / 2, ch - ch / 2);
                break;
            case 0x259E:
                fill_rect_px(surf, col, cw / 2, 0, cw - cw / 2, ch / 2);
                fill_rect_px(surf, col, 0, ch / 2, cw / 2, ch - ch / 2);
                break;
            case 0x259F:
                fill_rect_px(surf, col, cw / 2, 0, cw - cw / 2, ch / 2);
                fill_rect_px(surf, col, 0, ch / 2, cw / 2, ch - ch / 2);
                fill_rect_px(surf, col, cw / 2, ch / 2, cw - cw / 2, ch - ch / 2);
                break;
            default:
                fill_rect_px(surf, col, 0, 0, cw, ch);
                break;
        }
        return surf;
    }

    if (is_vt_geom_char(cp)) {
        if (cp == 0x25A0) {
            fill_rect_px(surf, col, cw / 4, ch / 4, cw / 2, ch / 2);
        } else {
            int radius = (cw < ch ? cw : ch) / 4, ox = cw / 2, oy = ch / 2;
            Uint32 *px = (Uint32 *) surf->pixels;
            int p4 = surf->pitch / 4;
            for (int y = 0; y < ch; y++) {
                for (int x = 0; x < cw; x++) {
                    int dx = x - ox, dy = y - oy;
                    if (dx * dx + dy * dy <= radius * radius) px[y * p4 + x] = col;
                }
            }
        }
        return surf;
    }

    if (is_vt_scanline_char(cp)) {
        int y;
        switch (cp) {
            case 0x23BA:
                y = ch / 6;
                break;
            case 0x23BB:
                y = (2 * ch) / 6;
                break;
            case 0x23BC:
                y = (4 * ch) / 6;
                break;
            case 0x23BD:
                y = (5 * ch) / 6;
                break;
            default:
                y = ch / 2;
                break;
        }

        if (y < 0) y = 0;
        if (y > ch - light) y = ch - light;

        draw_hline_px(surf, col, 0, cw, y, light);
        return surf;
    }

    if (is_vt_control_picture(cp)) {
        int t = light;

        int head = ch / 6;
        if (head < 2) head = 2;

        int left = cw / 6;
        int right = cw - cw / 6;
        if (right <= left) right = left + t + head + 1;

        int top = ch / 6;
        int bot = ch - ch / 6;
        if (bot <= top) bot = top + t + head + 1;

        int mx = (cw - t) / 2;
        int my = (ch - t) / 2;

        switch (cp) {
            case 0x2409:
                draw_arrow_right_px(surf, col, left, right - t - 1, my, t, head);
                draw_vline_px(surf, col, right - t, top, bot, t);
                break;
            case 0x240A:
                draw_arrow_down_px(surf, col, mx, top, bot, t, head);
                break;
            case 0x240B:
                draw_arrow_up_px(surf, col, mx, top, bot, t, head);
                draw_arrow_down_px(surf, col, mx, top, bot, t, head);
                break;
            case 0x240C:
                draw_frame_px(surf, col, left, top, right - left, bot - top, t);
                draw_hline_px(surf, col, left + t, right - t, bot - 2 * t, t);
                break;
            case 0x240D:
                draw_arrow_left_px(surf, col, left, right, my, t, head);
                break;
            case 0x2424:
                draw_vline_px(surf, col, right - t, top, my + t, t);
                draw_arrow_left_px(surf, col, left, right, my, t, head);
                break;
            default:
                break;
        }
        return surf;
    }

    if (cp >= 0x2800 && cp <= 0x28FF) {
        Uint32 bits = cp - 0x2800;

        int dot_w = cw / 4;
        int dot_h = ch / 8;

        if (dot_w < 1) dot_w = 1;
        if (dot_h < 1) dot_h = 1;

        int x0 = cw / 4 - dot_w / 2;
        int x1 = (3 * cw) / 4 - dot_w / 2;

        int y0 = ch / 8 - dot_h / 2;
        int y1 = (3 * ch) / 8 - dot_h / 2;
        int y2 = (5 * ch) / 8 - dot_h / 2;
        int y3 = (7 * ch) / 8 - dot_h / 2;

        if (x0 < 0) x0 = 0;
        if (x1 < 0) x1 = 0;

        if (x0 + dot_w > cw) x0 = cw - dot_w;
        if (x1 + dot_w > cw) x1 = cw - dot_w;

        if (y0 < 0) y0 = 0;
        if (y1 < 0) y1 = 0;
        if (y2 < 0) y2 = 0;
        if (y3 < 0) y3 = 0;

        if (y0 + dot_h > ch) y0 = ch - dot_h;
        if (y1 + dot_h > ch) y1 = ch - dot_h;
        if (y2 + dot_h > ch) y2 = ch - dot_h;
        if (y3 + dot_h > ch) y3 = ch - dot_h;

        if (bits & 0x01) fill_rect_px(surf, col, x0, y0, dot_w, dot_h);
        if (bits & 0x02) fill_rect_px(surf, col, x0, y1, dot_w, dot_h);
        if (bits & 0x04) fill_rect_px(surf, col, x0, y2, dot_w, dot_h);
        if (bits & 0x40) fill_rect_px(surf, col, x0, y3, dot_w, dot_h);

        if (bits & 0x08) fill_rect_px(surf, col, x1, y0, dot_w, dot_h);
        if (bits & 0x10) fill_rect_px(surf, col, x1, y1, dot_w, dot_h);
        if (bits & 0x20) fill_rect_px(surf, col, x1, y2, dot_w, dot_h);
        if (bits & 0x80) fill_rect_px(surf, col, x1, y3, dot_w, dot_h);

        return surf;
    }

    if (cp == 0x00B7) {
        fill_rect_px(surf, col, cw / 2, ch / 2, 1, 1);
        return surf;
    }

    SDL_FreeSurface(surf);
    return NULL;
}

static void glyph_cache_evict_oldest(void) {
    if (glyph_entries == 0) return;

    GlyphEntry *victim = evict_ring[evict_head];
    evict_head = (evict_head + 1) % GLYPH_MAX_ENTRIES;

    if (!victim) return;

    Uint32 h = glyph_hash(victim->codepoint, victim->fg, victim->style);
    unsigned b = (unsigned) (h % GLYPH_BUCKETS);

    GlyphEntry **pp = &glyph_table[b];
    while (*pp && *pp != victim)
        pp = &(*pp)->next;
    if (*pp) *pp = victim->next;

    if (victim->tex) SDL_DestroyTexture(victim->tex);
    free(victim);

    if (glyph_entries > 0) glyph_entries--;
}

static GlyphEntry *glyph_cache_get(SDL_Renderer *ren, Uint32 cp, SDL_Color fg, Uint8 style) {
    Uint8 glyph_style = style & (STYLE_BOLD | STYLE_UNDERLINE | STYLE_ITALIC);
    Uint32 h = glyph_hash(cp, fg, glyph_style);
    unsigned b = (unsigned) (h % GLYPH_BUCKETS);

    for (GlyphEntry *e = glyph_table[b]; e; e = e->next) {
        if (e->codepoint == cp && e->style == glyph_style && colour_equal(e->fg, fg)) return e;
    }

    if (glyph_entries >= GLYPH_MAX_ENTRIES) glyph_cache_evict_oldest();

    GlyphEntry *ne = (GlyphEntry *) calloc(1, sizeof(*ne));
    if (!ne) return NULL;

    ne->codepoint = cp;
    ne->fg = fg;
    ne->style = glyph_style;

    SDL_Surface *surf = NULL;
    if (is_vt_soft_char(cp)) {
        surf = render_vt_soft_glyph(cp, fg, g_cell_w, g_cell_h);
    } else {
        char utf8[8];
        int font_slot = 0;

        if (glyph_style & STYLE_BOLD) font_slot |= 1;
        if (glyph_style & STYLE_UNDERLINE) font_slot |= 2;
        if (glyph_style & STYLE_ITALIC) font_slot |= 4;

        TTF_Font *font = g_fonts[font_slot];
        if (!font) font = g_fonts[0];

        if (!font) {
            free(ne);
            return NULL;
        }

        utf8_encode(utf8, cp);
        surf = TTF_RenderUTF8_Blended(font, utf8, fg);
    }

    if (!surf) {
        free(ne);
        return NULL;
    }

    ne->tex = SDL_CreateTextureFromSurface(ren, surf);
    ne->w = surf->w;
    ne->h = surf->h;

    SDL_FreeSurface(surf);

    if (!ne->tex) {
        free(ne);
        return NULL;
    }

    ne->next = glyph_table[b];

    glyph_table[b] = ne;

    evict_ring[evict_tail] = ne;
    evict_tail = (evict_tail + 1) % GLYPH_MAX_ENTRIES;
    glyph_entries++;

    return ne;
}

void render_init(TTF_Font *fonts[8], int cell_w, int cell_h, SDL_Color def_fg, SDL_Color def_bg) {
    for (int i = 0; i < 8; i++)
        g_fonts[i] = fonts[i];

    g_cell_w = cell_w;
    g_cell_h = cell_h;

    g_def_fg = def_fg;
    g_def_bg = def_bg;

    sixel_set_cell_h(cell_h);
    sixel_set_cell_w(cell_w);
}

static void render_overlay_badge(SDL_Renderer *ren, const char *text, SDL_Color col, int screen_w, int *y) {
    SDL_Surface *s = TTF_RenderUTF8_Blended(g_fonts[0], text, col);
    if (!s) return;

    SDL_Texture *t = SDL_CreateTextureFromSurface(ren, s);
    if (t) {
        SDL_Rect dst = {screen_w - s->w - 8, *y, s->w, s->h};
        SDL_Rect bg = {dst.x - 4, dst.y - 2, dst.w + 8, dst.h + 4};
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
        SDL_RenderFillRect(ren, &bg);
        SDL_RenderCopy(ren, t, NULL, &dst);
        *y += s->h + 4;
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);
}

void render_screen(
    SDL_Renderer *ren, SDL_Texture *target, SDL_Texture *bg_tex, int screen_w, int vis_rows, SDL_Color solid_fg,
    int use_solid_fg, int use_solid_bg, SDL_Color solid_bg, int readonly
) {
    SDL_Texture *previous_target = SDL_GetRenderTarget(ren);
    int cols = vt_cols();
    int rows = vt_rows();
    int scroll_off = vt_scroll_offset();
    int sb_count = vt_scrollback_count();
    int cur_row = vt_cursor_row();
    int cur_col = vt_cursor_col();
    int cur_vis = vt_cursor_visible();
    int screen_h = vis_rows * g_cell_h;

    SDL_SetRenderTarget(ren, target);

    int full_clear = (scroll_off > 0) || (bg_tex != NULL);

    if (full_clear) {
        if (bg_tex) {
            SDL_RenderCopy(ren, bg_tex, NULL, NULL);
        } else if (use_solid_bg) {
            SDL_SetRenderDrawColor(ren, solid_bg.r, solid_bg.g, solid_bg.b, 255);
            SDL_RenderClear(ren);
        } else {
            SDL_SetRenderDrawColor(ren, g_def_bg.r, g_def_bg.g, g_def_bg.b, 255);
            SDL_RenderClear(ren);
        }
    }

#define RENDER_CELL(CELL_, PX_, PY_)                                                                                   \
    do {                                                                                                               \
        if ((CELL_)->width == 0) break;                                                                                \
        SDL_Color _fg = use_solid_fg ? solid_fg : (CELL_)->fg;                                                         \
        SDL_Color _bg = (CELL_)->bg;                                                                                   \
        if ((CELL_)->style & STYLE_REVERSE) {                                                                          \
            SDL_Color _t = _fg;                                                                                        \
            _fg = _bg;                                                                                                 \
            _bg = _t;                                                                                                  \
        }                                                                                                              \
        int _px_w = g_cell_w * ((CELL_)->width > 0 ? (CELL_)->width : 1);                                              \
        if (!colour_equal(_bg, g_def_bg)) {                                                                            \
            SDL_Rect _br = {(PX_), (PY_), _px_w, g_cell_h};                                                            \
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);                                                       \
            SDL_SetRenderDrawColor(ren, _bg.r, _bg.g, _bg.b, 255);                                                     \
            SDL_RenderFillRect(ren, &_br);                                                                             \
        }                                                                                                              \
        if ((CELL_)->codepoint != (Uint32) ' ') {                                                                      \
            GlyphEntry *_g = glyph_cache_get(ren, (CELL_)->codepoint, _fg, (CELL_)->style);                            \
            if (_g) {                                                                                                  \
                SDL_Rect _d = {(PX_), (PY_), _px_w, g_cell_h};                                                         \
                if ((CELL_)->style & STYLE_DIM) SDL_SetTextureAlphaMod(_g->tex, 128);                                  \
                SDL_RenderCopy(ren, _g->tex, NULL, &_d);                                                               \
                if ((CELL_)->style & STYLE_DIM) SDL_SetTextureAlphaMod(_g->tex, 255);                                  \
            }                                                                                                          \
        }                                                                                                              \
        if ((CELL_)->style & STYLE_STRIKE) {                                                                           \
            int _sy = (PY_) + g_cell_h / 2 - 1;                                                                        \
            SDL_Rect _sr = {(PX_), _sy, _px_w, (g_cell_h > 12 ? 2 : 1)};                                               \
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);                                                       \
            SDL_SetRenderDrawColor(ren, _fg.r, _fg.g, _fg.b, 255);                                                     \
            SDL_RenderFillRect(ren, &_sr);                                                                             \
        }                                                                                                              \
    } while (0)

    if (scroll_off > 0) {
        int sb_show = scroll_off;
        int grid_show;
        int sb_start;

        if (sb_show > sb_count) sb_show = sb_count;
        if (sb_show > vis_rows) sb_show = vis_rows;

        grid_show = vis_rows - sb_show;
        sb_start = sb_count - scroll_off;

        if (sb_start < 0) sb_start = 0;
        if (sb_start + sb_show > sb_count) sb_start = sb_count - sb_show;
        if (sb_start < 0) sb_start = 0;

        for (int vr = 0; vr < sb_show; vr++) {
            const Cell *row = vt_scrollback_row(sb_start + vr);
            if (!row) continue;
            for (int c = 0; c < cols; c++) {
                const Cell *cell = &row[c];
                RENDER_CELL(cell, c * g_cell_w, vr * g_cell_h);
                if (cell->width == 2) c++;
            }
        }

        for (int vr = 0; vr < grid_show && vr < rows; vr++) {
            for (int c = 0; c < cols; c++) {
                Cell *cell = vt_cell(vr, c);
                RENDER_CELL(cell, c * g_cell_w, (sb_show + vr) * g_cell_h);
                if (cell->width == 2) c++;
            }
        }
    } else {
        int draw_rows = vis_rows;
        if (draw_rows > rows) draw_rows = rows;

        int all_dirty = 1;
        for (int r = 0; r < draw_rows && all_dirty; r++) {
            if (!vt_row_is_dirty(r)) all_dirty = 0;
        }

        if (all_dirty && !bg_tex) {
            if (use_solid_bg) {
                SDL_SetRenderDrawColor(ren, solid_bg.r, solid_bg.g, solid_bg.b, 255);
            } else {
                SDL_SetRenderDrawColor(ren, g_def_bg.r, g_def_bg.g, g_def_bg.b, 255);
            }
            SDL_RenderClear(ren);
        }

        for (int r = 0; r < draw_rows; r++) {
            if (!vt_row_is_dirty(r)) continue;

            if (!bg_tex && !all_dirty) {
                SDL_Rect row_bg = {0, r * g_cell_h, cols * g_cell_w, g_cell_h};
                SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);

                if (use_solid_bg) {
                    SDL_SetRenderDrawColor(ren, solid_bg.r, solid_bg.g, solid_bg.b, 255);
                } else {
                    SDL_SetRenderDrawColor(ren, g_def_bg.r, g_def_bg.g, g_def_bg.b, 255);
                }

                SDL_RenderFillRect(ren, &row_bg);
            }

            for (int c = 0; c < cols; c++) {
                Cell *cell = vt_cell(r, c);
                RENDER_CELL(cell, c * g_cell_w, r * g_cell_h);
                if (cell->width == 2) c++;
            }

            vt_clear_row_dirty(r);
        }

        if (cur_vis && !readonly && cur_row < draw_rows && (SDL_GetTicks() % 1000) < 500) {
            Cell *cell = vt_cell(cur_row, cur_col);
            int w = (cell->width == 2) ? 2 : 1;
            SDL_Rect cr = {cur_col * g_cell_w, cur_row * g_cell_h, g_cell_w * w, g_cell_h};
            const SDL_Color cursor = use_solid_fg ? solid_fg : g_def_fg;

            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, cursor.r, cursor.g, cursor.b, 160);
            SDL_RenderFillRect(ren, &cr);
        }
    }

#undef RENDER_CELL

    if (sb_count > 0) {
        SDL_Rect track = {screen_w - 3, 0, 2, screen_h};
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 110);
        SDL_RenderFillRect(ren, &track);

        {
            int total = sb_count + vis_rows;
            int thumb_h = (vis_rows * screen_h) / (total > 0 ? total : 1);
            int thumb_y = 0;

            if (thumb_h < 12) thumb_h = 12;
            if (thumb_h > screen_h) thumb_h = screen_h;

            if (scroll_off > 0) {
                int max_off = sb_count;
                int free_h = screen_h - thumb_h;
                thumb_y = free_h - ((scroll_off * free_h) / max_off);
            } else {
                thumb_y = screen_h - thumb_h;
            }

            if (thumb_y < 0) thumb_y = 0;
            if (thumb_y + thumb_h > screen_h) thumb_y = screen_h - thumb_h;

            SDL_Rect thumb = {screen_w - 3, thumb_y, 2, thumb_h};
            SDL_SetRenderDrawColor(ren, 255, 200, 0, 220);
            SDL_RenderFillRect(ren, &thumb);
        }
    }

    {
        int badge_y = 4;

        if (scroll_off > 0) {
            char buf[32];
            SDL_Color ic = {255, 200, 0, 255};

            snprintf(buf, sizeof(buf), "^ %d lines", scroll_off);
            render_overlay_badge(ren, buf, ic, screen_w, &badge_y);
        }

        if (readonly) {
            SDL_Color rc = {255, 100, 60, 255};
            render_overlay_badge(ren, "[RO]", rc, screen_w, &badge_y);
        }
    }

    {
        int sb_show = 0;
        if (scroll_off > 0) {
            sb_show = scroll_off;
            if (sb_show > sb_count) sb_show = sb_count;
            if (sb_show > vis_rows) sb_show = vis_rows;
        }

        int view_bot = vis_rows * g_cell_h;
        int n = sixel_count();

        for (int si = 0; si < n; si++) {
            int sx, sy, sw, sh;
            const Uint32 *px = sixel_get(si, &sx, &sy, &sw, &sh);
            if (!px) continue;

            int dst_y = (sy + sb_show) * g_cell_h;
            int dst_bot = dst_y + sh;

            if (dst_bot <= 0 || dst_y >= view_bot) continue;

            int src_y_off = 0;
            int vis_h = sh;

            if (dst_y < 0) {
                src_y_off = -dst_y;
                vis_h -= src_y_off;
                dst_y = 0;
            }

            if (dst_y + vis_h > view_bot) vis_h = view_bot - dst_y;
            if (vis_h <= 0) continue;

            SDL_Texture *stex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, sw, sh);
            if (!stex) continue;

            SDL_UpdateTexture(stex, NULL, px, sw * (int) sizeof(Uint32));
            SDL_SetTextureBlendMode(stex, SDL_BLENDMODE_BLEND);

            SDL_Rect src = {0, src_y_off, sw, vis_h};
            SDL_Rect dst = {sx * g_cell_w, dst_y, sw, vis_h};
            SDL_RenderCopy(ren, stex, &src, &dst);

            SDL_DestroyTexture(stex);
        }
    }

    SDL_SetRenderTarget(ren, previous_target);
}
