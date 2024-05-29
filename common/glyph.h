#pragma once

extern struct glyph_config glyph;
extern struct mux_device device;

struct glyph_config {
    struct {
        struct {
            const char *ACTIVE;
            const char *DISABLED;
        } BLUETOOTH;

        struct {
            const char *ACTIVE;
            const char *DISABLED;
        } NETWORK;

        struct {
            const char *ACTIVE;
            const char *EMPTY;
            const char *LOW;
            const char *MEDIUM;
            const char *HIGH;
            const char *FULL;
        } BATTERY;
    } STATUS;

    struct {
        const char *A;
        const char *B;
        const char *C;
        const char *X;
        const char *Y;
        const char *Z;
        const char *MENU;
    } NAV;

    struct {
        const char *BRIGHTNESS;
        const char *VOLUME_MUTE;
        const char *VOLUME_LOW;
        const char *VOLUME_MEDIUM;
        const char *VOLUME_HIGH;
    } PROGRESS;

    struct {
        const char *ARCHIVE_MANAGER;
        const char *BACKUP_MANAGER;
        const char *PORTMASTER;
        const char *RETROARCH;
        const char *DINGUX;
        const char *GMU;
    } MUX_APPS;

    struct {
        const char *ITEM;
    } MUX_ARCHIVE;

    struct {
        const char *SYSTEM;
        const char *CORE;
    } MUX_ASSIGN;

    struct {
        const char *ITEM;
    } MUX_BACKUP;

    struct {
        const char *GENERAL_SETTINGS;
        const char *THEME_PICKER;
        const char *WIFI_NETWORK;
        const char *WEB_SERVICES;
        const char *DATE_TIME;
        const char *DEVICE_TYPE;
        const char *SYSTEM_REFRESH;
    } MUX_CONFIG;

    struct {
        const char *XONGLEBONGLE;
        const char *SUPPORTER;
    } MUX_CREDITS;

    struct {
        const char *ITEM;
    } MUX_DEVICE;

    struct {
        const char *INPUT_TESTER;
        const char *SYSTEM_DETAILS;
        const char *SUPPORTERS;
    } MUX_INFO;

    struct {
        const char *EXPLORE;
        const char *FAVOURITES;
        const char *HISTORY;
        const char *APPS;
        const char *INFO;
        const char *CONFIG;
        const char *REBOOT;
        const char *SHUTDOWN;
    } MUX_LAUNCH;

    struct {
        const char *ITEM;
    } MUX_NETSCAN;

    struct {
        const char *ENABLE;
        const char *SSID;
        const char *PASS;
        const char *TYPE;
        const char *IP;
        const char *CIDR;
        const char *GATEWAY;
        const char *DNS;
        const char *STATUS;
        const char *CONNECT;
    } MUX_NETWORK;

    struct {
        const char *DIRECTORY;
        const char *FILE;
        const char *STORAGE;
    } MUX_EXPLORE;

    struct {
        const char *CLEAR_FAVOURITES;
        const char *CLEAR_HISTORY;
        const char *CLEAR_ACTIVITY;
        const char *CLEAR_SYSTEM_CONFIG;
        const char *CLEAR_SYSTEM_CACHE;
        const char *RESTORE_RETROARCH_CONFIG;
        const char *RESTORE_NETWORK_CONFIG;
        const char *RESTORE_PORTMASTER;
    } MUX_RESET;

    struct {
        const char *YEAR;
        const char *MONTH;
        const char *DAY;
        const char *HOUR;
        const char *MINUTE;
        const char *NOTATION;
        const char *TIMEZONE;
    } MUX_RTC;

    struct {
        const char *VERSION;
        const char *RETROARCH;
        const char *KERNEL;
        const char *UPTIME;
        const char *CPU_INFO;
        const char *CPU_SPEED;
        const char *CPU_GOVERNOR;
        const char *RAM;
        const char *TEMP;
        const char *RUNNING;
        const char *BAT_CAP;
        const char *BAT_VOLT;
    } MUX_SYSINFO;

    struct {
        const char *ITEM;
    } MUX_THEME;

    struct {
        const char *ITEM;
    } MUX_TIMEZONE;

    struct {
        const char *ITEM;
    } MUX_TRACKER;

    struct {
        const char *AB_SWAP;
        const char *THERMAL_ZONE;
        const char *INTERFACE_FONT;
        const char *VERBOSE;
        const char *LOW_VOLUME;
        const char *BAT_OFFSET;
    } MUX_TWEAK_ADVANCED;

    struct {
        const char *HIDDEN_CONTENT;
        const char *LAUNCHER_SOUND;
        const char *STARTUP;
        const char *POWER;
        const char *LOW_BATTERY;
        const char *COLOUR_TEMP;
        const char *BRIGHTNESS;
        const char *HDMI;
        const char *INTERFACE;
        const char *ADVANCED;
    } MUX_TWEAK_GENERAL;

    struct {
        const char *BATTERY;
        const char *NETWORK;
        const char *BLUETOOTH;
        const char *CLOCK;
        const char *CONTENT;
    } MUX_VISUAL;

    struct {
        const char *SHELL;
        const char *SFTP;
        const char *VIRTUAL_TERM;
        const char *SYNCTHING;
        const char *NTP;
    } MUX_WEBSERVICE;
};

void load_glyph(struct glyph_config *glyph, struct mux_device *device, char *mux_name);
