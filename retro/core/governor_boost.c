#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../common/device.h"
#include "../../common/exec.h"
#include "../../common/fileio.h"
#include "../../common/init.h"
#include "../../common/log.h"
#include "governor_boost.h"

#define GOVERNOR_NAME_MAX 64

#define GAMEPLAY_ENTER_RATIO   0.97
#define GAMEPLAY_EXIT_RATIO    0.99
#define GAMEPLAY_ENTER_WINDOWS 2
#define GAMEPLAY_EXIT_WINDOWS  8

static unsigned boost_depth = 0;
static int governor_changed = 0;
static int cleanup_registered = 0;
static char saved_governor[GOVERNOR_NAME_MAX] = "";
static int gameplay_boost_active = 0;
static unsigned gameplay_miss_windows = 0;
static unsigned gameplay_stable_windows = 0;

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

    const int missed = observed_fps < target_fps * GAMEPLAY_ENTER_RATIO;
    const int stable = observed_fps >= target_fps * GAMEPLAY_EXIT_RATIO;

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

void governor_boost_gameplay_idle(void) {
    gameplay_miss_windows = 0;
    gameplay_stable_windows = 0;

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
