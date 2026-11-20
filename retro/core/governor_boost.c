#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <common/platform/device.h>
#include <common/runtime/exec.h>
#include <common/storage/fileio.h>
#include <common/runtime/init.h>
#include <common/runtime/log.h>
#include <common/platform/cpu_profile.h>
#include "governor_boost.h"

#define GOVERNOR_NAME_MAX 64

#define GAMEPLAY_BOOST_RATIO   0.99
#define GAMEPLAY_ENTER_WINDOWS 2
#define GAMEPLAY_EXIT_WINDOWS  8

#define GAMEPLAY_CORE_PRESSURE_RATIO  0.75
#define GAMEPLAY_CORE_PRESSURE_FRAMES 4

#define CADENCE_WINDOW_FRAMES 120
#define CADENCE_LATE_RATIO    1.15
#define CADENCE_RAISE_LATE    6
#define CADENCE_LOWER_WINDOWS 4

static unsigned boost_depth = 0;
static int governor_changed = 0;
static int cleanup_registered = 0;
static char saved_governor[GOVERNOR_NAME_MAX] = "";
static int gameplay_boost_active = 0;
static unsigned gameplay_miss_windows = 0;
static unsigned gameplay_stable_windows = 0;
static unsigned gameplay_pressure_frames = 0;

static struct cpu_policy cadence_policies[CPU_POLICY_MAX];
static unsigned cadence_saved_min[CPU_POLICY_MAX];
static unsigned cadence_saved_max[CPU_POLICY_MAX];
static int cadence_policy_count = -1;
static int cadence_step = -1;
static unsigned cadence_frames = 0;
static unsigned cadence_late = 0;
static unsigned cadence_quiet_windows = 0;

static int read_governor(char out[GOVERNOR_NAME_MAX]) {
    out[0] = '\0';
    if (!*device.cpu.governor) return -1;

    char *raw = read_all_char_from(device.cpu.governor);
    if (!raw || !*raw) {
        free(raw);
        return -1;
    }

    char *begin = raw;
    while (*begin && isspace((unsigned char) *begin))
        begin++;

    char *end = begin + strlen(begin);
    while (end > begin && isspace((unsigned char) end[-1]))
        end--;
    *end = '\0';

    if (!*begin) {
        free(raw);
        return -1;
    }

    snprintf(out, GOVERNOR_NAME_MAX, "%s", begin);
    free(raw);
    return 0;
}

void governor_boost_begin(const char *reason) {
    if (!cleanup_registered) {
        if (atexit(governor_boost_shutdown) == 0) cleanup_registered = 1;
    }
    if (boost_depth++ > 0) return;

    governor_changed = 0;
    saved_governor[0] = '\0';
    if (read_governor(saved_governor) != 0) {
        LOG_WARN(mux_module, "Governor boost unavailable: could not read current governor");
        return;
    }

    if (strcmp(saved_governor, "performance") == 0) {
        LOG_DEBUG(mux_module, "Governor boost already satisfied%s%s", reason ? " for " : "", reason ? reason : "");
        return;
    }

    if (set_scaling_governor("performance", 0) == 0) {
        governor_changed = 1;
        LOG_INFO(
            mux_module, "Governor boost: %s -> performance%s%s", saved_governor, reason ? " for " : "",
            reason ? reason : ""
        );
    }
}

void governor_boost_end(void) {
    if (boost_depth == 0) {
        LOG_WARN(mux_module, "Unbalanced governor boost end ignored");
        return;
    }
    if (--boost_depth > 0) return;

    if (governor_changed) {
        char current[GOVERNOR_NAME_MAX];
        if (read_governor(current) == 0 && strcmp(current, "performance") == 0) {
            if (set_scaling_governor(saved_governor, 0) == 0)
                LOG_INFO(mux_module, "Governor boost: restored %s", saved_governor);
        } else {
            LOG_INFO(mux_module, "Governor boost: governor changed externally; leaving it untouched");
        }
    }

    governor_changed = 0;
    saved_governor[0] = '\0';
}

void governor_boost_gameplay_update(const double observed_fps, const double target_fps, const int force) {
    if (target_fps <= 0.0 || observed_fps < 0.0) return;

    const int missed = observed_fps < target_fps * GAMEPLAY_BOOST_RATIO;
    const int stable = !missed;

    if (!gameplay_boost_active) {
        gameplay_stable_windows = 0;
        gameplay_miss_windows = missed ? gameplay_miss_windows + 1 : 0;

        if (!force && gameplay_miss_windows < GAMEPLAY_ENTER_WINDOWS) return;

        gameplay_miss_windows = 0;
        gameplay_boost_active = 1;
        governor_boost_begin(force ? "fast forward" : "sustained frame misses");
        return;
    }

    gameplay_miss_windows = 0;
    if (force) {
        gameplay_stable_windows = 0;
        return;
    }

    gameplay_stable_windows = stable ? gameplay_stable_windows + 1 : 0;
    if (gameplay_stable_windows < GAMEPLAY_EXIT_WINDOWS) return;

    LOG_INFO(mux_module, "Governor boost: gameplay stable; restoring the session governor");
    gameplay_boost_active = 0;
    gameplay_stable_windows = 0;
    governor_boost_end();
}

void governor_boost_gameplay_pressure(const double core_ms, const double frame_budget_ms) {
    if (core_ms <= 0.0 || frame_budget_ms <= 0.0) {
        gameplay_pressure_frames = 0;
        return;
    }

    if (core_ms < frame_budget_ms * GAMEPLAY_CORE_PRESSURE_RATIO) {
        gameplay_pressure_frames = 0;
        return;
    }

    if (gameplay_boost_active) {
        gameplay_pressure_frames = 0;
        gameplay_stable_windows = 0;
        return;
    }

    if (++gameplay_pressure_frames < GAMEPLAY_CORE_PRESSURE_FRAMES) return;

    gameplay_pressure_frames = 0;
    gameplay_miss_windows = 0;
    gameplay_stable_windows = 0;
    gameplay_boost_active = 1;
    governor_boost_begin("sustained core pressure");
}

static unsigned read_khz(const struct cpu_policy *policy, const char *name) {
    char path[CPU_PATH_MAX + 32];
    snprintf(path, sizeof(path), "%s/%s", policy->path, name);
    return (unsigned) read_line_int_from(path, 1);
}

static void write_min_khz(const struct cpu_policy *policy, const unsigned khz) {
    char path[CPU_PATH_MAX + 32];
    snprintf(path, sizeof(path), "%s/scaling_min_freq", policy->path);
    write_text_to_file(path, "w", INT, (int) khz);
}

static void cadence_apply(const int step) {
    for (int i = 0; i < cadence_policy_count; i++) {
        const struct cpu_policy *policy = &cadence_policies[i];
        if (step < 0 || !policy->freq_count) {
            write_min_khz(policy, cadence_saved_min[i]);
            continue;
        }

        const int index = step < policy->freq_count ? step : policy->freq_count - 1;
        unsigned khz = policy->freq[index];
        if (khz > cadence_saved_max[i]) khz = cadence_saved_max[i];
        if (khz < cadence_saved_min[i]) khz = cadence_saved_min[i];
        write_min_khz(policy, khz);
    }
}

static void cadence_reset(void) {
    cadence_frames = 0;
    cadence_late = 0;
    cadence_quiet_windows = 0;
    if (cadence_step < 0) return;

    cadence_step = -1;
    cadence_apply(-1);
    LOG_INFO(mux_module, "Pacing floor removed");
}

void governor_boost_gameplay_cadence(const double interval_ms, const double budget_ms) {
    if (interval_ms <= 0.0 || budget_ms <= 0.0 || gameplay_boost_active) return;

    if (cadence_policy_count < 0) {
        cadence_policy_count = cpu_policies_load(cadence_policies, CPU_POLICY_MAX);
        for (int i = 0; i < cadence_policy_count; i++) {
            cadence_saved_min[i] = read_khz(&cadence_policies[i], "scaling_min_freq");
            cadence_saved_max[i] = read_khz(&cadence_policies[i], "scaling_max_freq");
        }
    }
    if (cadence_policy_count <= 0 || cadence_policies[0].freq_count < 2) return;

    cadence_frames++;
    if (interval_ms > budget_ms * CADENCE_LATE_RATIO) cadence_late++;
    if (cadence_frames < CADENCE_WINDOW_FRAMES) return;

    const int first = cadence_policies[0].freq_count / 2;
    const int top = cadence_policies[0].freq_count - 1;
    const unsigned late = cadence_late;
    cadence_frames = 0;
    cadence_late = 0;

    if (late >= CADENCE_RAISE_LATE) {
        cadence_quiet_windows = 0;
        if (cadence_step >= top) return;
        cadence_step = cadence_step < 0 ? first : cadence_step + 1;
        cadence_apply(cadence_step);
        LOG_INFO(
            mux_module, "Pacing floor raised to %u kHz after %u late frames", cadence_policies[0].freq[cadence_step],
            late
        );
        return;
    }

    if (late || cadence_step < 0 || ++cadence_quiet_windows < CADENCE_LOWER_WINDOWS) return;

    cadence_quiet_windows = 0;
    if (cadence_step <= first) {
        cadence_reset();
        return;
    }
    cadence_apply(--cadence_step);
    LOG_INFO(mux_module, "Pacing floor lowered to %u kHz", cadence_policies[0].freq[cadence_step]);
}

void governor_boost_gameplay_idle(void) {
    cadence_reset();
    gameplay_miss_windows = 0;
    gameplay_stable_windows = 0;
    gameplay_pressure_frames = 0;

    if (!gameplay_boost_active) return;

    gameplay_boost_active = 0;
    governor_boost_end();
}

void governor_boost_shutdown(void) {
    governor_boost_gameplay_idle();
    if (boost_depth == 0) return;

    boost_depth = 1;
    governor_boost_end();
}
