#include <stdio.h>
#include "../../module/muxshare.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "../settings/submenu.h"

enum { row_volume = 0, row_sample_rate, row_audio_latency, row_audio_period, row_count };

static const char *row_labels[row_count] = {
    lang.muxretro.settings_screen.volume, lang.muxretro.settings_screen.sample_rate,
    lang.muxretro.settings_screen.audio_latency, lang.muxretro.settings_screen.audio_period
};

static const char *row_glyphs[row_count] = {"volume", "samplerate", "audiolatency", "audioperiod"};

static void row_value_text(const int index, char *buf, const size_t buf_len) {
    switch (index) {
        case row_volume:
            snprintf(buf, buf_len, "%d%%", session_settings.volume);
            break;
        case row_sample_rate:
            snprintf(buf, buf_len, "%s", session_settings_sample_rate_name(session_settings.sample_rate));
            break;
        case row_audio_latency:
            snprintf(buf, buf_len, "%s", session_settings_audio_latency_name(session_settings.audio_latency_profile));
            break;
        case row_audio_period:
            snprintf(buf, buf_len, "%s", session_settings_audio_period_name(session_settings.audio_period_frames));
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static void cycle_row(const int index, const int direction) {
    switch (index) {
        case row_volume:
            session_settings_cycle_volume(direction);
            break;
        case row_sample_rate:
            session_settings_cycle_sample_rate(direction);
            break;
        case row_audio_latency:
            session_settings_cycle_audio_latency(direction);
            break;
        case row_audio_period:
            session_settings_cycle_audio_period(direction);
            break;
        default:
            break;
    }
}

static void closed(void) {
    settings_menu_reopen_sound();
}

static submenu self;

static const submenu_def def = {
    .labels = row_labels,
    .glyphs = row_glyphs,
    .row_count = row_count,
    .value_text = row_value_text,
    .cycle = cycle_row,
    .closed = closed,
    .save_title = lang.muxretro.save.sound_title,
    .save_desc = lang.muxretro.save.sound_desc,
};

void sound_menu_init(void) {
    submenu_init(&self, &def);
}

void sound_menu_open(void) {
    submenu_open(&self);
}

int sound_menu_is_active(void) {
    return submenu_is_active(&self);
}

void sound_menu_tick(void) {
    submenu_tick(&self);
}
