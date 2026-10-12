#pragma once

typedef enum {
    usb_link_unsupported = 0,
    usb_link_off,
    usb_link_waiting,
    usb_link_device,
    usb_link_host,
    usb_link_failed
} usb_link_state;

int usb_link_supported(void);

int usb_link_start(int notify_fd);

void usb_link_stop(void);

usb_link_state usb_link_current(void);

const char *usb_link_state_name(usb_link_state state);
