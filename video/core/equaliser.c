#include "equaliser.h"

#include <SDL2/SDL.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <module/muxshare.h>
#include <common/storage/fileio.h>
#include "paths.h"

#define EQ_PATH          WASABI_SHARE_PATH "equaliser/"
#define EQ_EXTENSION     ".eq"
#define EQ_USER_LIMIT    64
#define EQ_NAME_LIMIT    64
#define EQ_CHANNELS      8
#define EQ_PEAK_Q        1.41f
#define EQ_CLIP_KNEE     0.9f
#define EQ_PI            3.14159265358979323846f
#define EQ_PRESET_PREFIX "preset:"
#define EQ_USER_PREFIX   "user:"

typedef struct {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
} eq_biquad;

typedef struct {
    const char *key;
    int gains[WASABI_EQ_GAINS];
} eq_preset;

static const float band_frequencies[WASABI_EQ_BANDS] = {31.0f,   62.0f,   125.0f,  250.0f,  500.0f,
                                                        1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f};
static const char *const band_labels[WASABI_EQ_BANDS] = {"31 Hz", "62 Hz", "125 Hz", "250 Hz", "500 Hz",
                                                         "1 kHz", "2 kHz", "4 kHz",  "8 kHz",  "16 kHz"};

static const eq_preset presets[] = {
    {"flat", {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"bass_boost", {-4, 6, 5, 4, 2, 0, 0, 0, 0, 0, 0}},
    {"treble_boost", {-4, 0, 0, 0, 0, 0, 0, 1, 3, 5, 6}},
    {"vocal", {-2, -2, -2, -1, 1, 3, 4, 4, 2, 0, -1}},
    {"rock", {-3, 4, 3, 2, -1, -2, -1, 1, 3, 4, 4}},
    {"pop", {-2, -1, 0, 2, 3, 4, 3, 1, 0, -1, -1}},
    {"jazz", {-2, 3, 2, 1, 2, -1, -1, 0, 1, 2, 3}},
    {"classical", {-2, 3, 2, 1, 0, 0, 0, -1, -1, 2, 3}},
    {"electronic", {-4, 5, 4, 1, 0, -2, 1, 0, 2, 4, 5}},
    {"spoken_word", {-2, -4, -3, -1, 1, 3, 4, 3, 1, -1, -3}},
    {"small_speakers", {-4, -6, -4, 1, 3, 3, 2, 1, 1, 2, 2}},
    {"headphones", {-2, 3, 2, 0, -1, -1, 0, 1, 2, 3, 2}},
    {"loudness", {-4, 5, 4, 2, 0, -1, -1, 0, 2, 4, 4}},
    {"late_night", {0, -4, -3, -2, 0, 1, 2, 1, 0, -2, -3}},
};

#define EQ_PRESET_COUNT ((int) (sizeof(presets) / sizeof(presets[0])))

static int gains[WASABI_EQ_GAINS];
static SDL_atomic_t generation;

static int applied_generation = -1;
static int applied_rate;
static int active_bands[WASABI_EQ_BANDS];
static int active_count;
static float preamp_scale = 1.0f;
static int bypass = 1;
static eq_biquad filters[WASABI_EQ_BANDS];
static float state[WASABI_EQ_BANDS][EQ_CHANNELS][2];

static char user_names[EQ_USER_LIMIT][EQ_NAME_LIMIT];
static int user_count;

static const char *preset_label(const int index) {
    static const char *const fallback = "";
    const char *labels[EQ_PRESET_COUNT] = {
        lang.muxmedia.eq_flat,        lang.muxmedia.eq_bass_boost,     lang.muxmedia.eq_treble_boost,
        lang.muxmedia.eq_vocal,       lang.muxmedia.eq_rock,           lang.muxmedia.eq_pop,
        lang.muxmedia.eq_jazz,        lang.muxmedia.eq_classical,      lang.muxmedia.eq_electronic,
        lang.muxmedia.eq_spoken_word, lang.muxmedia.eq_small_speakers, lang.muxmedia.eq_headphones,
        lang.muxmedia.eq_loudness,    lang.muxmedia.eq_late_night,
    };
    return index >= 0 && index < EQ_PRESET_COUNT ? labels[index] : fallback;
}

static void set_text(char *field, const size_t size, const char *prefix, const char *text) {
    memset(field, 0, size);
    snprintf(field, size, "%s%s", prefix, text);
}

static int clamp_gain(const int value) {
    const int limit = WASABI_EQ_LIMIT * WASABI_EQ_STEPS;
    return value < -limit ? -limit : value > limit ? limit : value;
}

void wasabi_eq_format(const int steps, char *value, const size_t size) {
    const int whole = abs(steps) / WASABI_EQ_STEPS;
    const int part = abs(steps) % WASABI_EQ_STEPS * (100 / WASABI_EQ_STEPS);
    const char *sign = steps > 0 ? "+" : steps < 0 ? "-" : "";
    if (!part)
        snprintf(value, size, "%s%d", sign, whole);
    else if (part % 10 == 0)
        snprintf(value, size, "%s%d.%d", sign, whole, part / 10);
    else
        snprintf(value, size, "%s%d.%02d", sign, whole, part);
}

static size_t append_gains(char *text, const size_t size, size_t used) {
    for (int index = 0; index < WASABI_EQ_GAINS && used < size; index++) {
        char value[16];
        wasabi_eq_format(gains[index], value, sizeof(value));
        used += (size_t) snprintf(text + used, size - used, index ? ",%s" : "%s", value);
    }
    return used;
}

static void store_gains(void) {
    char text[MAX_BUFFER_SIZE];
    text[0] = '\0';
    append_gains(text, sizeof(text), 0);
    int flat = 1;
    for (int index = 0; index < WASABI_EQ_GAINS; index++)
        if (gains[index]) flat = 0;
    set_text(config.video.equaliser, sizeof(config.video.equaliser), "", flat ? "" : text);
    SDL_AtomicAdd(&generation, 1);
}

static void parse_gains(const char *text, int *out) {
    memset(out, 0, sizeof(int) * WASABI_EQ_GAINS);
    if (!text) return;
    const char *cursor = text;
    for (int index = 0; index < WASABI_EQ_GAINS && *cursor; index++) {
        char *end = NULL;
        const double value = strtod(cursor, &end);
        if (end == cursor) break;
        out[index] = clamp_gain((int) lround(value * WASABI_EQ_STEPS));
        cursor = *end == ',' ? end + 1 : end;
    }
}

void wasabi_eq_changed(void) {
    parse_gains(config.video.equaliser, gains);
    SDL_AtomicAdd(&generation, 1);
}

int wasabi_eq_gain(const int index) {
    return index >= 0 && index < WASABI_EQ_GAINS ? gains[index] : 0;
}

int wasabi_eq_set_gain(const int index, const int value) {
    if (index < 0 || index >= WASABI_EQ_GAINS) return 0;
    const int clamped = clamp_gain(value);
    if (clamped == gains[index]) return 0;
    gains[index] = clamped;
    set_text(config.video.equaliser_profile, sizeof(config.video.equaliser_profile), "", "");
    store_gains();
    return 1;
}

const char *wasabi_eq_band_label(const int band) {
    return band >= 0 && band < WASABI_EQ_BANDS ? band_labels[band] : "";
}

static eq_biquad design_band(const int band, const float decibels, const int rate) {
    float frequency = band_frequencies[band];
    if (frequency > (float) rate * 0.45f) frequency = (float) rate * 0.45f;
    const float amplitude = powf(10.0f, decibels / 40.0f);
    const float omega = 2.0f * EQ_PI * frequency / (float) rate;
    const float cosine = cosf(omega);
    const float sine = sinf(omega);
    float b0, b1, b2, a0, a1, a2;

    if (band == 0 || band == WASABI_EQ_BANDS - 1) {
        const float alpha = sine / 2.0f * sqrtf(2.0f);
        const float root = 2.0f * sqrtf(amplitude) * alpha;
        const float up = amplitude + 1.0f;
        const float down = amplitude - 1.0f;
        if (band == 0) {
            b0 = amplitude * (up - down * cosine + root);
            b1 = 2.0f * amplitude * (down - up * cosine);
            b2 = amplitude * (up - down * cosine - root);
            a0 = up + down * cosine + root;
            a1 = -2.0f * (down + up * cosine);
            a2 = up + down * cosine - root;
        } else {
            b0 = amplitude * (up + down * cosine + root);
            b1 = -2.0f * amplitude * (down + up * cosine);
            b2 = amplitude * (up + down * cosine - root);
            a0 = up - down * cosine + root;
            a1 = 2.0f * (down - up * cosine);
            a2 = up - down * cosine - root;
        }
    } else {
        const float alpha = sine / (2.0f * EQ_PEAK_Q);
        b0 = 1.0f + alpha * amplitude;
        b1 = -2.0f * cosine;
        b2 = 1.0f - alpha * amplitude;
        a0 = 1.0f + alpha / amplitude;
        a1 = -2.0f * cosine;
        a2 = 1.0f - alpha / amplitude;
    }

    return (eq_biquad) {b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

static void prepare(const int rate, const int current) {
    int wanted[WASABI_EQ_GAINS];
    memcpy(wanted, gains, sizeof(wanted));

    int previous[WASABI_EQ_BANDS];
    memcpy(previous, active_bands, sizeof(previous));
    memset(active_bands, 0, sizeof(active_bands));
    active_count = 0;
    for (int band = 0; band < WASABI_EQ_BANDS; band++) {
        if (!wanted[band + 1]) continue;
        if (!previous[band] || rate != applied_rate) memset(state[band], 0, sizeof(state[band]));
        filters[band] = design_band(band, (float) wanted[band + 1] / WASABI_EQ_STEPS, rate);
        active_bands[band] = 1;
        active_count++;
    }
    preamp_scale = powf(10.0f, (float) wanted[0] / WASABI_EQ_STEPS / 20.0f);
    bypass = !active_count && !wanted[0];
    applied_rate = rate;
    applied_generation = current;
}

static float soft_clip(const float sample) {
    const float magnitude = fabsf(sample);
    if (magnitude <= EQ_CLIP_KNEE) return sample;
    const float over = (magnitude - EQ_CLIP_KNEE) / (1.0f - EQ_CLIP_KNEE);
    const float shaped = EQ_CLIP_KNEE + (1.0f - EQ_CLIP_KNEE) * (over / (1.0f + over));
    return sample < 0.0f ? -shaped : shaped;
}

void wasabi_eq_process(float *samples, const int frames, const int channels, const int rate) {
    if (!samples || frames <= 0 || channels <= 0 || rate <= 0) return;
    const int current = SDL_AtomicGet(&generation);
    if (current != applied_generation || rate != applied_rate) prepare(rate, current);
    if (bypass) return;

    const int used = channels < EQ_CHANNELS ? channels : EQ_CHANNELS;
    for (int band = 0; band < WASABI_EQ_BANDS; band++) {
        if (!active_bands[band]) continue;
        const eq_biquad filter = filters[band];
        for (int channel = 0; channel < used; channel++) {
            float z1 = state[band][channel][0];
            float z2 = state[band][channel][1];
            float *sample = samples + channel;
            for (int frame = 0; frame < frames; frame++, sample += channels) {
                const float input = *sample;
                const float output = filter.b0 * input + z1;
                z1 = filter.b1 * input - filter.a1 * output + z2;
                z2 = filter.b2 * input - filter.a2 * output;
                *sample = output;
            }
            state[band][channel][0] = z1;
            state[band][channel][1] = z2;
        }
    }

    const size_t total = (size_t) frames * (size_t) channels;
    for (size_t index = 0; index < total; index++)
        samples[index] = soft_clip(samples[index] * preamp_scale);
}

static int compare_names(const void *left, const void *right) {
    return strcasecmp((const char *) left, (const char *) right);
}

void wasabi_eq_profiles_refresh(void) {
    user_count = 0;
    DIR *directory = opendir(EQ_PATH);
    if (!directory) return;
    const struct dirent *entry;
    while ((entry = readdir(directory)) && user_count < EQ_USER_LIMIT) {
        const char *dot = strrchr(entry->d_name, '.');
        if (entry->d_name[0] == '.' || !dot || strcasecmp(dot, EQ_EXTENSION) != 0) continue;
        const size_t length = (size_t) (dot - entry->d_name);
        if (!length || length >= EQ_NAME_LIMIT) continue;
        memcpy(user_names[user_count], entry->d_name, length);
        user_names[user_count][length] = '\0';
        user_count++;
    }
    closedir(directory);
    qsort(user_names, (size_t) user_count, sizeof(user_names[0]), compare_names);
}

int wasabi_eq_profile_count(void) {
    return EQ_PRESET_COUNT + user_count;
}

const char *wasabi_eq_profile_name(const int index) {
    if (index < EQ_PRESET_COUNT) return preset_label(index);
    return index - EQ_PRESET_COUNT < user_count ? user_names[index - EQ_PRESET_COUNT] : "";
}

int wasabi_eq_profile_current(void) {
    const char *profile = config.video.equaliser_profile;
    if (!profile[0]) {
        for (int index = 0; index < WASABI_EQ_GAINS; index++)
            if (gains[index]) return -1;
        return 0;
    }
    if (strncmp(profile, EQ_PRESET_PREFIX, strlen(EQ_PRESET_PREFIX)) == 0) {
        for (int index = 0; index < EQ_PRESET_COUNT; index++)
            if (strcmp(profile + strlen(EQ_PRESET_PREFIX), presets[index].key) == 0) return index;
    } else if (strncmp(profile, EQ_USER_PREFIX, strlen(EQ_USER_PREFIX)) == 0) {
        for (int index = 0; index < user_count; index++)
            if (strcmp(profile + strlen(EQ_USER_PREFIX), user_names[index]) == 0) return EQ_PRESET_COUNT + index;
    }
    return -1;
}

void wasabi_eq_profile_label(char *value, const size_t size) {
    const int current = wasabi_eq_profile_current();
    snprintf(value, size, "%s", current < 0 ? lang.muxmedia.equaliser_custom : wasabi_eq_profile_name(current));
}

static void user_path(const char *name, char *path, const size_t size) {
    snprintf(path, size, "%s%s%s", EQ_PATH, name, EQ_EXTENSION);
}

static int load_user(const char *name, int *out) {
    char path[PATH_MAX];
    user_path(name, path, sizeof(path));
    char *text = read_all_char_from(path);
    if (!text) return 0;
    char *line = strstr(text, "gains=");
    parse_gains(line ? line + 6 : text, out);
    free(text);
    return 1;
}

static int apply_profile(const int index) {
    int wanted[WASABI_EQ_GAINS];
    if (index < EQ_PRESET_COUNT) {
        for (int gain = 0; gain < WASABI_EQ_GAINS; gain++)
            wanted[gain] = presets[index].gains[gain] * WASABI_EQ_STEPS;
        set_text(
            config.video.equaliser_profile, sizeof(config.video.equaliser_profile), EQ_PRESET_PREFIX, presets[index].key
        );
    } else {
        const char *name = user_names[index - EQ_PRESET_COUNT];
        if (!load_user(name, wanted)) return 0;
        set_text(config.video.equaliser_profile, sizeof(config.video.equaliser_profile), EQ_USER_PREFIX, name);
    }
    memcpy(gains, wanted, sizeof(gains));
    store_gains();
    if (index == 0) set_text(config.video.equaliser_profile, sizeof(config.video.equaliser_profile), "", "");
    return 1;
}

int wasabi_eq_profile_cycle(const int direction) {
    const int count = wasabi_eq_profile_count();
    if (count < 1) return 0;
    int current = wasabi_eq_profile_current();
    if (current < 0) current = direction > 0 ? -1 : 0;
    int next = (current + (direction < 0 ? -1 : 1) + count) % count;
    for (int attempt = 0; attempt < count; attempt++) {
        if (apply_profile(next)) return 1;
        next = (next + (direction < 0 ? -1 : 1) + count) % count;
    }
    return 0;
}

static void clean_name(const char *name, char *out, const size_t size) {
    size_t used = 0;
    for (const char *cursor = name; *cursor && used + 1 < size; cursor++) {
        const unsigned char character = (unsigned char) *cursor;
        if (character < 0x20 || strchr("/\\:*?\"<>|", character)) continue;
        out[used++] = (char) character;
    }
    while (used && (out[used - 1] == ' ' || out[used - 1] == '.'))
        used--;
    out[used] = '\0';
    size_t start = 0;
    while (out[start] == ' ' || out[start] == '.')
        start++;
    if (start) memmove(out, out + start, used - start + 1);
}

int wasabi_eq_profile_save(const char *name) {
    char clean[EQ_NAME_LIMIT];
    clean_name(name ? name : "", clean, sizeof(clean));
    if (!clean[0]) return 0;

    create_directories(EQ_PATH, 0);
    char path[PATH_MAX];
    user_path(clean, path, sizeof(path));
    char text[MAX_BUFFER_SIZE];
    size_t used = (size_t) snprintf(text, sizeof(text), "gains=");
    used = append_gains(text, sizeof(text), used);
    if (used < sizeof(text)) snprintf(text + used, sizeof(text) - used, "\n");
    if (!write_text_to_file_atomic(path, CHAR, text)) return 0;

    wasabi_eq_profiles_refresh();
    set_text(config.video.equaliser_profile, sizeof(config.video.equaliser_profile), EQ_USER_PREFIX, clean);
    return 1;
}

int wasabi_eq_profile_deletable(void) {
    return wasabi_eq_profile_current() >= EQ_PRESET_COUNT;
}

int wasabi_eq_profile_delete(void) {
    const int current = wasabi_eq_profile_current();
    if (current < EQ_PRESET_COUNT) return 0;
    char path[PATH_MAX];
    user_path(user_names[current - EQ_PRESET_COUNT], path, sizeof(path));
    if (unlink(path) != 0) return 0;
    set_text(config.video.equaliser_profile, sizeof(config.video.equaliser_profile), "", "");
    wasabi_eq_profiles_refresh();
    return 1;
}
