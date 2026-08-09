#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../../common/init.h"
#include "../../common/log.h"
#include "../netplay/netplay.h"
#include "../state/gamestate.h"
#include "../state/sram.h"
#include "../video/image_writer.h"
#include "muxretro.h"
#include "power.h"
#include "power_protocol.h"

static int power_save_prepared = 0;
static const char power_save_ready_path[] = "/run/muos/muxretro_save_ready";

void power_session_init(void) {
    if (power_protocol_init() != 0) LOG_WARN(mux_module, "Could not initialise power lifecycle signals");
    unlink(power_save_ready_path);
}

static void acknowledge_power_save(void) {
    const int descriptor =
        open(power_save_ready_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (descriptor < 0) {
        LOG_WARN(mux_module, "Could not acknowledge power save: %s", strerror(errno));
        return;
    }

    char value[32];
    const int length = snprintf(value, sizeof(value), "%ld\n", (long) getpid());
    if (length <= 0 || write(descriptor, value, (size_t) length) != length)
        LOG_WARN(mux_module, "Could not complete power-save acknowledgement");
    close(descriptor);
}

static void prepare_power_save(const char *reason) {
    LOG_INFO(mux_module, "Preparing content for %s", reason);

    if (netplay_is_active()) netplay_disconnect();

    int state_saved = 1;
    if (state_saves_supported()) {
        gamestate_capture_pending(1);
        if (gamestate_autosave_save() == 0)
            LOG_SUCCESS(mux_module, "Auto save completed before %s", reason);
        else {
            LOG_WARN(mux_module, "Auto save could not be completed before %s", reason);
            state_saved = 0;
        }
    }

    sram_bridge_save();
    sram_bridge_flush();
    power_save_prepared = state_saved;
}

int power_session_poll(void) {
    power_protocol_events events = {0};
    power_protocol_take(&events);

    if (events.sleep) {

        prepare_power_save("suspend");
        if (!pause_menu_is_active()) pause_menu_toggle();
        image_writer_flush();
        acknowledge_power_save();
    }

    if (events.wake) {
        LOG_INFO(mux_module, "Received resume signal (SIGUSR2)");
        power_save_prepared = 0;
        unlink(power_save_ready_path);

        if (pause_menu_is_active()) pause_menu_toggle();
    }

    if (events.exit_signal) {
        LOG_INFO(mux_module, "Received exit signal %d", events.exit_signal);
        if (!power_save_prepared) prepare_power_save("shutdown");
        return 1;
    }

    return 0;
}
