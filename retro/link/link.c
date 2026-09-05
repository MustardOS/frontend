#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include "../../common/config.h"
#include "../../common/init.h"
#include "../../common/log.h"
#include "../../common/options.h"
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
#define DIRECT_LINK_STATE RUN_PATH "network/link"
#define DIRECT_LINK_POLL_MS 500
#define LINK_CONTROL_MAGIC "PKGL"
#define LINK_CONTROL_VERSION 1
#define LINK_CONTROL_PACKET_SIZE 8
#define LINK_CONTROL_SEND_MS 100
#define LINK_CONTROL_TIMEOUT_MS 1500

static int local_pending = 0;
static int direct_pending = 0;
static int direct_suppressed = 0;
static int direct_paired = 0;
static int direct_role = link_mode_off;
static uint32_t direct_poll_at = 0;
static char direct_host[LINK_HOST_LEN] = "";
static char direct_peer[LINK_HOST_LEN] = "";

static int control_fd = -1;
static enum link_mode control_role = link_mode_off;
static uint16_t control_port = 0;
static char control_host[LINK_HOST_LEN] = "";
static struct sockaddr_in control_peer;
static int control_peer_known = 0;
static int control_connected = 0;
static int control_local_menu = 0;
static int control_peer_menu = 0;
static uint32_t control_last_rx = 0;
static uint32_t control_next_tx = 0;

struct direct_link_report {
    char status[16];
    char mac[18];
    char peer_mac[18];
    char address[LINK_HOST_LEN];
    char peer_address[LINK_HOST_LEN];
};

static void direct_store_field(struct direct_link_report *report, const char *key, const char *value) {
    struct {
        const char *key;
        char *target;
        size_t length;
    } fields[] = {
        {"status", report->status, sizeof(report->status)},
        {"mac", report->mac, sizeof(report->mac)},
        {"peer_mac", report->peer_mac, sizeof(report->peer_mac)},
        {"address", report->address, sizeof(report->address)},
        {"peer_address", report->peer_address, sizeof(report->peer_address)},
    };

    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        if (strcmp(key, fields[i].key) != 0) continue;
        snprintf(fields[i].target, fields[i].length, "%s", value);
        return;
    }
}

static int direct_read_report(struct direct_link_report *report) {
    memset(report, 0, sizeof(*report));

    FILE *file = fopen(DIRECT_LINK_STATE, "r");
    if (!file) return 0;

    char line[160];
    while (fgets(line, sizeof(line), file)) {
        char *separator = strchr(line, '=');
        if (!separator) continue;

        *separator = '\0';
        char *value = separator + 1;
        value[strcspn(value, "\r\n")] = '\0';
        direct_store_field(report, line, value);
    }

    fclose(file);
    return 1;
}

static int direct_parse_mac(const char *text, uint8_t out[6]) {
    unsigned int octet[6];
    int consumed = 0;
    if (!text
        || sscanf(
               text, "%2x:%2x:%2x:%2x:%2x:%2x%n", &octet[0], &octet[1], &octet[2], &octet[3], &octet[4],
               &octet[5], &consumed
           )
               != 6
        || text[consumed] != '\0')
        return 0;

    for (int i = 0; i < 6; i++)
        out[i] = (uint8_t) octet[i];
    return 1;
}

static int direct_valid_address(const char *text) {
    struct in_addr address;
    return text && inet_pton(AF_INET, text, &address) == 1;
}

static const char *mode_value(const link_provider *p, enum link_mode mode);

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

static int direct_set_core_mode(const enum link_mode mode, const char *host) {
    const link_provider *p = active_provider();
    if (!p) return 0;

    if (mode == link_mode_join && (!host || !link_set_host(host))) return 0;
    if (!options_set(p->mode_key, mode_value(p, mode))) return 0;

    direct_role = mode;
    snprintf(direct_host, sizeof(direct_host), "%s", host ? host : "");
    return 1;
}

static void direct_refresh(void) {
    struct direct_link_report report;
    const int report_read = direct_read_report(&report);

    uint8_t self[6];
    uint8_t peer[6];
    const int paired = config.settings.network.link && report_read && strcmp(report.status, "paired") == 0
                       && direct_parse_mac(report.mac, self) && direct_parse_mac(report.peer_mac, peer)
                       && memcmp(self, peer, sizeof(self)) != 0
                       && direct_valid_address(report.address) && direct_valid_address(report.peer_address);

    if (!direct_pending) {
        if (!paired || direct_suppressed || link_get_mode() != link_mode_off) return;
        direct_pending = 1;
        LOG_INFO(mux_module, "Game Link automatically selected Direct Link for the paired peer");
    }

    if (!paired) {
        direct_paired = 0;
        direct_host[0] = '\0';
        direct_peer[0] = '\0';
        if (direct_role != link_mode_off && direct_set_core_mode(link_mode_off, NULL))
            LOG_INFO(mux_module, "Direct Link peer disappeared; Game Link is waiting for it to return");
        return;
    }

    const enum link_mode role = memcmp(self, peer, sizeof(self)) < 0 ? link_mode_host : link_mode_join;
    const char *host = role == link_mode_host ? report.address : report.peer_address;
    if (direct_paired && direct_role == role && strcmp(direct_host, host) == 0
        && strcmp(direct_peer, report.peer_address) == 0)
        return;

    if (!direct_set_core_mode(role, host)) {
        direct_paired = 0;
        LOG_WARN(mux_module, "Could not apply the paired Direct Link to this core");
        return;
    }

    direct_paired = 1;
    snprintf(direct_peer, sizeof(direct_peer), "%s", report.peer_address);
    LOG_INFO(
        mux_module, "Direct Link configured Game Link as %s at %s", role == link_mode_host ? "host" : "client", host
    );
}

void link_direct_init(void) {
    direct_pending = 0;
    direct_paired = 0;
    direct_role = link_mode_off;
    direct_poll_at = 0;
    direct_host[0] = '\0';
    direct_peer[0] = '\0';

    const enum link_mode saved_mode = link_get_mode();
    direct_suppressed = saved_mode != link_mode_off;
    direct_refresh();
    direct_poll_at = SDL_GetTicks() + DIRECT_LINK_POLL_MS;
}

int link_direct_is_paired(void) {
    return direct_pending && direct_paired;
}

void link_direct_get_host(char *out, const int len) {
    if (len <= 0) return;
    snprintf(out, (size_t) len, "%s", direct_paired ? direct_host : "");
}

static void control_close(void) {
    if (control_fd >= 0) close(control_fd);
    control_fd = -1;
    control_role = link_mode_off;
    control_port = 0;
    control_host[0] = '\0';
    memset(&control_peer, 0, sizeof(control_peer));
    control_peer_known = 0;
    control_connected = 0;
    control_peer_menu = 0;
    control_last_rx = 0;
    control_next_tx = 0;
}

static enum link_mode control_wanted_role(void) {
    const enum link_mode mode = link_get_mode();
    if (mode == link_mode_direct) return direct_paired ? (enum link_mode) direct_role : link_mode_off;
    return mode == link_mode_host || mode == link_mode_join ? mode : link_mode_off;
}

static uint16_t control_wanted_port(void) {
    const int game_port = link_get_port();
    if (game_port <= 0 || game_port > 65535) return 0;
    return (uint16_t) (game_port == 65535 ? 65534 : game_port + 1);
}

static void control_wanted_host(char out[LINK_HOST_LEN], const enum link_mode role) {
    out[0] = '\0';
    if (role != link_mode_join) return;
    if (link_get_mode() == link_mode_direct)
        snprintf(out, LINK_HOST_LEN, "%s", direct_host);
    else
        link_get_host(out, LINK_HOST_LEN);
}

static int control_open(const enum link_mode role, const uint16_t port, const char *host) {
    const int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0) return 0;

    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) != 0) {
        close(fd);
        return 0;
    }

    struct sockaddr_in peer = {.sin_family = AF_INET, .sin_port = htons(port)};
    if (role == link_mode_host) {
        const int one = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        peer.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(fd, (const struct sockaddr *) &peer, sizeof(peer)) != 0) {
            close(fd);
            return 0;
        }
        memset(&peer, 0, sizeof(peer));
    } else if (!host || inet_pton(AF_INET, host, &peer.sin_addr) != 1) {
        close(fd);
        return 0;
    }

    control_fd = fd;
    control_role = role;
    control_port = port;
    snprintf(control_host, sizeof(control_host), "%s", host ? host : "");
    control_peer = peer;
    control_peer_known = role == link_mode_join;
    control_next_tx = 0;
    LOG_INFO(mux_module, "Game Link pause channel opened as %s on UDP %u", role == link_mode_host ? "host" : "client", port);
    return 1;
}

static void control_configure(void) {
    const enum link_mode role = control_wanted_role();
    const uint16_t port = control_wanted_port();
    char host[LINK_HOST_LEN];
    control_wanted_host(host, role);

    if (role == link_mode_off || !port || (role == link_mode_join && !host[0])) {
        if (control_fd >= 0) control_close();
        return;
    }

    if (control_fd >= 0 && role == control_role && port == control_port && strcmp(host, control_host) == 0) return;
    control_close();
    if (!control_open(role, port, host))
        LOG_WARN(mux_module, "Could not open the Game Link pause channel on UDP %u: %s", port, strerror(errno));
}

static int control_source_allowed(const struct sockaddr_in *source, const uint32_t now) {
    if (control_role == link_mode_join)
        return source->sin_addr.s_addr == control_peer.sin_addr.s_addr && source->sin_port == control_peer.sin_port;

    if (link_get_mode() == link_mode_direct) {
        struct in_addr expected;
        if (inet_pton(AF_INET, direct_peer, &expected) != 1 || source->sin_addr.s_addr != expected.s_addr) return 0;
    }

    if (control_peer_known && !SDL_TICKS_PASSED(now, control_last_rx + LINK_CONTROL_TIMEOUT_MS))
        return source->sin_addr.s_addr == control_peer.sin_addr.s_addr && source->sin_port == control_peer.sin_port;
    return 1;
}

static void control_receive(const uint32_t now) {
    for (int count = 0; count < 8; count++) {
        uint8_t packet[LINK_CONTROL_PACKET_SIZE];
        struct sockaddr_in source;
        socklen_t source_len = sizeof(source);
        const ssize_t received = recvfrom(
            control_fd, packet, sizeof(packet), MSG_DONTWAIT, (struct sockaddr *) &source, &source_len
        );
        if (received < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                LOG_WARN(mux_module, "Game Link pause channel receive failed: %s", strerror(errno));
            break;
        }

        const uint8_t wanted_role = control_role == link_mode_host ? link_mode_join : link_mode_host;
        if (received != LINK_CONTROL_PACKET_SIZE || memcmp(packet, LINK_CONTROL_MAGIC, 4) != 0
            || packet[4] != LINK_CONTROL_VERSION || packet[5] > 1 || packet[6] != wanted_role || packet[7] != 0
            || source_len != sizeof(source) || source.sin_family != AF_INET || !control_source_allowed(&source, now))
            continue;

        control_peer = source;
        control_peer_known = 1;
        control_connected = 1;
        control_peer_menu = packet[5];
        control_last_rx = now;
    }
}

static void control_send(const uint32_t now) {
    if (!control_peer_known || !SDL_TICKS_PASSED(now, control_next_tx)) return;

    const uint8_t packet[LINK_CONTROL_PACKET_SIZE] = {
        'P', 'K', 'G', 'L', LINK_CONTROL_VERSION, control_local_menu != 0, (uint8_t) control_role, 0
    };
    const ssize_t sent = sendto(
        control_fd, packet, sizeof(packet), MSG_DONTWAIT, (const struct sockaddr *) &control_peer, sizeof(control_peer)
    );
    if (sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        LOG_WARN(mux_module, "Game Link pause channel send failed: %s", strerror(errno));
    control_next_tx = now + LINK_CONTROL_SEND_MS;
}

static void control_tick(const uint32_t now) {
    control_configure();
    if (control_fd < 0) return;

    if (control_role == link_mode_join) control_send(now);
    control_receive(now);
    if (control_role == link_mode_host) control_send(now);

    if (control_connected && SDL_TICKS_PASSED(now, control_last_rx + LINK_CONTROL_TIMEOUT_MS)) {
        control_connected = 0;
        control_peer_menu = 0;
        if (control_role == link_mode_host) control_peer_known = 0;
    }
}

void link_tick(const unsigned int now, const int local_menu_open) {
    control_local_menu = local_menu_open != 0;

    if (!direct_poll_at || SDL_TICKS_PASSED(now, direct_poll_at)) {
        direct_refresh();
        direct_poll_at = now + DIRECT_LINK_POLL_MS;
    }

    control_tick(now);
}

int link_menu_paused(void) {
    return control_connected && (control_local_menu || control_peer_menu);
}

int link_peer_menu_open(void) {
    return control_connected && control_peer_menu;
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
    return mode == link_mode_host || mode == link_mode_join || (mode == link_mode_direct && direct_paired);
}

const char *link_mode_name(const enum link_mode mode) {
    if (mode == link_mode_direct) return "Direct Link";

    const link_provider *p = active_provider();
    return p ? mode_value(p, mode) : "";
}

enum link_mode link_get_mode(void) {
    if (direct_pending) return link_mode_direct;
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
        case link_mode_direct:
            return config.settings.network.link && active_provider() != NULL;
        case link_mode_local:
            return link_local_supported();
        default:
            return 1;
    }
}

int link_set_mode(const enum link_mode mode) {
    if (mode >= link_mode_count || !link_mode_available(mode)) return 0;

    if (mode == link_mode_direct) {
        local_pending = 0;
        direct_pending = 1;
        direct_suppressed = 0;
        direct_paired = 0;
        direct_role = link_mode_off;
        direct_host[0] = '\0';
        direct_peer[0] = '\0';

        const link_provider *p = active_provider();
        if (!p || !options_set(p->mode_key, p->mode_off)) return 0;

        direct_refresh();
        direct_poll_at = SDL_GetTicks() + DIRECT_LINK_POLL_MS;
        LOG_INFO(mux_module, "Game Link mode set to Direct Link");
        return 1;
    }

    direct_pending = 0;
    direct_suppressed = 1;
    direct_paired = 0;
    direct_role = link_mode_off;
    direct_host[0] = '\0';
    direct_peer[0] = '\0';
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
