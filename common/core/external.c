#include <stdlib.h>
#include "external.h"

const struct ext_core_name ext_core_names[] = {
        {"external",                  "Portmaster"},

        // With the exception of above all the following use the
        // core name without "ext-" as we go past those characters...

        // Media
        {"ffplay",                    "FFPlay"},
        {"mpv-livetv",                "MPV LiveTV"},
        {"mpv-general",               "MPV"},

        // Book Reader
        {"mreader-landscape",         "mReader Ln."},
        {"mreader-portrait",          "mReader Pt."},

        // Java J2ME
        {"freej2me-128",              "J2ME 128x128"},
        {"freej2me-176",              "J2ME 176x208"},
        {"freej2me-240",              "J2ME 240x320"},
        {"freej2me-320",              "J2ME 320x240"},
        {"freej2me-640",              "J2ME 640x360"},

        // Nontondo
        {"drastic",                   "DraStic Advanced"},
        {"drastic-legacy",            "DraStic Legacy"},
        {"mupen64plus-gliden64",      "Mupen64+ Glide"},
        {"mupen64plus-gliden64-full", "Mupen64+ Glide - Full"},
        {"mupen64plus-glidemk2",      "Mupen64+ GlideMK2"},
        {"mupen64plus-glidemk2-full", "Mupen64+ GlideMK2 - Full"},
        {"mupen64plus-rice",          "Mupen64+ Rice"},
        {"mupen64plus-rice-full",     "Mupen64+ Rice - Full"},

        // OpenBOR
        {"openbor4432",               "OpenBOR v4432"},
        {"openbor6412",               "OpenBOR v6412"},
        {"openbor7142",               "OpenBOR v7142"},
        {"openbor7530",               "OpenBOR v7530"},

        // SEGA
        {"yabasanshiro-hle",          "YabaSanshiro HLE"},
        {"yabasanshiro-bios",         "YabaSanshiro BIOS"},

        // PICO-8
        {"pico8-pixel",               "PICO-8 Pixel"},
        {"pico8-scale",               "PICO-8 Scaled"},

        // Misc
        {"scummvm",                   "ScummVM"},
        {"flycast",                   "Flycast"},
        {"ppsspp",                    "PPSSPP"},
        {"amiberry",                  "Amiberry"},
        {"pyxel",                     "Pyxel"},
        {"crisp",                     "Crisp Game Lib"},

        {NULL, NULL}
};
