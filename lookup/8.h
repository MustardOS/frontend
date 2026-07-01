#pragma once

#include "../common/options.h"
#include "../common/lookup.h"

const lookup_internal_name lookup_8_table[] = {
    {"800fath", "800 Fathoms"},
    {"800fatha", "800 Fathoms (older)"},
    {"88games", "'88 Games"},
    {"8ball", "Video Eight Ball"},
    {"8ball1", "Video Eight Ball (Rev.1)"},
    {"8ballact", "Eight Ball Action (DK conversion)"},
    {"8ballact2", "Eight Ball Action (DKJr conversion)"},
    {"8ballat2", "Eight Ball Action (DKJr conversion)"},
    {"8bpm", "Eight Ball Action (Pac-Man conversion)"},
};

const size_t lookup_8_count = A_SIZE(lookup_8_table);
