#include <dlfcn.h>
#include <pthread.h>
#include <common/runtime/log.h>
#include <common/storage/curl_api.h>

static curl_api api;
static int api_ready = 0;
static pthread_once_t api_once = PTHREAD_ONCE_INIT;

static void curl_api_load(void) {
    void *library = dlopen("libcurl.so.4", RTLD_NOW | RTLD_LOCAL);
    if (!library) {
        LOG_ERROR("curl", "Unable to load libcurl: %s", dlerror());
        return;
    }

    *(void **) &api.global_init = dlsym(library, "curl_global_init");
    *(void **) &api.easy_init = dlsym(library, "curl_easy_init");
    *(void **) &api.easy_setopt = dlsym(library, "curl_easy_setopt");
    *(void **) &api.easy_perform = dlsym(library, "curl_easy_perform");
    *(void **) &api.easy_getinfo = dlsym(library, "curl_easy_getinfo");
    *(void **) &api.easy_cleanup = dlsym(library, "curl_easy_cleanup");
    *(void **) &api.easy_strerror = dlsym(library, "curl_easy_strerror");

    api_ready = api.global_init && api.easy_init && api.easy_setopt && api.easy_perform && api.easy_getinfo
                && api.easy_cleanup && api.easy_strerror;

    if (!api_ready) {
        LOG_ERROR("curl", "libcurl is missing a required function");
        dlclose(library);
    }
}

const curl_api *curl_api_get(void) {
    pthread_once(&api_once, curl_api_load);
    return api_ready ? &api : NULL;
}
