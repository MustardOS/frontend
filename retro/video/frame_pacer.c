#include <string.h>
#include <time.h>
#include <SDL2/SDL.h>
#include "../../common/display.h"
#include "../input/hotkeys.h"
#include "../core/muxretro.h"
#include "../settings/settings.h"
#include "hw_render.h"

#define FRAME_PACER_EMERGENCY_AUDIO_MS 20
#define FRAME_PACER_MISS_RATIO         1.5
#define FRAME_PACER_OUTLIER_RATIO      3.0

#define FRAME_PACER_WORK_HISTORY 32
#define FRAME_PACER_MIN_SAMPLES  8

#define FRAME_PACER_FLIP_TARGET_NS 1500000.0

#define FRAME_PACER_MARGIN_MAX_NS    8000000.0
#define FRAME_PACER_MARGIN_GROW_NS   1000000.0
#define FRAME_PACER_MARGIN_SHRINK_NS 8000.0

#define FRAME_PACER_WORK_CEILING_RATIO 0.70

#define FRAME_PACER_SPIN_NS           300000ULL
#define FRAME_PACER_RESET_GAP_NS      250000000.0
#define FRAME_PACER_REFRESH_MIN_HZ    45.0
#define FRAME_PACER_REFRESH_MAX_HZ    75.0
#define FRAME_PACER_REFRESH_SMOOTHING 0.10

static double work_samples_ns[FRAME_PACER_WORK_HISTORY];
static int work_count = 0;
static int work_next = 0;

static double refresh_period_ns = 0.0;
static int refresh_period_known = 0;
static uint64_t last_present_counter = 0;
static int last_tick_missed = 0;
static double extra_margin_ns = 0.0;
static uint64_t measure_start_counter = 0;
static int measuring = 0;
static double last_delay_ns = 0.0;

static void sleep_ns_coarse(const uint64_t ns) {
    if (ns == 0) return;

    const struct timespec ts = {.tv_sec = (time_t) (ns / 1000000000ULL), .tv_nsec = (long) (ns % 1000000000ULL)};
    nanosleep(&ts, NULL);
}

static double perf_ns(const uint64_t counter_delta) {
    static double ns_per_tick = 0.0;
    if (ns_per_tick == 0.0) ns_per_tick = 1e9 / (double) SDL_GetPerformanceFrequency();
    return (double) counter_delta * ns_per_tick;
}

static void frame_pacer_reset_state(void) {
    memset(work_samples_ns, 0, sizeof(work_samples_ns));
    work_count = 0;
    work_next = 0;

    refresh_period_ns = 0.0;
    refresh_period_known = 0;
    last_present_counter = 0;
    last_tick_missed = 0;

    extra_margin_ns = 0.0;
    measure_start_counter = 0;
    measuring = 0;
    last_delay_ns = 0.0;
}

static int frame_pacer_timing_available(void) {
    if (session_settings.fps_limit != fps_limit_auto) return 0;
    if (hotkeys_is_fast_forward_active() || hotkeys_is_slow_motion_active()) return 0;
    if (audio_bridge_is_active() && audio_bridge_queued_ms() < FRAME_PACER_EMERGENCY_AUDIO_MS) return 0;

    return 1;
}

static int refresh_interval_plausible(const double interval_ns) {
    const double min_period_ns = 1e9 / FRAME_PACER_REFRESH_MAX_HZ;
    const double max_period_ns = 1e9 / FRAME_PACER_REFRESH_MIN_HZ;

    return interval_ns >= min_period_ns && interval_ns <= max_period_ns;
}

static void work_push(const double ns) {
    work_samples_ns[work_next] = ns;
    work_next = (work_next + 1) % FRAME_PACER_WORK_HISTORY;
    if (work_count < FRAME_PACER_WORK_HISTORY) work_count++;
}

static double work_worst_ns(void) {
    double worst = 0.0;
    for (int i = 0; i < work_count; i++)
        if (work_samples_ns[i] > worst) worst = work_samples_ns[i];

    return worst;
}

static void frame_pacer_wait(void) {
    last_delay_ns = 0.0;

    if (session_settings.frame_delay_ms == FRAME_DELAY_OFF) return;
    if (!audio_bridge_is_active()) return;
    if (!frame_pacer_timing_available()) return;

    uint64_t sleep_ns;

    if (session_settings.frame_delay_ms == FRAME_DELAY_AUTO) {
        if (hw_render_bridge_active()) return;

        if (!refresh_period_known || work_count < FRAME_PACER_MIN_SAMPLES || last_present_counter == 0) return;

        const double work_ns = work_worst_ns();
        if (work_ns >= refresh_period_ns * FRAME_PACER_WORK_CEILING_RATIO) return;

        const double budget_ns = refresh_period_ns - work_ns - FRAME_PACER_FLIP_TARGET_NS - extra_margin_ns;
        if (budget_ns <= 0.0) return;

        const double spent_ns = perf_ns(SDL_GetPerformanceCounter() - last_present_counter);
        if (spent_ns >= budget_ns) return;

        sleep_ns = (uint64_t) (budget_ns - spent_ns);
    } else {
        if (last_tick_missed) return;

        sleep_ns = (uint64_t) session_settings.frame_delay_ms * 1000000ULL;
        if (refresh_period_known && (double) sleep_ns > refresh_period_ns * 0.9)
            sleep_ns = (uint64_t) (refresh_period_ns * 0.9);
    }

    if (sleep_ns == 0) return;

    const uint64_t start = SDL_GetPerformanceCounter();
    const uint64_t spin_ns = sleep_ns > FRAME_PACER_SPIN_NS ? FRAME_PACER_SPIN_NS : sleep_ns;

    sleep_ns_coarse(sleep_ns - spin_ns);
    while (perf_ns(SDL_GetPerformanceCounter() - start) < (double) sleep_ns) {
    }

    last_delay_ns = perf_ns(SDL_GetPerformanceCounter() - start);
}

static void frame_pacer_begin_measure(void) {
    measure_start_counter = SDL_GetPerformanceCounter();
    measuring = 1;
}

void frame_pacer_maybe_wait(void) {
    if (!frame_pacer_timing_available()) return;

    frame_pacer_wait();
    frame_pacer_begin_measure();
}

void frame_pacer_after_present(void) {
    if (!frame_pacer_timing_available()) {
        last_present_counter = 0;
        measuring = 0;
        return;
    }

    const uint64_t now = SDL_GetPerformanceCounter();

    if (measuring) {
        measuring = 0;

        const double span_ns = perf_ns(now - measure_start_counter);
        const double flip_ns = display_take_flip_ms() * 1000000.0;
        work_push(span_ns > flip_ns ? span_ns - flip_ns : span_ns);
    }

    if (last_present_counter == 0) {
        last_present_counter = now;
        return;
    }

    const double interval_ns = perf_ns(now - last_present_counter);
    last_present_counter = now;

    if (interval_ns >= FRAME_PACER_RESET_GAP_NS) {
        frame_pacer_reset_state();
        last_present_counter = now;
        return;
    }

    if (!refresh_period_known) {
        if (!refresh_interval_plausible(interval_ns)) return;

        refresh_period_ns = interval_ns;
        refresh_period_known = 1;
        return;
    }

    if (interval_ns > refresh_period_ns * FRAME_PACER_OUTLIER_RATIO) return;

    last_tick_missed = interval_ns > refresh_period_ns * FRAME_PACER_MISS_RATIO;

    if (last_tick_missed) {
        extra_margin_ns += FRAME_PACER_MARGIN_GROW_NS;
        if (extra_margin_ns > FRAME_PACER_MARGIN_MAX_NS) extra_margin_ns = FRAME_PACER_MARGIN_MAX_NS;
        return;
    }

    extra_margin_ns -= FRAME_PACER_MARGIN_SHRINK_NS;
    if (extra_margin_ns < 0.0) extra_margin_ns = 0.0;

    if (!refresh_interval_plausible(interval_ns)) return;

    refresh_period_ns =
        refresh_period_ns * (1.0 - FRAME_PACER_REFRESH_SMOOTHING) + interval_ns * FRAME_PACER_REFRESH_SMOOTHING;
}

void frame_pacer_wait_until(const uint64_t deadline_counter) {
    uint64_t now = SDL_GetPerformanceCounter();
    if (now >= deadline_counter) return;

    uint64_t remaining_ns = (uint64_t) perf_ns(deadline_counter - now);
    const uint64_t spin_ns = remaining_ns > FRAME_PACER_SPIN_NS ? FRAME_PACER_SPIN_NS : remaining_ns;
    sleep_ns_coarse(remaining_ns - spin_ns);

    while ((now = SDL_GetPerformanceCounter()) < deadline_counter) {
    }
}

float frame_pacer_get_refresh_hz(void) {
    if (refresh_period_known && refresh_period_ns > 0.0) return (float) (1e9 / refresh_period_ns);
    return 60.0f;
}

float frame_pacer_get_delay_ms(void) {
    return (float) (last_delay_ns / 1000000.0);
}
