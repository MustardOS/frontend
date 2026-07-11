#include <stdlib.h>
#include <SDL2/SDL.h>
#include "../../common/catalogue.h"
#include "../../common/core/common.h"
#include "../../common/display.h"
#include "../../common/init.h"
#include "../../common/log.h"
#include "../../common/overlay.h"
#include "../../common/strutil.h"
#include "../../common/ui/image.h"
#include "../../common/util.h"
#include "overlay_bridge.h"
#include "../settings/settings.h"

static char catalogue_overlay_path[MAX_BUFFER_SIZE];

static SDL_Texture *current_overlay_tex = NULL;
static uint8_t current_overlay_opacity = 0;

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

    if (load_image_catalogue(
            catalogue_name, program, program, "", "", "overlay", catalogue_overlay_path, sizeof(catalogue_overlay_path)
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
        default:
            break;
    }
}

void overlay_bridge_render(SDL_Renderer *renderer, const int canvas_w, const int canvas_h) {
    if (!current_overlay_tex) return;

    int tex_w = 0, tex_h = 0;
    SDL_QueryTexture(current_overlay_tex, NULL, NULL, &tex_w, &tex_h);

    const SDL_Rect dst = {(canvas_w - tex_w) / 2, (canvas_h - tex_h) / 2, tex_w, tex_h};

    SDL_SetTextureBlendMode(current_overlay_tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(current_overlay_tex, current_overlay_opacity);
    SDL_RenderCopy(renderer, current_overlay_tex, NULL, &dst);
}

void overlay_bridge_shutdown(void) {
    if (current_overlay_tex) {
        SDL_DestroyTexture(current_overlay_tex);
        current_overlay_tex = NULL;
    }
}
