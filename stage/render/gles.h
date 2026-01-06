#pragma once

#include "../common/common.h"

const struct overlay_resolver GLES_RESOLVER = {
        .render_method = RENDER_GLES,
        .get_dimension = get_dimension,
};
