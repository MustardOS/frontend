#include <dlfcn.h>
#include <link.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "../settings/settings.h"
#include "clock.h"

typedef int (*clock_gettime_fn)(clockid_t, struct timespec *);
typedef int (*gettimeofday_fn)(struct timeval *, void *);

static uintptr_t core_start;
static uintptr_t core_end;
static time_t offset_seconds;
static int applied[6] = {-1, -1, -1, -1, -1, -1};

static clock_gettime_fn real_clock_gettime(void) {
    static clock_gettime_fn fn;
    if (!fn) {
        void *symbol = dlsym(RTLD_NEXT, "clock_gettime");
        memcpy(&fn, &symbol, sizeof(fn));
    }
    return fn;
}

static gettimeofday_fn real_gettimeofday(void) {
    static gettimeofday_fn fn;
    if (!fn) {
        void *symbol = dlsym(RTLD_NEXT, "gettimeofday");
        memcpy(&fn, &symbol, sizeof(fn));
    }
    return fn;
}

static int from_core(const void *caller) {
    const uintptr_t address = (uintptr_t) caller;
    if (address < core_start || address >= core_end) return 0;
    content_clock_apply();
    return offset_seconds != 0;
}

static int core_range(struct dl_phdr_info *info, size_t size, void *data) {
    (void) size;
    if (info->dlpi_addr != (uintptr_t) data) return 0;

    for (int i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr) *phdr = &info->dlpi_phdr[i];
        if (phdr->p_type != PT_LOAD) continue;

        const uintptr_t start = info->dlpi_addr + phdr->p_vaddr;
        const uintptr_t end = start + phdr->p_memsz;
        if (!core_start || start < core_start) core_start = start;
        if (end > core_end) core_end = end;
    }

    return 1;
}

void content_clock_register_core(void *handle) {
    Dl_info info;
    void *symbol = handle ? dlsym(handle, "retro_run") : NULL;

    core_start = 0;
    core_end = 0;
    if (!symbol || !dladdr(symbol, &info)) return;

    struct link_map *map = NULL;
    if (dlinfo(handle, RTLD_DI_LINKMAP, &map) != 0 || !map) return;

    dl_iterate_phdr(core_range, (void *) map->l_addr);
}

static time_t real_seconds(void) {
    struct timespec now = {0};
    clock_gettime_fn fn = real_clock_gettime();
    if (fn) fn(CLOCK_REALTIME, &now);
    return now.tv_sec;
}

void content_clock_apply(void) {
    const int current[6] = {session_settings.content_clock, session_settings.clock_year, session_settings.clock_month,
                            session_settings.clock_day,     session_settings.clock_hour, session_settings.clock_minute};
    if (memcmp(current, applied, sizeof(current)) == 0) return;
    memcpy(applied, current, sizeof(applied));

    if (!session_settings.content_clock) {
        offset_seconds = 0;
        return;
    }

    struct tm custom = {0};
    custom.tm_year = session_settings.clock_year - 1900;
    custom.tm_mon = session_settings.clock_month - 1;
    custom.tm_mday = session_settings.clock_day;
    custom.tm_hour = session_settings.clock_hour;
    custom.tm_min = session_settings.clock_minute;
    custom.tm_isdst = -1;

    const time_t target = mktime(&custom);
    offset_seconds = target == (time_t) -1 ? 0 : target - real_seconds();
}

time_t content_clock_now(void) {
    return real_seconds() + offset_seconds;
}

static int shift_realtime(const clockid_t id) {
    return id == CLOCK_REALTIME || id == CLOCK_REALTIME_COARSE;
}

__attribute__((visibility("default"))) int clock_gettime(const clockid_t id, struct timespec *ts) {
    clock_gettime_fn fn = real_clock_gettime();
    const int result = fn ? fn(id, ts) : -1;
    if (result == 0 && shift_realtime(id) && from_core(__builtin_return_address(0))) ts->tv_sec += offset_seconds;
    return result;
}

__attribute__((visibility("default"))) int gettimeofday(struct timeval *tv, void *tz) {
    gettimeofday_fn fn = real_gettimeofday();
    const int result = fn ? fn(tv, tz) : -1;
    if (result == 0 && from_core(__builtin_return_address(0))) tv->tv_sec += offset_seconds;
    return result;
}

__attribute__((visibility("default"))) time_t time(time_t *out) {
    time_t now = real_seconds();
    if (from_core(__builtin_return_address(0))) now += offset_seconds;
    if (out) *out = now;
    return now;
}
