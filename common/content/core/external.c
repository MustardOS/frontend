#include <stdlib.h>
#include <common/content/core/external.h>

const struct ext_core_name ext_core_names[] = {
    // PortMaster is a special little fella...
    {"external", "PortMaster", stage_overlay_disabled},

    // With the exception of above all the following use the
    // core name without "ext-" as we go past those characters...

    // Media
    {"ffplay", "FFPlay", stage_overlay_disabled},
    {"mpv-livetv", "MPV LiveTV", stage_overlay_disabled},
    {"mpv-general", "MPV", stage_overlay_disabled},

    // Book Reader
    {"mreader-landscape", "mReader Landscape", stage_overlay_disabled},
    {"mreader-portrait", "mReader Portrait", stage_overlay_disabled},

    // Nontondo
    {"drastic", "Drastic - Standalone", stage_overlay_enabled},
    {"drastic-legacy", "Drastic Legacy - Standalone", stage_overlay_enabled},
    {"mupen64plus-gliden64", "Mupen64Plus - Standalone - Glide", stage_overlay_enabled},
    {"mupen64plus-gliden64-full", "Mupen64Plus - Standalone - Glide - Full", stage_overlay_enabled},
    {"mupen64plus-glidemk2", "Mupen64Plus - Standalone - GlideMk2", stage_overlay_enabled},
    {"mupen64plus-glidemk2-full", "Mupen64Plus - Standalone - GlideMk2 - Full", stage_overlay_enabled},
    {"mupen64plus-rice", "Mupen64Plus - Standalone - Rice", stage_overlay_enabled},
    {"mupen64plus-rice-full", "Mupen64Plus - Standalone - Rice - Full", stage_overlay_enabled},

    // OpenBOR
    {"openbor4432", "OpenBOR v4432 - Standalone", stage_overlay_enabled},
    {"openbor6412", "OpenBOR v6412 - Standalone", stage_overlay_enabled},
    {"openbor7142", "OpenBOR v7142 - Standalone", stage_overlay_enabled},
    {"openbor7530", "OpenBOR v7530 - Standalone", stage_overlay_enabled},

    // SEGA
    {"yabasanshiro-hle", "YabaSanshiro - Standalone - HLE", stage_overlay_enabled},
    {"yabasanshiro-bios", "YabaSanshiro", stage_overlay_enabled},

    // PICO-8
    {"pico8-pixel", "PICO-8 - Pixel-Perfect - Standalone", stage_overlay_enabled},
    {"pico8-scale", "PICO-8 - Standalone", stage_overlay_enabled},

    // Misc
    {"azahar", "Azahar - Standalone", stage_overlay_enabled},
    {"scummvm", "ScummVM", stage_overlay_enabled},
    {"flycast", "Flycast", stage_overlay_enabled},
    {"ppsspp", "PPSSPP", stage_overlay_enabled},
    {"pyxel", "Pyxel", stage_overlay_enabled},
    {"crisp", "Crisp Game Lib", stage_overlay_enabled},
    {"terminal", "Terminal", stage_overlay_disabled},
    {"frotz", "Frotz", stage_overlay_enabled},
    {"gen-overlay", "PortOverlay", stage_overlay_enabled},

    {NULL, NULL, stage_overlay_disabled}
};
