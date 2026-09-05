#pragma once

#define LINK_HOST_LEN 16

enum link_mode {
    link_mode_off = 0,
    link_mode_host,
    link_mode_join,
    link_mode_local,
    link_mode_direct,
    link_mode_count,
};

enum link_mode link_get_mode(void);

int link_is_supported(void);

int link_is_engaged(void);

void link_direct_init(void);

void link_tick(unsigned int now, int local_menu_open);

int link_direct_is_paired(void);

void link_direct_get_host(char *out, int len);

int link_menu_paused(void);

int link_peer_menu_open(void);

int link_local_supported(void);

int link_local_pending(void);

const char *link_local_ident(void);

int link_local_active(void);

int link_get_focus(void);

void link_set_focus(int slot);

void link_toggle_focus(void);

int link_single_screen(void);

int link_single_screen_setting(void);

void link_single_screen_set(int enabled);

int link_split_is_horizontal(void);

int link_mode_available(enum link_mode mode);

int link_set_mode(enum link_mode mode);

void link_get_host(char *out, int len);

int link_set_host(const char *host);

int link_get_port(void);

int link_set_port(int port);

int link_align_port(void);

void link_reveal_settings(void);

const char *link_mode_name(enum link_mode mode);
