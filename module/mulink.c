#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <poll.h>
#include <signal.h>
#include <sys/inotify.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "../common/options.h"

#define LINK_ETHERTYPE  0x88B5
#define LINK_MAGIC      "MUOSLINK"
#define LINK_MAGIC_LEN  8
#define LINK_VERSION    1
#define LINK_FRAME_SIZE 60
#define LINK_STATE_DIR  RUN_PATH "network"
#define LINK_STATE_FILE LINK_STATE_DIR "/link"

#define POLL_IDLE_MS    -1
#define POLL_SEARCH_MS  1000
#define POLL_PAIRED_MS  5000
#define PEER_TIMEOUT_MS 15000

#define SYS_NET "/sys/class/net"

#define LINK_SETTING_DIR  CONF_CONFIG_PATH "settings/network"
#define LINK_SETTING_FILE LINK_SETTING_DIR "/link"

static volatile sig_atomic_t running = 1;

struct link_state {
    char iface[IF_NAMESIZE];
    uint8_t self_mac[ETH_ALEN];
    uint8_t peer_mac[ETH_ALEN];
    int peer_seen;
    uint32_t peer_last_ms;
    uint32_t local_ip;
    uint32_t peer_ip;
    int applied;
};

static void handle_signal(const int sig) {
    (void) sig;
    running = 0;
}

static uint32_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t) (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static int read_small_file(const char *path, char *out, const size_t size) {
    const int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;

    const ssize_t count = read(fd, out, size - 1);
    close(fd);
    if (count <= 0) return -1;

    out[count] = '\0';
    return 0;
}

static int path_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int iface_is_candidate(const char *name) {
    if (strcmp(name, "lo") == 0) return 0;
    if (strlen(name) >= IF_NAMESIZE) return 0;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), SYS_NET "/%s/phy80211", name);
    if (path_exists(path)) return 0;

    snprintf(path, sizeof(path), SYS_NET "/%s/wireless", name);
    if (path_exists(path)) return 0;

    snprintf(path, sizeof(path), SYS_NET "/%s/type", name);
    char value[32];
    if (read_small_file(path, value, sizeof(value)) != 0) return 0;
    if (strtol(value, NULL, 10) != ARPHRD_ETHER) return 0;

    return 1;
}

static int iface_has_carrier(const char *name) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), SYS_NET "/%s/carrier", name);

    char value[32];
    if (read_small_file(path, value, sizeof(value)) != 0) return 0;

    return strtol(value, NULL, 10) == 1;
}

static int interface_up(const int control_fd, const char *name) {
    struct ifreq request = {0};
    snprintf(request.ifr_name, sizeof(request.ifr_name), "%s", name);

    if (ioctl(control_fd, SIOCGIFFLAGS, &request) != 0) return -1;
    if (request.ifr_flags & IFF_UP) return 0;

    request.ifr_flags |= IFF_UP | IFF_RUNNING;
    return ioctl(control_fd, SIOCSIFFLAGS, &request);
}

static int find_wired_interface(const int control_fd, char *out, const size_t size) {
    DIR *dir = opendir(SYS_NET);
    if (!dir) return 0;

    char carrier_name[IF_NAMESIZE] = "";
    char any_name[IF_NAMESIZE] = "";

    const struct dirent *entry;
    while ((entry = readdir(dir))) {
        if (entry->d_name[0] == '.') continue;
        if (!iface_is_candidate(entry->d_name)) continue;

        if (!any_name[0]) snprintf(any_name, sizeof(any_name), "%.*s", IF_NAMESIZE - 1, entry->d_name);
        interface_up(control_fd, entry->d_name);

        if (iface_has_carrier(entry->d_name)) {
            snprintf(carrier_name, sizeof(carrier_name), "%.*s", IF_NAMESIZE - 1, entry->d_name);
            break;
        }
    }

    closedir(dir);

    if (carrier_name[0]) {
        snprintf(out, size, "%s", carrier_name);
        return 1;
    }

    if (any_name[0]) snprintf(out, size, "%s", any_name);
    return 0;
}

static int interface_has_routable_address(const int control_fd, const char *name) {
    struct ifreq request = {0};
    snprintf(request.ifr_name, sizeof(request.ifr_name), "%s", name);

    if (ioctl(control_fd, SIOCGIFADDR, &request) != 0) return 0;

    const struct sockaddr_in *address = (const struct sockaddr_in *) &request.ifr_addr;
    const uint32_t value = ntohl(address->sin_addr.s_addr);
    if (!value) return 0;

    return (value & 0xffff0000u) != (169u << 24 | 254u << 16);
}

static int interface_mac(const int control_fd, const char *name, uint8_t *mac) {
    struct ifreq request = {0};
    snprintf(request.ifr_name, sizeof(request.ifr_name), "%s", name);

    if (ioctl(control_fd, SIOCGIFHWADDR, &request) != 0) return -1;

    memcpy(mac, request.ifr_hwaddr.sa_data, ETH_ALEN);
    return 0;
}

static int set_interface_address(const int control_fd, const char *name, const uint32_t address, const uint32_t mask) {
    struct ifreq request = {0};
    snprintf(request.ifr_name, sizeof(request.ifr_name), "%s", name);

    struct sockaddr_in *target = (struct sockaddr_in *) &request.ifr_addr;
    target->sin_family = AF_INET;

    target->sin_addr.s_addr = htonl(address);
    if (ioctl(control_fd, SIOCSIFADDR, &request) != 0) return -1;

    target->sin_addr.s_addr = htonl(mask);
    if (ioctl(control_fd, SIOCSIFNETMASK, &request) != 0) return -1;

    return interface_up(control_fd, name);
}

static void derive_addresses(const uint8_t *self, const uint8_t *peer, uint32_t *local, uint32_t *remote) {
    const int self_is_lower = memcmp(self, peer, ETH_ALEN) < 0;
    const uint8_t *lower = self_is_lower ? self : peer;
    const uint8_t *higher = self_is_lower ? peer : self;

    uint32_t hash = 2166136261u;
    for (int i = 0; i < ETH_ALEN; i++) {
        hash = (hash ^ lower[i]) * 16777619u;
    }
    for (int i = 0; i < ETH_ALEN; i++) {
        hash = (hash ^ higher[i]) * 16777619u;
    }

    const uint32_t third = hash % 254u + 1u;
    const uint32_t base = 169u << 24 | 254u << 16 | third << 8;

    *local = base | (self_is_lower ? 1u : 2u);
    *remote = base | (self_is_lower ? 2u : 1u);
}

static void format_mac(const uint8_t *mac, char *out, const size_t size) {
    snprintf(out, size, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void format_ip(const uint32_t address, char *out, const size_t size) {
    const struct in_addr value = {.s_addr = htonl(address)};
    if (!inet_ntop(AF_INET, &value, out, (socklen_t) size)) snprintf(out, size, "-");
}

static void write_state(const struct link_state *state, const int carrier) {
    mkdir(RUN_PATH, 0755);
    mkdir(LINK_STATE_DIR, 0755);

    char temporary[PATH_MAX];
    snprintf(temporary, sizeof(temporary), "%s.tmp.%ld", LINK_STATE_FILE, (long) getpid());

    const int descriptor = open(temporary, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0644);
    FILE *file = descriptor >= 0 ? fdopen(descriptor, "w") : NULL;
    if (!file) {
        if (descriptor >= 0) close(descriptor);
        unlink(temporary);
        return;
    }

    const char *status = "absent";
    if (state->peer_seen && state->applied)
        status = "paired";
    else if (carrier)
        status = "waiting";
    else if (state->iface[0])
        status = "unplugged";

    fprintf(file, "status=%s\n", status);
    fprintf(file, "interface=%s\n", carrier && state->iface[0] ? state->iface : "-");

    char buffer[64];
    format_mac(state->self_mac, buffer, sizeof(buffer));
    fprintf(file, "mac=%s\n", buffer);

    if (state->peer_seen) {
        format_mac(state->peer_mac, buffer, sizeof(buffer));
        fprintf(file, "peer_mac=%s\n", buffer);
        format_ip(state->local_ip, buffer, sizeof(buffer));
        fprintf(file, "address=%s\n", buffer);
        format_ip(state->peer_ip, buffer, sizeof(buffer));
        fprintf(file, "peer_address=%s\n", buffer);
    }

    int okay = fflush(file) == 0 && fsync(descriptor) == 0;
    if (fclose(file) != 0) okay = 0;
    if (!okay || rename(temporary, LINK_STATE_FILE) != 0) unlink(temporary);
}

static int open_beacon_socket(const char *iface, const int control_fd) {
    const int fd = socket(AF_PACKET, SOCK_RAW | SOCK_CLOEXEC, htons(LINK_ETHERTYPE));
    if (fd < 0) return -1;

    struct ifreq request = {0};
    snprintf(request.ifr_name, sizeof(request.ifr_name), "%s", iface);
    if (ioctl(control_fd, SIOCGIFINDEX, &request) != 0) {
        close(fd);
        return -1;
    }

    const struct sockaddr_ll address = {
        .sll_family = AF_PACKET, .sll_protocol = htons(LINK_ETHERTYPE), .sll_ifindex = request.ifr_ifindex
    };

    if (bind(fd, (const struct sockaddr *) &address, sizeof(address)) != 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static void send_beacon(const int fd, const uint8_t *self_mac) {
    uint8_t frame[LINK_FRAME_SIZE] = {0};

    memset(frame, 0xff, ETH_ALEN);
    memcpy(frame + ETH_ALEN, self_mac, ETH_ALEN);
    frame[12] = LINK_ETHERTYPE >> 8;
    frame[13] = LINK_ETHERTYPE & 0xff;
    memcpy(frame + ETH_HLEN, LINK_MAGIC, LINK_MAGIC_LEN);
    frame[ETH_HLEN + LINK_MAGIC_LEN] = LINK_VERSION;

    if (send(fd, frame, sizeof(frame), MSG_DONTWAIT) < 0 && errno != EAGAIN && errno != ENETDOWN) return;
}

static int receive_beacon(const int fd, const uint8_t *self_mac, uint8_t *peer_mac) {
    uint8_t frame[256];

    for (;;) {
        const ssize_t count = recv(fd, frame, sizeof(frame), MSG_DONTWAIT);
        if (count < 0) return 0;
        if (count < ETH_HLEN + LINK_MAGIC_LEN + 1) continue;
        if (memcmp(frame + ETH_HLEN, LINK_MAGIC, LINK_MAGIC_LEN) != 0) continue;
        if (frame[ETH_HLEN + LINK_MAGIC_LEN] != LINK_VERSION) continue;
        if (memcmp(frame + ETH_ALEN, self_mac, ETH_ALEN) == 0) continue;

        memcpy(peer_mac, frame + ETH_ALEN, ETH_ALEN);
        return 1;
    }
}

static int link_is_enabled(void) {
    char value[32];
    if (read_small_file(LINK_SETTING_FILE, value, sizeof(value)) != 0) return 1;

    return strtol(value, NULL, 10) != 0;
}

static int open_setting_watch(void) {
    const int fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (fd < 0) return -1;

    if (inotify_add_watch(fd, LINK_SETTING_DIR, IN_CLOSE_WRITE | IN_MOVED_TO) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static void drain_watch(const int fd) {
    uint8_t buffer[sizeof(struct inotify_event) + NAME_MAX + 1];
    while (read(fd, buffer, sizeof(buffer)) > 0) {
    }
}

static int open_netlink_socket(void) {
    const int fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK, NETLINK_ROUTE);
    if (fd < 0) return -1;

    const struct sockaddr_nl address = {.nl_family = AF_NETLINK, .nl_groups = RTMGRP_LINK};
    if (bind(fd, (const struct sockaddr *) &address, sizeof(address)) != 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static void drain_netlink(const int fd) {
    uint8_t buffer[4096];
    while (recv(fd, buffer, sizeof(buffer), MSG_DONTWAIT) > 0) {
    }
}

int main(void) {
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    signal(SIGPIPE, SIG_IGN);

    const int control_fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (control_fd < 0) return 1;

    const int netlink_fd = open_netlink_socket();
    if (netlink_fd < 0) {
        close(control_fd);
        return 1;
    }

    const int watch_fd = open_setting_watch();

    struct link_state state = {0};
    int beacon_fd = -1;
    int carrier = 0;
    int rescan = 1;
    int enabled = 1;

    while (running) {
        if (rescan) {
            rescan = 0;
            enabled = link_is_enabled();

            char iface[IF_NAMESIZE] = "";
            const int found = enabled ? find_wired_interface(control_fd, iface, sizeof(iface)) : 0;

            if (strcmp(iface, state.iface) != 0 || found != carrier) {
                if (beacon_fd >= 0) {
                    close(beacon_fd);
                    beacon_fd = -1;
                }

                memset(&state.peer_mac, 0, sizeof(state.peer_mac));
                memset(&state.self_mac, 0, sizeof(state.self_mac));
                state.peer_seen = 0;
                state.applied = 0;
                state.local_ip = 0;
                state.peer_ip = 0;
                snprintf(state.iface, sizeof(state.iface), "%s", iface);
                carrier = found;

                if (carrier && state.iface[0] && interface_mac(control_fd, state.iface, state.self_mac) == 0)
                    beacon_fd = open_beacon_socket(state.iface, control_fd);

                write_state(&state, carrier);
            }
        }

        struct pollfd watch[3];
        int count = 0;

        const int netlink_slot = count;
        watch[count].fd = netlink_fd;
        watch[count].events = POLLIN;
        count++;

        int watch_slot = -1;
        if (watch_fd >= 0) {
            watch_slot = count;
            watch[count].fd = watch_fd;
            watch[count].events = POLLIN;
            count++;
        }

        int beacon_slot = -1;
        if (beacon_fd >= 0) {
            beacon_slot = count;
            watch[count].fd = beacon_fd;
            watch[count].events = POLLIN;
            count++;
        }

        int timeout = POLL_IDLE_MS;
        if (beacon_fd >= 0) timeout = state.peer_seen ? POLL_PAIRED_MS : POLL_SEARCH_MS;

        const int ready = poll(watch, (nfds_t) count, timeout);
        if (ready < 0 && errno != EINTR) break;
        if (!running) break;

        if (watch_slot >= 0 && watch[watch_slot].revents & POLLIN) {
            drain_watch(watch_fd);
            rescan = 1;
            continue;
        }

        if (watch[netlink_slot].revents & POLLIN) {
            drain_netlink(netlink_fd);
            rescan = 1;
            continue;
        }

        if (beacon_fd < 0) continue;

        if (beacon_slot >= 0 && watch[beacon_slot].revents & POLLIN) {
            uint8_t peer_mac[ETH_ALEN];

            if (receive_beacon(beacon_fd, state.self_mac, peer_mac)) {
                const int changed = !state.peer_seen || memcmp(peer_mac, state.peer_mac, ETH_ALEN) != 0;

                memcpy(state.peer_mac, peer_mac, ETH_ALEN);
                state.peer_seen = 1;
                state.peer_last_ms = now_ms();

                if (changed) state.applied = 0;

                if (!state.applied) {
                    derive_addresses(state.self_mac, state.peer_mac, &state.local_ip, &state.peer_ip);

                    if (interface_has_routable_address(control_fd, state.iface))
                        state.applied = 1;
                    else
                        state.applied =
                            set_interface_address(control_fd, state.iface, state.local_ip, 0xffffff00u) == 0;

                    write_state(&state, carrier);
                }
            }
        }

        if (state.peer_seen && now_ms() - state.peer_last_ms > PEER_TIMEOUT_MS) {
            memset(&state.peer_mac, 0, sizeof(state.peer_mac));
            state.peer_seen = 0;
            state.applied = 0;
            state.local_ip = 0;
            state.peer_ip = 0;
            write_state(&state, carrier);
        }

        send_beacon(beacon_fd, state.self_mac);
    }

    if (beacon_fd >= 0) close(beacon_fd);
    if (watch_fd >= 0) close(watch_fd);
    close(netlink_fd);
    close(control_fd);
    remove(LINK_STATE_FILE);

    return 0;
}
