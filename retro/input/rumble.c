#include <string.h>
#include "../../common/device.h"
#include "../../common/fileio.h"
#include "rumble.h"
#include "../settings/settings.h"

static int rumble_on_value(void) {
    if (strncmp(device.board.name, "rg-vita", 7) == 0) return 0;
    return 1;
}

static int rumble_off_value(void) {
    if (strncmp(device.board.name, "rk", 2) == 0) return 1000000;
    if (strncmp(device.board.name, "rg-vita", 7) == 0) return 1;
    return 0;
}

static int rumble_last_on = -1;
static int rumble_suppressed = 0;

static void rumble_write(const int want_on) {
    if (!device.board.rumble[0] || want_on == rumble_last_on) return;

    write_text_to_file(device.board.rumble, "w", INT, want_on ? rumble_on_value() : rumble_off_value());
    rumble_last_on = want_on;
}

void rumble_bridge_set_suppressed(const int suppressed) {
    rumble_suppressed = suppressed;
    if (suppressed) rumble_write(0);
}

static bool rumble_set_state(const unsigned port, const enum retro_rumble_effect effect, const uint16_t strength) {
    (void) port;
    (void) effect;

    if (rumble_suppressed) return true;

    rumble_write(session_settings.rumble_enabled && strength > 0 ? 1 : 0);
    return true;
}

static struct retro_rumble_interface rumble_iface = {
    .set_rumble_state = rumble_set_state,
};

bool rumble_bridge_get_interface(struct retro_rumble_interface *iface) {
    if (!iface) return false;
    *iface = rumble_iface;
    return true;
}

void rumble_bridge_shutdown(void) {
    rumble_write(0);
}
