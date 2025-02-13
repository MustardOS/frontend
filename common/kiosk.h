#pragma once

#include "options.h"

extern struct mux_kiosk kiosk;

struct mux_kiosk {
    struct {
        int16_t ARCHIVE;
        int16_t TASK;
    } APPLICATION;

    struct {
        int16_t CUSTOMISATION;
        int16_t LANGUAGE;
        int16_t CONNECTIVITY;
        int16_t NETWORK;
        int16_t STORAGE;
        int16_t WEB_SERVICES;
    } CONFIG;

    struct {
        int16_t ASSIGN_CORE;
        int16_t ASSIGN_GOVERNOR;
        int16_t OPTION;
        int16_t RETROARCH;
        int16_t SEARCH;
    } CONTENT;

    struct {
        int16_t CATALOGUE;
        int16_t CONFIGURATION;
        int16_t THEME;
    } CUSTOM;

    struct {
        int16_t CLOCK;
        int16_t TIMEZONE;
    } DATETIME;

    struct {
        int16_t APPLICATION;
        int16_t CONFIGURATION;
        int16_t EXPLORE;
        int16_t COLLECTION;
        int16_t HISTORY;
        int16_t INFORMATION;
    } LAUNCH;

    struct {
        int16_t ADVANCED;
        int16_t GENERAL;
        int16_t HDMI;
        int16_t POWER;
        int16_t VISUAL;
    } SETTING;
};

void load_kiosk(struct mux_kiosk *kiosk);
