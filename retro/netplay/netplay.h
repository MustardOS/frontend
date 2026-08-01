#pragma once

#include <stddef.h>
#include <stdint.h>
#include "../core/libretro.h"

#define NETPLAY_PORT_COUNT     4
#define NETPLAY_DEFAULT_PORT   55435
#define NETPLAY_DISCOVERY_PORT 55436
#define NETPLAY_HOST_NAME_MAX  48
#define NETPLAY_HOST_NAME_SIZE (NETPLAY_HOST_NAME_MAX + 1)

typedef enum {
    netplay_status_idle = 0,
    netplay_status_discovering,
    netplay_status_hosting,
    netplay_status_connecting,
    netplay_status_pairing,
    netplay_status_checking,
    netplay_status_synchronising,
    netplay_status_playing,
    netplay_status_reconnecting,
    netplay_status_failed
} netplay_status;

typedef enum { netplay_role_none = 0, netplay_role_host, netplay_role_client } netplay_role;

typedef enum { netplay_mode_separate = 0, netplay_mode_play_together } netplay_mode;

typedef struct {
    uint16_t buttons;
    int16_t axes[4];
    uint8_t connected;
} netplay_pad_state;

typedef struct {
    netplay_status status;
    netplay_role role;
    netplay_mode mode;
    unsigned player_count;
    unsigned local_port;
    unsigned input_delay;
    unsigned rollback_count;
    unsigned rollback_depth;
    unsigned ping_ms;
    unsigned jitter_ms;
    uint64_t frame;
    char pairing_code[8];
    int pairing_local_confirmed;
    int pairing_peer_confirmed;
    unsigned pairing_confirmed_count;
    unsigned pairing_remaining_count;
    char peer[128];
    char failure[192];
} netplay_info;

typedef struct {
    char label[64];
    char address[64];
    uint16_t port;
} netplay_discovered_host;

int netplay_init(const char *core_path, const char *content_path);

void netplay_shutdown(void);

int netplay_host(uint16_t port);

int netplay_join(const char *address, uint16_t port);

int netplay_parse_address(const char *specification, char *address, size_t address_size, uint16_t *port);

int netplay_discover(void);

unsigned netplay_discovered_count(void);

int netplay_discovered_get(unsigned index, netplay_discovered_host *host);

int netplay_join_discovered(unsigned index);

void netplay_get_host_name(char *name, size_t size);

int netplay_set_host_name(const char *name);

netplay_mode netplay_get_host_mode(void);

int netplay_set_host_mode(netplay_mode mode);

int netplay_play_together_available(void);

unsigned netplay_get_host_slots(void);

int netplay_set_host_slots(unsigned slots);

unsigned netplay_get_host_slot_limit(void);

void netplay_confirm_pairing(void);

void netplay_disconnect(void);

void netplay_tick(void);

int netplay_before_frame(void);

void netplay_after_frame(void);

int netplay_is_active(void);

int netplay_is_playing(void);

int netplay_menu_paused(void);

int netplay_blocks_core(void);

void netplay_get_info(netplay_info *info);

const char *netplay_status_name(netplay_status status);

int netplay_get_client_index(unsigned *index);

void netplay_set_netpacket_interface(const struct retro_netpacket_callback *callback);

void netplay_input_get_local(netplay_pad_state *state);

void netplay_input_set_port(unsigned port, const netplay_pad_state *state);
