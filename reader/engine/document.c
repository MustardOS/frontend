#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <mupdf/fitz.h>
#include <mupdf/ucdn.h>
#include <common/base/options.h>
#include <common/config/config.h>
#include <common/runtime/init.h>
#include <common/runtime/log.h>
#include "document.h"

#define DOC_STORE       (32 << 20)
#define DOC_MAX_PIXELS  (12.0f * 1024.0f * 1024.0f)
#define DOC_MIN_LAYOUT  64
#define DOC_CJK_COUNT   4
#define DOC_FONT_PATH   OPT_PATH "share/font/internal/"
#define DOC_POSITION    100000
#define DOC_USER_FAMILY "ChutneyFont"
#define DOC_CACHE_MAX   4
#define DOC_CACHE_BYTES (24u << 20)
#define DOC_TRIM_PAD    0.015f
#define DOC_TRIM_MIN    0.10f

typedef struct {
    fz_pixmap *pix;
    int page;
    doc_view view;
    uint32_t used;
} doc_slot;

struct doc_engine {
    fz_context *ctx;
    fz_document *doc;
    fz_pixmap *pix;
    doc_slot cache[DOC_CACHE_MAX];
    int cache_slots;
    uint32_t cache_clock;
    int tried_page;
    doc_view tried_view;
    fz_rect *trim_boxes;
    unsigned char *trim_known;
    int trim_count;
    fz_font *cjk[DOC_CJK_COUNT];
    fz_font *user[4];
    char font_path[PATH_MAX];
    char path[PATH_MAX];
    int reflowable;
    int comic;
    int page_count;
    int line_spacing;
    int default_ordering;
};

static const char *cjk_files[DOC_CJK_COUNT] = {
    "Chinese (Traditional)/Noto Sans TC.ttf",
    "Chinese (Simplified)/Noto Sans SC.ttf",
    "Japanese/Noto Sans JP.ttf",
    "Korean/Noto Sans KR.ttf",
};

static const char *comic_extensions[] = {"cbz", "cbt"};
static const char *supported_extensions[] = {"pdf", "epub", "cbz", "cbt", "txt"};

static const char *path_extension(const char *path) {
    const char *slash = strrchr(path, '/');
    const char *dot = strrchr(path, '.');
    if (!dot || (slash && dot < slash)) return "";
    return dot + 1;
}

static int extension_in(const char *path, const char **list, const size_t count) {
    const char *extension = path_extension(path);
    for (size_t i = 0; i < count; i++)
        if (strcasecmp(extension, list[i]) == 0) return 1;
    return 0;
}

int doc_extension_supported(const char *path) {
    return path && extension_in(path, supported_extensions, A_SIZE(supported_extensions));
}

static int ui_ordering(void) {
    const char *language = config.settings.general.language;
    if (strstr(language, "Japanese")) return FZ_ADOBE_JAPAN;
    if (strstr(language, "Korean")) return FZ_ADOBE_KOREA;
    if (strstr(language, "Traditional")) return FZ_ADOBE_CNS;
    return FZ_ADOBE_GB;
}

static fz_font *cjk_font(fz_context *ctx, const int ordering) {
    doc_engine *engine = fz_user_context(ctx);
    if (!engine || ordering < 0 || ordering >= DOC_CJK_COUNT) return NULL;

    if (!engine->cjk[ordering]) {
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s%s", DOC_FONT_PATH, cjk_files[ordering]);

        fz_try(ctx) {
            engine->cjk[ordering] = fz_new_font_from_file(ctx, NULL, path, 0, 0);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            LOG_WARN(mux_module, "Document: no CJK font at '%s'", path);
            return NULL;
        }
    }

    return fz_keep_font(ctx, engine->cjk[ordering]);
}

static int font_variant_path(const char *base, const int bold, const int italic, char *out, const size_t size) {
    const char *dot = strrchr(base, '.');
    const char *slash = strrchr(base, '/');
    if (!dot || (slash && dot < slash)) return 0;

    static const char *bold_italic[] = {"-BoldItalic", "-Bold Italic", " Bold Italic", "-BoldOblique", NULL};
    static const char *bold_only[] = {"-Bold", " Bold", NULL};
    static const char *italic_only[] = {"-Italic", " Italic", "-Oblique", NULL};
    const char **suffixes = bold && italic ? bold_italic : bold ? bold_only : italic_only;

    const int stem = (int) (dot - base);
    for (int i = 0; suffixes[i]; i++) {
        snprintf(out, size, "%.*s%s%s", stem, base, suffixes[i], dot);
        FILE *probe = fopen(out, "rb");
        if (probe) {
            fclose(probe);
            return 1;
        }
    }

    return 0;
}

static fz_font *user_font(fz_context *ctx, const int bold, const int italic) {
    doc_engine *engine = fz_user_context(ctx);
    if (!engine || !engine->font_path[0]) return NULL;

    const int slot = (bold ? 2 : 0) | (italic ? 1 : 0);
    if (!engine->user[slot]) {
        char variant[PATH_MAX];
        const int exact = slot ? font_variant_path(engine->font_path, bold, italic, variant, sizeof(variant)) : 0;
        const char *file = exact ? variant : engine->font_path;

        fz_try(ctx) {
            engine->user[slot] = fz_new_font_from_file(ctx, NULL, file, 0, 0);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            LOG_WARN(mux_module, "Document: could not load font '%s'", file);
            return NULL;
        }

        if (!exact) {
            fz_font_flags(engine->user[slot])->fake_bold = bold ? 1 : 0;
            fz_font_flags(engine->user[slot])->fake_italic = italic ? 1 : 0;
        }
    }

    return fz_keep_font(ctx, engine->user[slot]);
}

static fz_font *
load_system_font(fz_context *ctx, const char *name, const int bold, const int italic, const int needs_exact_metrics) {
    (void) needs_exact_metrics;
    if (!name || strcmp(name, DOC_USER_FAMILY) != 0) return NULL;
    return user_font(ctx, bold, italic);
}

static fz_font *load_cjk_font(fz_context *ctx, const char *name, const int ordering, const int serif) {
    (void) name;
    (void) serif;
    return cjk_font(ctx, ordering);
}

static fz_font *load_fallback_font(
    fz_context *ctx, const int script, const int language, const int serif, const int bold, const int italic
) {
    (void) serif;
    (void) bold;
    (void) italic;

    const doc_engine *engine = fz_user_context(ctx);
    const int fallback = engine ? engine->default_ordering : FZ_ADOBE_GB;

    if (language == FZ_LANG_ja) return cjk_font(ctx, FZ_ADOBE_JAPAN);
    if (language == FZ_LANG_ko) return cjk_font(ctx, FZ_ADOBE_KOREA);
    if (language == FZ_LANG_zh_Hant) return cjk_font(ctx, FZ_ADOBE_CNS);
    if (language == FZ_LANG_zh_Hans) return cjk_font(ctx, FZ_ADOBE_GB);
    if (language == FZ_LANG_zh) return cjk_font(ctx, fallback == FZ_ADOBE_CNS ? FZ_ADOBE_CNS : FZ_ADOBE_GB);

    switch (script) {
        case UCDN_SCRIPT_HANGUL:
            return cjk_font(ctx, FZ_ADOBE_KOREA);
        case UCDN_SCRIPT_HIRAGANA:
        case UCDN_SCRIPT_KATAKANA:
            return cjk_font(ctx, FZ_ADOBE_JAPAN);
        case UCDN_SCRIPT_BOPOMOFO:
            return cjk_font(ctx, FZ_ADOBE_CNS);
        case UCDN_SCRIPT_HAN:
            return cjk_font(ctx, fallback);
        default:
            return NULL;
    }
}

static void user_css(const doc_engine *engine, char *css, const size_t size) {
    int used = snprintf(css, size, "@page{margin:0 !important}body{margin:0 !important}");

    if (engine->line_spacing > 0 && used > 0 && (size_t) used < size)
        used += snprintf(css + used, size - (size_t) used, "body{line-height:%d%% !important}", engine->line_spacing);

    if (engine->font_path[0] && used > 0 && (size_t) used < size)
        snprintf(
            css + used, size - (size_t) used,
            "body,p,div,span,pre,li,td,th,blockquote,a,em,strong,b,i,h1,h2,h3,h4,h5,h6{font-family:%s !important}",
            DOC_USER_FAMILY
        );
}

static fz_document *open_accelerated(doc_engine *engine, const char *accel_path) {
    if (!accel_path || !accel_path[0] || access(accel_path, R_OK) != 0) return NULL;

    fz_document *document = NULL;
    fz_var(document);

    fz_try(engine->ctx) {
        document = fz_open_accelerated_document(engine->ctx, engine->path, accel_path);
    }
    fz_catch(engine->ctx) {
        fz_report_error(engine->ctx);
        LOG_WARN(mux_module, "Document: ignoring layout cache '%s'", accel_path);
        document = NULL;
    }

    return document;
}

static int open_document(doc_engine *engine, const char *accel_path) {
    fz_document *document = open_accelerated(engine, accel_path);
    fz_var(document);

    char css[512];
    user_css(engine, css, sizeof(css));

    fz_try(engine->ctx) {
        if (!document) document = fz_open_document(engine->ctx, engine->path);
        fz_style_document(engine->ctx, document, 1, css);
        if (fz_needs_password(engine->ctx, document) && !fz_authenticate_password(engine->ctx, document, ""))
            fz_throw(engine->ctx, FZ_ERROR_ARGUMENT, "password required");
        engine->reflowable = fz_is_document_reflowable(engine->ctx, document);
        engine->page_count = engine->reflowable ? 0 : fz_count_pages(engine->ctx, document);
    }
    fz_catch(engine->ctx) {
        LOG_ERROR(mux_module, "Document: could not open '%s': %s", engine->path, fz_caught_message(engine->ctx));
        fz_drop_document(engine->ctx, document);
        return 0;
    }

    if (!engine->reflowable && engine->page_count <= 0) {
        LOG_ERROR(mux_module, "Document: '%s' has no pages", engine->path);
        fz_drop_document(engine->ctx, document);
        return 0;
    }

    engine->doc = document;
    return 1;
}

doc_engine *doc_open(const char *path, const int line_spacing) {
    return doc_open_full(path, line_spacing, NULL, NULL);
}

doc_engine *doc_open_full(const char *path, const int line_spacing, const char *font_path, const char *accel_path) {
    if (!path || !path[0]) return NULL;

    doc_engine *engine = calloc(1, sizeof(*engine));
    if (!engine) return NULL;

    snprintf(engine->path, sizeof(engine->path), "%s", path);
    snprintf(engine->font_path, sizeof(engine->font_path), "%s", font_path ? font_path : "");
    engine->line_spacing = line_spacing;
    engine->tried_page = -1;
    engine->default_ordering = ui_ordering();
    engine->comic = extension_in(path, comic_extensions, A_SIZE(comic_extensions));

    engine->ctx = fz_new_context(NULL, NULL, DOC_STORE);
    if (!engine->ctx) {
        LOG_ERROR(mux_module, "Document: could not create a context");
        free(engine);
        return NULL;
    }

    fz_set_user_context(engine->ctx, engine);
    fz_install_load_system_font_funcs(engine->ctx, load_system_font, load_cjk_font, load_fallback_font);

    int registered = 1;
    fz_try(engine->ctx) {
        fz_register_document_handlers(engine->ctx);
    }
    fz_catch(engine->ctx) {
        fz_report_error(engine->ctx);
        registered = 0;
    }

    if (!registered || !open_document(engine, accel_path)) {
        doc_close(engine);
        return NULL;
    }

    LOG_INFO(mux_module, "Document: opened '%s'", path);
    return engine;
}

void doc_close(doc_engine *engine) {
    if (!engine) return;

    if (engine->ctx) {
        for (int i = 0; i < DOC_CACHE_MAX; i++)
            fz_drop_pixmap(engine->ctx, engine->cache[i].pix);
        free(engine->trim_boxes);
        free(engine->trim_known);
        fz_drop_pixmap(engine->ctx, engine->pix);
        fz_drop_document(engine->ctx, engine->doc);
        for (int i = 0; i < DOC_CJK_COUNT; i++)
            fz_drop_font(engine->ctx, engine->cjk[i]);
        for (int i = 0; i < 4; i++)
            fz_drop_font(engine->ctx, engine->user[i]);
        fz_drop_context(engine->ctx);
    }

    free(engine);
}

int doc_is_reflowable(const doc_engine *engine) {
    return engine && engine->reflowable;
}

int doc_is_comic(const doc_engine *engine) {
    return engine && engine->comic;
}

static void cache_clear(doc_engine *engine) {
    for (int i = 0; i < DOC_CACHE_MAX; i++) {
        fz_drop_pixmap(engine->ctx, engine->cache[i].pix);
        engine->cache[i].pix = NULL;
    }
    engine->tried_page = -1;
}

static void trim_clear(doc_engine *engine) {
    free(engine->trim_boxes);
    free(engine->trim_known);
    engine->trim_boxes = NULL;
    engine->trim_known = NULL;
    engine->trim_count = 0;
}

static int same_view(const doc_view *a, const doc_view *b) {
    return a->view_w == b->view_w && a->view_h == b->view_h && a->fit_page == b->fit_page && a->zoom == b->zoom
           && a->trim == b->trim;
}

static doc_slot *cache_find(doc_engine *engine, const int page, const doc_view *view) {
    for (int i = 0; i < engine->cache_slots; i++) {
        doc_slot *slot = &engine->cache[i];
        if (slot->pix && slot->page == page && same_view(&slot->view, view)) {
            slot->used = ++engine->cache_clock;
            return slot;
        }
    }
    return NULL;
}

static size_t pixmap_bytes(fz_context *ctx, fz_pixmap *pix) {
    return (size_t) fz_pixmap_stride(ctx, pix) * (size_t) fz_pixmap_height(ctx, pix);
}

static void cache_store(doc_engine *engine, const int page, const doc_view *view, fz_pixmap *pix) {
    if (engine->cache_slots <= 0) return;

    const size_t bytes = pixmap_bytes(engine->ctx, pix);
    if (bytes > DOC_CACHE_BYTES / 2) return;

    /* The page on screen is the anchor, so with the page either side kept a forward or backward
     * turn never throws out what the next turn will want */
    int anchor = page;
    for (int i = 0; i < engine->cache_slots; i++)
        if (engine->cache[i].pix && engine->cache[i].pix == engine->pix) anchor = engine->cache[i].page;

    /* Make room by dropping the pages furthest from the one on screen, never that page itself */
    for (;;) {
        size_t total = bytes;
        int free_slot = -1;
        int victim = -1;
        int victim_distance = -1;

        for (int i = 0; i < engine->cache_slots; i++) {
            const doc_slot *slot = &engine->cache[i];
            if (!slot->pix) {
                if (free_slot < 0) free_slot = i;
                continue;
            }
            total += pixmap_bytes(engine->ctx, slot->pix);
            if (slot->pix == engine->pix) continue;

            const int distance = abs(slot->page - anchor);
            if (distance > victim_distance
                || (distance == victim_distance && slot->used < engine->cache[victim].used)) {
                victim = i;
                victim_distance = distance;
            }
        }

        if (free_slot >= 0 && total <= DOC_CACHE_BYTES) {
            engine->cache[free_slot] =
                (doc_slot) {fz_keep_pixmap(engine->ctx, pix), page, *view, ++engine->cache_clock};
            return;
        }
        if (victim < 0) return;

        fz_drop_pixmap(engine->ctx, engine->cache[victim].pix);
        engine->cache[victim].pix = NULL;
    }
}

void doc_set_cache(doc_engine *engine, const int slots) {
    if (!engine) return;
    cache_clear(engine);
    engine->cache_slots = slots < 0 ? 0 : slots > DOC_CACHE_MAX ? DOC_CACHE_MAX : slots;
}

static int restyle(doc_engine *engine) {
    fz_drop_pixmap(engine->ctx, engine->pix);
    engine->pix = NULL;
    cache_clear(engine);

    char css[512];
    user_css(engine, css, sizeof(css));

    fz_try(engine->ctx) {
        fz_style_document(engine->ctx, engine->doc, 1, css);
    }
    fz_catch(engine->ctx) {
        LOG_ERROR(mux_module, "Document: restyle failed: %s", fz_caught_message(engine->ctx));
        return 0;
    }

    return 1;
}

int doc_set_line_spacing(doc_engine *engine, const int line_spacing) {
    if (!engine || !engine->reflowable || engine->line_spacing == line_spacing) return 1;

    engine->line_spacing = line_spacing;
    return restyle(engine);
}

int doc_set_font(doc_engine *engine, const char *font_path) {
    if (!engine) return 0;

    const char *wanted = font_path ? font_path : "";
    if (strcmp(engine->font_path, wanted) == 0) return 1;

    for (int i = 0; i < 4; i++) {
        fz_drop_font(engine->ctx, engine->user[i]);
        engine->user[i] = NULL;
    }
    snprintf(engine->font_path, sizeof(engine->font_path), "%s", wanted);

    return !engine->doc || !engine->reflowable || restyle(engine);
}

int doc_layout(doc_engine *engine, const int view_w, const int view_h, const doc_reflow *reflow) {
    if (!engine || !engine->doc) return 0;
    if (!engine->reflowable || !reflow) return engine->page_count;

    int width = view_w - reflow->margin * 2;
    int height = view_h - reflow->margin * 2;
    if (width < DOC_MIN_LAYOUT) width = DOC_MIN_LAYOUT;
    if (height < DOC_MIN_LAYOUT) height = DOC_MIN_LAYOUT;

    int count = 0;
    fz_var(count);

    cache_clear(engine);

    fz_try(engine->ctx) {
        fz_layout_document(engine->ctx, engine->doc, (float) width, (float) height, (float) reflow->font_size);
        count = fz_count_pages(engine->ctx, engine->doc);
    }
    fz_catch(engine->ctx) {
        LOG_ERROR(mux_module, "Document: layout failed: %s", fz_caught_message(engine->ctx));
        return 0;
    }

    engine->page_count = count;
    return count;
}

int doc_page_count(const doc_engine *engine) {
    return engine ? engine->page_count : 0;
}

static fz_rect trim_box(doc_engine *engine, const int page, fz_page *loaded, const fz_rect bounds) {
    if (engine->trim_count != engine->page_count) {
        trim_clear(engine);
        engine->trim_boxes = calloc((size_t) engine->page_count, sizeof(*engine->trim_boxes));
        engine->trim_known = calloc((size_t) engine->page_count, sizeof(*engine->trim_known));
        if (!engine->trim_boxes || !engine->trim_known) {
            trim_clear(engine);
            return bounds;
        }
        engine->trim_count = engine->page_count;
    }

    if (engine->trim_known[page]) return engine->trim_boxes[page];

    fz_rect content = fz_empty_rect;
    fz_device *device = NULL;
    fz_var(device);

    fz_try(engine->ctx) {
        device = fz_new_bbox_device(engine->ctx, &content);
        fz_run_page(engine->ctx, loaded, device, fz_identity, NULL);
        fz_close_device(engine->ctx, device);
    }
    fz_always(engine->ctx) {
        fz_drop_device(engine->ctx, device);
    }
    fz_catch(engine->ctx) {
        fz_report_error(engine->ctx);
        content = fz_empty_rect;
    }

    const float page_w = bounds.x1 - bounds.x0;
    const float page_h = bounds.y1 - bounds.y0;
    fz_rect box = bounds;

    content = fz_intersect_rect(content, bounds);
    if (!fz_is_empty_rect(content) && content.x1 - content.x0 >= page_w * DOC_TRIM_MIN
        && content.y1 - content.y0 >= page_h * DOC_TRIM_MIN) {
        const float pad = (page_w > page_h ? page_w : page_h) * DOC_TRIM_PAD;
        box = fz_intersect_rect(fz_expand_rect(content, pad), bounds);
    }

    engine->trim_boxes[page] = box;
    engine->trim_known[page] = 1;
    return box;
}

/* A limit above zero skips pages that would come out larger than that many bytes */
static fz_pixmap *render_pixmap(doc_engine *engine, const int page, const doc_view *view, const size_t limit) {
    fz_page *loaded = NULL;
    fz_pixmap *pix = NULL;
    fz_device *device = NULL;
    fz_var(loaded);
    fz_var(pix);
    fz_var(device);

    fz_try(engine->ctx) {
        loaded = fz_load_page(engine->ctx, engine->doc, page);
        const fz_rect bounds = fz_bound_page(engine->ctx, loaded);
        if (bounds.x1 - bounds.x0 <= 0.0f || bounds.y1 - bounds.y0 <= 0.0f)
            fz_throw(engine->ctx, FZ_ERROR_FORMAT, "empty page");

        const fz_rect area = !engine->reflowable && view->trim ? trim_box(engine, page, loaded, bounds) : bounds;
        const float area_w = area.x1 - area.x0;
        const float area_h = area.y1 - area.y0;

        float scale = engine->reflowable ? 1.0f : (float) view->view_w / area_w;
        if (!engine->reflowable && view->fit_page && (float) view->view_h / area_h < scale)
            scale = (float) view->view_h / area_h;
        if (!engine->reflowable) scale *= (float) (view->zoom > 0 ? view->zoom : 100) / 100.0f;

        const float pixels = area_w * scale * area_h * scale;
        if (pixels > DOC_MAX_PIXELS) scale *= sqrtf(DOC_MAX_PIXELS / pixels);

        const fz_matrix ctm = fz_scale(scale, scale);
        /* Sized to the nearest pixel, as rounding outwards can make a page one pixel wider than the screen */
        fz_irect box = fz_round_rect(fz_transform_rect(area, ctm));
        box.x1 = box.x0 + (int) fmaxf(1.0f, roundf(area_w * scale));
        box.y1 = box.y0 + (int) fmaxf(1.0f, roundf(area_h * scale));
        const size_t bytes = (size_t) (box.x1 - box.x0) * (size_t) (box.y1 - box.y0) * 3u;
        if (limit > 0 && bytes > limit) fz_throw(engine->ctx, FZ_ERROR_LIMIT, "page too large to keep");

        pix = fz_new_pixmap_with_bbox(engine->ctx, fz_device_rgb(engine->ctx), box, NULL, 0);
        fz_clear_pixmap_with_value(engine->ctx, pix, 0xFF);

        device = fz_new_draw_device(engine->ctx, ctm, pix);
        fz_run_page(engine->ctx, loaded, device, fz_identity, NULL);
        fz_close_device(engine->ctx, device);
    }
    fz_always(engine->ctx) {
        fz_drop_device(engine->ctx, device);
        fz_drop_page(engine->ctx, loaded);
    }
    fz_catch(engine->ctx) {
        if (fz_caught(engine->ctx) == FZ_ERROR_LIMIT && limit > 0) return NULL;
        LOG_ERROR(mux_module, "Document: page %d failed: %s", page + 1, fz_caught_message(engine->ctx));
        fz_drop_pixmap(engine->ctx, pix);
        return NULL;
    }

    return pix;
}

static int view_valid(const doc_engine *engine, const int page, const doc_view *view) {
    if (!engine || !engine->doc || !view || page < 0 || page >= engine->page_count) return 0;
    return view->view_w > 0 && view->view_h > 0;
}

int doc_render(doc_engine *engine, const int page, const doc_view *view, doc_image *out) {
    if (!view_valid(engine, page, view) || !out) return 0;

    fz_pixmap *pix = NULL;
    const doc_slot *slot = cache_find(engine, page, view);

    if (slot) {
        pix = fz_keep_pixmap(engine->ctx, slot->pix);
    } else {
        /* Without a cache the old page is released first to keep the peak memory down */
        if (engine->cache_slots <= 0) {
            fz_drop_pixmap(engine->ctx, engine->pix);
            engine->pix = NULL;
        }
        pix = render_pixmap(engine, page, view, 0);
    }

    fz_drop_pixmap(engine->ctx, engine->pix);
    engine->pix = pix;
    if (!pix) return 0;
    if (!slot) cache_store(engine, page, view, pix);

    out->width = fz_pixmap_width(engine->ctx, pix);
    out->height = fz_pixmap_height(engine->ctx, pix);
    out->stride = fz_pixmap_stride(engine->ctx, pix);
    out->components = fz_pixmap_components(engine->ctx, pix);
    out->samples = fz_pixmap_samples(engine->ctx, pix);
    return 1;
}

int doc_prefetch(doc_engine *engine, const int page, const doc_view *view) {
    if (!view_valid(engine, page, view) || engine->cache_slots <= 0) return 0;
    if (cache_find(engine, page, view)) return 0;
    if (engine->tried_page == page && same_view(&engine->tried_view, view)) return 0;

    engine->tried_page = page;
    engine->tried_view = *view;

    fz_pixmap *pix = render_pixmap(engine, page, view, DOC_CACHE_BYTES / 2);
    if (!pix) return 0;

    cache_store(engine, page, view, pix);
    fz_drop_pixmap(engine->ctx, pix);
    return 1;
}

int doc_save_accelerator(doc_engine *engine, const char *accel_path) {
    if (!engine || !engine->doc || !accel_path || !accel_path[0]) return 0;
    if (!fz_document_supports_accelerator(engine->ctx, engine->doc)) return 0;

    char temp[PATH_MAX];
    snprintf(temp, sizeof(temp), "%s.tmp", accel_path);

    int saved = 1;
    fz_try(engine->ctx) {
        fz_save_accelerator(engine->ctx, engine->doc, temp);
    }
    fz_catch(engine->ctx) {
        fz_report_error(engine->ctx);
        saved = 0;
    }

    if (saved && rename(temp, accel_path) == 0) return 1;

    remove(temp);
    return 0;
}

static int clamp_page(const doc_engine *engine, const int page) {
    if (page < 0) return 0;
    if (page >= engine->page_count) return engine->page_count > 0 ? engine->page_count - 1 : 0;
    return page;
}

int64_t doc_bookmark(doc_engine *engine, const int page) {
    if (!engine || !engine->doc || engine->page_count <= 0) return 0;

    int64_t mark = 0;
    fz_var(mark);

    fz_try(engine->ctx) {
        const fz_location location = fz_location_from_page_number(engine->ctx, engine->doc, clamp_page(engine, page));
        mark = (int64_t) fz_make_bookmark(engine->ctx, engine->doc, location);
    }
    fz_catch(engine->ctx) {
        fz_report_error(engine->ctx);
        return 0;
    }

    return mark;
}

int doc_bookmark_page(doc_engine *engine, const int64_t mark) {
    if (!engine || !engine->doc || engine->page_count <= 0) return 0;

    int page = 0;
    fz_var(page);

    fz_try(engine->ctx) {
        const fz_location location = fz_lookup_bookmark(engine->ctx, engine->doc, (fz_bookmark) mark);
        page = fz_page_number_from_location(engine->ctx, engine->doc, location);
    }
    fz_catch(engine->ctx) {
        fz_report_error(engine->ctx);
        return 0;
    }

    return clamp_page(engine, page);
}

int doc_position(doc_engine *engine, const int page) {
    if (!engine || !engine->doc || engine->page_count <= 0) return 0;
    if (!engine->reflowable) return clamp_page(engine, page);

    int position = 0;
    fz_var(position);

    fz_try(engine->ctx) {
        const fz_location location = fz_location_from_page_number(engine->ctx, engine->doc, clamp_page(engine, page));
        const int pages = fz_count_chapter_pages(engine->ctx, engine->doc, location.chapter);
        /* The middle of the page is stored so reading it back always lands on the same page */
        const int fraction = pages > 0 ? (int) ((int64_t) (2 * location.page + 1) * DOC_POSITION / (2 * pages)) : 0;
        position = location.chapter * DOC_POSITION + fraction;
    }
    fz_catch(engine->ctx) {
        fz_report_error(engine->ctx);
        return 0;
    }

    return position;
}

int doc_position_page(doc_engine *engine, const int position) {
    if (!engine || !engine->doc || engine->page_count <= 0 || position < 0) return 0;
    if (!engine->reflowable) return clamp_page(engine, position);

    int page = 0;
    fz_var(page);

    fz_try(engine->ctx) {
        const int chapters = fz_count_chapters(engine->ctx, engine->doc);
        int chapter = position / DOC_POSITION;
        if (chapter >= chapters) chapter = chapters - 1;
        if (chapter < 0) chapter = 0;

        const int pages = fz_count_chapter_pages(engine->ctx, engine->doc, chapter);
        int within = (int) ((int64_t) (position % DOC_POSITION) * pages / DOC_POSITION);
        if (within >= pages) within = pages - 1;
        if (within < 0) within = 0;

        page = fz_page_number_from_location(engine->ctx, engine->doc, fz_make_location(chapter, within));
    }
    fz_catch(engine->ctx) {
        fz_report_error(engine->ctx);
        return 0;
    }

    return clamp_page(engine, page);
}

typedef struct {
    doc_toc_entry *entries;
    int count;
    int capacity;
} toc_builder;

static int toc_push(toc_builder *builder, const char *title, const int depth, const int page) {
    if (builder->count >= builder->capacity) {
        const int capacity = builder->capacity ? builder->capacity * 2 : 64;
        doc_toc_entry *grown = realloc(builder->entries, (size_t) capacity * sizeof(*grown));
        if (!grown) return 0;
        builder->entries = grown;
        builder->capacity = capacity;
    }

    char *copy = strdup(title && title[0] ? title : "-");
    if (!copy) return 0;

    builder->entries[builder->count++] = (doc_toc_entry) {copy, depth, page};
    return 1;
}

static int outline_page(doc_engine *engine, const fz_outline *item) {
    fz_location location = item->page;
    fz_var(location);

    if (item->uri && !fz_is_external_link(engine->ctx, item->uri)) {
        fz_try(engine->ctx) {
            location = fz_resolve_link(engine->ctx, engine->doc, item->uri, NULL, NULL);
        }
        fz_catch(engine->ctx) {
            fz_report_error(engine->ctx);
        }
    }

    if (location.chapter < 0 || location.page < 0) return -1;

    int page = -1;
    fz_var(page);
    fz_try(engine->ctx) {
        page = fz_page_number_from_location(engine->ctx, engine->doc, location);
    }
    fz_catch(engine->ctx) {
        fz_report_error(engine->ctx);
        page = -1;
    }

    return page;
}

static int toc_walk(doc_engine *engine, toc_builder *builder, const fz_outline *item, const int depth) {
    for (; item; item = item->next) {
        const int page = outline_page(engine, item);
        if (page >= 0 && !toc_push(builder, item->title, depth, clamp_page(engine, page))) return 0;
        if (item->down && !toc_walk(engine, builder, item->down, depth + 1)) return 0;
    }
    return 1;
}

int doc_has_toc(doc_engine *engine) {
    if (!engine || !engine->doc) return 0;

    fz_outline *outline = NULL;
    fz_var(outline);

    fz_try(engine->ctx) {
        outline = fz_load_outline(engine->ctx, engine->doc);
    }
    fz_catch(engine->ctx) {
        fz_report_error(engine->ctx);
        return 0;
    }

    const int found = outline != NULL;
    fz_drop_outline(engine->ctx, outline);
    return found;
}

int doc_toc(doc_engine *engine, doc_toc_entry **entries) {
    *entries = NULL;
    if (!engine || !engine->doc) return 0;

    fz_outline *outline = NULL;
    fz_var(outline);

    fz_try(engine->ctx) {
        outline = fz_load_outline(engine->ctx, engine->doc);
    }
    fz_catch(engine->ctx) {
        fz_report_error(engine->ctx);
        return 0;
    }

    toc_builder builder = {0};
    if (!toc_walk(engine, &builder, outline, 0)) {
        doc_toc_free(builder.entries, builder.count);
        builder.entries = NULL;
        builder.count = 0;
    }

    fz_drop_outline(engine->ctx, outline);
    *entries = builder.entries;
    return builder.count;
}

void doc_toc_free(doc_toc_entry *entries, const int count) {
    if (!entries) return;
    for (int i = 0; i < count; i++)
        free(entries[i].title);
    free(entries);
}

int doc_title(doc_engine *engine, char *title, const int size) {
    if (!engine || !engine->doc || !title || size <= 0) return 0;
    title[0] = '\0';

    int found = 0;
    fz_var(found);

    fz_try(engine->ctx) {
        found = fz_lookup_metadata(engine->ctx, engine->doc, FZ_META_INFO_TITLE, title, (size_t) size) > 0;
    }
    fz_catch(engine->ctx) {
        fz_report_error(engine->ctx);
        found = 0;
    }

    if (!found || !title[0]) {
        title[0] = '\0';
        return 0;
    }
    return 1;
}
