#include <stdlib.h>
#include "external.h"

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
    {"mreader-landscape", "mReader Ln.", stage_overlay_disabled},
    {"mreader-portrait", "mReader Pt.", stage_overlay_disabled},

    // Java J2ME
    {"freej2me-128", "J2ME 128x128", stage_overlay_enabled},
    {"freej2me-176", "J2ME 176x208", stage_overlay_enabled},
    {"freej2me-240", "J2ME 240x320", stage_overlay_enabled},
    {"freej2me-320", "J2ME 320x240", stage_overlay_enabled},
    {"freej2me-640", "J2ME 640x360", stage_overlay_enabled},

    // Nontondo
    {"drastic", "DraStic Advanced", stage_overlay_enabled},
    {"drastic-legacy", "DraStic Legacy", stage_overlay_enabled},
    {"mupen64plus-gliden64", "Mupen64+ Glide", stage_overlay_enabled},
    {"mupen64plus-gliden64-full", "Mupen64+ Glide - Full", stage_overlay_enabled},
    {"mupen64plus-glidemk2", "Mupen64+ GlideMK2", stage_overlay_enabled},
    {"mupen64plus-glidemk2-full", "Mupen64+ GlideMK2 - Full", stage_overlay_enabled},
    {"mupen64plus-rice", "Mupen64+ Rice", stage_overlay_enabled},
    {"mupen64plus-rice-full", "Mupen64+ Rice - Full", stage_overlay_enabled},

    // OpenBOR
    {"openbor4432", "OpenBOR v4432", stage_overlay_enabled},
    {"openbor6412", "OpenBOR v6412", stage_overlay_enabled},
    {"openbor7142", "OpenBOR v7142", stage_overlay_enabled},
    {"openbor7530", "OpenBOR v7530", stage_overlay_enabled},

    // SEGA
    {"yabasanshiro-hle", "YabaSanshiro HLE", stage_overlay_enabled},
    {"yabasanshiro-bios", "YabaSanshiro BIOS", stage_overlay_enabled},

    // PICO-8
    {"pico8-pixel", "PICO-8 Pixel", stage_overlay_enabled},
    {"pico8-scale", "PICO-8 Scaled", stage_overlay_enabled},

    // Misc
    {"azahar", "Azahar", stage_overlay_enabled},
    {"scummvm", "ScummVM", stage_overlay_enabled},
    {"flycast", "Flycast", stage_overlay_enabled},
    {"ppsspp", "PPSSPP", stage_overlay_enabled},
    {"amiberry", "Amiberry", stage_overlay_enabled},
    {"pyxel", "Pyxel", stage_overlay_enabled},
    {"crisp", "Crisp Game Lib", stage_overlay_enabled},
    {"terminal", "Linux Script", stage_overlay_disabled},
    {"frotz", "Frotz - Z-Machine", stage_overlay_enabled},
    {"gen-overlay", "PortMaster + Overlay", stage_overlay_enabled},

    {NULL, NULL, stage_overlay_disabled}
};
