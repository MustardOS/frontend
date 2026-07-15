#pragma once

#include "options.h"

extern struct mux_kiosk kiosk;

struct mux_kiosk {
    int16_t enable;
    int16_t message;

    struct {
        int16_t archive;
        int16_t task;
    } application;

    struct {
        int16_t customisation;
        int16_t language;
        int16_t connectivity;
        int16_t network;
        int16_t storage;
        int16_t web_services;
        int16_t net_settings;
        int16_t proxy;
        int16_t backup;
    } config;

    struct {
        int16_t core;
        int16_t governor;
        int16_t control;
        int16_t tag;
        int16_t option;
        int16_t retroarch;
        int16_t colfilter;
        int16_t shader;
        int16_t overlay;
        int16_t remconfig;
        int16_t search;
        int16_t history;
    } content;

    struct {
        int16_t add_con;
        int16_t new_dir;
        int16_t remove;
        int16_t access;
    } collect;

    struct {
        int16_t catalogue;
        int16_t raconfig;
        int16_t theme;
        int16_t theme_down;
    } custom;

    struct {
        int16_t clock;
        int16_t timezone;
    } datetime;

    struct {
        int16_t application;
        int16_t configuration;
        int16_t explore;
        int16_t collection;
        int16_t history;
        int16_t information;
    } launch;

    struct {
        int16_t advanced;
        int16_t rgb;
        int16_t general;
        int16_t hdmi;
        int16_t power;
        int16_t visual;
    } setting;
};

void load_kiosk(struct mux_kiosk *kiosk);

void kiosk_denied(void);
