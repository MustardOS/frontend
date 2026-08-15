#include <stdlib.h>
#include "../../common/init.h"
#include "../../common/language.h"
#include "../../common/log.h"
#include "../input/hotkeys.h"
#include "../input/rumble.h"
#include "../coreinfo/coreinfo.h"
#include "../settings/settings.h"
#include "../state/core_state.h"
#include "../video/hw_render.h"
#include "core.h"
#include "muxretro.h"
#include "runahead.h"

static struct core_state_buffer anchor;
static size_t anchor_size = 0;
static int anchor_valid = 0;
static uint64_t anchor_sig = 0;

static int failed = 0;
static int failure_announced = 0;

static void runahead_fail(const char *reason) {
    failed = 1;
    anchor_valid = 0;
    LOG_WARN(mux_module, "Run-ahead disabled for this session: %s", reason);

    if (!failure_announced) {
        failure_announced = 1;
        pause_menu_show_toast(lang.muxretro.settings_screen.run_ahead_failed);
    }
}

#define SERIALIZE_SIZE_REFRESH_FRAMES 120

static uint32_t size_refresh_countdown = 0;

static int ensure_anchor_buf(void) {
    if (anchor_valid && anchor_size > 0 && size_refresh_countdown > 0) {
        size_refresh_countdown--;
        return 1;
    }

    const size_t size = core_state_reported_size(0, "run-ahead size");
    if (size == 0) return 0;

    anchor_size = size;
    size_refresh_countdown = SERIALIZE_SIZE_REFRESH_FRAMES;

    if (size + size / 8 > anchor.capacity) {
        const size_t want = size + size / 8 + (64 << 10);
        uint8_t *grown = realloc(anchor.data, want);
        if (!grown) return 0;
        anchor.data = grown;
        anchor.capacity = want;
    }

    return 1;
}

void runahead_before_frame(const int allow_replay) {
    if (!session_settings.run_ahead || failed) return;

    if (!coreinfo_feature_enabled(coreinfo_feature_run_ahead) || !state_saves_supported()
        || hw_render_bridge_active())
        return;
    if (!current_core.retro_serialize || !current_core.retro_unserialize || !current_core.retro_serialize_size) return;

    if (hotkeys_is_fast_forward_active() || hotkeys_is_slow_motion_active()) {
        anchor_valid = 0;
        return;
    }

    if (!ensure_anchor_buf()) {
        runahead_fail("core reports no usable serialize size");
        return;
    }

    const uint64_t sig = input_bridge_snapshot_signature();

    if (allow_replay && anchor_valid && sig != anchor_sig) {
        if (core_state_restore(anchor.data, anchor.size, 0, "run-ahead restore") == 0) {
            rumble_bridge_set_suppressed(1);
            audio_bridge_set_muted(1);
            video_bridge_set_frame_skip(1);

            current_core.retro_run();

            video_bridge_set_frame_skip(0);
            audio_bridge_discard_sample_fifo();
            audio_bridge_set_muted(0);
            rumble_bridge_set_suppressed(0);
        } else {
            runahead_fail("retro_unserialize failed");
            return;
        }
    }

    if (core_state_capture(&anchor, anchor_size, 0, 0, "run-ahead capture") != 0) {
        size_refresh_countdown = 0;
        runahead_fail("retro_serialize failed");
        return;
    }

    anchor_size = anchor.size;
    anchor_valid = 1;
    anchor_sig = sig;
}

void runahead_invalidate(void) {
    anchor_valid = 0;
    size_refresh_countdown = 0;
}

void runahead_shutdown(void) {
    core_state_buffer_release(&anchor);
    anchor_size = 0;
    anchor_valid = 0;
    failed = 0;
    failure_announced = 0;
}
