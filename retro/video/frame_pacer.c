#include <string.h>
#include <SDL2/SDL.h>
#include "../input/hotkeys.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"

#define FRAME_PACER_HISTORY            16
#define FRAME_PACER_MIN_SAMPLES        8
#define FRAME_PACER_EMERGENCY_AUDIO_MS 20
#define FRAME_PACER_MISS_RATIO         1.5

#define FRAME_PACER_OUTLIER_RATIO    3.0
#define FRAME_PACER_MARGIN_MIN_NS    300000
#define FRAME_PACER_MARGIN_MAX_NS    3000000
#define FRAME_PACER_MARGIN_GROW_NS   250000
#define FRAME_PACER_MARGIN_SHRINK_NS 15000
#define FRAME_PACER_STABLE_STREAK    90
#define FRAME_PACER_SPIN_NS          300000

typedef struct {
    uint64_t samples_ns[FRAME_PACER_HISTORY];
    int count;
    int next;
} rolling_ns_t;

static rolling_ns_t work_history;
static double refresh_period_ns = 0.0;
static int refresh_period_known = 0;
static uint64_t last_present_counter = 0;
static double safety_margin_ns = FRAME_PACER_MARGIN_MAX_NS;
static int stable_streak = 0;
static int last_tick_missed = 0;
static uint64_t measure_start_counter = 0;
static int measuring = 0;

static double perf_ns(const uint64_t counter_delta) {
    static double ns_per_tick = 0.0;
    if (ns_per_tick == 0.0) ns_per_tick = 1e9 / (double) SDL_GetPerformanceFrequency();
    return (double) counter_delta * ns_per_tick;
}

static void rolling_ns_push(rolling_ns_t *r, const uint64_t ns) {
    r->samples_ns[r->next] = ns;
    r->next = (r->next + 1) % FRAME_PACER_HISTORY;
    if (r->count < FRAME_PACER_HISTORY) r->count++;
}

static uint64_t rolling_ns_percentile95(const rolling_ns_t *r) {
    if (r->count == 0) return 0;

    uint64_t sorted[FRAME_PACER_HISTORY];
    memcpy(sorted, r->samples_ns, sizeof(uint64_t) * (size_t) r->count);

    for (int i = 1; i < r->count; i++) {
        const uint64_t key = sorted[i];
        int j = i - 1;
        while (j >= 0 && sorted[j] > key) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }

    int idx = (int) (0.95 * (double) (r->count - 1));
    if (idx >= r->count) idx = r->count - 1;
    return sorted[idx];
}

static void frame_pacer_begin_measure(void) {
    measure_start_counter = SDL_GetPerformanceCounter();
    measuring = 1;
}

void frame_pacer_maybe_wait(void) {
    frame_pacer_begin_measure();

    if (session_settings.frame_delay_ms == FRAME_DELAY_OFF) return;
    if (session_settings.fps_limit != fps_limit_60) return;
    if (hotkeys_is_fast_forward_active() || hotkeys_is_slow_motion_active()) return;
    if (!audio_bridge_is_active() || audio_bridge_queued_ms() < FRAME_PACER_EMERGENCY_AUDIO_MS) return;
    if (last_tick_missed) return;

    uint64_t sleep_ns;

    if (session_settings.frame_delay_ms == FRAME_DELAY_AUTO) {
        if (!refresh_period_known || work_history.count < FRAME_PACER_MIN_SAMPLES) return;

        const uint64_t work_ns = rolling_ns_percentile95(&work_history);
        if ((double) work_ns >= refresh_period_ns * 0.9) return;

        const double budget_ns = refresh_period_ns - (double) work_ns - safety_margin_ns;
        if (budget_ns <= 0.0) return;
        sleep_ns = (uint64_t) budget_ns;
    } else {
        sleep_ns = (uint64_t) session_settings.frame_delay_ms * 1000000ULL;
        if (refresh_period_known && (double) sleep_ns > refresh_period_ns * 0.9)
            sleep_ns = (uint64_t) (refresh_period_ns * 0.9);
    }

    if (sleep_ns == 0) return;

    const uint64_t start = SDL_GetPerformanceCounter();
    const uint64_t spin_ns = sleep_ns > FRAME_PACER_SPIN_NS ? FRAME_PACER_SPIN_NS : sleep_ns;
    const uint64_t coarse_ns = sleep_ns - spin_ns;

    if (coarse_ns > 1000000) SDL_Delay((uint32_t) (coarse_ns / 1000000));
    while (perf_ns(SDL_GetPerformanceCounter() - start) < (double) sleep_ns) {
    }
}

void frame_pacer_after_present(void) {
    if (measuring) {
        measuring = 0;
        rolling_ns_push(&work_history, (uint64_t) perf_ns(SDL_GetPerformanceCounter() - measure_start_counter));
    }

    const int applicable = session_settings.fps_limit == fps_limit_60 && !hotkeys_is_fast_forward_active()
                           && !hotkeys_is_slow_motion_active();

    const uint64_t now = SDL_GetPerformanceCounter();

    if (!applicable) {
        last_present_counter = 0;
        return;
    }

    if (last_present_counter == 0) {
        last_present_counter = now;
        return;
    }

    const double interval_ns = perf_ns(now - last_present_counter);
    last_present_counter = now;

    if (!refresh_period_known) {
        refresh_period_ns = interval_ns;
        refresh_period_known = 1;
        return;
    }

    if (interval_ns > refresh_period_ns * FRAME_PACER_OUTLIER_RATIO) return;
    last_tick_missed = interval_ns > refresh_period_ns * FRAME_PACER_MISS_RATIO;

    if (last_tick_missed) {
        safety_margin_ns += FRAME_PACER_MARGIN_GROW_NS;
        if (safety_margin_ns > FRAME_PACER_MARGIN_MAX_NS) safety_margin_ns = FRAME_PACER_MARGIN_MAX_NS;
        stable_streak = 0;
    } else if (++stable_streak >= FRAME_PACER_STABLE_STREAK) {
        stable_streak = 0;
        safety_margin_ns -= FRAME_PACER_MARGIN_SHRINK_NS;
        if (safety_margin_ns < FRAME_PACER_MARGIN_MIN_NS) safety_margin_ns = FRAME_PACER_MARGIN_MIN_NS;
    }

    refresh_period_ns = refresh_period_ns * 0.9 + interval_ns * 0.1;
}

float frame_pacer_get_refresh_hz(void) {
    if (refresh_period_known && refresh_period_ns > 0.0) return (float) (1e9 / refresh_period_ns);
    return 60.0f;
}
