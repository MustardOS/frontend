#include <stdatomic.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <curl/curl.h>
#include <SDL2/SDL.h>
#include "../../common/config.h"
#include "../../common/device.h"
#include "../../common/fileio.h"
#include "../../common/init.h"
#include "../../common/language.h"
#include "../../common/log.h"
#include "../../common/json/json.h"
#include "../../common/ui/nav.h"
#include "../core/core.h"
#include "../core/paths.h"
#include "../core/perf.h"
#include "../core/runahead.h"
#include "../ui/ui_loading.h"
#include "../ui/cheats.h"
#include "../state/patch.h"
#include "../core/muxretro.h"
#include "../video/hw_render.h"
#include "../video/image_writer.h"
#include "account.h"
#include "cache.h"
#include "cheevo.h"
#include "../state/vfs.h"
#include "vendor/rcheevos/include/rc_client.h"
#include "vendor/rcheevos/include/rc_error.h"
#include "vendor/rcheevos/include/rc_hash.h"
#include "vendor/rcheevos/src/rc_libretro.h"

#define CHEEVO_ACCOUNT_DIR              STORAGE_NETWORK "/cheevo"
#define CHEEVO_PREVIEW_DIR              CHEEVO_ACCOUNT_DIR "/previews"
#define CHEEVO_PREVIEW_CAP              16
#define CHEEVO_HTTP_CAP                 (2U * 1024U * 1024U)
#define CHEEVO_QUEUE_CAP                8
#define CHEEVO_PROGRESS_CAP             (16U * 1024U * 1024U)
#define CHEEVO_UNKNOWN_EMULATOR_WARNING "Warning: Unknown Emulator"
#define CHEEVO_LEADERBOARD_CAP          10
#define CHEEVO_MEMORY_DESCRIPTOR_CAP    256
#define CHEEVO_MEMORY_WAIT_FRAMES       120
#define CHEEVO_UNLOCK_TOAST_MS          3192
#define CHEEVO_FRAME_COMPLETIONS        1
#define CHEEVO_STARTUP_COMPLETIONS      4

typedef struct {
    char *url;
    char *post;
    rc_client_server_callback_t callback;
    void *callback_data;
    char cache_name[96];
    int cache_read;
    int cache_write;
    int cache_fallback;
    uint64_t queued_at;
} cheevo_http_request;

typedef struct {
    char *body;
    size_t body_size;
    long status;
    char error[256];
    rc_client_server_callback_t callback;
    void *callback_data;
    uint64_t queued_at;
    int cache_hit;
    int cache_miss;
    int cache_fallback;
} cheevo_http_completion;

typedef struct {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t wake;
    atomic_int stop;
    int running;
    cheevo_http_request requests[CHEEVO_QUEUE_CAP];
    unsigned request_head;
    unsigned request_count;
    cheevo_http_completion completions[CHEEVO_QUEUE_CAP];
    unsigned completion_head;
    unsigned completion_count;
    uint64_t active_queued_at;
    unsigned cache_hits;
    unsigned cache_misses;
    unsigned cache_fallbacks;
    unsigned queue_rejections;
} cheevo_http;

typedef struct {
    char *data;
    size_t size;
    size_t cap;
    int failed;
} cheevo_http_buffer;

static rc_client_t *client;
static rc_libretro_memory_regions_t memory_regions;
static struct retro_memory_descriptor *memory_descriptors;
static unsigned memory_descriptor_count;
static cheevo_http http_worker;
static cheevo_status status = cheevo_status_disabled;
static char content_file[PATH_MAX];
static char username[128];
static char token[256];
static char failure[256];
static int enabled;
static int hardcore_preference;
static int unofficial;
static int notifications;
static cheevo_achievement_sort achievement_sort;
static cheevo_achievement_view achievement_view;
static int core_supports_cheevo = 1;
static int netplay_active;
static uint32_t cheevo_last_idle;
static uint8_t *pending_progress;
static size_t pending_progress_size;
static int curl_ready;
static int reset_pending;
static uint32_t preview_game_id;
static uint32_t preview_achievement_ids[CHEEVO_PREVIEW_CAP];
static unsigned preview_achievement_count;
static uint64_t preview_queued_at;
static unsigned preview_drops;
static int cache_refresh_pending;
static int memory_available;
static int memory_initialisation_deferred;
static int memory_wait_frames;
static rc_client_async_handle_t *leaderboard_fetch_handle;
static cheevo_leaderboard_state leaderboard_state;
static cheevo_leaderboard_rank leaderboard_ranks[CHEEVO_LEADERBOARD_CAP];
static unsigned leaderboard_rank_count;
static unsigned leaderboard_total;
static uint32_t leaderboard_id;

static void leaderboard_reset(void);

static int text_safe(const char *value) {
    return value && !strchr(value, '\n') && !strchr(value, '\r');
}

static void preview_path(char *path, const size_t path_size, const uint32_t game_id, const uint32_t achievement_id) {
    snprintf(path, path_size, "%s/%u/%u.png", CHEEVO_PREVIEW_DIR, game_id, achievement_id);
}

static void preview_queue(const uint32_t game_id, const uint32_t achievement_id) {
    if (!game_id || !achievement_id) return;
    if (preview_game_id != game_id) {
        preview_drops += preview_achievement_count;
        preview_game_id = game_id;
        preview_achievement_count = 0;
        preview_queued_at = 0;
    }
    for (unsigned index = 0; index < preview_achievement_count; index++)
        if (preview_achievement_ids[index] == achievement_id) return;
    if (preview_achievement_count < CHEEVO_PREVIEW_CAP) {
        if (!preview_achievement_count) preview_queued_at = SDL_GetPerformanceCounter();
        preview_achievement_ids[preview_achievement_count++] = achievement_id;
    } else {
        preview_drops++;
    }
}

static void preview_capture(void) {
    if (!preview_achievement_count) return;
    const rc_client_game_t *game = client ? rc_client_get_game_info(client) : NULL;
    if (!game || game->id != preview_game_id) {
        preview_drops += preview_achievement_count;
        preview_achievement_count = 0;
        preview_queued_at = 0;
        return;
    }

    char directory[PATH_MAX];
    snprintf(directory, sizeof(directory), "%s/%u", CHEEVO_PREVIEW_DIR, preview_game_id);
    create_directories(directory, 0);

    char first_path[PATH_MAX];
    preview_path(first_path, sizeof(first_path), preview_game_id, preview_achievement_ids[0]);

    uint8_t *pixels = image_writer_available() ? image_writer_claim(device.screen.width, device.screen.height) : NULL;

    if (!pixels) {
        if (image_writer_available()) return;
        LOG_WARN(mux_module, "cheevo: achievement preview worker is unavailable");
        preview_drops += preview_achievement_count;
        preview_achievement_count = 0;
        preview_queued_at = 0;
        return;
    }

    if (pause_menu_capture_clean_pixels(pixels, 1) != 0) {
        image_writer_release();
        LOG_WARN(mux_module, "cheevo: achievement preview capture failed");
        preview_drops += preview_achievement_count;
        preview_achievement_count = 0;
        preview_queued_at = 0;
        return;
    }

    static char copies[IMAGE_WRITER_COPY_MAX][IMAGE_WRITER_PATH_MAX];
    static const char *copy_list[IMAGE_WRITER_COPY_MAX];
    unsigned copy_count = 0;
    for (unsigned index = 1; index < preview_achievement_count && copy_count < IMAGE_WRITER_COPY_MAX; index++) {
        preview_path(copies[copy_count], sizeof(copies[copy_count]), preview_game_id, preview_achievement_ids[index]);
        copy_list[copy_count] = copies[copy_count];
        copy_count++;
    }

    image_writer_commit(first_path, copy_list, copy_count);
    LOG_INFO(mux_module, "cheevo: capturing achievement preview '%s'", first_path);
    preview_achievement_count = 0;
    preview_queued_at = 0;
}

static void account_load(void) {
    cheevo_account account;
    const int result = cheevo_account_load(&account);
    if (result == -2) LOG_WARN(mux_module, "cheevo: refusing an insecure or invalid account file");
    enabled = account.enabled;
    hardcore_preference = account.hardcore;
    unofficial = account.unofficial;
    notifications = account.notifications;
    achievement_sort = account.achievement_sort;
    achievement_view = account.achievement_view;
    snprintf(username, sizeof(username), "%s", account.username);
    snprintf(token, sizeof(token), "%s", account.token);
    explicit_bzero(&account, sizeof(account));
}

static int account_save(void) {
    cheevo_account account = {
        .enabled = enabled,
        .hardcore = hardcore_preference,
        .unofficial = unofficial,
        .notifications = (cheevo_notification_mode) notifications,
        .achievement_sort = achievement_sort,
        .achievement_view = achievement_view,
    };
    snprintf(account.username, sizeof(account.username), "%s", username);
    snprintf(account.token, sizeof(account.token), "%s", token);
    const int result = cheevo_account_save(&account);
    explicit_bzero(&account, sizeof(account));
    return result;
}

static void account_delete(void) {
    cheevo_account_delete();
}
static size_t http_write(void *data, const size_t size, const size_t count, void *userdata) {
    cheevo_http_buffer *buffer = userdata;
    if (size != 0 && count > SIZE_MAX / size) return 0;
    const size_t incoming = size * count;
    if (incoming > CHEEVO_HTTP_CAP - buffer->size) {
        buffer->failed = 1;
        return 0;
    }

    const size_t needed = buffer->size + incoming + 1;
    if (needed > buffer->cap) {
        size_t cap = buffer->cap ? buffer->cap : 4096;
        while (cap < needed && cap < CHEEVO_HTTP_CAP + 1)
            cap *= 2;
        if (cap > CHEEVO_HTTP_CAP + 1) cap = CHEEVO_HTTP_CAP + 1;
        char *grown = realloc(buffer->data, cap);
        if (!grown) {
            buffer->failed = 1;
            return 0;
        }
        buffer->data = grown;
        buffer->cap = cap;
    }

    memcpy(buffer->data + buffer->size, data, incoming);
    buffer->size += incoming;
    buffer->data[buffer->size] = '\0';
    return incoming;
}

static int http_progress(void *userdata, curl_off_t a, curl_off_t b, curl_off_t c, curl_off_t d) {
    (void) a;
    (void) b;
    (void) c;
    (void) d;
    return atomic_load((atomic_int *) userdata) != 0;
}

static void request_free(cheevo_http_request *request) {
    free(request->url);
    if (request->post) explicit_bzero(request->post, strlen(request->post));
    free(request->post);
    memset(request, 0, sizeof(*request));
}

static cheevo_http_completion request_perform(cheevo_http_request *request) {
    cheevo_http_completion completion = {0};
    completion.callback = request->callback;
    completion.callback_data = request->callback_data;
    completion.queued_at = request->queued_at;

    if (request->cache_read && cheevo_cache_load(request->cache_name, &completion.body, &completion.body_size) == 0) {
        completion.status = 200;
        completion.cache_hit = 1;
        return completion;
    }
    completion.cache_miss = request->cache_read;

    CURL *curl = curl_easy_init();
    if (!curl) {
        snprintf(completion.error, sizeof(completion.error), "%s", lang.muxretro.cheevo.http_failed);
        return completion;
    }

    cheevo_http_buffer buffer = {0};
    char user_agent[256];
    char curl_error[CURL_ERROR_SIZE] = {0};
    snprintf(user_agent, sizeof(user_agent), "Pickles/1.0 (MustardOS) rcheevos/12.4.0");

    curl_easy_setopt(curl, CURLOPT_URL, request->url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 15000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 45000L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 100L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 10L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl, CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    static const char *certificate_bundles[] = {
        "/etc/ssl/certs/ca-certificates.crt", "/etc/ssl/cert.pem", "/etc/pki/tls/certs/ca-bundle.crt"
    };
    for (size_t index = 0; index < sizeof(certificate_bundles) / sizeof(certificate_bundles[0]); index++) {
        if (access(certificate_bundles[index], R_OK) == 0) {
            curl_easy_setopt(curl, CURLOPT_CAINFO, certificate_bundles[index]);
            break;
        }
    }
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, http_progress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &http_worker.stop);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
#endif

    if (request->post) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->post);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long) strlen(request->post));
    }
    if (config.settings.network.proxy_enabled && config.settings.network.proxy_server[0]) {
        curl_easy_setopt(curl, CURLOPT_PROXY, config.settings.network.proxy_server);
        if (config.settings.network.proxy_noproxy[0])
            curl_easy_setopt(curl, CURLOPT_NOPROXY, config.settings.network.proxy_noproxy);
    }

    const CURLcode result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &completion.status);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        snprintf(
            completion.error, sizeof(completion.error), "%s",
            buffer.failed   ? lang.muxretro.cheevo.response_too_large
            : curl_error[0] ? curl_error
                            : curl_easy_strerror(result)
        );
    }

    completion.body = buffer.data;
    completion.body_size = buffer.size;
    const int succeeded = result == CURLE_OK && completion.status >= 200 && completion.status < 300;
    if (succeeded && request->cache_write)
        cheevo_cache_store(request->cache_name, completion.body, completion.body_size);
    else if (!succeeded && request->cache_fallback) {
        char *cached_body = NULL;
        size_t cached_size = 0;
        if (cheevo_cache_load(request->cache_name, &cached_body, &cached_size) == 0) {
            if (completion.body) explicit_bzero(completion.body, completion.body_size);
            free(completion.body);
            completion.body = cached_body;
            completion.body_size = cached_size;
            completion.status = 200;
            completion.error[0] = '\0';
            completion.cache_fallback = 1;
        }
    }
    return completion;
}

static void completion_free(cheevo_http_completion *completion) {
    if (completion->body) explicit_bzero(completion->body, completion->body_size);
    free(completion->body);
    memset(completion, 0, sizeof(*completion));
}

static void *http_thread(void *userdata) {
    cheevo_http *worker = userdata;

    for (;;) {
        pthread_mutex_lock(&worker->mutex);
        while (!worker->request_count && !atomic_load(&worker->stop))
            pthread_cond_wait(&worker->wake, &worker->mutex);
        if (atomic_load(&worker->stop)) {
            pthread_mutex_unlock(&worker->mutex);
            break;
        }

        cheevo_http_request request = worker->requests[worker->request_head];
        memset(&worker->requests[worker->request_head], 0, sizeof(request));
        worker->request_head = (worker->request_head + 1) % CHEEVO_QUEUE_CAP;
        worker->request_count--;
        worker->active_queued_at = request.queued_at;
        pthread_mutex_unlock(&worker->mutex);

        cheevo_http_completion completion = request_perform(&request);
        request_free(&request);

        pthread_mutex_lock(&worker->mutex);
        while (worker->completion_count >= CHEEVO_QUEUE_CAP && !atomic_load(&worker->stop))
            pthread_cond_wait(&worker->wake, &worker->mutex);
        worker->active_queued_at = 0;
        if (!atomic_load(&worker->stop)) {
            const unsigned tail = (worker->completion_head + worker->completion_count) % CHEEVO_QUEUE_CAP;
            worker->completions[tail] = completion;
            worker->completion_count++;
            worker->cache_hits += completion.cache_hit;
            worker->cache_misses += completion.cache_miss;
            worker->cache_fallbacks += completion.cache_fallback;
        } else {
            completion_free(&completion);
        }
        pthread_mutex_unlock(&worker->mutex);
    }

    return NULL;
}

static int http_start(void) {
    memset(&http_worker, 0, sizeof(http_worker));
    pthread_mutex_init(&http_worker.mutex, NULL);
    pthread_cond_init(&http_worker.wake, NULL);
    atomic_store(&http_worker.stop, 0);
    if (pthread_create(&http_worker.thread, NULL, http_thread, &http_worker) != 0) {
        pthread_mutex_destroy(&http_worker.mutex);
        pthread_cond_destroy(&http_worker.wake);
        return -1;
    }
    http_worker.running = 1;
    return 0;
}

static void server_error_now(rc_client_server_callback_t callback, void *callback_data, const char *message) {
    rc_api_server_response_t response = {0};
    response.body = message;
    response.body_length = strlen(message);
    response.http_status_code = RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR;
    callback(&response, callback_data);
}

static void server_call(
    const rc_api_request_t *request, rc_client_server_callback_t callback, void *callback_data, rc_client_t *unused
) {
    (void) unused;
    if (!request || !request->url || strlen(request->url) > 2048
        || (request->post_data && strlen(request->post_data) > 65536)) {
        server_error_now(callback, callback_data, lang.muxretro.cheevo.request_rejected);
        return;
    }

    cheevo_http_request queued = {0};
    queued.url = strdup(request->url);
    queued.post = request->post_data ? strdup(request->post_data) : NULL;
    queued.callback = callback;
    queued.callback_data = callback_data;
    queued.queued_at = SDL_GetPerformanceCounter();
    if (!queued.url || (request->post_data && !queued.post)) {
        request_free(&queued);
        server_error_now(callback, callback_data, lang.muxretro.cheevo.request_memory_failed);
        return;
    }

    const int cache_policy = cheevo_cache_request_name(queued.post, queued.cache_name, sizeof(queued.cache_name));
    if (cache_policy) {
        queued.cache_read =
            (cache_policy == 1 && !cache_refresh_pending) || (cache_policy == 2 && status == cheevo_status_offline);
        queued.cache_write = 1;
        queued.cache_fallback = cache_policy == 2;
        if (cache_policy == 1) cache_refresh_pending = 0;
    }

    pthread_mutex_lock(&http_worker.mutex);
    if (http_worker.request_count >= CHEEVO_QUEUE_CAP || atomic_load(&http_worker.stop)) {
        http_worker.queue_rejections++;
        pthread_mutex_unlock(&http_worker.mutex);
        request_free(&queued);
        server_error_now(callback, callback_data, lang.muxretro.cheevo.request_queue_full);
        return;
    }

    const unsigned tail = (http_worker.request_head + http_worker.request_count) % CHEEVO_QUEUE_CAP;
    http_worker.requests[tail] = queued;
    http_worker.request_count++;
    pthread_cond_signal(&http_worker.wake);
    pthread_mutex_unlock(&http_worker.mutex);
}

static int http_drain_limited(const unsigned budget) {
    int drained = 0;
    unsigned taken = 0;
    for (;;) {
        if (budget && taken >= budget) break;

        pthread_mutex_lock(&http_worker.mutex);
        if (!http_worker.completion_count) {
            pthread_mutex_unlock(&http_worker.mutex);
            break;
        }

        cheevo_http_completion completion = http_worker.completions[http_worker.completion_head];
        memset(&http_worker.completions[http_worker.completion_head], 0, sizeof(completion));
        http_worker.completion_head = (http_worker.completion_head + 1) % CHEEVO_QUEUE_CAP;
        http_worker.completion_count--;
        pthread_cond_signal(&http_worker.wake);
        pthread_mutex_unlock(&http_worker.mutex);

        rc_api_server_response_t response = {0};
        if (completion.error[0])
            LOG_WARN(mux_module, "cheevo: HTTPS request failed: %s", completion.error);
        else if (completion.status < 200 || completion.status >= 300)
            LOG_WARN(mux_module, "cheevo: RetroAchievements returned HTTP %ld", completion.status);
        response.body = completion.error[0] ? completion.error : completion.body;
        response.body_length = completion.error[0] ? strlen(completion.error) : completion.body_size;
        response.http_status_code =
            completion.error[0] ? RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR : (int) completion.status;
        const uint64_t callback_start = perf_begin();
        completion.callback(&response, completion.callback_data);
        perf_end(perf_stage_cheevo_callback, callback_start);
        completion_free(&completion);
        drained = 1;
        taken++;
    }
    return drained;
}

static int http_drain(void) {
    return http_drain_limited(0);
}

static void http_stop(void) {
    if (!http_worker.running) return;
    atomic_store(&http_worker.stop, 1);
    pthread_mutex_lock(&http_worker.mutex);
    pthread_cond_broadcast(&http_worker.wake);
    pthread_mutex_unlock(&http_worker.mutex);
    pthread_join(http_worker.thread, NULL);
    http_worker.running = 0;
    http_drain();

    for (unsigned i = 0; i < CHEEVO_QUEUE_CAP; i++) {
        request_free(&http_worker.requests[i]);
        completion_free(&http_worker.completions[i]);
    }
    pthread_mutex_destroy(&http_worker.mutex);
    pthread_cond_destroy(&http_worker.wake);
}

static void log_message(const char *message, const rc_client_t *unused) {
    (void) unused;
    LOG_DEBUG(mux_module, "cheevo: %s", message);
}

static void memory_log_message(const char *message) {
    LOG_DEBUG(mux_module, "cheevo: memory: %s", message);
}

static void get_core_memory(const uint32_t id, rc_libretro_core_memory_info_t *info) {
    info->data = current_core.retro_get_memory_data ? current_core.retro_get_memory_data(id) : NULL;
    info->size = current_core.retro_get_memory_size ? current_core.retro_get_memory_size(id) : 0;
}

static int memory_init(rc_client_t *runtime_client) {
    if (!runtime_client) return 0;
    const rc_client_game_t *game = rc_client_get_game_info(runtime_client);
    const uint32_t console_id = game ? game->console_id : 0;
    struct retro_memory_map map = {memory_descriptors, memory_descriptor_count};
    memory_available =
        rc_libretro_memory_init(&memory_regions, memory_descriptor_count ? &map : NULL, get_core_memory, console_id);
    if (memory_available)
        LOG_INFO(
            mux_module, "cheevo: mapped %u achievement memory region(s), %zu bytes", memory_regions.count,
            memory_regions.total_size
        );
    return memory_available;
}

void cheevo_refresh_memory(void) {
    memory_initialisation_deferred = 0;
    memory_init(client);
}

static uint32_t
read_memory(const uint32_t address, uint8_t *buffer, const uint32_t bytes, rc_client_t *runtime_client) {
    if (!runtime_client || !buffer) return 0;
    if (!memory_available && !memory_initialisation_deferred && !memory_init(runtime_client))
        memory_initialisation_deferred = 1;
    if (!memory_available) {
        if (!rc_client_is_game_loaded(runtime_client)) {
            memset(buffer, 0, bytes);
            return bytes;
        }
        return 0;
    }
    return rc_libretro_memory_read(&memory_regions, address, buffer, bytes);
}

static void game_loaded(const int result, const char *error, rc_client_t *unused, void *userdata) {
    (void) unused;
    (void) userdata;
    if (result != RC_OK) {
        status = result == RC_NO_GAME_LOADED ? cheevo_status_unsupported : cheevo_status_failed;
        snprintf(failure, sizeof(failure), "%s", error ? error : rc_error_str(result));
        LOG_WARN(mux_module, "cheevo: content identification failed: %s", error ? error : rc_error_str(result));
        if (notifications) pause_menu_show_toast_timed(lang.muxretro.cheevo.identify_failed, tst_wait_s);
        return;
    }

    cheevo_refresh_memory();
    if (!memory_available) {
        status = cheevo_status_unsupported;
        snprintf(failure, sizeof(failure), "%s", lang.muxretro.cheevo.memory_unavailable);
        LOG_WARN(mux_module, "cheevo: no usable memory exposed by the core");
        if (notifications) pause_menu_show_toast_timed(lang.muxretro.cheevo.memory_unavailable, tst_wait_s);
        rc_client_unload_game(client);
        return;
    }
    failure[0] = '\0';
    const int allow_hardcore = hardcore_preference && core_active_patch_count == 0 && !netplay_active;
    rc_client_set_hardcore_enabled(client, allow_hardcore);
    cheats_set_suppressed(allow_hardcore);
    status = allow_hardcore ? cheevo_status_active_hardcore : cheevo_status_active_softcore;
    if (hardcore_preference && !allow_hardcore && core_active_patch_count > 0)
        pause_menu_show_toast_timed(lang.muxretro.cheevo.hardcore_patches, tst_wait_s);

    rc_client_user_game_summary_t summary = {0};
    const rc_client_game_t *game = rc_client_get_game_info(client);
    rc_client_get_user_game_summary(client, &summary);
    LOG_SUCCESS(
        mux_module, "cheevo: active for %s (%u of %u achievements already unlocked)",
        game && game->title ? game->title : "unknown content", summary.num_unlocked_achievements,
        summary.num_core_achievements
    );

    if (pending_progress) {
        const int restored = rc_client_deserialize_progress_sized(client, pending_progress, pending_progress_size);
        free(pending_progress);
        pending_progress = NULL;
        pending_progress_size = 0;
        if (restored != RC_OK)
            LOG_WARN(mux_module, "cheevo: deferred progress restore failed: %s", rc_error_str(restored));
    }

    if (notifications && config.visual.pickles_startup_messages)
        pause_menu_show_toast_timed(lang.muxretro.cheevo.active, tst_wait_s);
}

static int core_memory_ready(void) {
    if (memory_descriptor_count > 0) return 1;
    return current_core.retro_get_memory_data && current_core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM) != NULL;
}

static void begin_game_now(void) {
    leaderboard_reset();
    rc_libretro_memory_destroy(&memory_regions);
    memory_available = 0;
    memory_initialisation_deferred = 0;
    memory_wait_frames = 0;
    status = cheevo_status_identifying;
    rc_client_begin_identify_and_load_game(client, RC_CONSOLE_UNKNOWN, content_file, NULL, 0, game_loaded, NULL);
}

static void begin_game(void) {
    if (!client || !content_file[0] || netplay_active || !core_supports_cheevo) return;

    if (!core_memory_ready()) {
        memory_wait_frames = CHEEVO_MEMORY_WAIT_FRAMES;
        status = cheevo_status_identifying;
        LOG_INFO(mux_module, "cheevo: waiting for the core to map its memory before identifying");
        return;
    }

    begin_game_now();
}

static void login_complete(const int result, const char *error, rc_client_t *unused, void *userdata) {
    (void) unused;
    (void) userdata;
    if (result != RC_OK) {
        status = cheevo_status_signed_out;
        snprintf(failure, sizeof(failure), "%s", error ? error : rc_error_str(result));
        LOG_WARN(mux_module, "cheevo: login failed: %s", error ? error : rc_error_str(result));
        if (notifications) pause_menu_show_toast_timed(lang.muxretro.cheevo.sign_in_failed, tst_wait_s);
        return;
    }

    const rc_client_user_t *user = rc_client_get_user_info(client);
    failure[0] = '\0';
    if (user) {
        snprintf(username, sizeof(username), "%s", user->username ? user->username : "");
        snprintf(token, sizeof(token), "%s", user->token ? user->token : "");
        enabled = 1;
        if (account_save() != 0) {
            account_delete();
            LOG_WARN(mux_module, "cheevo: logged in, but the account token could not be stored securely");
            if (notifications) pause_menu_show_toast_timed(lang.muxretro.cheevo.account_save_failed, tst_wait_s);
        }
    }
    begin_game();
}

static void event_handler(const rc_client_event_t *event, rc_client_t *unused) {
    (void) unused;
    if (event->type == RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED) {
        const rc_client_game_t *game = rc_client_get_game_info(client);
        if (game) preview_queue(game->id, event->achievement->id);
        LOG_SUCCESS(
            mux_module, "cheevo: achievement unlocked: %s (id=%u, %u points)", event->achievement->title,
            event->achievement->id, event->achievement->points
        );
    }
    if (!notifications && event->type != RC_CLIENT_EVENT_RESET && event->type != RC_CLIENT_EVENT_SERVER_ERROR) return;
    char message[512];

    switch (event->type) {
        case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
            snprintf(
                message, sizeof(message), lang.muxretro.cheevo.unlock, event->achievement->title,
                event->achievement->points
            );
            pause_menu_show_glyph_toast_timed(message, "trophy", CHEEVO_UNLOCK_TOAST_MS);
            break;
        case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW:
        case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE:
            if (notifications < cheevo_notifications_detailed) break;
            snprintf(
                message, sizeof(message), lang.muxretro.cheevo.measured_progress, event->achievement->title,
                event->achievement->measured_progress
            );
            pause_menu_show_toast_timed(message, tst_wait_s);
            break;
        case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW:
            if (notifications < cheevo_notifications_detailed) break;
            snprintf(message, sizeof(message), lang.muxretro.cheevo.challenge_active, event->achievement->title);
            pause_menu_show_toast_timed(message, tst_wait_s);
            break;
        case RC_CLIENT_EVENT_LEADERBOARD_STARTED:
            if (notifications < cheevo_notifications_detailed) break;
            snprintf(message, sizeof(message), lang.muxretro.cheevo.leaderboard_started, event->leaderboard->title);
            pause_menu_show_toast_timed(message, tst_wait_s);
            break;
        case RC_CLIENT_EVENT_LEADERBOARD_FAILED:
            if (notifications < cheevo_notifications_detailed) break;
            snprintf(message, sizeof(message), lang.muxretro.cheevo.leaderboard_ended, event->leaderboard->title);
            pause_menu_show_toast_timed(message, tst_wait_s);
            break;
        case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
            if (notifications < cheevo_notifications_detailed) break;
            snprintf(message, sizeof(message), lang.muxretro.cheevo.leaderboard_submitted, event->leaderboard->title);
            pause_menu_show_toast_timed(message, tst_wait_s);
            break;
        case RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD:
            if (notifications < cheevo_notifications_detailed) break;
            snprintf(
                message, sizeof(message), lang.muxretro.cheevo.leaderboard_submitted,
                event->leaderboard_scoreboard->submitted_score
            );
            pause_menu_show_toast_timed(message, tst_wait_s);
            break;
        case RC_CLIENT_EVENT_GAME_COMPLETED:
            pause_menu_show_toast_timed(lang.muxretro.cheevo.game_completed, tst_wait_s);
            break;
        case RC_CLIENT_EVENT_SUBSET_COMPLETED:
            snprintf(message, sizeof(message), lang.muxretro.cheevo.subset_completed, event->subset->title);
            pause_menu_show_toast_timed(message, tst_wait_s);
            break;
        case RC_CLIENT_EVENT_RESET:
            reset_pending = 1;
            break;
        case RC_CLIENT_EVENT_DISCONNECTED:
            status = cheevo_status_offline;
            pause_menu_show_toast_timed(lang.muxretro.cheevo.offline_retry, tst_wait_s);
            break;
        case RC_CLIENT_EVENT_RECONNECTED:
            status =
                rc_client_get_hardcore_enabled(client) ? cheevo_status_active_hardcore : cheevo_status_active_softcore;
            pause_menu_show_toast_timed(lang.muxretro.cheevo.reconnected, tst_wait_s);
            break;
        case RC_CLIENT_EVENT_SERVER_ERROR:
            LOG_WARN(mux_module, "cheevo: %s failed: %s", event->server_error->api, event->server_error->error_message);
            break;
        default:
            break;
    }
}

static void RC_CCONV leaderboard_entries_loaded(
    const int result, const char *error, rc_client_leaderboard_entry_list_t *list, rc_client_t *unused, void *userdata
) {
    (void) unused;
    (void) userdata;
    leaderboard_fetch_handle = NULL;
    leaderboard_rank_count = 0;
    leaderboard_total = 0;
    if (result != RC_OK || !list) {
        leaderboard_state = cheevo_leaderboard_failed;
        LOG_WARN(mux_module, "cheevo: leaderboard details failed: %s", error ? error : rc_error_str(result));
        if (list) rc_client_destroy_leaderboard_entry_list(list);
        return;
    }

    leaderboard_total = list->total_entries;
    leaderboard_rank_count = list->num_entries < CHEEVO_LEADERBOARD_CAP ? list->num_entries : CHEEVO_LEADERBOARD_CAP;
    for (unsigned index = 0; index < leaderboard_rank_count; index++) {
        const rc_client_leaderboard_entry_t *source = &list->entries[index];
        cheevo_leaderboard_rank *entry = &leaderboard_ranks[index];
        memset(entry, 0, sizeof(*entry));
        entry->rank = source->rank;
        snprintf(entry->user, sizeof(entry->user), "%s", source->user ? source->user : "");
        snprintf(entry->score, sizeof(entry->score), "%s", source->display);
        entry->current_user = list->user_index == (int32_t) index;
    }
    leaderboard_state = cheevo_leaderboard_ready;
    rc_client_destroy_leaderboard_entry_list(list);
}

static void leaderboard_reset(void) {
    if (client && leaderboard_fetch_handle) rc_client_abort_async(client, leaderboard_fetch_handle);
    leaderboard_fetch_handle = NULL;
    leaderboard_state = cheevo_leaderboard_idle;
    leaderboard_rank_count = 0;
    leaderboard_total = 0;
    leaderboard_id = 0;
    memset(leaderboard_ranks, 0, sizeof(leaderboard_ranks));
}

static void *hash_file_open(const char *path) {
    return vfs_bridge_interface()->open(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
}

static void hash_file_seek(void *file_handle, const int64_t offset, const int origin) {
    const int position = origin == SEEK_CUR   ? RETRO_VFS_SEEK_POSITION_CURRENT
                         : origin == SEEK_END ? RETRO_VFS_SEEK_POSITION_END
                                              : RETRO_VFS_SEEK_POSITION_START;

    vfs_bridge_interface()->seek(file_handle, offset, position);
}

static int64_t hash_file_tell(void *file_handle) {
    return vfs_bridge_interface()->tell(file_handle);
}

static size_t hash_file_read(void *file_handle, void *buffer, const size_t requested_bytes) {
    const int64_t got = vfs_bridge_interface()->read(file_handle, buffer, requested_bytes);
    return got < 0 ? 0 : (size_t) got;
}

static void hash_file_close(void *file_handle) {
    vfs_bridge_interface()->close(file_handle);
}

static rc_hash_filereader_t hash_reader = {
    .open = hash_file_open,
    .seek = hash_file_seek,
    .tell = hash_file_tell,
    .read = hash_file_read,
    .close = hash_file_close,
};

int cheevo_hash_content(const char *content_path, char out[33]) {
    out[0] = '\0';
    if (!content_path || !content_path[0]) return 0;

    rc_hash_iterator_t iterator;
    rc_hash_initialize_iterator(&iterator, content_path, NULL, 0);

    memcpy(&iterator.callbacks.filereader, &hash_reader, sizeof(hash_reader));

    const int ok = rc_hash_iterate(out, &iterator);
    rc_hash_destroy_iterator(&iterator);

    return ok;
}

static int runtime_start(void) {
    if (client) return 0;
    if (!curl_ready) {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
            status = cheevo_status_failed;
            snprintf(failure, sizeof(failure), "%s", lang.muxretro.cheevo.secure_network_failed);
            return -1;
        }
        curl_ready = 1;
    }
    if (http_start() != 0) {
        status = cheevo_status_failed;
        snprintf(failure, sizeof(failure), "%s", lang.muxretro.cheevo.network_worker_failed);
        return -1;
    }

    client = rc_client_create(read_memory, server_call);
    if (!client) {
        http_stop();
        status = cheevo_status_failed;
        snprintf(failure, sizeof(failure), "%s", lang.muxretro.cheevo.client_failed);
        return -1;
    }

    rc_hash_init_custom_filereader(&hash_reader);

    rc_client_enable_logging(client, RC_CLIENT_LOG_LEVEL_WARN, log_message);
    rc_libretro_init_verbose_message_callback(memory_log_message);
    rc_client_set_event_handler(client, event_handler);
    rc_client_set_hardcore_enabled(client, 0);
    rc_client_set_unofficial_enabled(client, unofficial);
    rc_client_set_allow_background_memory_reads(client, 0);
    return 0;
}

static void runtime_stop(void) {
    leaderboard_reset();
    if (client) rc_client_unload_game(client);
    http_stop();
    if (client) rc_client_destroy(client);
    client = NULL;
    reset_pending = 0;
}

int cheevo_init(const char *content_path) {
    account_load();
    failure[0] = '\0';
    snprintf(content_file, sizeof(content_file), "%s", content_path ? content_path : "");
    status = enabled ? cheevo_status_signed_out : cheevo_status_disabled;
    if (!enabled) return 0;
    if (runtime_start() != 0) return -1;

    if (enabled && username[0] && token[0]) {
        status = cheevo_status_signing_in;
        rc_client_begin_login_with_token(client, username, token, login_complete, NULL);
    }
    return 0;
}

void cheevo_shutdown(void) {
    runtime_stop();
    if (curl_ready) curl_global_cleanup();
    curl_ready = 0;
    rc_libretro_memory_destroy(&memory_regions);
    free(memory_descriptors);
    memory_descriptors = NULL;
    memory_descriptor_count = 0;
    free(pending_progress);
    pending_progress = NULL;
    pending_progress_size = 0;
    preview_game_id = 0;
    preview_achievement_count = 0;
    preview_queued_at = 0;
    preview_drops = 0;
    cache_refresh_pending = 0;
    memory_available = 0;
    memory_initialisation_deferred = 0;
    explicit_bzero(token, sizeof(token));
    explicit_bzero(username, sizeof(username));
    explicit_bzero(content_file, sizeof(content_file));
    explicit_bzero(failure, sizeof(failure));
    status = cheevo_status_disabled;
}

static void cheevo_note_performance(void) {
    if (!perf_is_enabled() || !http_worker.running) return;

    perf_cheevo_snapshot snapshot = {0};
    uint64_t oldest = preview_queued_at;
    pthread_mutex_lock(&http_worker.mutex);
    snapshot.request_queue = http_worker.request_count;
    snapshot.completion_queue = http_worker.completion_count;
    snapshot.cache_hits = http_worker.cache_hits;
    snapshot.cache_misses = http_worker.cache_misses;
    snapshot.cache_fallbacks = http_worker.cache_fallbacks;
    snapshot.queue_rejections = http_worker.queue_rejections;
    if (http_worker.active_queued_at && (!oldest || http_worker.active_queued_at < oldest))
        oldest = http_worker.active_queued_at;
    if (http_worker.request_count) {
        const uint64_t queued = http_worker.requests[http_worker.request_head].queued_at;
        if (queued && (!oldest || queued < oldest)) oldest = queued;
    }
    if (http_worker.completion_count) {
        const uint64_t queued = http_worker.completions[http_worker.completion_head].queued_at;
        if (queued && (!oldest || queued < oldest)) oldest = queued;
    }
    pthread_mutex_unlock(&http_worker.mutex);

    snapshot.preview_queue = preview_achievement_count;
    snapshot.preview_drops = preview_drops;
    if (oldest) {
        const uint64_t frequency = SDL_GetPerformanceFrequency();
        if (frequency) {
            const double milliseconds = (double) (SDL_GetPerformanceCounter() - oldest) * 1000.0 / (double) frequency;
            snapshot.oldest_job_ms = milliseconds < (double) UINT_MAX ? (unsigned) milliseconds : UINT_MAX;
        }
    }
    perf_note_cheevo(&snapshot);
}

void cheevo_present_tick(void) {
    if (!client) return;
    if (!cheevo_is_starting()) {
        http_drain_limited(CHEEVO_FRAME_COMPLETIONS);
        preview_capture();
    }
    cheevo_note_performance();
}

void cheevo_tick(void) {
    if (!client) return;
    if (cheevo_is_starting() && http_drain_limited(CHEEVO_STARTUP_COMPLETIONS)) rc_client_idle(client);
    if (reset_pending) {
        reset_pending = 0;
        hw_render_bridge_enter_core_call();
        if (current_core.retro_reset) current_core.retro_reset();
        hw_render_bridge_exit_core_call();
        audio_bridge_clear_queued();
        runahead_invalidate();
        rc_client_reset(client);
    }
}

static void memory_wait_tick(void) {
    if (memory_wait_frames <= 0) return;

    if (core_memory_ready()) {
        LOG_INFO(mux_module, "cheevo: core memory is mapped, identifying now");
        begin_game_now();
        return;
    }

    if (--memory_wait_frames > 0) return;

    LOG_WARN(mux_module, "cheevo: core never mapped its memory, identifying anyway");
    begin_game_now();
}

void cheevo_do_frame(void) {
    if (!client || netplay_active) return;

    memory_wait_tick();

    if (status == cheevo_status_active_softcore || status == cheevo_status_active_hardcore
        || status == cheevo_status_offline)
        rc_client_do_frame(client);
}

void cheevo_idle(void) {
    if (!client || netplay_active) return;
    const uint32_t now = SDL_GetTicks();
    if (now - cheevo_last_idle < 1000) return;
    cheevo_last_idle = now;
    rc_client_idle(client);
}

int cheevo_is_starting(void) {
    return status == cheevo_status_signing_in || status == cheevo_status_identifying;
}

void cheevo_play_without(void) {
    runtime_stop();
    status = cheevo_status_disabled;
    reset_pending = 0;
}

void cheevo_reset(void) {
    if (!client) return;
    rc_client_reset(client);
    cheevo_refresh_memory();
}

void cheevo_set_memory_map(const struct retro_memory_map *map) {
    free(memory_descriptors);
    memory_descriptors = NULL;
    memory_descriptor_count = 0;
    rc_libretro_memory_destroy(&memory_regions);
    memory_available = 0;
    memory_initialisation_deferred = 0;
    if (map && map->descriptors && map->num_descriptors > CHEEVO_MEMORY_DESCRIPTOR_CAP)
        LOG_WARN(
            mux_module, "cheevo: ignoring a %u entry memory map, achievements will read the core memory blocks instead",
            map->num_descriptors
        );

    if (!map || !map->descriptors || !map->num_descriptors || map->num_descriptors > CHEEVO_MEMORY_DESCRIPTOR_CAP) {
        cheevo_refresh_memory();
        return;
    }

    memory_descriptors = malloc(sizeof(*memory_descriptors) * map->num_descriptors);
    if (!memory_descriptors) {
        LOG_WARN(mux_module, "cheevo: could not copy the core memory map, reading the core memory blocks instead");
        cheevo_refresh_memory();
        return;
    }

    memcpy(memory_descriptors, map->descriptors, sizeof(*memory_descriptors) * map->num_descriptors);
    memory_descriptor_count = map->num_descriptors;
    cheevo_refresh_memory();
}

void cheevo_set_core_support(const int supported) {
    core_supports_cheevo = supported;
    if (!supported && client && rc_client_is_game_loaded(client)) {
        rc_client_unload_game(client);
        status = cheevo_status_unsupported;
    }
}

int cheevo_core_support(void) {
    return core_supports_cheevo;
}

void cheevo_set_netplay_active(const int active) {
    netplay_active = active != 0;
    if (client) {
        rc_client_set_spectator_mode_enabled(client, netplay_active);
        rc_client_set_hardcore_enabled(client, netplay_active ? 0 : hardcore_preference);
        cheats_set_suppressed(netplay_active || hardcore_preference);
        if (!netplay_active && rc_client_is_game_loaded(client))
            status = hardcore_preference ? cheevo_status_active_hardcore : cheevo_status_active_softcore;
    }
}

int cheevo_netplay_active(void) {
    return netplay_active;
}

cheevo_status cheevo_get_status(void) {
    return status;
}

int cheevo_is_configured(void) {
    return enabled && username[0] && token[0];
}

void cheevo_get_info(cheevo_info *info) {
    if (!info) return;
    memset(info, 0, sizeof(*info));
    info->enabled = enabled;
    info->hardcore = cheevo_hardcore_active();
    info->unofficial = unofficial;
    info->notifications = notifications;
    snprintf(info->username, sizeof(info->username), "%s", username);
    snprintf(info->failure, sizeof(info->failure), "%s", failure);

    if (!client) return;
    const rc_client_user_t *user = rc_client_get_user_info(client);
    if (user) {
        snprintf(info->display_name, sizeof(info->display_name), "%s", user->display_name ? user->display_name : "");
        info->score = user->score;
    }
    const rc_client_game_t *game = rc_client_get_game_info(client);
    if (game && game->title) snprintf(info->game_title, sizeof(info->game_title), "%s", game->title);
    if (rc_client_has_rich_presence(client))
        rc_client_get_rich_presence_message(client, info->rich_presence, sizeof(info->rich_presence));
    rc_client_user_game_summary_t summary = {0};
    rc_client_get_user_game_summary(client, &summary);
    info->unlocked = summary.num_unlocked_achievements;
    info->total = summary.num_core_achievements;
}

unsigned cheevo_game_entries(const cheevo_game_entry_type type, cheevo_game_entry *entries, const unsigned capacity) {
    if (!client || !entries || !capacity || !rc_client_is_game_loaded(client)) return 0;

    unsigned count = 0;
    const rc_client_game_t *game = rc_client_get_game_info(client);
    if (type == cheevo_game_entry_achievement) {
        const int category =
            unofficial ? RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL : RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE;
        rc_client_achievement_list_t *achievements =
            rc_client_create_achievement_list(client, category, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
        if (achievements) {
            for (uint32_t bucket = 0; bucket < achievements->num_buckets && count < capacity; bucket++) {
                const rc_client_achievement_bucket_t *group = &achievements->buckets[bucket];
                for (uint32_t index = 0; index < group->num_achievements && count < capacity; index++) {
                    const rc_client_achievement_t *source = group->achievements[index];
                    if (!source || source->id == 0
                        || (source->title && strcmp(source->title, CHEEVO_UNKNOWN_EMULATOR_WARNING) == 0))
                        continue;
                    cheevo_game_entry *entry = &entries[count++];
                    memset(entry, 0, sizeof(*entry));
                    entry->type = cheevo_game_entry_achievement;
                    entry->id = source->id;
                    snprintf(
                        entry->title, sizeof(entry->title), "%s",
                        source->title ? source->title : lang.muxretro.cheevo.achievement
                    );
                    snprintf(
                        entry->description, sizeof(entry->description), "%s",
                        source->description ? source->description : ""
                    );
                    snprintf(entry->progress, sizeof(entry->progress), "%s", source->measured_progress);
                    entry->points = source->points;
                    const int hardcore = cheevo_hardcore_active();
                    entry->rarity = hardcore ? source->rarity_hardcore : source->rarity;
                    entry->unlocked = hardcore ? (source->unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE) != 0
                                               : source->unlocked != RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE;
                    if (game && game->id)
                        preview_path(entry->preview_path, sizeof(entry->preview_path), game->id, source->id);
                }
            }
            rc_client_destroy_achievement_list(achievements);
        }
        return count;
    }

    rc_client_leaderboard_list_t *leaderboards =
        rc_client_create_leaderboard_list(client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
    if (leaderboards) {
        for (uint32_t bucket = 0; bucket < leaderboards->num_buckets && count < capacity; bucket++) {
            const rc_client_leaderboard_bucket_t *group = &leaderboards->buckets[bucket];
            for (uint32_t index = 0; index < group->num_leaderboards && count < capacity; index++) {
                const rc_client_leaderboard_t *source = group->leaderboards[index];
                if (!source || !source->id) continue;
                cheevo_game_entry *entry = &entries[count++];
                memset(entry, 0, sizeof(*entry));
                entry->type = cheevo_game_entry_leaderboard;
                entry->id = source->id;
                snprintf(
                    entry->title, sizeof(entry->title), "%s",
                    source->title ? source->title : lang.muxretro.cheevo.leaderboard
                );
                snprintf(
                    entry->description, sizeof(entry->description), "%s", source->description ? source->description : ""
                );
                snprintf(
                    entry->progress, sizeof(entry->progress), "%s", source->tracker_value ? source->tracker_value : ""
                );
                entry->active = source->state == RC_CLIENT_LEADERBOARD_STATE_ACTIVE
                                || source->state == RC_CLIENT_LEADERBOARD_STATE_TRACKING;
            }
        }
        rc_client_destroy_leaderboard_list(leaderboards);
    }

    return count;
}

int cheevo_leaderboard_fetch(const uint32_t requested_id) {
    if (!client || !requested_id || !rc_client_is_game_loaded(client)) return -1;
    if (leaderboard_state == cheevo_leaderboard_ready && leaderboard_id == requested_id) return 0;
    if (leaderboard_state == cheevo_leaderboard_loading && leaderboard_id == requested_id) return 0;
    if (leaderboard_fetch_handle) rc_client_abort_async(client, leaderboard_fetch_handle);
    leaderboard_fetch_handle = NULL;
    leaderboard_rank_count = 0;
    leaderboard_total = 0;
    leaderboard_id = requested_id;
    leaderboard_state = cheevo_leaderboard_loading;
    leaderboard_fetch_handle = rc_client_begin_fetch_leaderboard_entries(
        client, requested_id, 1, CHEEVO_LEADERBOARD_CAP, leaderboard_entries_loaded, NULL
    );
    return leaderboard_state == cheevo_leaderboard_failed ? -1 : 0;
}

cheevo_leaderboard_state cheevo_leaderboard_get_state(void) {
    return leaderboard_state;
}

unsigned cheevo_leaderboard_ranks(cheevo_leaderboard_rank *entries, const unsigned capacity, unsigned *total) {
    if (total) *total = leaderboard_total;
    if (leaderboard_state != cheevo_leaderboard_ready || !entries || !capacity) return 0;
    const unsigned count = leaderboard_rank_count < capacity ? leaderboard_rank_count : capacity;
    memcpy(entries, leaderboard_ranks, count * sizeof(*entries));
    return count;
}

int cheevo_login(const char *new_username, const char *password) {
    if (!text_safe(new_username) || !password || !new_username[0] || !password[0]) return -1;
    if (runtime_start() != 0) return -1;
    failure[0] = '\0';
    status = cheevo_status_signing_in;
    rc_client_begin_login_with_password(client, new_username, password, login_complete, NULL);
    return 0;
}

void cheevo_logout(void) {
    if (client) {
        rc_client_unload_game(client);
        rc_client_logout(client);
    }
    runtime_stop();
    username[0] = '\0';
    token[0] = '\0';
    enabled = 0;
    failure[0] = '\0';
    status = cheevo_status_disabled;
    if (account_save() != 0) account_delete();
}

int cheevo_set_enabled(const int new_enabled) {
    enabled = new_enabled != 0;
    if (!enabled) {
        runtime_stop();
        status = cheevo_status_disabled;
    } else if (username[0] && token[0] && runtime_start() == 0) {
        status = cheevo_status_signing_in;
        rc_client_begin_login_with_token(client, username, token, login_complete, NULL);
    } else {
        status = cheevo_status_signed_out;
    }
    return account_save();
}

int cheevo_set_hardcore(const int new_enabled) {
    if (!client || !rc_client_is_game_loaded(client) || netplay_active || (new_enabled && core_active_patch_count > 0))
        return -1;
    hardcore_preference = new_enabled != 0;
    rc_client_set_hardcore_enabled(client, hardcore_preference);
    cheats_set_suppressed(hardcore_preference);
    status = hardcore_preference ? cheevo_status_active_hardcore : cheevo_status_active_softcore;
    account_save();
    return 0;
}

int cheevo_set_unofficial(const int new_enabled) {
    unofficial = new_enabled != 0;
    if (client) {
        rc_client_set_unofficial_enabled(client, unofficial);
        if (rc_client_is_game_loaded(client)) {
            rc_client_unload_game(client);
            begin_game();
        }
    }
    return account_save();
}

int cheevo_set_notifications(const cheevo_notification_mode mode) {
    notifications = mode;
    if (notifications < cheevo_notifications_disabled)
        notifications = cheevo_notifications_disabled;
    else if (notifications > cheevo_notifications_detailed)
        notifications = cheevo_notifications_detailed;
    return account_save();
}

cheevo_achievement_sort cheevo_get_achievement_sort(void) {
    return achievement_sort;
}

cheevo_achievement_view cheevo_get_achievement_view(void) {
    return achievement_view;
}

int cheevo_set_achievement_preferences(const cheevo_achievement_sort sort, const cheevo_achievement_view view) {
    cheevo_achievement_sort next_sort = sort;
    if (next_sort < cheevo_sort_alphanumeric_ascending || next_sort >= cheevo_sort_count)
        next_sort = cheevo_sort_alphanumeric_ascending;
    cheevo_achievement_view next_view = view;
    if (next_view < cheevo_view_achievements || next_view >= cheevo_view_count) next_view = cheevo_view_achievements;
    if (achievement_sort == next_sort && achievement_view == next_view) return 0;
    achievement_sort = next_sort;
    achievement_view = next_view;
    return account_save();
}

int cheevo_refresh_data(void) {
    if (!client || !username[0] || !content_file[0] || netplay_active || !core_supports_cheevo || cheevo_is_starting())
        return -1;
    cache_refresh_pending = 1;
    rc_client_unload_game(client);
    begin_game();
    return 0;
}

int cheevo_hardcore_active(void) {
    return client && rc_client_get_hardcore_enabled(client);
}

int cheevo_restricted(void) {
    return cheevo_hardcore_active();
}

int cheevo_can_pause(uint32_t *frames_remaining) {
    return !client || !cheevo_hardcore_active() || rc_client_can_pause(client, frames_remaining);
}

size_t cheevo_progress_size(void) {
    if (!client || !rc_client_is_game_loaded(client)) return 0;
    return rc_client_progress_size(client);
}

int cheevo_progress_save(void *data, const size_t size) {
    if (!client || !data) return -1;
    return rc_client_serialize_progress_sized(client, data, size) == RC_OK ? 0 : -1;
}

int cheevo_progress_load(const void *data, const size_t size) {
    if (!client || !data || !size || size > CHEEVO_PROGRESS_CAP) return -1;
    if (rc_client_is_game_loaded(client))
        return rc_client_deserialize_progress_sized(client, data, size) == RC_OK ? 0 : -1;

    uint8_t *copy = malloc(size);
    if (!copy) return -1;
    memcpy(copy, data, size);
    free(pending_progress);
    pending_progress = copy;
    pending_progress_size = size;
    return 0;
}

void cheevo_progress_reset(void) {
    free(pending_progress);
    pending_progress = NULL;
    pending_progress_size = 0;
    if (client) rc_client_reset(client);
}
