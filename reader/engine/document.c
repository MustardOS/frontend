#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <mupdf/fitz.h>
#include <mupdf/ucdn.h>
#include <common/base/options.h>
#include <common/config/config.h>
#include <common/runtime/init.h>
#include <common/runtime/log.h>
#include "document.h"

#define DOC_STORE      (32 << 20)
#define DOC_MAX_PIXELS (12.0f * 1024.0f * 1024.0f)
#define DOC_MIN_LAYOUT 64
#define DOC_CJK_COUNT  4
#define DOC_FONT_PATH  OPT_PATH "share/font/internal/"
#define DOC_POSITION   100000

struct doc_engine {
    fz_context *ctx;
    fz_document *doc;
    fz_pixmap *pix;
    fz_font *cjk[DOC_CJK_COUNT];
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

static fz_font *
load_system_font(fz_context *ctx, const char *name, const int bold, const int italic, const int needs_exact_metrics) {
    (void) ctx;
    (void) name;
    (void) bold;
    (void) italic;
    (void) needs_exact_metrics;
    return NULL;
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
    if (engine->line_spacing > 0)
        snprintf(
            css, size, "@page{margin:0 !important}body{margin:0 !important;line-height:%d%% !important}",
            engine->line_spacing
        );
    else
        snprintf(css, size, "@page{margin:0 !important}body{margin:0 !important}");
}

static int open_document(doc_engine *engine) {
    fz_document *document = NULL;
    fz_var(document);

    char css[256];
    user_css(engine, css, sizeof(css));

    fz_try(engine->ctx) {
        document = fz_open_document(engine->ctx, engine->path);
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
    if (!path || !path[0]) return NULL;

    doc_engine *engine = calloc(1, sizeof(*engine));
    if (!engine) return NULL;

    snprintf(engine->path, sizeof(engine->path), "%s", path);
    engine->line_spacing = line_spacing;
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

    if (!registered || !open_document(engine)) {
        doc_close(engine);
        return NULL;
    }

    LOG_INFO(mux_module, "Document: opened '%s'", path);
    return engine;
}

void doc_close(doc_engine *engine) {
    if (!engine) return;

    if (engine->ctx) {
        fz_drop_pixmap(engine->ctx, engine->pix);
        fz_drop_document(engine->ctx, engine->doc);
        for (int i = 0; i < DOC_CJK_COUNT; i++)
            fz_drop_font(engine->ctx, engine->cjk[i]);
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

int doc_set_line_spacing(doc_engine *engine, const int line_spacing) {
    if (!engine || !engine->reflowable || engine->line_spacing == line_spacing) return 1;

    fz_drop_pixmap(engine->ctx, engine->pix);
    engine->pix = NULL;

    engine->line_spacing = line_spacing;

    char css[256];
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

int doc_layout(doc_engine *engine, const int view_w, const int view_h, const doc_reflow *reflow) {
    if (!engine || !engine->doc) return 0;
    if (!engine->reflowable || !reflow) return engine->page_count;

    int width = view_w - reflow->margin * 2;
    int height = view_h - reflow->margin * 2;
    if (width < DOC_MIN_LAYOUT) width = DOC_MIN_LAYOUT;
    if (height < DOC_MIN_LAYOUT) height = DOC_MIN_LAYOUT;

    int count = 0;
    fz_var(count);

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

int doc_render(doc_engine *engine, const int page, const doc_view *view, doc_image *out) {
    if (!engine || !engine->doc || !view || !out || page < 0 || page >= engine->page_count) return 0;
    if (view->view_w <= 0 || view->view_h <= 0) return 0;

    fz_drop_pixmap(engine->ctx, engine->pix);
    engine->pix = NULL;

    fz_page *loaded = NULL;
    fz_pixmap *pix = NULL;
    fz_var(loaded);
    fz_var(pix);

    fz_try(engine->ctx) {
        loaded = fz_load_page(engine->ctx, engine->doc, page);
        const fz_rect bounds = fz_bound_page(engine->ctx, loaded);
        const float page_w = bounds.x1 - bounds.x0;
        const float page_h = bounds.y1 - bounds.y0;
        if (page_w <= 0.0f || page_h <= 0.0f) fz_throw(engine->ctx, FZ_ERROR_FORMAT, "empty page");

        float scale = engine->reflowable ? 1.0f : (float) view->view_w / page_w;
        if (!engine->reflowable && view->fit_page && (float) view->view_h / page_h < scale)
            scale = (float) view->view_h / page_h;
        if (!engine->reflowable) scale *= (float) (view->zoom > 0 ? view->zoom : 100) / 100.0f;

        const float pixels = page_w * scale * page_h * scale;
        if (pixels > DOC_MAX_PIXELS) scale *= sqrtf(DOC_MAX_PIXELS / pixels);

        pix = fz_new_pixmap_from_page(engine->ctx, loaded, fz_scale(scale, scale), fz_device_rgb(engine->ctx), 0);
    }
    fz_always(engine->ctx) {
        fz_drop_page(engine->ctx, loaded);
    }
    fz_catch(engine->ctx) {
        LOG_ERROR(mux_module, "Document: page %d failed: %s", page + 1, fz_caught_message(engine->ctx));
        fz_drop_pixmap(engine->ctx, pix);
        return 0;
    }

    engine->pix = pix;
    out->width = fz_pixmap_width(engine->ctx, pix);
    out->height = fz_pixmap_height(engine->ctx, pix);
    out->stride = fz_pixmap_stride(engine->ctx, pix);
    out->components = fz_pixmap_components(engine->ctx, pix);
    out->samples = fz_pixmap_samples(engine->ctx, pix);
    return 1;
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
        const int fraction = pages > 0 ? (int) ((int64_t) location.page * DOC_POSITION / pages) : 0;
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
