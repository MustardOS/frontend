#pragma once

#include <curl/curl.h>

typedef struct {
    CURLcode (*global_init)(long flags);
    CURL *(*easy_init)(void);
    CURLcode (*easy_setopt)(CURL *curl, CURLoption option, ...);
    CURLcode (*easy_perform)(CURL *curl);
    CURLcode (*easy_getinfo)(CURL *curl, CURLINFO info, ...);
    void (*easy_cleanup)(CURL *curl);
    const char *(*easy_strerror)(CURLcode code);
} curl_api;

const curl_api *curl_api_get(void);
