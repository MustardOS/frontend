#pragma once

#include <stdint.h>
#include "options.h"

extern struct mux_device device;

struct mux_device {
    struct {
        char name[MAX_BUFFER_SIZE];
        int16_t has_network;
        int16_t has_bluetooth;
        int16_t has_portmaster;
        int16_t has_lid;
        int16_t has_hdmi;
        int16_t has_event;
        int16_t has_debugfs;
        int16_t has_rgb;
        int16_t has_stick;
        int16_t has_touch;
        char sdl_map[MAX_BUFFER_SIZE];
        char joy_hall[MAX_BUFFER_SIZE];
        char led[MAX_BUFFER_SIZE];
        char rtc_clock[MAX_BUFFER_SIZE];
        char rtc_wake[MAX_BUFFER_SIZE];
        char rumble[MAX_BUFFER_SIZE];
    } board;

    struct {
        int16_t min;
        int16_t max;
    } audio;

    struct {
        int16_t width;
        int16_t height;
        int16_t buffer;
    } mux;

    struct {
        struct {
            char device[MAX_BUFFER_SIZE];
            char separator[MAX_BUFFER_SIZE];
            int16_t partition;
            char mount[MAX_BUFFER_SIZE];
            char type[MAX_BUFFER_SIZE];
            char label[MAX_BUFFER_SIZE];
        } boot;
        struct {
            char device[MAX_BUFFER_SIZE];
            char separator[MAX_BUFFER_SIZE];
            int16_t partition;
            char mount[MAX_BUFFER_SIZE];
            char type[MAX_BUFFER_SIZE];
            char label[MAX_BUFFER_SIZE];
        } rom;
        struct {
            char device[MAX_BUFFER_SIZE];
            char separator[MAX_BUFFER_SIZE];
            int16_t partition;
            char mount[MAX_BUFFER_SIZE];
            char type[MAX_BUFFER_SIZE];
            char label[MAX_BUFFER_SIZE];
        } root;
        struct {
            char device[MAX_BUFFER_SIZE];
            char separator[MAX_BUFFER_SIZE];
            int16_t partition;
            char mount[MAX_BUFFER_SIZE];
            char type[MAX_BUFFER_SIZE];
            char label[MAX_BUFFER_SIZE];
        } sdcard;
        struct {
            char device[MAX_BUFFER_SIZE];
            char separator[MAX_BUFFER_SIZE];
            int16_t partition;
            char mount[MAX_BUFFER_SIZE];
            char type[MAX_BUFFER_SIZE];
            char label[MAX_BUFFER_SIZE];
        } usb;
    } storage;

    struct {
        char dflt[MAX_BUFFER_SIZE];
        char available[MAX_BUFFER_SIZE];
        char governor[MAX_BUFFER_SIZE];
        char scaler[MAX_BUFFER_SIZE];
    } cpu;

    struct {
        char module[MAX_BUFFER_SIZE];
        char name[MAX_BUFFER_SIZE];
        char type[MAX_BUFFER_SIZE];
        char interface[MAX_BUFFER_SIZE];
        char state[MAX_BUFFER_SIZE];
    } network;

    struct {
        char device[MAX_BUFFER_SIZE];
        char hdmi[MAX_BUFFER_SIZE];
        int16_t bright;
        int16_t rotate;
        int16_t rotate_pivot_x;
        int16_t rotate_pivot_y;
        int16_t render_offset_x;
        int16_t render_offset_y;
        int16_t wait;
        int16_t width;
        int16_t height;
        float zoom;
        float zoom_width;
        float zoom_height;
        struct {
            int16_t width;
            int16_t height;
        } internal;
        struct {
            int16_t width;
            int16_t height;
        } external;
    } screen;

    struct {
        int16_t scaler;
        int16_t rotate;
    } sdl;

    struct {
        int16_t red;
        int16_t green;
        int16_t blue;
    } colour;

    struct {
        char capacity[MAX_BUFFER_SIZE];
        char health[MAX_BUFFER_SIZE];
        char voltage[MAX_BUFFER_SIZE];
        char charger[MAX_BUFFER_SIZE];
        int16_t volt_min;
        int16_t volt_max;
        int16_t size;
        struct {
            char charge[MAX_BUFFER_SIZE];
            char discharge[MAX_BUFFER_SIZE];
        } curve;
    } battery;
};

void load_device(struct mux_device *device);
