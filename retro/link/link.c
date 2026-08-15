#include <stdio.h>
#include <string.h>
#include "../../common/init.h"
#include "../../common/log.h"
#include "../core/subsystem.h"
#include "../ui/options.h"
#include "link.h"

typedef struct {
    const char *mode_key;
    const char *mode_off;
    const char *mode_host;
    const char *mode_join;
    const char *port_key;
    const char *address_prefix;
    int address_digits;
    const char *reveal_key;
} link_provider;

static const link_provider providers[] = {
    {
        .mode_key = "gambatte_gb_link_mode",
        .mode_off = "Not Connected",
        .mode_host = "Network Server",
        .mode_join = "Network Client",
        .port_key = "gambatte_gb_link_network_port",
        .address_prefix = "gambatte_gb_link_network_server_ip_",
        .address_digits = 12,
        .reveal_key = "gambatte_show_gb_link_settings",
    },
};

#define PROVIDER_COUNT ((int) (sizeof(providers) / sizeof(providers[0])))

static int local_pending = 0;

int link_local_supported(void) {
    return subsystem_count > 0;
}

int link_local_pending(void) {
    return local_pending;
}

static int focus_slot = 0;

int link_local_active(void) {
    return subsystem_pending_count() > 0;
}

int link_get_focus(void) {
    return focus_slot;
}

void link_set_focus(const int slot) {
    if (slot < 0 || slot > 1 || !link_local_active()) return;

    focus_slot = slot;
    if (!options_set("sameboy_audio_output", slot == 0 ? "Game Boy #1" : "Game Boy #2"))
        LOG_WARN(mux_module, "Core would not move its sound to Game Boy #%d", slot + 1);
    LOG_INFO(mux_module, "Game Link focus moved to Game Boy #%d", slot + 1);
}

void link_toggle_focus(void) {
    link_set_focus(focus_slot == 0 ? 1 : 0);
}

static int single_screen = 0;

int link_single_screen_setting(void) {
    return single_screen;
}

int link_single_screen(void) {
    return single_screen && link_local_active();
}

void link_single_screen_set(const int enabled) {
    single_screen = enabled ? 1 : 0;
}

int link_split_is_horizontal(void) {
    const char *layout = options_get_value("sameboy_screen_layout");
    return layout && strcmp(layout, "left-right") == 0;
}

const char *link_local_ident(void) {
    return subsystem_count > 0 ? subsystem_list[0].ident : "";
}

static const link_provider *active_provider(void) {
    for (int i = 0; i < PROVIDER_COUNT; i++) {
        if (options_find(providers[i].mode_key) >= 0) return &providers[i];
    }

    return NULL;
}

static const char *mode_value(const link_provider *p, const enum link_mode mode) {
    switch (mode) {
        case link_mode_host:
            return p->mode_host;
        case link_mode_join:
            return p->mode_join;
        default:
            return p->mode_off;
    }
}

static void digit_key(const link_provider *p, char *out, const int digit) {
    snprintf(out, 96, "%s%d", p->address_prefix, digit + 1);
}

int link_is_supported(void) {
    return active_provider() != NULL || link_local_supported();
}

int link_is_engaged(void) {
    const enum link_mode mode = link_get_mode();
    return mode == link_mode_host || mode == link_mode_join;
}

const char *link_mode_name(const enum link_mode mode) {
    const link_provider *p = active_provider();
    return p ? mode_value(p, mode) : "";
}

enum link_mode link_get_mode(void) {
    if (local_pending) return link_mode_local;

    const link_provider *p = active_provider();
    if (!p) return link_mode_off;

    const char *value = options_get_value(p->mode_key);
    if (!value) return link_mode_off;

    if (strcmp(value, p->mode_host) == 0) return link_mode_host;
    if (strcmp(value, p->mode_join) == 0) return link_mode_join;

    return link_mode_off;
}

int link_mode_available(const enum link_mode mode) {
    switch (mode) {
        case link_mode_host:
        case link_mode_join:
            return active_provider() != NULL;
        case link_mode_local:
            return link_local_supported();
        default:
            return 1;
    }
}

int link_set_mode(const enum link_mode mode) {
    if (mode >= link_mode_count || !link_mode_available(mode)) return 0;

    local_pending = mode == link_mode_local;

    const link_provider *p = active_provider();
    if (p) {
        const enum link_mode network = mode == link_mode_local ? link_mode_off : mode;
        if (!options_set(p->mode_key, mode_value(p, network))) return 0;
    }

    LOG_INFO(
        mux_module, "Game Link mode set to %s", local_pending ? "two players on this device" : link_mode_name(mode)
    );
    return 1;
}

void link_get_host(char *out, const int len) {
    if (len <= 0) return;
    out[0] = '\0';

    const link_provider *p = active_provider();
    if (!p || p->address_digits < 4 || p->address_digits % 4 != 0) return;

    char padded[64];
    if (p->address_digits >= (int) sizeof(padded)) return;

    for (int i = 0; i < p->address_digits; i++) {
        char key[96];
        digit_key(p, key, i);

        const char *value = options_get_value(key);
        padded[i] = value && value[0] >= '0' && value[0] <= '9' ? value[0] : '0';
    }
    padded[p->address_digits] = '\0';

    const int width = p->address_digits / 4;
    int octet[4] = {0};

    for (int i = 0; i < 4; i++) {
        for (int d = 0; d < width; d++)
            octet[i] = octet[i] * 10 + (padded[i * width + d] - '0');
    }

    snprintf(out, len, "%d.%d.%d.%d", octet[0], octet[1], octet[2], octet[3]);
}

int link_set_host(const char *host) {
    const link_provider *p = active_provider();
    if (!p || p->address_digits < 4 || p->address_digits % 4 != 0) return 0;

    char padded[64];
    if (p->address_digits >= (int) sizeof(padded)) return 0;

    int octet[4];
    if (!host || sscanf(host, "%d.%d.%d.%d", &octet[0], &octet[1], &octet[2], &octet[3]) != 4) return 0;

    for (int i = 0; i < 4; i++) {
        if (octet[i] < 0 || octet[i] > 255) return 0;
    }

    const int width = p->address_digits / 4;
    for (int i = 0; i < 4; i++)
        snprintf(padded + i * width, sizeof(padded) - (size_t) (i * width), "%0*d", width, octet[i]);

    for (int i = 0; i < p->address_digits; i++) {
        char key[96];
        digit_key(p, key, i);

        const char digit[2] = {padded[i], '\0'};
        if (!options_set(key, digit)) return 0;
    }

    LOG_INFO(mux_module, "Game Link host set to %s", host);
    return 1;
}

int link_get_port(void) {
    const link_provider *p = active_provider();
    if (!p) return 0;

    const char *value = options_get_value(p->port_key);
    if (!value) return 0;

    int port = 0;
    return sscanf(value, "%d", &port) == 1 ? port : 0;
}

int link_set_port(const int port) {
    const link_provider *p = active_provider();
    if (!p || port <= 0 || port > 65535) return 0;

    char value[8];
    snprintf(value, sizeof(value), "%d", port);

    return options_set(p->port_key, value);
}

int link_align_port(void) {
    const link_provider *p = active_provider();
    if (!p) return 0;

    const int index = options_find(p->port_key);
    if (index < 0 || options_list[index].value_count <= 0) return 0;

    const char *first = options_list[index].values[0];
    if (!options_set(p->port_key, first)) return 0;

    LOG_INFO(mux_module, "Game Link port aligned to %s", first);
    return 1;
}

void link_reveal_settings(void) {
    const link_provider *p = active_provider();
    if (p && p->reveal_key && !options_set(p->reveal_key, "enabled"))
        LOG_WARN(mux_module, "Core would not reveal its link settings");
}
