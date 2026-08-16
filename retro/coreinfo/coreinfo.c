#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "../../common/init.h"
#include "../../common/log.h"
#include "../../common/options.h"
#include "../../common/strutil.h"
#include "../core/core.h"
#include "coreinfo.h"

#define CORE_INFO_PATH    OPT_SHARE_PATH "emulator/retroarch/info/"
#define STATE_MAX_DEFAULT (512U * 1024U * 1024U)

#define COREINFO(IDENT) COREINFO_DECLARE(IDENT)
COREINFO_LIST
#undef COREINFO

static const struct coreinfo_override *const registry[] = {
#define COREINFO(IDENT) &coreinfo_##IDENT,
    COREINFO_LIST
#undef COREINFO
};

static char active_name[128];
static int features[coreinfo_feature_count];
static enum coreinfo_feature_reason feature_reasons[coreinfo_feature_count];
static size_t state_max_bytes;
static int state_warmup_frames;
static enum coreinfo_state_load_policy state_load_policy;

static int parse_switch(const char *value, int *result) {
    if (!value) return 0;

    if (strcasecmp(value, "true") == 0 || strcasecmp(value, "enabled") == 0 || strcmp(value, "1") == 0
        || strcasecmp(value, "yes") == 0) {
        *result = 1;
        return 1;
    }
    if (strcasecmp(value, "false") == 0 || strcasecmp(value, "disabled") == 0 || strcmp(value, "0") == 0
        || strcasecmp(value, "no") == 0) {
        *result = 0;
        return 1;
    }
    return 0;
}

static void apply_metadata_value(const char *key, const char *value) {
    int enabled = 0;
    if (strcmp(key, "savestate") == 0 && parse_switch(value, &enabled)) {
        features[coreinfo_feature_save_states] = enabled;
        feature_reasons[coreinfo_feature_save_states] =
            enabled ? coreinfo_reason_available : coreinfo_reason_core_metadata;
    } else if (strcmp(key, "savestate_support") == 0 && parse_switch(value, &enabled)) {
        features[coreinfo_feature_save_states] = enabled;
        feature_reasons[coreinfo_feature_save_states] =
            enabled ? coreinfo_reason_available : coreinfo_reason_core_metadata;
    } else if (strcmp(key, "runahead_support") == 0 && parse_switch(value, &enabled)) {
        features[coreinfo_feature_run_ahead] = enabled;
        feature_reasons[coreinfo_feature_run_ahead] =
            enabled ? coreinfo_reason_available : coreinfo_reason_core_metadata;
    } else if (strcmp(key, "netplay_support") == 0 && parse_switch(value, &enabled)) {
        features[coreinfo_feature_netplay] = enabled;
        feature_reasons[coreinfo_feature_netplay] = enabled ? coreinfo_reason_available : coreinfo_reason_core_metadata;
    } else if (strcmp(key, "savestate_warmup_frames") == 0) {
        char *end = NULL;
        errno = 0;
        const long frames = strtol(value, &end, 10);
        if (!errno && end != value && !*end && frames >= 0 && frames <= 600) state_warmup_frames = (int) frames;
    } else if (strcmp(key, "savestate_max_bytes") == 0) {
        char *end = NULL;
        errno = 0;
        const unsigned long long bytes = strtoull(value, &end, 10);
        if (!errno && end != value && !*end && bytes > 0 && bytes <= SIZE_MAX) state_max_bytes = (size_t) bytes;
    }
}

static void load_metadata(void) {
    char path[MAX_BUFFER_SIZE];
    if (!str_format_checked(path, sizeof(path), "%s%s_libretro.info", CORE_INFO_PATH, active_name)) {
        LOG_WARN(mux_module, "Core information path is too long for '%s'", active_name);
        return;
    }

    FILE *file = fopen(path, "r");
    if (!file) {
        LOG_WARN(mux_module, "Core information is missing for '%s' at '%s'", active_name, path);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char *equals = strchr(line, '=');
        if (!equals) continue;
        *equals = '\0';

        const char *key = str_trim(line);
        char *value = str_trim(equals + 1);
        const size_t length = strlen(value);
        if (length >= 2 && value[0] == '"' && value[length - 1] == '"') {
            value[length - 1] = '\0';
            value++;
        }
        apply_metadata_value(key, value);
    }
    fclose(file);
}

static void apply_override(void) {
    for (size_t index = 0; index < sizeof(registry) / sizeof(registry[0]); index++) {
        const struct coreinfo_override *override = registry[index];
        if (strcmp(override->name, active_name) != 0) continue;

        for (int feature = 0; feature < coreinfo_feature_count; feature++) {
            if (override->features[feature] == COREINFO_INHERIT) continue;
            features[feature] = override->features[feature] != 0;
            feature_reasons[feature] = features[feature] ? coreinfo_reason_available : coreinfo_reason_packaged_rule;
        }

        if (override->state_max_bytes) state_max_bytes = override->state_max_bytes;
        if (override->state_warmup_frames != COREINFO_INHERIT) state_warmup_frames = override->state_warmup_frames;
        state_load_policy = override->state_load_policy;
        LOG_INFO(mux_module, "Applied Pickles core information override for '%s'", active_name);
        return;
    }
}

void coreinfo_init(const char *core_path) {
    memset(active_name, 0, sizeof(active_name));
    for (int feature = 0; feature < coreinfo_feature_count; feature++) {
        features[feature] = 1;
        feature_reasons[feature] = coreinfo_reason_available;
    }
    state_max_bytes = STATE_MAX_DEFAULT;
    state_warmup_frames = 0;
    state_load_policy = coreinfo_state_load_exact;

    if (!core_get_name(core_path, active_name, sizeof(active_name))) {
        LOG_WARN(mux_module, "Could not resolve core information name");
        return;
    }

    load_metadata();
    apply_override();
    if (!features[coreinfo_feature_save_states]) {
        if (features[coreinfo_feature_run_ahead]) {
            features[coreinfo_feature_run_ahead] = 0;
            feature_reasons[coreinfo_feature_run_ahead] = coreinfo_reason_requires_save_states;
        }
        if (features[coreinfo_feature_netplay]) {
            features[coreinfo_feature_netplay] = 0;
            feature_reasons[coreinfo_feature_netplay] = coreinfo_reason_requires_save_states;
        }
    }

    LOG_INFO(
        mux_module, "Core information '%s': states=%s run-ahead=%s netplay=%s state-limit=%zu warm-up=%d", active_name,
        features[coreinfo_feature_save_states] ? "enabled" : "disabled",
        features[coreinfo_feature_run_ahead] ? "enabled" : "disabled",
        features[coreinfo_feature_netplay] ? "enabled" : "disabled", state_max_bytes, state_warmup_frames
    );
}

int coreinfo_feature_enabled(const enum coreinfo_feature feature) {
    return feature < coreinfo_feature_count ? features[feature] : 0;
}

enum coreinfo_feature_reason coreinfo_feature_reason_for(const enum coreinfo_feature feature) {
    return feature < coreinfo_feature_count ? feature_reasons[feature] : coreinfo_reason_core_metadata;
}

size_t coreinfo_state_max_bytes(void) {
    return state_max_bytes;
}

int coreinfo_state_warmup_frames(void) {
    return state_warmup_frames;
}

enum coreinfo_state_load_policy coreinfo_state_load_policy(void) {
    return state_load_policy;
}

const char *coreinfo_name(void) {
    return active_name;
}
