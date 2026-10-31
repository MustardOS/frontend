#include <pthread.h>
#include <time.h>
#include "muxretro.h"
#include "../video/hw_render.h"

static pthread_t worker;
static int worker_started;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

static void (*run_fn)(void);
static int requested;
static int busy;
static int stopping;
static double run_ms;

static unsigned env_cmd;
static void *env_data;
static int env_pending;
static bool env_result;

static double now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec * 1000.0 + (double) ts.tv_nsec / 1000000.0;
}

static void *worker_main(void *arg) {
    (void) arg;

    pthread_mutex_lock(&lock);
    for (;;) {
        while (!requested && !stopping)
            pthread_cond_wait(&cond, &lock);
        if (stopping) break;

        requested = 0;
        void (*fn)(void) = run_fn;
        pthread_mutex_unlock(&lock);

        const double start = now_ms();
        fn();
        const double elapsed = now_ms() - start;

        pthread_mutex_lock(&lock);
        run_ms = elapsed;
        busy = 0;
        pthread_cond_broadcast(&cond);
    }
    pthread_mutex_unlock(&lock);

    return NULL;
}

int run_thread_start(void) {
    if (worker_started) return 1;

    stopping = 0;
    requested = 0;
    busy = 0;
    env_pending = 0;

    if (pthread_create(&worker, NULL, worker_main, NULL) != 0) return 0;

    worker_started = 1;
    return 1;
}

void run_thread_stop(void) {
    if (!worker_started) return;

    run_thread_wait();

    pthread_mutex_lock(&lock);
    stopping = 1;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock);

    pthread_join(worker, NULL);
    worker_started = 0;
}

int run_thread_active(void) {
    return worker_started;
}

void run_thread_kick(void (*run)(void)) {
    pthread_mutex_lock(&lock);
    run_fn = run;
    busy = 1;
    requested = 1;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock);
}

double run_thread_wait(void) {
    pthread_mutex_lock(&lock);
    while (busy) {
        if (env_pending) {
            const unsigned cmd = env_cmd;
            void *data = env_data;
            pthread_mutex_unlock(&lock);

            const bool result = mux_retro_environment_cb(cmd, data);

            pthread_mutex_lock(&lock);
            env_result = result;
            env_pending = 0;
            pthread_cond_broadcast(&cond);
            continue;
        }

        pthread_cond_wait(&cond, &lock);
    }

    const double elapsed = run_ms;
    pthread_mutex_unlock(&lock);

    return elapsed;
}

static int on_worker(void) {
    return worker_started && pthread_equal(pthread_self(), worker);
}

static int environment_safe_on_worker(const unsigned cmd) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_GET_VARIABLE:
        case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        case RETRO_ENVIRONMENT_GET_OVERSCAN:
        case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
        case RETRO_ENVIRONMENT_GET_LANGUAGE:
        case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
        case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
        case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
        case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE:
        case RETRO_ENVIRONMENT_GET_THROTTLE_STATE:
        case RETRO_ENVIRONMENT_GET_PERF_INTERFACE:
        case RETRO_ENVIRONMENT_GET_INPUT_MAX_USERS:
            return 1;
        default:
            return 0;
    }
}

int run_thread_marshal(const unsigned cmd, void *data, bool *result) {
    if (!on_worker() || environment_safe_on_worker(cmd)) return 0;

    hw_render_bridge_worker_suspend();

    pthread_mutex_lock(&lock);
    env_cmd = cmd;
    env_data = data;
    env_pending = 1;
    pthread_cond_broadcast(&cond);
    while (env_pending)
        pthread_cond_wait(&cond, &lock);
    *result = env_result;
    pthread_mutex_unlock(&lock);

    hw_render_bridge_worker_resume();

    return 1;
}
