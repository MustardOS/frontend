#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include "muxshare.h"

#define SHARE_DIR "/opt/muos/share/"
#define MEDIA_DIR SHARE_DIR "media/credits"
#define RMSG_FILE SHARE_DIR "message.txt"
#define FONT_FILE SHARE_DIR "font/mucredits.ttf"

#define REF_H 480

#define PAGE_PAD_FRAC 0.06f

#define INTRO_HOLD_S 0.8f
#define OUTRO_HOLD_S 3.0f

#define SCROLL_PX_PER_S 38.0f
#define MUSIC_FADE_IN_S 2.0f

#define QUIT_FADE_S 0.6f
#define BG_FADE_S   2.5f

#define KB_ZOOM 0.18f
#define KB_PAN  0.06f

#define COL_TITLE_R 247
#define COL_TITLE_G 227
#define COL_TITLE_B 24

#define COL_BODY_R 221
#define COL_BODY_G 162
#define COL_BODY_B 0

#define COL_CMD_R 255
#define COL_CMD_G 0
#define COL_CMD_B 0

#define COL_ENF_R 255
#define COL_ENF_G 69
#define COL_ENF_B 0

#define COL_WIZ_R 237
#define COL_WIZ_G 105
#define COL_WIZ_B 0

#define COL_HERO_R 255
#define COL_HERO_G 221
#define COL_HERO_B 34

#define COL_KNIGHT_R 237
#define COL_KNIGHT_G 105
#define COL_KNIGHT_B 0

#define COL_CON_R 135
#define COL_CON_G 201
#define COL_CON_B 124

#define BG_DARKEN_ALPHA  0
#define TXT_SHADOW_ALPHA 160

static const char *commanders[] = {
        "xonglebongle", "antikk", "corey", "bitter_bizarro",
        NULL
};

static const char *enforcers[] = {
        "acmeplus", "bgelmini", "ilfordhp5", "duncanyoyo1",
        "illumini_85", "delibirb77",
        NULL
};

static const char *wizards[] = {
        "xquader", "ee1000", "kloptops", "thegammasqueeze", ".tokyovigilante",
        "joyrider3774", ".cebion", "irilivibi", "vagueparade", "shengy.",
        "siliconexarch", "shauninman", "johnnyonflame", "snowram", "habbening",
        "trngaje", "skorpy", "stanley_00", "ajmandourah", "bcat24",
        "xanxic", "arkun_", "mikhailzrick", "retrogfx_",
        "aeverdyn", "kitfox618", "spycat88", "sundownersport",
        NULL
};

static const char *heroes[] = {
        "romus85", "x_tremis", "lmarcomiranda", "timecarp", "intelliaim",
        "kentonftw", "bazkart", "msx6011", "btreecat", "teggydave",
        "zazouboy", "robbiet480", "luzfcb", "brohsnbluffs",
        "superzu", "pr0j3kt2501", "rosemelody254", "bigbossman0816",
        "warlordwossman", "meowman_", "kaeltis", "paletochen",
        "benjaminbercy", "snesfan1", "suribii", "asiaclonk", "jimmycrackedcorn_4711",
        "rabite890", "hueykablooey", "mrwhistles", "losermatic",
        "ivar2028", ".dririan", "spivvmeister", "sol6_vi", "qpla",
        "amos_06286", "techagent", "meanagar", "roundpi", "chiefwally_73445",
        "scy0n", "luckyphil", "nahck", "mach5682", "foamygames",
        "xraygoggles", "hybrid_sith", "mxdamp", "ownedmumbles", "exe0237",
        "kernelkritic", "verctexius", "misfitsorbet",
        "bigolpeewee", ".zerohalo", "milkworlds", "amildinconvenience.",
        "turner74.", "jmtn070", "izzythefool", "nahck", "cjiiio",
        NULL
};

static const char *knights[] = {
        "notflacko", "jdanteq_18123", "skyarcher", "hai6266", "galloc",
        "_maxwellsdemon", "kiko_lake", "arkholt", "julas8799", "allepac", "drisc",
        "clempurp9868", "aj15", "retrogamecorps", "rbndr_", "sanelessone",
        "billynaing", "nuke_67641", "zauberpony", "smittywerbenjaegermanjensen9250",
        "wizardfights", "_wizdude", "stin87", "surge_84306",
        "mrcee1503", "admiralthrawn_1", "penpen2crayon", "jupyter.", "bburbank",
        "crownlessk", "johnunsc", "crusader4hope3222",
        NULL
};

static const char *contributors[] = {
        "antikk", "artur_ditu", "bitter_bizarro", "corey", "imcokeman",
        "key777", "mugwomp93", "saitamasahil", "thewalruzz", "xonglebongle",
        NULL
};

#define LANG_THANKS "Thank you for choosing MustardOS"
#define LANG_MESSAGE \
    "The people listed throughout have been amazing supporters and " \
    "contributors to MustardOS, helping shape its wild future.\n\n" \
    "From the bottom of my heart, thank you for your time, ideas, " \
    "testing, and support. MustardOS is better because of you."

#define LANG_SPECIAL "Special Thanks"
#define LANG_EVERYONE \
    "A heartfelt thanks to the artificers, druids, porters, theme creators, " \
    "and translators who have helped shape MustardOS. Your dedication, " \
    "creativity, testing, and care have enriched this project in so many " \
    "ways, and I am truly grateful for everything you have contributed."

#define LANG_SHOUTS "Shout Out"
#define LANG_AWESOME \
    "I would like to thank one of our MustardOS community members. Here is " \
    "to Egg for keeping a much needed positive and warm energy within the " \
    "MustardOS community. Thank you for being someone people can talk to, " \
    "for greeting new people, and for always sharing good vibes. Your kindness " \
    "and presence help make this community feel welcoming, friendly, and special."

#define LANG_SUPPORT "My One True Supporter"
#define LANG_BONGLE \
    "To my wonderful wife, Mrs. Bongle, whose endless support, guidance, " \
    "and patience have been my light throughout this entire adventure. " \
    "Thank you for believing in me, standing beside me, and helping make " \
    "this journey possible. You mean everything to me, and I love you more " \
    "than words could ever express!"

#define LANG_DONATE \
    "You can support MustardOS by donating or subscribing, which helps " \
    "keep the project running, from development and new device support " \
    "to the website, forums, and other community services.\n\nMustardOS " \
    "is made and maintained as a free time hobby!"

#define LANG_QRCODE "Scan the QR code below to visit the Ko-fi page!"

#define SONG_TITLE   "Supporter Music"
#define SONG_TRACK   "Andromeda"
#define SONG_ARTIST  "Selfmademusic"
#define SONG_REBOOT  "Please wait while we reboot…"
#define SONG_BLESSED "Have a blessed day…"

typedef enum {
    BLK_TITLE_BIG,
    BLK_TITLE_MED,
    BLK_PARAGRAPH,
    BLK_NAMES,
    BLK_SPACER,
    BLK_QR,
} block_kind_t;

typedef struct {
    block_kind_t kind;
    const char *bg_key;

    SDL_Texture *image;
    int img_w, img_h;

    SDL_Texture **lines;
    int *line_w;
    int *line_h;
    int line_count;

    int spacer_h;

    int height;
    int y_top;
} block_t;

#define MAX_BLOCKS 64
static block_t g_blocks[MAX_BLOCKS];
static int g_block_count = 0;

static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;

static int g_screen_w = 0;
static int g_screen_h = 0;
static float g_scale = 1.0f;

static TTF_Font *g_font_huge = NULL;
static TTF_Font *g_font_big = NULL;
static TTF_Font *g_font_med = NULL;
static TTF_Font *g_font_sml = NULL;

static Mix_Music *g_music = NULL;

static int g_reel_height = 0;
static int g_last_content_y_centre = 0;
static float g_scroll_y = 0.0f;

static block_t *g_quote_block = NULL;
static char *g_quote_raw = NULL;

typedef struct {
    char key[32];
    SDL_Texture *tex;
    int w, h;
} bg_image_t;

#define MAX_BG 16
static bg_image_t g_bgs[MAX_BG];
static int g_bg_count = 0;

static bg_image_t *g_bg_current = NULL;
static bg_image_t *g_bg_previous = NULL;
static float g_bg_fade = 1.0f;
static float g_bg_kb_t = 0.0f;
static float g_bg_kb_dir = 1.0f;
static const char *g_bg_active_key = "";

static bool g_running = true;
static bool g_first_pass_done = false;
static bool g_skip_requested = false;
static bool g_quit_requested = false;
static float g_quit_fade = 0.0f;

static bool g_factory_music_fading = false;

static int sx(int v) {
    return (int) lroundf((float) v * g_scale);
}

static void die(const char *msg) {
    LOG_ERROR(mux_module, "%s: %s", msg, SDL_GetError());
    exit(1);
}

static SDL_Texture *load_image_scaled(const char *path, int target_w, int *out_w, int *out_h) {
    SDL_Surface *surf = IMG_Load(path);
    if (!surf) return NULL;

    SDL_Surface *scaled = surf;
    if (target_w > 0 && surf->w != target_w) {
        float r = (float) target_w / (float) surf->w;
        int new_h = (int) lroundf((float) surf->h * r);
        scaled = SDL_CreateRGBSurfaceWithFormat(0, target_w, new_h, 32, SDL_PIXELFORMAT_RGBA32);
        if (scaled) {
            SDL_BlitScaled(surf, NULL, scaled, NULL);
        } else {
            scaled = surf;
        }
    }

    SDL_Texture *t = SDL_CreateTextureFromSurface(g_renderer, scaled);
    *out_w = scaled->w;
    *out_h = scaled->h;

    if (scaled != surf) SDL_FreeSurface(scaled);
    SDL_FreeSurface(surf);
    return t;
}

static bool random_u32(uint32_t *out) {
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f) return false;

    uint32_t v = 0;
    size_t n = fread(&v, 1, sizeof(v), f);
    int saved_errno = ferror(f) ? errno : 0;

    fclose(f);

    if (n != sizeof(v)) {
        errno = saved_errno;
        return false;
    }

    *out = v;
    return true;
}

static int random_index(int count) {
    if (count <= 1) return 0;

    uint32_t limit = UINT32_MAX - (UINT32_MAX % (uint32_t) count);
    uint32_t v = 0;

    do {
        if (!random_u32(&v)) return 0;
    } while (v >= limit);

    return (int) (v % (uint32_t) count);
}

static char *pick_random_quote_excluding(const char *exclude) {
    FILE *f = fopen(RMSG_FILE, "r");
    if (!f) return NULL;

    char buf[4096];
    int eligible = 0;
    int line_no = 0;

    while (fgets(buf, sizeof(buf), f)) {
        line_no++;
        if (line_no == 1) continue;

        char *p = buf;
        while (*p) ++p;
        while (p > buf && (p[-1] == '\n' || p[-1] == '\r' || p[-1] == ' ' || p[-1] == '\t')) --p;
        *p = '\0';

        char *start = buf;
        while (*start == ' ' || *start == '\t') ++start;

        if (*start == '\0') continue;
        if (exclude && strcmp(start, exclude) == 0) continue;

        ++eligible;
    }

    if (eligible == 0) {
        fclose(f);
        return NULL;
    }

    int pick = random_index(eligible);
    rewind(f);

    line_no = 0;
    int seen = 0;
    char *result = NULL;

    while (fgets(buf, sizeof(buf), f)) {
        line_no++;
        if (line_no == 1) continue;

        char *p = buf;
        while (*p) ++p;
        while (p > buf && (p[-1] == '\n' || p[-1] == '\r' || p[-1] == ' ' || p[-1] == '\t')) --p;
        *p = '\0';

        char *start = buf;
        while (*start == ' ' || *start == '\t') ++start;
        if (*start == '\0') continue;
        if (exclude && strcmp(start, exclude) == 0) continue;

        if (seen == pick) {
            result = strdup(start);
            break;
        }

        ++seen;
    }

    fclose(f);
    return result;
}

static char *pick_random_quote(void) {
    return pick_random_quote_excluding(NULL);
}

static bg_image_t *bg_lookup_or_load(const char *key) {
    if (!key || !*key) return NULL;

    for (int i = 0; i < g_bg_count; ++i) {
        if (strcmp(g_bgs[i].key, key) == 0) return g_bgs[i].tex ? &g_bgs[i] : NULL;
    }

    if (g_bg_count >= MAX_BG) return NULL;

    bg_image_t *slot = &g_bgs[g_bg_count++];
    snprintf(slot->key, sizeof(slot->key), "%s", key);
    slot->tex = NULL;

    static const char *exts[] = {"png", "jpg", "jpeg", NULL};
    for (int i = 0; exts[i]; ++i) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s.%s", MEDIA_DIR, key, exts[i]);
        SDL_Surface *surf = IMG_Load(path);
        if (surf) {
            slot->w = surf->w;
            slot->h = surf->h;
            slot->tex = SDL_CreateTextureFromSurface(g_renderer, surf);
            if (slot->tex) {
                SDL_SetTextureScaleMode(slot->tex, SDL_ScaleModeLinear);
                SDL_SetTextureBlendMode(slot->tex, SDL_BLENDMODE_BLEND);
            }
            SDL_FreeSurface(surf);
            break;
        }
    }
    return slot->tex ? slot : NULL;
}

static void prewarm_backgrounds(void) {
    for (int i = 0; i < g_block_count; ++i) {
        const char *k = g_blocks[i].bg_key;
        if (k && *k) bg_lookup_or_load(k);
    }
}

static float g_bg_prev_kb_frozen = 0.0f;
static float g_bg_prev_dir = 1.0f;

static void bg_set_active(const char *key) {
    if (key == g_bg_active_key) return;
    if (g_bg_active_key && key && strcmp(g_bg_active_key, key) == 0) return;

    g_bg_prev_kb_frozen = g_bg_kb_t;
    g_bg_prev_dir = g_bg_kb_dir;
    g_bg_previous = g_bg_current;

    g_bg_current = bg_lookup_or_load(key);
    g_bg_active_key = key ? key : "";
    g_bg_fade = 0.0f;

    g_bg_kb_t = 0.0f;
    g_bg_kb_dir = -g_bg_kb_dir;

    if (g_bg_kb_dir == 0.0f) g_bg_kb_dir = 1.0f;
}

static float smoothstep01(float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;

    return t * t * (3.0f - 2.0f * t);
}

static void bg_render_one(bg_image_t *bg, float kb_t, float kb_dir, Uint8 alpha) {
    if (!bg->tex) return;

    float img_w = (float) bg->w;
    float img_h = (float) bg->h;

    float screen_aspect = (float) g_screen_w / (float) g_screen_h;
    float img_aspect = img_w / img_h;

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

    if (src.w > bg->w) src.w = bg->w;
    if (src.h > bg->h) src.h = bg->h;

    src.x = (bg->w - src.w) / 2;
    src.y = (bg->h - src.h) / 2;

    float t = smoothstep01(kb_t);

    float zoom = (kb_dir > 0) ? (1.0f + KB_ZOOM * t) : (1.0f + KB_ZOOM * (1.0f - t));

    float dw = (float) g_screen_w * zoom;
    float dh = (float) g_screen_h * zoom;

    float slack_x = dw - (float) g_screen_w;
    float slack_y = dh - (float) g_screen_h;

    float pan_x = (t - 0.5f) * 2.0f * KB_PAN * slack_x * 0.5f;
    float pan_y = (t - 0.5f) * 2.0f * KB_PAN * slack_y * 0.5f * 0.6f;

    SDL_FRect dst;

    dst.x = -slack_x * 0.5f + pan_x;
    dst.y = -slack_y * 0.5f + pan_y;

    dst.w = dw;
    dst.h = dh;

    SDL_SetTextureAlphaMod(bg->tex, alpha);
    SDL_RenderCopyF(g_renderer, bg->tex, &src, &dst);
}

static void render_backgrounds(float dt) {
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 255);
    SDL_RenderClear(g_renderer);

    if (g_bg_fade < 1.0f) {
        g_bg_fade += dt / BG_FADE_S;
        if (g_bg_fade > 1.0f) g_bg_fade = 1.0f;
    }

    g_bg_kb_t += dt / 45.0f;
    if (g_bg_kb_t > 1.0f) g_bg_kb_t = 1.0f;

    if (g_bg_previous && g_bg_fade < 1.0f) {
        float eased_out = smoothstep01(g_bg_fade);
        bg_render_one(g_bg_previous, g_bg_prev_kb_frozen, g_bg_prev_dir, (Uint8) ((1.0f - eased_out) * 255));
    }
    if (g_bg_current) {
        float eased_in = smoothstep01(g_bg_fade);
        bg_render_one(g_bg_current, g_bg_kb_t, g_bg_kb_dir, (Uint8) (eased_in * 255));
    }

    if (BG_DARKEN_ALPHA > 0) {
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, BG_DARKEN_ALPHA);
        SDL_Rect full = {0, 0, g_screen_w, g_screen_h};
        SDL_RenderFillRect(g_renderer, &full);
    }
}

static int g_text_shadow_offset = 1;

static void recompute_text_shadow_offset(void) {
    int v = sx(1);
    g_text_shadow_offset = v < 1 ? 1 : v;
}

static int text_shadow_offset(void) {
    return g_text_shadow_offset;
}

static SDL_Surface *render_text_surface(TTF_Font *font, const char *text, SDL_Color col) {
    if (!text || !*text) return NULL;

    int offset = text_shadow_offset();

    SDL_Color shadow_col = {0, 0, 0, 255};

    SDL_Surface *shadow = TTF_RenderUTF8_Blended(font, text, shadow_col);
    if (!shadow) return NULL;

    SDL_Surface *front = TTF_RenderUTF8_Blended(font, text, col);
    if (!front) {
        SDL_FreeSurface(shadow);
        return NULL;
    }

    SDL_Surface *out = SDL_CreateRGBSurfaceWithFormat(0, front->w + offset, front->h + offset, 32, SDL_PIXELFORMAT_RGBA32);
    if (!out) {
        SDL_FreeSurface(front);
        SDL_FreeSurface(shadow);
        return NULL;
    }

    SDL_FillRect(out, NULL, SDL_MapRGBA(out->format, 0, 0, 0, 0));

    SDL_Rect dst;
    dst.x = offset;
    dst.y = offset;
    dst.w = shadow->w;
    dst.h = shadow->h;

    SDL_SetSurfaceBlendMode(shadow, SDL_BLENDMODE_BLEND);
    SDL_SetSurfaceAlphaMod(shadow, TXT_SHADOW_ALPHA);
    SDL_BlitSurface(shadow, NULL, out, &dst);

    dst.x = 0;
    dst.y = 0;
    dst.w = front->w;
    dst.h = front->h;

    SDL_SetSurfaceBlendMode(front, SDL_BLENDMODE_BLEND);
    SDL_BlitSurface(front, NULL, out, &dst);

    SDL_FreeSurface(front);
    SDL_FreeSurface(shadow);

    return out;
}

static SDL_Surface *render_text_wrapped_surface(TTF_Font *font, const char *text, SDL_Color col, int wrap_w) {
    if (!*text) return NULL;

    int offset = text_shadow_offset();

    SDL_Color shadow_col = {0, 0, 0, 255};

    TTF_SetFontWrappedAlign(font, TTF_WRAPPED_ALIGN_CENTER);

    SDL_Surface *shadow = TTF_RenderUTF8_Blended_Wrapped(font, text, shadow_col, wrap_w);
    if (!shadow) return NULL;

    SDL_Surface *front = TTF_RenderUTF8_Blended_Wrapped(font, text, col, wrap_w);
    if (!front) {
        SDL_FreeSurface(shadow);
        return NULL;
    }

    SDL_Surface *out = SDL_CreateRGBSurfaceWithFormat(0, front->w + offset, front->h + offset, 32, SDL_PIXELFORMAT_RGBA32);
    if (!out) {
        SDL_FreeSurface(front);
        SDL_FreeSurface(shadow);
        return NULL;
    }

    SDL_FillRect(out, NULL, SDL_MapRGBA(out->format, 0, 0, 0, 0));

    SDL_Rect dst;
    dst.x = offset;
    dst.y = offset;
    dst.w = shadow->w;
    dst.h = shadow->h;

    SDL_SetSurfaceBlendMode(shadow, SDL_BLENDMODE_BLEND);
    SDL_SetSurfaceAlphaMod(shadow, TXT_SHADOW_ALPHA);
    SDL_BlitSurface(shadow, NULL, out, &dst);

    dst.x = 0;
    dst.y = 0;
    dst.w = front->w;
    dst.h = front->h;

    SDL_SetSurfaceBlendMode(front, SDL_BLENDMODE_BLEND);
    SDL_BlitSurface(front, NULL, out, &dst);

    SDL_FreeSurface(front);
    SDL_FreeSurface(shadow);

    return out;
}

static SDL_Texture *render_text(TTF_Font *font, const char *text, SDL_Color col, int *out_w, int *out_h) {
    SDL_Surface *surf = render_text_surface(font, text, col);
    if (!surf) return NULL;

    SDL_Texture *t = SDL_CreateTextureFromSurface(g_renderer, surf);
    *out_w = surf->w;
    *out_h = surf->h;

    SDL_FreeSurface(surf);

    return t;
}

static SDL_Texture *render_text_wrapped(TTF_Font *font, const char *text, SDL_Color col, int wrap_w, int *out_w, int *out_h) {
    SDL_Surface *surf = render_text_wrapped_surface(font, text, col, wrap_w);
    if (!surf) return NULL;

    SDL_Texture *t = SDL_CreateTextureFromSurface(g_renderer, surf);
    *out_w = surf->w;
    *out_h = surf->h;

    SDL_FreeSurface(surf);

    return t;
}

static block_t *new_block(block_kind_t kind, const char *bg_key) {
    if (g_block_count >= MAX_BLOCKS) {
        fprintf(stderr, "%s: too many blocks\n", mux_module);
        exit(1);
    }

    block_t *b = &g_blocks[g_block_count++];
    memset(b, 0, sizeof(*b));

    b->kind = kind;
    b->bg_key = bg_key;

    return b;
}

static bool block_install_single(block_t *b, SDL_Texture *tex, int w, int h) {
    SDL_Texture **arr = malloc(sizeof(SDL_Texture *));
    int *aw = malloc(sizeof(int));
    int *ah = malloc(sizeof(int));

    if (!arr || !aw || !ah) {
        free(arr);
        free(aw);
        free(ah);
        return false;
    }

    arr[0] = tex;
    aw[0] = w;
    ah[0] = h;

    b->lines = arr;
    b->line_w = aw;
    b->line_h = ah;
    b->line_count = 1;
    b->height = h;

    return true;
}

static void block_free_lines(block_t *b) {
    for (int i = 0; i < b->line_count; ++i) {
        if (b->lines[i]) SDL_DestroyTexture(b->lines[i]);
    }

    free(b->lines);
    free(b->line_w);
    free(b->line_h);

    b->lines = NULL;
    b->line_w = NULL;
    b->line_h = NULL;
    b->line_count = 0;
    b->height = 0;
}

static char *format_quote_text(const char *quote) {
    if (!quote || !*quote) return NULL;

    size_t len = strlen(quote) + 16;
    char *text = malloc(len);
    if (!text) return NULL;

    snprintf(text, len, "\u201C%s\u201D", quote);
    return text;
}

static void add_spacer(int px, const char *bg_key) {
    block_t *b = new_block(BLK_SPACER, bg_key);
    b->spacer_h = px;
    b->height = px;
}

static void add_kofi_qr() {
    char path[512];
    snprintf(path, sizeof(path), "%s/kofi_qr.png", MEDIA_DIR);

    int target_w = (int) ((float) g_screen_h * 0.45f);
    int w = 0, h = 0;
    SDL_Texture *t = load_image_scaled(path, target_w, &w, &h);
    if (!t) return;

    block_t *b = new_block(BLK_QR, "kofi");
    b->image = t;
    b->img_w = w;
    b->img_h = h;
    b->height = h;
}

static void add_logo() {
    char path[512];
    snprintf(path, sizeof(path), "%s/logo.png", MEDIA_DIR);

    int natural_w = 0, natural_h = 0;
    SDL_Texture *natural = load_image_scaled(path, 0, &natural_w, &natural_h);
    if (!natural) return;

    int target_h = (int) ((float) g_screen_h * 0.32f);
    float aspect = (natural_h > 0) ? (float) natural_w / (float) natural_h : 1.0f;
    int target_w = (int) lroundf((float) target_h * aspect);

    int page_pad = (int) ((float) g_screen_w * PAGE_PAD_FRAC);
    int max_w = g_screen_w - 2 * page_pad;
    if (target_w > max_w) {
        target_w = max_w;
        target_h = (int) lroundf((float) target_w / aspect);
    }

    block_t *b = new_block(BLK_QR, "intro");
    b->image = natural;
    b->img_w = target_w;
    b->img_h = target_h;
    b->height = target_h;
}

static void add_title(TTF_Font *font, const char *text, SDL_Color col, const char *bg_key) {
    int w = 0, h = 0;
    SDL_Texture *t = render_text(font, text, col, &w, &h);
    if (!t) return;

    int page_pad = (int) ((float) g_screen_w * PAGE_PAD_FRAC);
    int max_w = g_screen_w - 2 * page_pad;
    if (w > max_w) {
        SDL_DestroyTexture(t);
        t = render_text_wrapped(font, text, col, max_w, &w, &h);
        if (!t) return;
    }

    block_t *b = new_block(font == g_font_huge ? BLK_TITLE_BIG : BLK_TITLE_MED, bg_key);
    if (!block_install_single(b, t, w, h)) {
        SDL_DestroyTexture(t);
        --g_block_count;
    }
}

static block_t *add_paragraph(const char *text, SDL_Color col, const char *bg_key) {
    int page_pad = (int) ((float) g_screen_w * PAGE_PAD_FRAC);
    int wrap = g_screen_w - 2 * page_pad;
    int w = 0, h = 0;
    SDL_Texture *t = render_text_wrapped(g_font_med, text, col, wrap, &w, &h);
    if (!t) return NULL;

    block_t *b = new_block(BLK_PARAGRAPH, bg_key);
    if (!block_install_single(b, t, w, h)) {
        SDL_DestroyTexture(t);
        --g_block_count;
        return NULL;
    }

    return b;
}

static void add_names(const char **names, SDL_Color col, const char *bg_key, int force_cols) {
    int n = 0;
    while (names[n]) ++n;
    if (n == 0) return;

    int *widths = NULL;
    int *regular = NULL;
    int *oversized = NULL;
    SDL_Surface *full = NULL;

    widths = malloc(sizeof(*widths) * (size_t) n);
    if (!widths) goto cleanup;

    int max_name_w = 0;
    for (int i = 0; i < n; ++i) {
        int tw = 0, th = 0;
        TTF_SizeUTF8(g_font_sml, names[i], &tw, &th);
        widths[i] = tw;
        if (tw > max_name_w) max_name_w = tw;
    }

    int page_pad = (int) ((float) g_screen_w * PAGE_PAD_FRAC);
    int avail = g_screen_w - 2 * page_pad;
    if (avail < 1) goto cleanup;

    int cell_inset = sx(8);
    int line_h = TTF_FontHeight(g_font_sml) + sx(4) + text_shadow_offset();
    if (line_h < 1) line_h = 1;

    int cols;
    if (force_cols > 0) {
        cols = force_cols;
    } else {
        cols = avail / (max_name_w + cell_inset);
        if (cols < 1) cols = 1;
        if (cols > 4) cols = 4;
    }

    if (cols < 1) cols = 1;

    int cell_w = avail / cols;
    int cell_budget = cell_w - cell_inset;
    if (cell_budget < 1) cell_budget = 1;

    regular = malloc(sizeof(*regular) * (size_t) n);
    oversized = malloc(sizeof(*oversized) * (size_t) n);
    if (!regular || !oversized) goto cleanup;

    int reg_n = 0, over_n = 0;
    for (int i = 0; i < n; ++i) {
        if (widths[i] > cell_budget) oversized[over_n++] = i;
        else regular[reg_n++] = i;
    }

    if (reg_n < cols) cols = reg_n > 0 ? reg_n : 1;
    if (cols < 1) cols = 1;

    int cell_w2 = avail / cols;
    int reg_rows = (reg_n + cols - 1) / cols;
    int total_rows = reg_rows + over_n;

    if (total_rows == 0) goto cleanup;

    int total_h = total_rows * line_h;

    full = SDL_CreateRGBSurfaceWithFormat(0, avail, total_h, 32, SDL_PIXELFORMAT_RGBA32);
    if (!full) goto cleanup;

    SDL_FillRect(full, NULL, SDL_MapRGBA(full->format, 0, 0, 0, 0));

    int cursor_y = 0;

    for (int r = 0; r < reg_rows; ++r) {
        int cells_in_row = cols;
        if (r == reg_rows - 1) {
            int leftover = reg_n - r * cols;
            if (leftover > 0 && leftover < cols) cells_in_row = leftover;
        }

        int row_x_offset = (avail - cells_in_row * cell_w2) / 2;

        for (int c = 0; c < cells_in_row; ++c) {
            int reg_idx = r * cols + c;
            if (reg_idx >= reg_n) break;

            int idx = regular[reg_idx];
            SDL_Surface *ns = render_text_surface(g_font_sml, names[idx], col);
            if (!ns) continue;

            SDL_Rect dst;
            dst.x = row_x_offset + c * cell_w2 + (cell_w2 - ns->w) / 2;
            dst.y = cursor_y + (line_h - ns->h) / 2;
            dst.w = ns->w;
            dst.h = ns->h;

            SDL_SetSurfaceBlendMode(ns, SDL_BLENDMODE_BLEND);
            SDL_BlitSurface(ns, NULL, full, &dst);
            SDL_FreeSurface(ns);
        }

        cursor_y += line_h;
    }

    for (int o = 0; o < over_n; ++o) {
        int idx = oversized[o];

        SDL_Surface *ns = render_text_surface(g_font_sml, names[idx], col);
        if (ns) {
            SDL_Rect dst;

            if (ns->w <= avail) {
                dst.x = (avail - ns->w) / 2;
                dst.y = cursor_y + (line_h - ns->h) / 2;
                dst.w = ns->w;
                dst.h = ns->h;

                SDL_SetSurfaceBlendMode(ns, SDL_BLENDMODE_BLEND);
                SDL_BlitSurface(ns, NULL, full, &dst);
            } else {
                float k = (float) avail / (float) ns->w;
                int tw = avail;
                int th = (int) lroundf((float) ns->h * k);

                dst.x = 0;
                dst.y = cursor_y + (line_h - th) / 2;
                dst.w = tw;
                dst.h = th;

                SDL_SetSurfaceBlendMode(ns, SDL_BLENDMODE_BLEND);
                SDL_BlitScaled(ns, NULL, full, &dst);
            }

            SDL_FreeSurface(ns);
        }

        cursor_y += line_h;
    }

    SDL_Texture *tex = SDL_CreateTextureFromSurface(g_renderer, full);
    if (!tex) goto cleanup;

    block_t *b = new_block(BLK_NAMES, bg_key);
    if (!block_install_single(b, tex, avail, total_h)) {
        SDL_DestroyTexture(tex);
        --g_block_count;
        goto cleanup;
    }

    cleanup:
    if (full) SDL_FreeSurface(full);
    free(widths);
    free(regular);
    free(oversized);
}

static void relayout_reel(void) {
    int y = 0;

    for (int i = 0; i < g_block_count; ++i) {
        g_blocks[i].y_top = y;
        y += g_blocks[i].height;
    }

    g_reel_height = y;

    g_last_content_y_centre = g_reel_height / 2;
    for (int i = g_block_count - 1; i >= 0; --i) {
        block_t *b = &g_blocks[i];
        if (b->kind != BLK_SPACER) {
            g_last_content_y_centre = b->y_top + b->height / 2;
            break;
        }
    }
}

static bool replace_quote_block(block_t *b, const char *quote) {
    if (!quote || !*quote) return false;

    char *text = format_quote_text(quote);
    if (!text) return false;

    SDL_Color C_TITLE = {COL_TITLE_R, COL_TITLE_G, COL_TITLE_B, 255};

    int page_pad = (int) ((float) g_screen_w * PAGE_PAD_FRAC);
    int wrap = g_screen_w - 2 * page_pad;
    int w = 0, h = 0;

    SDL_Texture *t = render_text_wrapped(g_font_med, text, C_TITLE, wrap, &w, &h);
    free(text);

    if (!t) return false;

    block_free_lines(b);

    if (!block_install_single(b, t, w, h)) {
        SDL_DestroyTexture(t);
        return false;
    }

    return true;
}

static void refresh_quote_block(void) {
    if (!g_quote_block) return;

    char *quote = pick_random_quote_excluding(g_quote_raw);
    if (!quote) return;

    if (replace_quote_block(g_quote_block, quote)) {
        free(g_quote_raw);
        g_quote_raw = quote;
        relayout_reel();
        return;
    }

    free(quote);
}

static void build_reel(void) {
    SDL_Color C_TITLE = {COL_TITLE_R, COL_TITLE_G, COL_TITLE_B, 255};
    SDL_Color C_BODY = {COL_BODY_R, COL_BODY_G, COL_BODY_B, 255};
    SDL_Color C_CMD = {COL_CMD_R, COL_CMD_G, COL_CMD_B, 255};
    SDL_Color C_ENF = {COL_ENF_R, COL_ENF_G, COL_ENF_B, 255};
    SDL_Color C_WIZ = {COL_WIZ_R, COL_WIZ_G, COL_WIZ_B, 255};
    SDL_Color C_HERO = {COL_HERO_R, COL_HERO_G, COL_HERO_B, 255};
    SDL_Color C_KNT = {COL_KNIGHT_R, COL_KNIGHT_G, COL_KNIGHT_B, 255};
    SDL_Color C_CON = {COL_CON_R, COL_CON_G, COL_CON_B, 255};

    int big_gap = sx(80);
    int sec_gap = sx(48);
    int sml_gap = sx(20);

    int wrap_head = sx(60);
    int wrap_tail = sx(60);

    add_spacer(wrap_head, "intro");

    add_logo();
    add_spacer(sec_gap, "intro");
    add_title(g_font_huge, LANG_THANKS, C_TITLE, "intro");
    add_spacer(sml_gap, "intro");
    add_paragraph(LANG_MESSAGE, C_BODY, "intro");
    add_spacer(big_gap, "intro");

    add_title(g_font_big, "Commanders", C_CMD, "commanders");
    add_spacer(sml_gap, "commanders");
    add_names(commanders, C_CMD, "commanders", 0);
    add_spacer(sec_gap, "commanders");

    add_title(g_font_big, "Enforcers", C_ENF, "commanders");
    add_spacer(sml_gap, "commanders");
    add_names(enforcers, C_ENF, "commanders", 0);
    add_spacer(big_gap, "commanders");

    add_title(g_font_big, "Wizards", C_WIZ, "wizards");
    add_spacer(sml_gap, "wizards");
    add_names(wizards, C_WIZ, "wizards", 0);
    add_spacer(big_gap, "wizards");

    add_title(g_font_big, "Heroes", C_HERO, "heroes");
    add_spacer(sml_gap, "heroes");
    add_names(heroes, C_HERO, "heroes", 2);
    add_spacer(big_gap, "heroes");

    add_title(g_font_big, "Knights", C_KNT, "knights");
    add_spacer(sml_gap, "knights");
    add_names(knights, C_KNT, "knights", 2);
    add_spacer(big_gap, "knights");

    char ctitle[128];
    char ver[64] = "MustardOS";
    snprintf(ver, sizeof(ver), "%s", config.SYSTEM.VERSION);

    for (char *p = ver; *p; ++p) if (*p == '_') *p = ' ';
    snprintf(ctitle, sizeof(ctitle), "%s Contributors", ver);

    add_title(g_font_big, ctitle, C_CON, "contributors");
    add_spacer(sml_gap, "contributors");
    add_names(contributors, C_CON, "contributors", 0);
    add_spacer(big_gap, "contributors");

    add_title(g_font_big, LANG_SPECIAL, C_CON, "special");
    add_spacer(sml_gap, "special");
    add_paragraph(LANG_EVERYONE, C_CON, "special");
    add_spacer(sec_gap, "special");

    add_title(g_font_big, LANG_SHOUTS, C_CON, "special");
    add_spacer(sml_gap, "special");
    add_paragraph(LANG_AWESOME, C_CON, "special");
    add_spacer(sec_gap, "special");

    add_title(g_font_big, LANG_SUPPORT, C_CON, "special");
    add_spacer(sml_gap, "special");
    add_paragraph(LANG_BONGLE, C_CON, "special");
    add_spacer(big_gap, "special");

    add_title(g_font_huge, LANG_THANKS, C_TITLE, "kofi");
    add_spacer(sml_gap, "kofi");
    add_paragraph(LANG_DONATE, C_BODY, "kofi");
    add_spacer(sec_gap, "kofi");
    add_paragraph(LANG_QRCODE, C_BODY, "kofi");
    add_spacer(sml_gap, "kofi");
    add_kofi_qr();
    add_spacer(big_gap, "kofi");

    add_title(g_font_big, SONG_TITLE, C_TITLE, "music");
    add_spacer(sml_gap, "music");

    char buf[256];
    snprintf(buf, sizeof(buf), "Track \u2014 %s", SONG_TRACK);
    add_paragraph(buf, C_BODY, "music");

    snprintf(buf, sizeof(buf), "Artist \u2014 %s", SONG_ARTIST);
    add_paragraph(buf, C_BODY, "music");

    add_spacer(sec_gap, "music");

    const char *signoff = config.BOOT.FACTORY_RESET ? SONG_REBOOT : SONG_BLESSED;
    add_title(g_font_big, signoff, C_TITLE, "music");

    char *quote = pick_random_quote();
    if (quote) {
        add_spacer(big_gap, "music");

        char *qbuf = format_quote_text(quote);
        if (qbuf) {
            g_quote_block = add_paragraph(qbuf, C_TITLE, "music");
            free(qbuf);
        }

        if (g_quote_block) {
            free(g_quote_raw);
            g_quote_raw = quote;
        } else {
            free(quote);
        }
    }

    add_spacer(wrap_tail, "music");
    relayout_reel();
}

static void render_block(block_t *b, float draw_y) {
    switch (b->kind) {
        case BLK_QR: {
            SDL_FRect dst;
            dst.x = (float) ((g_screen_w - b->img_w) / 2);
            dst.y = draw_y;
            dst.w = (float) b->img_w;
            dst.h = (float) b->img_h;
            SDL_RenderCopyF(g_renderer, b->image, NULL, &dst);
            break;
        }
        case BLK_SPACER:
            break;
        default: {
            float y = draw_y;
            for (int i = 0; i < b->line_count; ++i) {
                SDL_FRect dst;
                dst.w = (float) b->line_w[i];
                dst.h = (float) b->line_h[i];
                dst.x = (float) ((g_screen_w - b->line_w[i]) / 2);
                dst.y = y;
                SDL_RenderCopyF(g_renderer, b->lines[i], NULL, &dst);
                y += (float) b->line_h[i];
            }
            break;
        }
    }
}

static void render_reel_pass(float top, float bottom, int reel_offset, const char **want_bg) {
    float centre = top + (float) g_screen_h * 0.5f;
    for (int i = 0; i < g_block_count; ++i) {
        block_t *b = &g_blocks[i];
        float by = (float) (b->y_top + reel_offset);
        float bh = (float) b->height;
        if (by + bh < top) continue;
        if (by > bottom) break;

        float draw_y = by - top;
        render_block(b, draw_y);

        if (!*want_bg && by <= centre && by + bh >= centre) *want_bg = b->bg_key;
    }
}

static void render_reel(void) {
    float top = g_scroll_y;
    float bottom = top + (float) g_screen_h;

    const char *want_bg = NULL;

    render_reel_pass(top, bottom, 0, &want_bg);

    if (!config.BOOT.FACTORY_RESET) {
        render_reel_pass(top, bottom, g_reel_height, &want_bg);
        if (g_first_pass_done) render_reel_pass(top, bottom, -g_reel_height, &want_bg);
    }

    if (!want_bg) {
        for (int i = 0; i < g_block_count; ++i) {
            block_t *b = &g_blocks[i];
            if ((float) (b->y_top + b->height) >= top) {
                want_bg = b->bg_key;
                break;
            }
        }
    }

    if (want_bg) bg_set_active(want_bg);
}

static bool g_audio_open = false;

static void try_start_music(void) {
    if (!g_audio_open) return;
    static const char *exts[] = {"ogg", "mp3", "wav", NULL};
    for (int i = 0; exts[i]; ++i) {
        char path[512];
        snprintf(path, sizeof(path), "%s/music.%s", MEDIA_DIR, exts[i]);
        g_music = Mix_LoadMUS(path);
        if (g_music) break;
    }
    if (g_music) {
        Mix_VolumeMusic(MIX_MAX_VOLUME * 3 / 4);
        Mix_FadeInMusic(g_music, -1, (int) (MUSIC_FADE_IN_S * 1000));
    }
}

static void finish_credits(void) {
    if (!config.BOOT.FACTORY_RESET) {
        write_text_to_file(MUOS_PDI_LOAD, "w", CHAR, "credit");
        write_text_to_file(MUOS_ACT_LOAD, "w", CHAR, "info");
    }

    g_running = false;
}

static void quit_credits(void) {
    if (g_quit_requested) return;

    g_quit_requested = true;
    g_quit_fade = 0.0f;

    if (g_music && Mix_PlayingMusic()) Mix_FadeOutMusic((int) (QUIT_FADE_S * 1000.0f));
}

static void render_quit_fade(float dt) {
    if (!g_quit_requested) return;

    g_quit_fade += dt / QUIT_FADE_S;
    if (g_quit_fade > 1.0f) g_quit_fade = 1.0f;

    float eased = smoothstep01(g_quit_fade);
    Uint8 alpha = (Uint8) lroundf(eased * 255.0f);

    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, alpha);

    SDL_Rect full = {0, 0, g_screen_w, g_screen_h};
    SDL_RenderFillRect(g_renderer, &full);

    if (g_quit_fade >= 1.0f) finish_credits();
}

static void handle_event(SDL_Event *ev) {
    if (ev->type == SDL_QUIT) {
        if (config.BOOT.FACTORY_RESET && !g_first_pass_done) return;
        quit_credits();
        return;
    }

    if (ev->type == SDL_KEYDOWN || ev->type == SDL_JOYBUTTONDOWN || ev->type == SDL_CONTROLLERBUTTONDOWN) {
        if (config.BOOT.FACTORY_RESET) return;
        g_skip_requested = true;
    }
}

static void init_sdl(void) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) < 0) die("SDL_Init");
    if (TTF_Init() < 0) die("TTF_Init");

    int img_flags = IMG_INIT_PNG | IMG_INIT_JPG;
    if ((IMG_Init(img_flags) & img_flags) != img_flags) LOG_WARN(mux_module, "IMG_Init partial: %s", IMG_GetError());

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) < 0) {
        LOG_WARN(mux_module, "Mix_OpenAudio failed: %s", Mix_GetError());
        g_audio_open = false;
    } else {
        g_audio_open = true;
    }

    g_window = SDL_CreateWindow(mux_module, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0,
                                SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS);

    if (!g_window) die("SDL_CreateWindow");

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_renderer) g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    if (!g_renderer) die("SDL_CreateRenderer");

    SDL_GetRendererOutputSize(g_renderer, &g_screen_w, &g_screen_h);
    g_scale = (float) g_screen_h / (float) REF_H;

    recompute_text_shadow_offset();

    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_ShowCursor(SDL_DISABLE);

    int n = SDL_NumJoysticks();
    for (int i = 0; i < n; ++i) SDL_JoystickOpen(i);
}

static void load_fonts(void) {
    int s_huge = sx(40);
    int s_big = sx(30);
    int s_med = sx(22);
    int s_sml = sx(22);

    g_font_huge = TTF_OpenFont(FONT_FILE, s_huge);
    g_font_big = TTF_OpenFont(FONT_FILE, s_big);
    g_font_med = TTF_OpenFont(FONT_FILE, s_med);
    g_font_sml = TTF_OpenFont(FONT_FILE, s_sml);

    if (!g_font_huge || !g_font_big || !g_font_med || !g_font_sml) die("TTF_OpenFont");
}

static void main_loop(void) {
    Uint64 prev = SDL_GetPerformanceCounter();
    double freq = (double) SDL_GetPerformanceFrequency();

    g_scroll_y = (float) -g_screen_h;

    float intro_hold = INTRO_HOLD_S;

    while (true) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) handle_event(&ev);

        if (!g_running) break;

        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float) ((double) (now - prev) / freq);
        prev = now;
        if (dt > 0.1f) dt = 0.1f;

        float scroll_speed = SCROLL_PX_PER_S * g_scale;

        if (!g_quit_requested) {
            if (intro_hold > 0.0f) {
                intro_hold -= dt;
            } else if (config.BOOT.FACTORY_RESET) {
                g_scroll_y += scroll_speed * dt;

                float end_y = (float) g_reel_height;
                float fade_trigger_y = end_y - scroll_speed * OUTRO_HOLD_S;

                if (!g_factory_music_fading && g_scroll_y >= fade_trigger_y) {
                    g_factory_music_fading = true;
                    if (g_music && Mix_PlayingMusic()) Mix_FadeOutMusic((int) (OUTRO_HOLD_S * 1000.0f));
                }

                if (g_scroll_y >= end_y) {
                    g_scroll_y = end_y;
                    g_first_pass_done = true;
                    quit_credits();
                }
            } else {
                g_scroll_y += scroll_speed * dt;
                if (g_scroll_y >= (float) g_reel_height) {
                    float previous_reel_height = (float) g_reel_height;

                    g_scroll_y -= previous_reel_height;
                    g_first_pass_done = true;

                    refresh_quote_block();
                }
            }

            if (g_skip_requested && !config.BOOT.FACTORY_RESET) quit_credits();
        }

        render_backgrounds(dt);
        render_reel();
        render_quit_fade(dt);
        SDL_RenderPresent(g_renderer);
    }
}

static void cleanup(void) {
    for (int i = 0; i < g_block_count; ++i) {
        block_t *b = &g_blocks[i];
        if (b->image) SDL_DestroyTexture(b->image);
        for (int j = 0; j < b->line_count; ++j) {
            if (b->lines[j]) SDL_DestroyTexture(b->lines[j]);
        }
        free(b->lines);
        free(b->line_w);
        free(b->line_h);
    }

    g_block_count = 0;

    for (int i = 0; i < g_bg_count; ++i) {
        if (g_bgs[i].tex) SDL_DestroyTexture(g_bgs[i].tex);
    }

    g_bg_count = 0;

    if (g_font_huge) TTF_CloseFont(g_font_huge);
    if (g_font_big) TTF_CloseFont(g_font_big);
    if (g_font_med) TTF_CloseFont(g_font_med);
    if (g_font_sml) TTF_CloseFont(g_font_sml);

    if (g_music) {
        Mix_HaltMusic();
        Mix_FreeMusic(g_music);
    }

    if (g_audio_open) Mix_CloseAudio();

    if (g_renderer) SDL_DestroyRenderer(g_renderer);
    if (g_window) SDL_DestroyWindow(g_window);

    free(g_quote_raw);
    g_quote_raw = NULL;
    g_quote_block = NULL;

    IMG_Quit();
    TTF_Quit();
    Mix_Quit();
    SDL_Quit();
}

int main(void) {
    init_module("mucredits");
    load_device(&device);
    load_config(&config);

    init_sdl();
    load_fonts();
    try_start_music();
    build_reel();
    prewarm_backgrounds();

    main_loop();

    cleanup();
    return 0;
}
