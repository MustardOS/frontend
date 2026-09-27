#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <common/runtime/init.h>
#include <common/runtime/log.h>
#include "perf_interface.h"

#if defined(__linux__)
#include <sys/auxv.h>
#endif

#if defined(__aarch64__) || defined(__arm__)
#include <asm/hwcap.h>
#endif

#define PERF_INTERFACE_COUNTER_MAX 64

static struct retro_perf_counter *registered_counters[PERF_INTERFACE_COUNTER_MAX];
static size_t registered_counter_count;

static retro_time_t perf_time_usec(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return 0;
    return (retro_time_t) now.tv_sec * 1000000 + now.tv_nsec / 1000;
}

static retro_perf_tick_t perf_counter(void) {
    return SDL_GetPerformanceCounter();
}

static uint64_t cpu_features(void) {
    static int detected;
    static uint64_t cached_features;
    if (detected) return cached_features;

    uint64_t features = 0;

#if defined(__aarch64__)
    const unsigned long capabilities = getauxval(AT_HWCAP);
    if (capabilities & HWCAP_ASIMD) features |= RETRO_SIMD_NEON | RETRO_SIMD_ASIMD;
    if (capabilities & HWCAP_AES) features |= RETRO_SIMD_AES;
#elif defined(__arm__)
    const unsigned long capabilities = getauxval(AT_HWCAP);
#ifdef HWCAP_NEON
    if (capabilities & HWCAP_NEON) features |= RETRO_SIMD_NEON;
#endif
#ifdef HWCAP_VFPv3
    if (capabilities & HWCAP_VFPv3) features |= RETRO_SIMD_VFPV3;
#endif
#ifdef HWCAP_VFPv4
    if (capabilities & HWCAP_VFPv4) features |= RETRO_SIMD_VFPV4;
#endif
#elif defined(__x86_64__) || defined(__i386__)
    __builtin_cpu_init();
    if (__builtin_cpu_supports("sse")) features |= RETRO_SIMD_SSE;
    if (__builtin_cpu_supports("sse2")) features |= RETRO_SIMD_SSE2;
    if (__builtin_cpu_supports("sse3")) features |= RETRO_SIMD_SSE3;
    if (__builtin_cpu_supports("ssse3")) features |= RETRO_SIMD_SSSE3;
    if (__builtin_cpu_supports("sse4.1")) features |= RETRO_SIMD_SSE4;
    if (__builtin_cpu_supports("sse4.2")) features |= RETRO_SIMD_SSE42;
    if (__builtin_cpu_supports("avx")) features |= RETRO_SIMD_AVX;
    if (__builtin_cpu_supports("avx2")) features |= RETRO_SIMD_AVX2;
    if (__builtin_cpu_supports("aes")) features |= RETRO_SIMD_AES;
    if (__builtin_cpu_supports("popcnt")) features |= RETRO_SIMD_POPCNT;
    features |= RETRO_SIMD_CMOV;
#endif

    cached_features = features;
    detected = 1;
    return cached_features;
}

static void perf_register(struct retro_perf_counter *counter) {
    if (!counter || counter->registered || registered_counter_count >= PERF_INTERFACE_COUNTER_MAX) return;
    registered_counters[registered_counter_count++] = counter;
    counter->registered = true;
}

static void perf_start(struct retro_perf_counter *counter) {
    if (!counter || !counter->registered) return;
    counter->call_cnt++;
    counter->start = perf_counter();
}

static void perf_stop(struct retro_perf_counter *counter) {
    if (!counter || !counter->registered) return;
    counter->total += perf_counter() - counter->start;
}

static void perf_log(void) {
    for (size_t index = 0; index < registered_counter_count; index++) {
        const struct retro_perf_counter *counter = registered_counters[index];
        if (!counter || !counter->ident) continue;
        LOG_DEBUG(
            mux_module, "core perf: %s calls=%llu ticks=%llu", counter->ident, (unsigned long long) counter->call_cnt,
            (unsigned long long) counter->total
        );
    }
}

bool perf_interface_get(struct retro_perf_callback *callback) {
    if (!callback) return false;

    *callback = (struct retro_perf_callback){
        .get_time_usec = perf_time_usec,
        .get_cpu_features = cpu_features,
        .get_perf_counter = perf_counter,
        .perf_register = perf_register,
        .perf_start = perf_start,
        .perf_stop = perf_stop,
        .perf_log = perf_log,
    };
    return true;
}

void perf_interface_reset(void) {
    for (size_t index = 0; index < registered_counter_count; index++) {
        if (registered_counters[index]) registered_counters[index]->registered = false;
        registered_counters[index] = NULL;
    }
    registered_counter_count = 0;
}
