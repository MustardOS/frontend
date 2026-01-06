#pragma once

#include "../common/common.h"

const struct overlay_resolver SDL_RESOLVER = {
        .render_method = RENDER_SDL,
        .get_dimension = get_dimension,
};
