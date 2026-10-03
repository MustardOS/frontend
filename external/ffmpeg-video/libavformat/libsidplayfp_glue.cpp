#include "libsidplayfp_glue.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

#include <sidplayfp/SidConfig.h>
#include <sidplayfp/SidTune.h>
#include <sidplayfp/SidTuneInfo.h>
#include <sidplayfp/builders/sidlite.h>
#include <sidplayfp/sidplayfp.h>

static const unsigned int SIDPLAY_CYCLES = 20000;

struct SidplayGlue {
    sidplayfp engine;
    SIDLiteBuilder builder;
    SidTune tune;
    std::vector<short> pending;
    size_t pending_offset;
    size_t pending_count;
    unsigned int song;
    int64_t length_ms;

    SidplayGlue(const char *path)
        : builder("Wasabi"), tune(path), pending_offset(0), pending_count(0), song(0), length_ms(-1) {
    }
};

static void copy_error(char *error, size_t size, const char *text) {
    if (error && size) std::snprintf(error, size, "%s", text ? text : "unknown error");
}

static int64_t parse_length(const char *text) {
    char *end = nullptr;
    const long minutes = std::strtol(text, &end, 10);
    if (end == text || *end != ':') return -1;
    const char *seconds_text = end + 1;
    const long seconds = std::strtol(seconds_text, &end, 10);
    if (end == seconds_text || seconds < 0 || seconds > 59) return -1;
    long milliseconds = 0;
    if (*end == '.') {
        const char *fraction = end + 1;
        long scale = 100;
        for (; *fraction >= '0' && *fraction <= '9' && scale > 0; fraction++, scale /= 10)
            milliseconds += (*fraction - '0') * scale;
    }
    return (static_cast<int64_t>(minutes) * 60 + seconds) * 1000 + milliseconds;
}

static int64_t lookup_length(const char *database, const char *md5, unsigned int song) {
    FILE *file = std::fopen(database, "r");
    if (!file) return -1;
    const size_t md5_length = std::strlen(md5);
    char line[4096];
    int64_t length = -1;
    while (std::fgets(line, sizeof(line), file)) {
        if (std::strncmp(line, md5, md5_length) != 0 || line[md5_length] != '=') continue;
        const char *cursor = line + md5_length + 1;
        for (unsigned int index = 1; *cursor && index < song; index++) {
            cursor = std::strchr(cursor, ' ');
            if (!cursor) break;
            while (*cursor == ' ')
                cursor++;
        }
        if (cursor && *cursor) length = parse_length(cursor);
        break;
    }
    std::fclose(file);
    return length;
}

static int start_tune(SidplayGlue *glue) {
    if (!glue->engine.load(&glue->tune)) return 0;
    glue->engine.initMixer(true);
    const int size = glue->engine.getBufSize(SIDPLAY_CYCLES);
    if (size <= 0) return 0;
    glue->pending.assign(static_cast<size_t>(size), 0);
    glue->pending_offset = 0;
    glue->pending_count = 0;
    return 1;
}

static int refill(SidplayGlue *glue) {
    const int samples = glue->engine.play(SIDPLAY_CYCLES);
    if (samples <= 0) return samples;
    glue->pending_count = glue->engine.mix(glue->pending.data(), static_cast<unsigned int>(samples)) / 2;
    glue->pending_offset = 0;
    return 1;
}

extern "C" SidplayGlue *sidplay_glue_open(
    const char *path, unsigned int song, unsigned int sample_rate, const char *database, char *error, size_t error_size
) {
    SidplayGlue *glue = nullptr;
    try {
        glue = new (std::nothrow) SidplayGlue(path);
        if (!glue) {
            copy_error(error, error_size, "out of memory");
            return nullptr;
        }
        if (!glue->tune.getStatus()) {
            copy_error(error, error_size, glue->tune.statusString());
            delete glue;
            return nullptr;
        }
        glue->song = glue->tune.selectSong(song);

        SidConfig config = glue->engine.config();
        config.frequency = sample_rate;
        config.samplingMethod = SidConfig::INTERPOLATE;
        config.sidEmulation = &glue->builder;
        if (!glue->engine.config(config) || !start_tune(glue)) {
            copy_error(error, error_size, glue->engine.error());
            delete glue;
            return nullptr;
        }

        if (database && database[0]) {
            const char *md5 = glue->tune.createMD5New();
            if (md5 && md5[0]) glue->length_ms = lookup_length(database, md5, glue->song);
        }
        return glue;
    } catch (...) {
        copy_error(error, error_size, "emulation failure");
        delete glue;
        return nullptr;
    }
}

extern "C" void sidplay_glue_close(SidplayGlue *glue) {
    delete glue;
}

extern "C" void sidplay_glue_info(const SidplayGlue *glue, SidplayGlueInfo *info) {
    std::memset(info, 0, sizeof(*info));
    const SidTuneInfo *tune = glue->tune.getInfo();
    if (!tune) return;
    info->songs = tune->songs();
    info->song = glue->song;
    info->chips = tune->sidChips();
    info->length_ms = glue->length_ms;
    info->title = tune->numberOfInfoStrings() > 0 ? tune->infoString(0) : nullptr;
    info->author = tune->numberOfInfoStrings() > 1 ? tune->infoString(1) : nullptr;
    info->released = tune->numberOfInfoStrings() > 2 ? tune->infoString(2) : nullptr;
    info->format = tune->formatString();
    switch (tune->sidModel(0)) {
        case SidTuneInfo::SIDMODEL_6581:
            info->model = "MOS 6581";
            break;
        case SidTuneInfo::SIDMODEL_8580:
            info->model = "MOS 8580";
            break;
        default:
            info->model = nullptr;
            break;
    }
}

extern "C" int sidplay_glue_render(SidplayGlue *glue, int16_t *output, int frames) {
    try {
        int written = 0;
        while (written < frames) {
            if (glue->pending_offset >= glue->pending_count) {
                const int result = refill(glue);
                if (result < 0) return -1;
                if (result == 0) break;
            }
            size_t count = glue->pending_count - glue->pending_offset;
            if (count > static_cast<size_t>(frames - written)) count = static_cast<size_t>(frames - written);
            std::memcpy(
                output + static_cast<size_t>(written) * 2, glue->pending.data() + glue->pending_offset * 2,
                count * 2 * sizeof(int16_t)
            );
            glue->pending_offset += count;
            written += static_cast<int>(count);
        }
        return written;
    } catch (...) {
        return -1;
    }
}

extern "C" int sidplay_glue_restart(SidplayGlue *glue) {
    try {
        glue->tune.selectSong(glue->song);
        return start_tune(glue) ? 0 : -1;
    } catch (...) {
        return -1;
    }
}

extern "C" int64_t sidplay_glue_skip(SidplayGlue *glue, int64_t frames, int (*interrupted)(void *), void *opaque) {
    try {
        const int64_t buffered = static_cast<int64_t>(glue->pending_count - glue->pending_offset);
        if (frames <= buffered) {
            glue->pending_offset += static_cast<size_t>(frames);
            return frames;
        }
        int64_t skipped = buffered;
        glue->pending_offset = glue->pending_count = 0;
        while (skipped < frames) {
            if (interrupted && interrupted(opaque)) break;
            const int samples = glue->engine.play(SIDPLAY_CYCLES);
            if (samples < 0) return -1;
            if (samples == 0) break;
            skipped += samples;
        }
        return skipped;
    } catch (...) {
        return -1;
    }
}
