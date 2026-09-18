#include <stdlib.h>
#include <SDL2/SDL.h>
#include <common/content/catalogue.h>
#include <common/content/core/common.h>
#include <common/platform/display.h>
#include <common/runtime/init.h>
#include <common/runtime/log.h>
#include <common/display/overlay.h>
#include <common/base/strutil.h>
#include <common/ui/image.h>
#include <common/base/util.h>
#include "overlay_bridge.h"
#include "overlay_library.h"
#include "../settings/settings.h"

static char catalogue_overlay_path[MAX_BUFFER_SIZE];

static SDL_Texture *current_overlay_tex = NULL;
static uint8_t current_overlay_opacity = 0;
static int current_overlay_w = 0;
static int current_overlay_h = 0;

void overlay_bridge_init(const char *core_path_arg, const char *content_path) {
    (void) core_path_arg;

    catalogue_overlay_path[0] = '\0';

    char *sys_dir = get_content_path((char *) content_path);
    const char *content_label = get_file_name(content_path);

    char catalogue_name[MAX_BUFFER_SIZE];
    get_catalogue_name(sys_dir, content_label, catalogue_name, sizeof(catalogue_name));
    free(sys_dir);

    if (!catalogue_name[0]) return;

    char *program_no_ext = strip_ext(content_label);
    char program[MAX_BUFFER_SIZE];
    snprintf(program, sizeof(program), "%s", program_no_ext ? program_no_ext : content_label);
    free(program_no_ext);

    if (load_png_catalogue(
            catalogue_name, program, program, "default", mux_dim, "overlay/base", catalogue_overlay_path,
            sizeof(catalogue_overlay_path)
        )) {
        LOG_INFO(mux_module, "Catalogue overlay found: %s", catalogue_overlay_path);
    }
}

void overlay_bridge_apply(void) {
    if (current_overlay_tex) {
        SDL_DestroyTexture(current_overlay_tex);
        current_overlay_tex = NULL;
    }

    current_overlay_opacity = (uint8_t) pct_to_int(session_settings.overlay_opacity, 0, 255);

    switch (session_settings.overlay_source) {
        case overlay_source_pattern: {
            char path[MAX_BUFFER_SIZE];
            if (resolve_overlay_pattern_image(
                    overlay_pattern_to_value(session_settings.overlay_pattern), path, sizeof(path)
                )) {
                current_overlay_tex = display_load_png_texture(path);
            }
            break;
        }
        case overlay_source_catalogue:
            if (catalogue_overlay_path[0]) current_overlay_tex = display_load_png_texture(catalogue_overlay_path);
            break;
        case overlay_source_downloaded: {
            char path[MAX_BUFFER_SIZE];
            if (overlay_library_path(session_settings.overlay_image, path, sizeof(path)))
                current_overlay_tex = display_load_png_texture(path);
            break;
        }
        default:
            break;
    }

    if (current_overlay_tex) {
        SDL_QueryTexture(current_overlay_tex, NULL, NULL, &current_overlay_w, &current_overlay_h);
        SDL_SetTextureBlendMode(current_overlay_tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureAlphaMod(current_overlay_tex, current_overlay_opacity);
    }
}

static int overlay_suppressed;

void overlay_bridge_set_suppressed(const int suppressed) {
    overlay_suppressed = suppressed;
}

int overlay_bridge_active(void) {
    return !overlay_suppressed && current_overlay_tex != NULL;
}

static int trim(const int requested, const int extent, const int opposite) {
    const int room = extent - opposite - 1;
    if (room <= 0) return 0;
    return requested < 0 ? 0 : requested > room ? room : requested;
}

void overlay_bridge_render(SDL_Renderer *renderer, const int canvas_w, const int canvas_h, const int physical_output) {
    if (overlay_suppressed || !current_overlay_tex) return;

    const int left = trim(session_settings.overlay_crop_left, current_overlay_w, 0);
    const int right = trim(session_settings.overlay_crop_right, current_overlay_w, left);
    const int top = trim(session_settings.overlay_crop_top, current_overlay_h, 0);
    const int bottom = trim(session_settings.overlay_crop_bottom, current_overlay_h, top);

    const SDL_Rect src = {left, top, current_overlay_w - left - right, current_overlay_h - top - bottom};
    if (src.w <= 0 || src.h <= 0) return;

    int width = src.w;
    int height = src.h;

    if (session_settings.overlay_zoom != 100) {
        width = width * session_settings.overlay_zoom / 100;
        height = height * session_settings.overlay_zoom / 100;
    }

    width += session_settings.overlay_stretch_x;
    height += session_settings.overlay_stretch_y;
    if (width < 1) width = 1;
    if (height < 1) height = 1;

    const SDL_Rect logical_dst = {
        (canvas_w - width) / 2 + session_settings.overlay_offset_x,
        (canvas_h - height) / 2 + session_settings.overlay_offset_y, width, height
    };
    SDL_Rect output_dst = logical_dst;
    if (physical_output) display_map_logical_rect(&logical_dst, &output_dst);

    SDL_RenderCopy(renderer, current_overlay_tex, &src, &output_dst);
}

void overlay_bridge_shutdown(void) {
    if (current_overlay_tex) {
        SDL_DestroyTexture(current_overlay_tex);
        current_overlay_tex = NULL;
    }
}
