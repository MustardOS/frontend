#include "settings.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include <module/muxshare.h>
#include "assets.h"
#include "session.h"
#include "../core/player.h"
#include "../video/effects.h"

static int save_value(const char *key, const int value) {
    (void) key;
    (void) value;
    return 1;
}

static int audio_content;

void wasabi_settings_set_audio(const int active) {
    audio_content = active != 0;
}

int wasabi_settings_audio_active(void) {
    return audio_content;
}

static int cycle(const int value, const int direction, const int count) {
    return (value + direction + count) % count;
}

static int step(int value, const int direction, const int amount, const int low, const int high) {
    value += direction * amount;
    if (value < low) value = low;
    if (value > high) value = high;
    return value;
}

static const int hotkey_choices[] = {0, 1, 3, 4, 6, 9, 7, 10, 8, 11, 12, 13, 14, 17, 18};

static int cycle_hotkey(const int value, const int direction) {
    const int count = (int) (sizeof(hotkey_choices) / sizeof(hotkey_choices[0]));
    int position = 0;
    while (position < count && hotkey_choices[position] != value)
        position++;
    if (position >= count) position = 0;
    return hotkey_choices[cycle(position, direction, count)];
}

const char *wasabi_setting_label(const wasabi_setting setting) {
    switch (setting) {
        case wasabi_setting_scaling:
            return lang.muxretro.settings_screen.scaling_mode;
        case wasabi_setting_rotation:
            return lang.muxretro.settings_screen.rotate;
        case wasabi_setting_mirrored:
            return lang.muxretro.settings_screen.mirrored;
        case wasabi_setting_aspect:
            return lang.muxretro.settings_screen.aspect_ratio_mode;
        case wasabi_setting_scale:
            return lang.muxretro.settings_screen.integer_scale;
        case wasabi_setting_texture:
            return lang.muxretro.settings_screen.texture_filter;
        case wasabi_setting_border:
            return lang.muxretro.settings_screen.border_colour;
        case wasabi_setting_vignette:
            return lang.muxretro.display_screen.vignette;
        case wasabi_setting_colour_filter:
            return lang.muxretro.display_screen.filter;
        case wasabi_setting_shader:
            return lang.muxretro.display_screen.shaders;
        case wasabi_setting_brightness:
            return lang.muxretro.display_screen.brightness;
        case wasabi_setting_contrast:
            return lang.muxretro.display_screen.contrast;
        case wasabi_setting_saturation:
            return lang.muxretro.display_screen.saturation;
        case wasabi_setting_hue:
            return lang.muxretro.display_screen.hue_shift;
        case wasabi_setting_gamma:
            return lang.muxretro.display_screen.gamma;
        case wasabi_setting_progress_bar:
            return lang.muxmedia.progress_bar;
        case wasabi_setting_visualiser:
            return lang.muxmedia.visualisers;
        case wasabi_setting_artwork_position:
            return lang.muxmedia.cover_art;
        case wasabi_setting_repeat:
            return lang.muxmedia.repeat;
        case wasabi_setting_shuffle:
            return lang.muxmedia.shuffle;
        case wasabi_setting_overlay_source:
            return lang.muxretro.display_screen.overlay;
        case wasabi_setting_overlay_pattern:
            return lang.muxretro.display_screen.overlay_pattern;
        case wasabi_setting_overlay_image:
            return lang.muxretro.overlay_screen.image;
        case wasabi_setting_overlay_opacity:
            return lang.muxretro.display_screen.overlay_opacity;
        case wasabi_setting_overlay_adjustment:
            return lang.muxretro.overlay_screen.adjustment;
        case wasabi_setting_overlay_cropping:
            return lang.muxretro.overlay_screen.cropping;
        case wasabi_setting_overlay_reset:
            return lang.muxretro.overlay_screen.reset;
        case wasabi_setting_viewport_adjustment:
            return lang.muxretro.viewport_screen.adjustment;
        case wasabi_setting_viewport_cropping:
            return lang.muxretro.viewport_screen.cropping;
        case wasabi_setting_viewport_reset:
            return lang.muxretro.viewport_screen.reset;
        case wasabi_setting_playtime:
            return lang.muxretro.settings_screen.show_playtime;
        case wasabi_setting_header:
            return lang.muxretro.settings_screen.header_visibility;
        case wasabi_setting_volume:
            return lang.muxretro.settings_screen.volume;
        case wasabi_setting_sample_rate:
            return lang.muxretro.settings_screen.sample_rate;
        case wasabi_setting_audio_latency:
            return lang.muxretro.settings_screen.audio_latency;
        case wasabi_setting_audio_period:
            return lang.muxretro.settings_screen.audio_period;
        case wasabi_setting_audio_filter:
            return lang.muxretro.settings_screen.audio_filter;
        case wasabi_setting_rate_control:
            return lang.muxretro.settings_screen.audio_rate_control;
        case wasabi_setting_gapless:
            return lang.muxmedia.gapless;
        case wasabi_setting_crossfade:
            return lang.muxmedia.crossfade;
        case wasabi_setting_tracker_loop:
            return lang.muxmedia.tracker_loop;
        case wasabi_setting_fast_forward_mode:
            return lang.muxretro.hotkeys_screen.fast_forward;
        case wasabi_setting_fast_forward_speed:
            return lang.muxretro.hotkeys_screen.ff_speed;
        case wasabi_setting_slow_motion_mode:
            return lang.muxretro.hotkeys_screen.slow_motion;
        case wasabi_setting_slow_motion_speed:
            return lang.muxretro.hotkeys_screen.slowmo_speed;
        case wasabi_setting_hotkey_pause:
            return lang.muxmedia.pause_playback;
        case wasabi_setting_hotkey_save_bookmark:
            return lang.muxmedia.save_bookmark;
        case wasabi_setting_hotkey_load_bookmark:
            return lang.muxmedia.load_bookmark;
        case wasabi_setting_hotkey_seek_back:
            return lang.muxmedia.seek_back;
        case wasabi_setting_hotkey_seek_forward:
            return lang.muxmedia.seek_forward;
        case wasabi_setting_hotkey_seek_back_long:
            return lang.muxmedia.seek_back_long;
        case wasabi_setting_hotkey_seek_forward_long:
            return lang.muxmedia.seek_forward_long;
        case wasabi_setting_hotkey_header:
            return lang.muxretro.hotkeys_screen.toggle_header;
        case wasabi_setting_hotkey_repeat:
            return lang.muxmedia.change_repeat;
        case wasabi_setting_hotkey_shuffle:
            return lang.muxmedia.toggle_shuffle;
        case wasabi_setting_hotkey_fast_forward:
            return lang.muxretro.hotkeys_screen.fast_forward;
        case wasabi_setting_hotkey_slow_motion:
            return lang.muxretro.hotkeys_screen.slow_motion;
        case wasabi_setting_thumbnail:
            return lang.muxmedia.bookmark_thumbnail_size;
        case wasabi_setting_live_quality:
            return lang.wasabi_live_tv_quality;
        case wasabi_setting_live_buffer:
            return lang.wasabi_live_tv_buffer;
        case wasabi_setting_sleep:
            return lang.muxmedia.sleep;
        case wasabi_setting_idle_screensaver:
            return lang.muxmedia.idle_screensaver;
        case wasabi_setting_reset:
            return lang.muxretro.settings_screen.reset;
        default:
            return "";
    }
}

const char *wasabi_setting_glyph(const wasabi_setting setting) {
    static const char *const glyphs[] = {"scaling",        "rotate",
                                         "mirrored",       "aspectratio",
                                         "integerscale",   "texturefilter",
                                         "border",         "border",
                                         "filter",         "shader",
                                         "brightness",     "contrast",
                                         "saturation",     "hue",
                                         "gamma",          "overlay",
                                         "overlaypattern", "overlay",
                                         "overlayopacity", "viewport",
                                         "centrecrop",     "viewportreset",
                                         "viewport",       "centrecrop",
                                         "viewportreset",  "playtime",
                                         "header",         "playtime",
                                         "display",        "artwork",
                                         "repeat",         "shuffle",
                                         "volume",         "samplerate",
                                         "audiolatency",   "audioperiod",
                                         "audiofilter",    "audioratecontrol",
                                         "audio",          "playtime",
                                         "repeat",         "fastforward",
                                         "fastforward",    "slowmotion",
                                         "slowmotion",     "pause",
                                         "quicksave",      "quickload",
                                         "seek",           "seek",
                                         "seek",           "seek",
                                         "toggleheader",   "repeat",
                                         "shuffle",        "fastforward",
                                         "slowmotion",     "state",
                                         "quality",        "memory",
                                         "idle_sleep",     "idle_display",
                                         "reset"};
    _Static_assert(sizeof(glyphs) / sizeof(glyphs[0]) == wasabi_setting_count, "Wasabi setting glyph mismatch");
    return setting >= 0 && setting < wasabi_setting_count ? glyphs[setting] : "settings";
}

static const char *header_name(void) {
    static const char *names[4];
    names[0] = lang.muxretro.settings_screen.header_none;
    names[1] = lang.muxretro.settings_screen.header_clock;
    names[2] = lang.muxretro.settings_screen.header_battery;
    names[3] = lang.muxretro.settings_screen.header_both;
    return names[config.video.header_visibility];
}

void wasabi_setting_value(const wasabi_setting setting, char *value, const size_t size) {
    static const char *const rotation[] = {"0\xC2\xB0", "90\xC2\xB0", "180\xC2\xB0", "270\xC2\xB0"};
    static const char *const sample_rates[] = {"32 kHz", "44.1 kHz", "48 kHz"};
    static const char *const periods[] = {"256", "512", "1024", "2048"};
    static const int rate_control[] = {0, 25, 50, 100, 200};
    const char *scale_names[] = {lang.muxretro.settings_screen.aspect_ratio, lang.muxretro.settings_screen.integer_mode,
                                 lang.muxretro.settings_screen.stretch,      lang.muxretro.settings_screen.full_height,
                                 lang.muxretro.settings_screen.full_width,   lang.muxretro.settings_screen.fit_screen};
    switch (setting) {
        case wasabi_setting_scaling:
            snprintf(value, size, "%s", scale_names[config.video.scaling_mode]);
            break;
        case wasabi_setting_rotation:
            snprintf(value, size, "%s", rotation[config.video.rotation]);
            break;
        case wasabi_setting_mirrored:
            snprintf(value, size, "%s", config.video.mirrored ? lang.generic.enabled : lang.generic.disabled);
            break;
        case wasabi_setting_aspect: {
            const char *names[] = {
                lang.muxretro.settings_screen.auto_rate,   lang.muxretro.settings_screen.ratio_4_3,
                lang.muxretro.settings_screen.ratio_8_7,   lang.muxretro.settings_screen.ratio_16_9,
                lang.muxretro.settings_screen.ratio_16_10, lang.muxretro.settings_screen.pixel_perfect
            };
            snprintf(value, size, "%s", names[config.video.aspect_ratio]);
            break;
        }
        case wasabi_setting_scale:
            if (!config.video.scale_multiplier)
                snprintf(value, size, "%s", lang.muxretro.settings_screen.auto_rate);
            else
                snprintf(value, size, "%d\xC3\x97", config.video.scale_multiplier);
            break;
        case wasabi_setting_texture: {
            const char *names[] = {
                lang.muxretro.settings_screen.nearest,        lang.muxretro.settings_screen.smooth,
                lang.muxretro.settings_screen.scale2_x,       lang.muxretro.settings_screen.scale3_x,
                lang.muxretro.settings_screen.sharp_bilinear, lang.muxretro.settings_screen.scale2_x_smooth,
                lang.muxretro.settings_screen.super_eagle
            };
            snprintf(value, size, "%s", names[config.video.texture_filter]);
            break;
        }
        case wasabi_setting_border: {
            const char *names[] = {
                lang.muxretro.settings_screen.theme, lang.muxretro.settings_screen.black,
                lang.muxretro.settings_screen.dark_grey, lang.muxretro.settings_screen.white
            };
            snprintf(value, size, "%s", names[config.video.border_colour]);
            break;
        }
        case wasabi_setting_colour_filter:
            snprintf(value, size, "%s", wasabi_asset_display_value(wasabi_asset_filter));
            break;
        case wasabi_setting_shader:
            snprintf(value, size, "%s", wasabi_asset_display_value(wasabi_asset_shader));
            break;
        case wasabi_setting_brightness:
            snprintf(value, size, "%+d%%", config.video.brightness);
            break;
        case wasabi_setting_contrast:
            snprintf(value, size, "%d%%", config.video.contrast);
            break;
        case wasabi_setting_saturation:
            snprintf(value, size, "%d%%", config.video.saturation);
            break;
        case wasabi_setting_hue:
            snprintf(value, size, "%+d\xC2\xB0", config.video.hue_shift);
            break;
        case wasabi_setting_gamma:
            snprintf(value, size, "%d%%", config.video.gamma);
            break;
        case wasabi_setting_progress_bar: {
            const char *names[] = {lang.muxmedia.progress_classic,  lang.muxmedia.progress_centre_out,
                                   lang.muxmedia.progress_waveform, lang.muxmedia.progress_segmented,
                                   lang.muxmedia.progress_reverse,  lang.muxmedia.progress_dot_trail,
                                   lang.muxmedia.progress_curve,    lang.muxmedia.progress_position_marker,
                                   lang.muxmedia.progress_comet,    lang.muxmedia.progress_pulse};
            snprintf(value, size, "%s", names[config.video.progress_bar]);
            break;
        }
        case wasabi_setting_visualiser: {
            const char *names[] = {
                lang.generic.disabled,          lang.muxmedia.visualiser_spectrum, lang.muxmedia.visualiser_waveform,
                lang.muxmedia.visualiser_pulse, lang.muxmedia.visualiser_orbit,    lang.muxmedia.visualiser_meter,
                lang.muxmedia.visualiser_phase, lang.muxmedia.visualiser_radial,   lang.muxmedia.visualiser_starfield
            };
            snprintf(value, size, "%s", names[config.video.visualiser]);
            break;
        }
        case wasabi_setting_artwork_position: {
            const char *names[] = {lang.generic.right, lang.generic.left, lang.generic.disabled};
            snprintf(value, size, "%s", names[config.video.artwork_position]);
            break;
        }
        case wasabi_setting_repeat: {
            const char *names[] = {lang.generic.disabled, lang.muxmedia.repeat_one, lang.muxmedia.repeat_all};
            snprintf(value, size, "%s", names[config.video.repeat_mode]);
            break;
        }
        case wasabi_setting_shuffle:
            snprintf(value, size, "%s", config.video.shuffle ? lang.generic.enabled : lang.generic.disabled);
            break;
        case wasabi_setting_overlay_source: {
            const char *names[] = {
                lang.generic.disabled, lang.muxretro.settings_screen.overlay_pattern_mode,
                lang.muxretro.settings_screen.overlay_catalogue_mode,
                lang.muxretro.settings_screen.overlay_downloaded_mode
            };
            snprintf(value, size, "%s", names[config.video.overlay_mode]);
            break;
        }
        case wasabi_setting_overlay_pattern: {
            const char *names[] = {
                lang.muxretro.overlay_screen.checkerboard_1, lang.muxretro.overlay_screen.checkerboard_4,
                lang.muxretro.overlay_screen.diagonal_1,     lang.muxretro.overlay_screen.diagonal_2,
                lang.muxretro.overlay_screen.diagonal_4,     lang.muxretro.overlay_screen.lattice_1,
                lang.muxretro.overlay_screen.lattice_4,      lang.muxretro.overlay_screen.horizontal_1,
                lang.muxretro.overlay_screen.horizontal_2,   lang.muxretro.overlay_screen.horizontal_4,
                lang.muxretro.overlay_screen.vertical_1,     lang.muxretro.overlay_screen.vertical_2,
                lang.muxretro.overlay_screen.vertical_4
            };
            snprintf(value, size, "%s", names[config.video.overlay_pattern % 13]);
            break;
        }
        case wasabi_setting_overlay_image:
            snprintf(value, size, "%s", wasabi_asset_display_value(wasabi_asset_overlay));
            break;
        case wasabi_setting_overlay_opacity:
            snprintf(value, size, "%d%%", config.video.overlay_opacity);
            break;
        case wasabi_setting_playtime:
            snprintf(value, size, "%s", config.video.show_playtime ? lang.generic.enabled : lang.generic.disabled);
            break;
        case wasabi_setting_header:
            snprintf(value, size, "%s", header_name());
            break;
        case wasabi_setting_volume:
            snprintf(value, size, "%d%%", config.video.volume);
            break;
        case wasabi_setting_sample_rate:
            snprintf(value, size, "%s", sample_rates[config.video.sample_rate]);
            break;
        case wasabi_setting_audio_latency: {
            const char *names[] = {
                lang.muxretro.settings_screen.audio_latency_low, lang.muxretro.settings_screen.audio_latency_balanced,
                lang.muxretro.settings_screen.audio_latency_compat
            };
            snprintf(value, size, "%s", names[config.video.audio_latency]);
            break;
        }
        case wasabi_setting_audio_period:
            snprintf(value, size, "%s", periods[config.video.audio_period]);
            break;
        case wasabi_setting_audio_filter: {
            const char *names[] = {
                lang.muxretro.settings_screen.audio_filter_none, lang.muxretro.settings_screen.audio_filter_low_pass,
                lang.muxretro.settings_screen.audio_filter_high_pass
            };
            snprintf(value, size, "%s", names[config.video.audio_filter]);
            break;
        }
        case wasabi_setting_rate_control:
            if (!config.video.rate_control)
                snprintf(value, size, "%s", lang.muxretro.settings_screen.audio_rate_control_off);
            else
                snprintf(value, size, "%.2f%%", (double) rate_control[config.video.rate_control] / 100.0);
            break;
        case wasabi_setting_gapless:
            snprintf(value, size, "%s", config.video.gapless ? lang.generic.enabled : lang.generic.disabled);
            break;
        case wasabi_setting_crossfade:
            if (config.video.crossfade)
                snprintf(value, size, "%d s", config.video.crossfade);
            else
                snprintf(value, size, "%s", lang.generic.disabled);
            break;
        case wasabi_setting_tracker_loop:
            snprintf(value, size, "%s", config.video.tracker_loop ? lang.generic.enabled : lang.generic.disabled);
            break;
        case wasabi_setting_fast_forward_mode:
        case wasabi_setting_slow_motion_mode: {
            const char *names[] = {lang.generic.disabled, lang.generic.hold, lang.generic.press};
            const int mode = setting == wasabi_setting_fast_forward_mode ? config.video.fast_forward_mode
                                                                         : config.video.slow_motion_mode;
            snprintf(value, size, "%s", names[mode]);
            break;
        }
        case wasabi_setting_fast_forward_speed: {
            static const char *const names[] = {"2x", "3x", "4x", "8x"};
            snprintf(value, size, "%s", names[config.video.fast_forward_speed]);
            break;
        }
        case wasabi_setting_slow_motion_speed: {
            static const char *const names[] = {"1/2x", "1/4x", "1/8x"};
            snprintf(value, size, "%s", names[config.video.slow_motion_speed]);
            break;
        }
        case wasabi_setting_hotkey_pause:
        case wasabi_setting_hotkey_save_bookmark:
        case wasabi_setting_hotkey_load_bookmark:
        case wasabi_setting_hotkey_seek_back:
        case wasabi_setting_hotkey_seek_forward:
        case wasabi_setting_hotkey_seek_back_long:
        case wasabi_setting_hotkey_seek_forward_long:
        case wasabi_setting_hotkey_header:
        case wasabi_setting_hotkey_repeat:
        case wasabi_setting_hotkey_shuffle:
        case wasabi_setting_hotkey_fast_forward:
        case wasabi_setting_hotkey_slow_motion: {
            const int values[] = {
                config.video.hotkey_pause,
                config.video.hotkey_save_bookmark,
                config.video.hotkey_load_bookmark,
                config.video.hotkey_seek_back,
                config.video.hotkey_seek_forward,
                config.video.hotkey_seek_back_long,
                config.video.hotkey_seek_forward_long,
                config.video.hotkey_header,
                config.video.hotkey_repeat,
                config.video.hotkey_shuffle,
                config.video.hotkey_fast_forward,
                config.video.hotkey_slow_motion
            };
            const int index = setting - wasabi_setting_hotkey_pause;
            const int menu_combo = setting == wasabi_setting_hotkey_save_bookmark
                                   || setting == wasabi_setting_hotkey_load_bookmark
                                   || setting == wasabi_setting_hotkey_header;
            snprintf(value, size, menu_combo ? "M+%s" : "%s", wasabi_button_name(values[index]));
            break;
        }
        case wasabi_setting_thumbnail: {
            const char *names[] = {
                lang.muxretro.settings_screen.thumbnail_small, lang.muxretro.settings_screen.thumbnail_medium,
                lang.muxretro.settings_screen.thumbnail_large
            };
            snprintf(value, size, "%s", names[config.video.thumbnail_size]);
            break;
        }
        case wasabi_setting_live_quality:
            video_player_live_quality_value(value, size);
            break;
        case wasabi_setting_live_buffer:
            snprintf(value, size, "%d MB", config.wasabi.live_buffer);
            break;
        case wasabi_setting_sleep:
            snprintf(value, size, "%s", config.video.sleep ? lang.generic.enabled : lang.generic.disabled);
            break;
        case wasabi_setting_idle_screensaver:
            snprintf(value, size, "%s", config.video.idle_screensaver ? lang.generic.enabled : lang.generic.disabled);
            break;
        default:
            value[0] = '\0';
            break;
    }
}

int wasabi_setting_can_change(const wasabi_setting setting) {
    if (setting >= wasabi_setting_scaling && setting <= wasabi_setting_border) return 1;
    if (setting >= wasabi_setting_brightness && setting <= wasabi_setting_gamma) return 1;
    if (setting == wasabi_setting_overlay_source || setting == wasabi_setting_overlay_pattern
        || setting == wasabi_setting_overlay_opacity)
        return 1;
    if (setting >= wasabi_setting_playtime && setting <= wasabi_setting_shuffle) return 1;
    if (setting >= wasabi_setting_volume && setting <= wasabi_setting_slow_motion_speed) return 1;
    if (setting >= wasabi_setting_hotkey_pause && setting <= wasabi_setting_hotkey_slow_motion) return 1;
    return setting >= wasabi_setting_thumbnail && setting <= wasabi_setting_idle_screensaver;
}

int wasabi_setting_is_action(const wasabi_setting setting) {
    return setting == wasabi_setting_vignette || setting == wasabi_setting_colour_filter
           || setting == wasabi_setting_shader || setting == wasabi_setting_overlay_image
           || setting == wasabi_setting_overlay_adjustment || setting == wasabi_setting_overlay_cropping
           || setting == wasabi_setting_overlay_reset || setting == wasabi_setting_viewport_adjustment
           || setting == wasabi_setting_viewport_cropping || setting == wasabi_setting_viewport_reset
           || setting == wasabi_setting_reset;
}

static int cycle_root_hotkey(const wasabi_setting setting, const int direction) {
    int16_t *fields[] = {
        &config.video.hotkey_pause,
        &config.video.hotkey_save_bookmark,
        &config.video.hotkey_load_bookmark,
        &config.video.hotkey_seek_back,
        &config.video.hotkey_seek_forward,
        &config.video.hotkey_seek_back_long,
        &config.video.hotkey_seek_forward_long,
        &config.video.hotkey_header,
        &config.video.hotkey_repeat,
        &config.video.hotkey_shuffle,
        &config.video.hotkey_fast_forward,
        &config.video.hotkey_slow_motion
    };
    const char *keys[] = {
        "hotkey_pause",        "hotkey_save_bookmark",  "hotkey_load_bookmark",     "hotkey_seek_back",
        "hotkey_seek_forward", "hotkey_seek_back_long", "hotkey_seek_forward_long", "hotkey_header",
        "hotkey_repeat",       "hotkey_shuffle",        "hotkey_fast_forward",      "hotkey_slow_motion"
    };
    const int index = setting - wasabi_setting_hotkey_pause;
    const int previous = *fields[index];
    const int next = cycle_hotkey(previous, direction);
    for (int other = 0; other < 12; other++) {
        if (other == index || *fields[other] != next) continue;
        *fields[other] = (int16_t) previous;
        save_value(keys[other], previous);
        break;
    }
    *fields[index] = (int16_t) next;
    return save_value(keys[index], next);
}

int wasabi_setting_cycle(const wasabi_setting setting, const int direction) {
    if (setting >= wasabi_setting_hotkey_pause && setting <= wasabi_setting_hotkey_slow_motion)
        return cycle_root_hotkey(setting, direction);
    if (setting == wasabi_setting_live_quality) {
        config.wasabi.live_quality = (int16_t) cycle(config.wasabi.live_quality, direction, 4);
        return save_value("live_quality", config.wasabi.live_quality);
    }
    if (setting == wasabi_setting_live_buffer) {
        static const int sizes[] = {4, 8, 16, 32};
        int index = 0;
        while (index < 4 && sizes[index] != config.wasabi.live_buffer)
            index++;
        if (index >= 4) index = 2;
        config.wasabi.live_buffer = (int16_t) sizes[cycle(index, direction, 4)];
        return save_value("live_buffer", config.wasabi.live_buffer);
    }
    const char *key = NULL;
    int16_t *field = NULL;
    int result = 0;
#define SETTING(member, name, expression)                                                                              \
    do {                                                                                                               \
        key = name;                                                                                                    \
        field = &config.video.member;                                                                                  \
        result = (expression);                                                                                         \
    } while (0)
    switch (setting) {
        case wasabi_setting_scaling:
            SETTING(scaling_mode, "scaling_mode", cycle(config.video.scaling_mode, direction, 6));
            break;
        case wasabi_setting_rotation:
            SETTING(rotation, "rotation", cycle(config.video.rotation, direction, 4));
            break;
        case wasabi_setting_mirrored:
            SETTING(mirrored, "mirrored", !config.video.mirrored);
            break;
        case wasabi_setting_aspect:
            SETTING(aspect_ratio, "aspect_ratio", cycle(config.video.aspect_ratio, direction, 6));
            break;
        case wasabi_setting_scale:
            SETTING(scale_multiplier, "scale_multiplier", cycle(config.video.scale_multiplier, direction, 9));
            break;
        case wasabi_setting_texture:
            SETTING(texture_filter, "texture_filter", cycle(config.video.texture_filter, direction, 7));
            break;
        case wasabi_setting_border:
            SETTING(border_colour, "border_colour", cycle(config.video.border_colour, direction, 4));
            break;
        case wasabi_setting_brightness:
            SETTING(brightness, "brightness", step(config.video.brightness, direction, 5, -100, 100));
            break;
        case wasabi_setting_contrast:
            SETTING(contrast, "contrast", step(config.video.contrast, direction, 5, 0, 200));
            break;
        case wasabi_setting_saturation:
            SETTING(saturation, "saturation", step(config.video.saturation, direction, 5, 0, 200));
            break;
        case wasabi_setting_hue:
            SETTING(hue_shift, "hue_shift", step(config.video.hue_shift, direction, 5, -180, 180));
            break;
        case wasabi_setting_gamma:
            SETTING(gamma, "gamma", step(config.video.gamma, direction, 5, 50, 200));
            break;
        case wasabi_setting_visualiser:
            SETTING(visualiser, "visualiser", cycle(config.video.visualiser, direction, 9));
            break;
        case wasabi_setting_artwork_position:
            SETTING(artwork_position, "artwork_position", cycle(config.video.artwork_position, direction, 3));
            break;
        case wasabi_setting_repeat:
            SETTING(repeat_mode, "repeat_mode", cycle(config.video.repeat_mode, direction, 3));
            break;
        case wasabi_setting_shuffle:
            SETTING(shuffle, "shuffle", !config.video.shuffle);
            break;
        case wasabi_setting_overlay_source:
            SETTING(overlay_mode, "overlay_mode", cycle(config.video.overlay_mode, direction, 4));
            break;
        case wasabi_setting_overlay_pattern:
            SETTING(overlay_pattern, "overlay_pattern", cycle(config.video.overlay_pattern, direction, 13));
            break;
        case wasabi_setting_overlay_opacity:
            SETTING(overlay_opacity, "overlay_opacity", step(config.video.overlay_opacity, direction, 5, 0, 100));
            break;
        case wasabi_setting_playtime:
            SETTING(show_playtime, "show_playtime", !config.video.show_playtime);
            break;
        case wasabi_setting_header:
            SETTING(header_visibility, "header_visibility", cycle(config.video.header_visibility, direction, 4));
            break;
        case wasabi_setting_progress_bar:
            SETTING(progress_bar, "progress_bar", cycle(config.video.progress_bar, direction, 10));
            break;
        case wasabi_setting_volume:
            SETTING(volume, "volume", step(config.video.volume, direction, 5, 0, 100));
            break;
        case wasabi_setting_sample_rate:
            SETTING(sample_rate, "sample_rate", cycle(config.video.sample_rate, direction, 3));
            break;
        case wasabi_setting_audio_latency:
            SETTING(audio_latency, "audio_latency", cycle(config.video.audio_latency, direction, 3));
            break;
        case wasabi_setting_audio_period:
            SETTING(audio_period, "audio_period", cycle(config.video.audio_period, direction, 4));
            break;
        case wasabi_setting_audio_filter:
            SETTING(audio_filter, "audio_filter", cycle(config.video.audio_filter, direction, 3));
            break;
        case wasabi_setting_rate_control:
            SETTING(rate_control, "rate_control", cycle(config.video.rate_control, direction, 5));
            break;
        case wasabi_setting_gapless:
            SETTING(gapless, "gapless", !config.video.gapless);
            break;
        case wasabi_setting_crossfade:
            SETTING(crossfade, "crossfade", cycle(config.video.crossfade, direction, 11));
            break;
        case wasabi_setting_tracker_loop:
            SETTING(tracker_loop, "tracker_loop", !config.video.tracker_loop);
            break;
        case wasabi_setting_fast_forward_mode:
            SETTING(fast_forward_mode, "fast_forward_mode", cycle(config.video.fast_forward_mode, direction, 3));
            break;
        case wasabi_setting_fast_forward_speed:
            SETTING(fast_forward_speed, "fast_forward_speed", cycle(config.video.fast_forward_speed, direction, 4));
            break;
        case wasabi_setting_slow_motion_mode:
            SETTING(slow_motion_mode, "slow_motion_mode", cycle(config.video.slow_motion_mode, direction, 3));
            break;
        case wasabi_setting_slow_motion_speed:
            SETTING(slow_motion_speed, "slow_motion_speed", cycle(config.video.slow_motion_speed, direction, 3));
            break;
        case wasabi_setting_thumbnail:
            SETTING(thumbnail_size, "thumbnail_size", cycle(config.video.thumbnail_size, direction, 3));
            break;
        case wasabi_setting_sleep:
            SETTING(sleep, "sleep", !config.video.sleep);
            break;
        case wasabi_setting_idle_screensaver:
            SETTING(idle_screensaver, "idle_screensaver", !config.video.idle_screensaver);
            break;
        default:
            return 0;
    }
#undef SETTING
    *field = (int16_t) result;
    if (setting == wasabi_setting_sleep || setting == wasabi_setting_idle_screensaver)
        wasabi_session_apply_idle_policy();
    return save_value(key, result);
}

static int save_many(const char *const *keys, const int *values, const int count) {
    int okay = 1;
    for (int index = 0; index < count; index++)
        if (!save_value(keys[index], values[index])) okay = 0;
    return okay;
}

int wasabi_setting_reset_viewport(void) {
    config.video.viewport_x = config.video.viewport_y = 0;
    config.video.viewport_stretch_x = config.video.viewport_stretch_y = 0;
    config.video.viewport_zoom = 100;
    config.video.crop_left = config.video.crop_right = config.video.crop_top = config.video.crop_bottom = 0;
    config.video.viewport_centre_crop = 0;
    const char *keys[] = {"viewport_x",    "viewport_y",          "viewport_stretch_x", "viewport_stretch_y",
                          "viewport_zoom", "crop_left",           "crop_right",         "crop_top",
                          "crop_bottom",   "viewport_centre_crop"};
    const int values[] = {0, 0, 0, 0, 100, 0, 0, 0, 0, 0};
    return save_many(keys, values, 10);
}

int wasabi_setting_reset_overlay(void) {
    config.video.overlay_x = config.video.overlay_y = 0;
    config.video.overlay_stretch_x = config.video.overlay_stretch_y = 0;
    config.video.overlay_zoom = 100;
    config.video.overlay_crop_left = config.video.overlay_crop_right = 0;
    config.video.overlay_crop_top = config.video.overlay_crop_bottom = 0;
    config.video.overlay_centre_crop = 0;
    const char *keys[] = {"overlay_x",           "overlay_y",          "overlay_stretch_x",  "overlay_stretch_y",
                          "overlay_zoom",        "overlay_crop_left",  "overlay_crop_right", "overlay_crop_top",
                          "overlay_crop_bottom", "overlay_centre_crop"};
    const int values[] = {0, 0, 0, 0, 100, 0, 0, 0, 0, 0};
    return save_many(keys, values, 10);
}

const wasabi_setting_section *wasabi_setting_sections(size_t *count) {
    static wasabi_setting_section sections[7];
    if (audio_content) {
        sections[0] = (wasabi_setting_section
        ){lang.muxretro.settings_screen.category_display, wasabi_setting_playtime,
          wasabi_setting_volume - wasabi_setting_playtime};
        sections[1] = (wasabi_setting_section
        ){lang.muxretro.settings_screen.category_sound, wasabi_setting_volume,
          wasabi_setting_hotkey_pause - wasabi_setting_volume};
        sections[2] = (wasabi_setting_section
        ){lang.muxretro.settings_screen.category_input, wasabi_setting_hotkey_pause,
          wasabi_setting_thumbnail - wasabi_setting_hotkey_pause};
        sections[3] = (wasabi_setting_section
        ){lang.muxretro.settings_screen.category_advanced, wasabi_setting_sleep,
          wasabi_setting_count - wasabi_setting_sleep};
        if (count) *count = 4;
        return sections;
    }
    sections[0] = (wasabi_setting_section
    ){lang.muxretro.settings_screen.category_video, wasabi_setting_scaling,
      wasabi_setting_vignette - wasabi_setting_scaling};
    sections[1] = (wasabi_setting_section
    ){lang.muxretro.display, wasabi_setting_vignette, wasabi_setting_overlay_source - wasabi_setting_vignette};
    sections[2] = (wasabi_setting_section
    ){lang.muxretro.display_screen.overlay, wasabi_setting_overlay_source,
      wasabi_setting_viewport_adjustment - wasabi_setting_overlay_source};
    sections[3] = (wasabi_setting_section
    ){lang.muxretro.settings_screen.category_display, wasabi_setting_viewport_adjustment,
      wasabi_setting_volume - wasabi_setting_viewport_adjustment};
    sections[4] = (wasabi_setting_section
    ){lang.muxretro.settings_screen.category_sound, wasabi_setting_volume,
      wasabi_setting_hotkey_pause - wasabi_setting_volume};
    sections[5] = (wasabi_setting_section
    ){lang.muxretro.settings_screen.category_input, wasabi_setting_hotkey_pause,
      wasabi_setting_thumbnail - wasabi_setting_hotkey_pause};
    sections[6] = (wasabi_setting_section
    ){lang.muxretro.settings_screen.category_advanced, wasabi_setting_thumbnail,
      wasabi_setting_count - wasabi_setting_thumbnail};
    if (count) *count = 7;
    return sections;
}

const char *wasabi_page_title(const wasabi_settings_page page) {
    switch (page) {
        case wasabi_page_vignette:
            return lang.muxretro.display_screen.vignette;
        case wasabi_page_overlay_adjustment:
            return lang.muxretro.overlay_screen.adjustment;
        case wasabi_page_overlay_cropping:
            return lang.muxretro.overlay_screen.cropping;
        case wasabi_page_viewport_adjustment:
            return lang.muxretro.viewport_screen.adjustment;
        case wasabi_page_viewport_cropping:
            return lang.muxretro.viewport_screen.cropping;
        case wasabi_page_hotkeys:
            return lang.muxretro.hotkeys;
        case wasabi_page_shader_parameters:
            return lang.muxretro.display_screen.shaders;
        default:
            return lang.muxretro.settings;
    }
}

int wasabi_page_row_count(const wasabi_settings_page page) {
    switch (page) {
        case wasabi_page_vignette:
            return 9;
        case wasabi_page_overlay_adjustment:
        case wasabi_page_overlay_cropping:
        case wasabi_page_viewport_adjustment:
        case wasabi_page_viewport_cropping:
            return 5;
        case wasabi_page_hotkeys:
            return 12;
        case wasabi_page_shader_parameters:
            return video_effects_parameter_count() > 0 ? video_effects_parameter_count() + 1 : 0;
        default:
            return 0;
    }
}

const char *wasabi_page_label(const wasabi_settings_page page, const int row) {
    if (page == wasabi_page_vignette) {
        const char *labels[] = {lang.muxretro.vignette_screen.shape,    lang.muxretro.vignette_screen.scaling,
                                lang.muxretro.vignette_screen.width,    lang.muxretro.vignette_screen.height,
                                lang.muxretro.vignette_screen.offset_x, lang.muxretro.vignette_screen.offset_y,
                                lang.muxretro.vignette_screen.softness, lang.muxretro.vignette_screen.strength,
                                lang.muxretro.vignette_screen.colour};
        return labels[row];
    }
    if (page == wasabi_page_overlay_adjustment || page == wasabi_page_viewport_adjustment) {
        const char *labels[] = {
            lang.muxretro.viewport_screen.offset_x, lang.muxretro.viewport_screen.offset_y,
            lang.muxretro.viewport_screen.stretch_x, lang.muxretro.viewport_screen.stretch_y,
            lang.muxretro.viewport_screen.zoom
        };
        return labels[row];
    }
    if (page == wasabi_page_overlay_cropping || page == wasabi_page_viewport_cropping) {
        const char *labels[] = {
            lang.muxretro.viewport_screen.crop_top, lang.muxretro.viewport_screen.crop_bottom,
            lang.muxretro.viewport_screen.crop_left, lang.muxretro.viewport_screen.crop_right,
            lang.muxretro.viewport_screen.centre_crop
        };
        return labels[row];
    }
    if (page == wasabi_page_hotkeys) {
        const char *labels[] = {
            lang.muxmedia.pause_playback,
            lang.muxmedia.save_bookmark,
            lang.muxmedia.load_bookmark,
            lang.muxmedia.seek_back,
            lang.muxmedia.seek_forward,
            lang.muxmedia.seek_back_long,
            lang.muxmedia.seek_forward_long,
            lang.muxretro.hotkeys_screen.toggle_header,
            lang.muxmedia.change_repeat,
            lang.muxmedia.toggle_shuffle,
            lang.muxretro.hotkeys_screen.fast_forward,
            lang.muxretro.hotkeys_screen.slow_motion
        };
        return labels[row];
    }
    if (page == wasabi_page_shader_parameters)
        return row == video_effects_parameter_count() ? lang.muxretro.shader_screen.reset
                                                      : video_effects_parameter_label(row);
    return "";
}

const char *wasabi_page_glyph(const wasabi_settings_page page, const int row) {
    static const char *const vignette[] = {"border",    "aspectratio",   "viewportx", "viewporty", "viewportx",
                                           "viewporty", "texturefilter", "contrast",  "saturation"};
    static const char *const adjustment[] = {"viewportx", "viewporty", "viewportx", "viewporty", "viewportzoom"};
    static const char *const cropping[] = {"croptop", "cropbottom", "cropleft", "cropright", "centrecrop"};
    static const char *const hotkeys[] = {"pause", "quicksave",    "quickload", "seek",    "seek",        "seek",
                                          "seek",  "toggleheader", "repeat",    "shuffle", "fastforward", "slowmotion"};
    if (page == wasabi_page_vignette) return vignette[row];
    if (page == wasabi_page_overlay_adjustment || page == wasabi_page_viewport_adjustment) return adjustment[row];
    if (page == wasabi_page_overlay_cropping || page == wasabi_page_viewport_cropping) return cropping[row];
    if (page == wasabi_page_hotkeys) return hotkeys[row];
    if (page == wasabi_page_shader_parameters)
        return row == video_effects_parameter_count() ? "viewportreset" : "shader";
    return "settings";
}

const char *wasabi_button_name(const int input) {
    switch (input) {
        case mux_input_a:
            return lang.muxretro.settings_screen.target_a;
        case mux_input_b:
            return lang.muxretro.settings_screen.target_b;
        case mux_input_x:
            return lang.muxretro.settings_screen.target_x;
        case mux_input_y:
            return lang.muxretro.settings_screen.target_y;
        case mux_input_l1:
            return lang.muxretro.settings_screen.target_l1;
        case mux_input_l2:
            return lang.muxretro.settings_screen.target_l2;
        case mux_input_l3:
            return lang.muxretro.settings_screen.target_l3;
        case mux_input_r1:
            return lang.muxretro.settings_screen.target_r1;
        case mux_input_r2:
            return lang.muxretro.settings_screen.target_r2;
        case mux_input_r3:
            return lang.muxretro.settings_screen.target_r3;
        case mux_input_select:
            return lang.muxretro.settings_screen.target_select;
        case mux_input_start:
            return lang.muxretro.settings_screen.target_start;
        case mux_input_dpad_up:
            return lang.muxretro.settings_screen.target_dpad_up;
        case mux_input_dpad_down:
            return lang.muxretro.settings_screen.target_dpad_down;
        case mux_input_dpad_left:
            return lang.muxretro.settings_screen.target_dpad_left;
        case mux_input_dpad_right:
            return lang.muxretro.settings_screen.target_dpad_right;
        default:
            return lang.muxretro.settings_screen.unbound;
    }
}

int wasabi_vignette_secret(const int shape) {
    if (shape < 4 || shape > 5) return 0;
    config.video.vignette_shape = (int16_t) shape;
    return save_value("vignette_shape", shape);
}

void wasabi_page_value(const wasabi_settings_page page, const int row, char *value, const size_t size) {
    if (page == wasabi_page_vignette) {
        if (row == 0) {
            const char *names[] = {
                lang.generic.disabled,
                lang.muxretro.vignette_screen.shape_round,
                lang.muxretro.vignette_screen.shape_square,
                lang.muxretro.vignette_screen.shape_squircle,
                lang.muxretro.vignette_screen.shape_flower,
                lang.muxretro.vignette_screen.shape_triangle
            };
            snprintf(value, size, "%s", names[config.video.vignette_shape]);
        } else if (row == 1) {
            snprintf(
                value, size, "%s",
                config.video.vignette_scaling ? lang.muxretro.vignette_screen.scaling_aspect
                                              : lang.muxretro.vignette_screen.scaling_frame
            );
        } else if (row == 2)
            snprintf(value, size, "%d%%", config.video.vignette_width);
        else if (row == 3)
            snprintf(value, size, "%d%%", config.video.vignette_height);
        else if (row == 4)
            snprintf(value, size, "%+d%%", config.video.vignette_offset_x);
        else if (row == 5)
            snprintf(value, size, "%+d%%", config.video.vignette_offset_y);
        else if (row == 6)
            snprintf(value, size, "%d%%", config.video.vignette_softness);
        else if (row == 7)
            snprintf(value, size, "%d%%", config.video.vignette_strength);
        else {
            const char *names[] = {
                lang.muxretro.vignette_screen.colour_black, lang.muxretro.vignette_screen.colour_white
            };
            snprintf(value, size, "%s", names[config.video.vignette_colour]);
        }
        return;
    }

    if (page == wasabi_page_hotkeys) {
        const int values[] = {
            config.video.hotkey_pause,
            config.video.hotkey_save_bookmark,
            config.video.hotkey_load_bookmark,
            config.video.hotkey_seek_back,
            config.video.hotkey_seek_forward,
            config.video.hotkey_seek_back_long,
            config.video.hotkey_seek_forward_long,
            config.video.hotkey_header,
            config.video.hotkey_repeat,
            config.video.hotkey_shuffle,
            config.video.hotkey_fast_forward,
            config.video.hotkey_slow_motion
        };
        snprintf(value, size, (row == 1 || row == 2 || row == 7) ? "M+%s" : "%s", wasabi_button_name(values[row]));
        return;
    }
    if (page == wasabi_page_shader_parameters) {
        if (row == video_effects_parameter_count())
            value[0] = '\0';
        else
            video_effects_parameter_value(row, value, size);
        return;
    }

    const int overlay = page == wasabi_page_overlay_adjustment || page == wasabi_page_overlay_cropping;
    if (page == wasabi_page_overlay_adjustment || page == wasabi_page_viewport_adjustment) {
        const int values[] = {
            overlay ? config.video.overlay_x : config.video.viewport_x,
            overlay ? config.video.overlay_y : config.video.viewport_y,
            overlay ? config.video.overlay_stretch_x : config.video.viewport_stretch_x,
            overlay ? config.video.overlay_stretch_y : config.video.viewport_stretch_y,
            overlay ? config.video.overlay_zoom : config.video.viewport_zoom
        };
        snprintf(value, size, row == 4 ? "%d%%" : "%+d%%", values[row]);
        return;
    }
    if (page == wasabi_page_overlay_cropping || page == wasabi_page_viewport_cropping) {
        const int values[] = {
            overlay ? config.video.overlay_crop_top : config.video.crop_top,
            overlay ? config.video.overlay_crop_bottom : config.video.crop_bottom,
            overlay ? config.video.overlay_crop_left : config.video.crop_left,
            overlay ? config.video.overlay_crop_right : config.video.crop_right,
            overlay ? config.video.overlay_centre_crop : config.video.viewport_centre_crop
        };
        if (row == 4)
            snprintf(value, size, "%s", values[row] ? lang.generic.enabled : lang.generic.disabled);
        else
            snprintf(value, size, "%d%%", values[row]);
        return;
    }
    value[0] = '\0';
}

static int page_field(
    const wasabi_settings_page page, const int row, int16_t **field, const char **key, int *next, const int direction
) {
#define FIELD(member, name, expression)                                                                                \
    do {                                                                                                               \
        *field = &config.video.member;                                                                                 \
        *key = name;                                                                                                   \
        *next = (expression);                                                                                          \
    } while (0)
    if (page == wasabi_page_vignette) {
        switch (row) {
            case 0:
                FIELD(vignette_shape, "vignette_shape", cycle(config.video.vignette_shape, direction, 4));
                break;
            case 1:
                FIELD(vignette_scaling, "vignette_scaling", !config.video.vignette_scaling);
                break;
            case 2:
                FIELD(vignette_width, "vignette_width", step(config.video.vignette_width, direction, 5, 25, 200));
                break;
            case 3:
                FIELD(vignette_height, "vignette_height", step(config.video.vignette_height, direction, 5, 25, 200));
                break;
            case 4:
                FIELD(
                    vignette_offset_x, "vignette_offset_x",
                    step(config.video.vignette_offset_x, direction, 5, -100, 100)
                );
                break;
            case 5:
                FIELD(
                    vignette_offset_y, "vignette_offset_y",
                    step(config.video.vignette_offset_y, direction, 5, -100, 100)
                );
                break;
            case 6:
                FIELD(
                    vignette_softness, "vignette_softness", step(config.video.vignette_softness, direction, 5, 0, 100)
                );
                break;
            case 7:
                FIELD(
                    vignette_strength, "vignette_strength", step(config.video.vignette_strength, direction, 5, 0, 100)
                );
                break;
            case 8:
                FIELD(vignette_colour, "vignette_colour", cycle(config.video.vignette_colour, direction, 2));
                break;
            default:
                return 0;
        }
        return 1;
    }

    if (page == wasabi_page_hotkeys) {
        int16_t *fields[] = {
            &config.video.hotkey_pause,
            &config.video.hotkey_save_bookmark,
            &config.video.hotkey_load_bookmark,
            &config.video.hotkey_seek_back,
            &config.video.hotkey_seek_forward,
            &config.video.hotkey_seek_back_long,
            &config.video.hotkey_seek_forward_long,
            &config.video.hotkey_header,
            &config.video.hotkey_repeat,
            &config.video.hotkey_shuffle,
            &config.video.hotkey_fast_forward,
            &config.video.hotkey_slow_motion
        };
        const char *keys[] = {
            "hotkey_pause",        "hotkey_save_bookmark",  "hotkey_load_bookmark",     "hotkey_seek_back",
            "hotkey_seek_forward", "hotkey_seek_back_long", "hotkey_seek_forward_long", "hotkey_header",
            "hotkey_repeat",       "hotkey_shuffle",        "hotkey_fast_forward",      "hotkey_slow_motion"
        };
        *field = fields[row];
        *key = keys[row];
        *next = cycle_hotkey(*fields[row], direction);
        return 1;
    }

    const int overlay = page == wasabi_page_overlay_adjustment || page == wasabi_page_overlay_cropping;
    if (page == wasabi_page_overlay_adjustment || page == wasabi_page_viewport_adjustment) {
        if (overlay) {
            switch (row) {
                case 0:
                    FIELD(overlay_x, "overlay_x", step(config.video.overlay_x, direction, 1, -100, 100));
                    break;
                case 1:
                    FIELD(overlay_y, "overlay_y", step(config.video.overlay_y, direction, 1, -100, 100));
                    break;
                case 2:
                    FIELD(
                        overlay_stretch_x, "overlay_stretch_x",
                        step(config.video.overlay_stretch_x, direction, 1, -100, 100)
                    );
                    break;
                case 3:
                    FIELD(
                        overlay_stretch_y, "overlay_stretch_y",
                        step(config.video.overlay_stretch_y, direction, 1, -100, 100)
                    );
                    break;
                case 4:
                    FIELD(overlay_zoom, "overlay_zoom", step(config.video.overlay_zoom, direction, 5, 25, 200));
                    break;
            }
        } else {
            switch (row) {
                case 0:
                    FIELD(viewport_x, "viewport_x", step(config.video.viewport_x, direction, 1, -100, 100));
                    break;
                case 1:
                    FIELD(viewport_y, "viewport_y", step(config.video.viewport_y, direction, 1, -100, 100));
                    break;
                case 2:
                    FIELD(
                        viewport_stretch_x, "viewport_stretch_x",
                        step(config.video.viewport_stretch_x, direction, 1, -100, 100)
                    );
                    break;
                case 3:
                    FIELD(
                        viewport_stretch_y, "viewport_stretch_y",
                        step(config.video.viewport_stretch_y, direction, 1, -100, 100)
                    );
                    break;
                case 4:
                    FIELD(viewport_zoom, "viewport_zoom", step(config.video.viewport_zoom, direction, 5, 25, 200));
                    break;
            }
        }
        return 1;
    }
    if (page == wasabi_page_overlay_cropping || page == wasabi_page_viewport_cropping) {
        if (overlay) {
            switch (row) {
                case 0:
                    FIELD(
                        overlay_crop_top, "overlay_crop_top", step(config.video.overlay_crop_top, direction, 1, 0, 100)
                    );
                    break;
                case 1:
                    FIELD(
                        overlay_crop_bottom, "overlay_crop_bottom",
                        step(config.video.overlay_crop_bottom, direction, 1, 0, 100)
                    );
                    break;
                case 2:
                    FIELD(
                        overlay_crop_left, "overlay_crop_left",
                        step(config.video.overlay_crop_left, direction, 1, 0, 100)
                    );
                    break;
                case 3:
                    FIELD(
                        overlay_crop_right, "overlay_crop_right",
                        step(config.video.overlay_crop_right, direction, 1, 0, 100)
                    );
                    break;
                case 4:
                    FIELD(overlay_centre_crop, "overlay_centre_crop", !config.video.overlay_centre_crop);
                    break;
            }
        } else {
            switch (row) {
                case 0:
                    FIELD(crop_top, "crop_top", step(config.video.crop_top, direction, 1, 0, 100));
                    break;
                case 1:
                    FIELD(crop_bottom, "crop_bottom", step(config.video.crop_bottom, direction, 1, 0, 100));
                    break;
                case 2:
                    FIELD(crop_left, "crop_left", step(config.video.crop_left, direction, 1, 0, 100));
                    break;
                case 3:
                    FIELD(crop_right, "crop_right", step(config.video.crop_right, direction, 1, 0, 100));
                    break;
                case 4:
                    FIELD(viewport_centre_crop, "viewport_centre_crop", !config.video.viewport_centre_crop);
                    break;
            }
        }
        return 1;
    }
#undef FIELD
    return 0;
}

int wasabi_page_cycle(const wasabi_settings_page page, const int row, const int direction) {
    if (page == wasabi_page_shader_parameters)
        return row < video_effects_parameter_count() && video_effects_parameter_cycle(row, direction);
    int16_t *field = NULL;
    const char *key = NULL;
    int next = 0;
    if (!page_field(page, row, &field, &key, &next, direction) || !field || !key) return 0;

    if (page == wasabi_page_hotkeys) {
        int16_t *fields[] = {
            &config.video.hotkey_pause,
            &config.video.hotkey_save_bookmark,
            &config.video.hotkey_load_bookmark,
            &config.video.hotkey_seek_back,
            &config.video.hotkey_seek_forward,
            &config.video.hotkey_seek_back_long,
            &config.video.hotkey_seek_forward_long,
            &config.video.hotkey_header,
            &config.video.hotkey_repeat,
            &config.video.hotkey_shuffle,
            &config.video.hotkey_fast_forward,
            &config.video.hotkey_slow_motion
        };
        const char *keys[] = {
            "hotkey_pause",        "hotkey_save_bookmark",  "hotkey_load_bookmark",     "hotkey_seek_back",
            "hotkey_seek_forward", "hotkey_seek_back_long", "hotkey_seek_forward_long", "hotkey_header",
            "hotkey_repeat",       "hotkey_shuffle",        "hotkey_fast_forward",      "hotkey_slow_motion"
        };
        const int previous = *field;
        for (int index = 0; index < 12; index++) {
            if (fields[index] == field || *fields[index] != next) continue;
            *fields[index] = (int16_t) previous;
            save_value(keys[index], previous);
            break;
        }
    }
    const int previous = *field;
    *field = (int16_t) next;
    int saved = save_value(key, next);
    if (page == wasabi_page_vignette && row == 0 && previous == 0 && next != 0 && config.video.vignette_strength == 0) {
        config.video.vignette_strength = 70;
        saved = save_value("vignette_strength", config.video.vignette_strength) && saved;
    }
    return saved;
}

void wasabi_hotkey_reset(void) {
    config.video.hotkey_pause = 0;
    config.video.hotkey_save_bookmark = 10;
    config.video.hotkey_load_bookmark = 7;
    config.video.hotkey_seek_back = 17;
    config.video.hotkey_seek_forward = 18;
    config.video.hotkey_seek_back_long = 6;
    config.video.hotkey_seek_forward_long = 9;
    config.video.hotkey_header = 3;
    config.video.hotkey_repeat = 13;
    config.video.hotkey_shuffle = 12;
    config.video.hotkey_fast_forward = 11;
    config.video.hotkey_slow_motion = 8;
    const char *keys[] = {
        "hotkey_pause",        "hotkey_save_bookmark",  "hotkey_load_bookmark",     "hotkey_seek_back",
        "hotkey_seek_forward", "hotkey_seek_back_long", "hotkey_seek_forward_long", "hotkey_header",
        "hotkey_repeat",       "hotkey_shuffle",        "hotkey_fast_forward",      "hotkey_slow_motion"
    };
    const int values[] = {0, 10, 7, 17, 18, 6, 9, 3, 13, 12, 11, 8};
    save_many(keys, values, 12);
}
