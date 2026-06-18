#include <unistd.h>
#include "init.h"
#include "fileio.h"
#include "util.h"
#include "ui/nav.h"
#include "download.h"
#include "log.h"

#define MAX_DOWNLOAD_BYTES ((curl_off_t) (512L * 1024L * 1024L))
#define TEMP_DL_DIR "/opt/muos/temp_dl"

int cancel_download = 0;
int download_in_progress = 0;

volatile int download_finish_result = 0;

typedef struct {
    char *url;
    char *save_path;
} download_args_t;

static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream) * size;
}

static void (*download_finish_cb)(int) = NULL;

void (*download_finish_pending_cb)(int) = NULL;

void set_download_callbacks(void (*callback)(int)) {
    download_finish_cb = callback;
}

void download_poll(void) {
    if (download_finish_result == INT_MIN) return;

    int result = download_finish_result;
    download_finish_result = INT_MIN;

    void (*cb)(int) = download_finish_pending_cb;
    download_finish_pending_cb = NULL;

    if (result == 0) progress_bar_value = 100;
    hide_progress_bar();

    download_in_progress = 0;
    cancel_download = 0;

    if (cb) cb(result);
}

static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                             curl_off_t ultotal, curl_off_t ulnow) {
    if (cancel_download) {
        LOG_INFO(mux_module, "Cancelling download");
        return 1;
    }

    if (dltotal > 0) {
        int percent = (int) ((dlnow * 100) / dltotal);
        if (progress_bar_value != percent) progress_bar_value = percent;
    }

    return 0;
}

static void download_finished(int result) {
    LOG_INFO(mux_module, "Download finished with result: %d", result);

    download_finish_pending_cb = download_finish_cb;
    download_finish_cb = NULL;
    download_finish_result = result;
}

int download_file(const char *url, const char *output_path) {
    progress_bar_value = 0;
    cancel_download = 0;
    download_in_progress = 1;
    download_finish_result = INT_MIN;

    CURL *curl;
    FILE *fp;
    CURLcode res;

    curl = curl_easy_init();
    if (!curl) {
        download_finished(-1);
        return -1;
    }

    create_directories(TEMP_DL_DIR, 0);

    const char *base = strrchr(output_path, '/');
    base = base ? base + 1 : output_path;
    char *tmp_path = mux_malloc(sizeof(TEMP_DL_DIR) + strlen(base) + 6);
    sprintf(tmp_path, "%s/%s.part", TEMP_DL_DIR, base);

    fp = fopen(tmp_path, "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        free(tmp_path);
        download_finished(-2);
        return -2;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");

#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif

    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 300000L);

    // Optional: follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);

    curl_easy_setopt(curl, CURLOPT_MAXFILESIZE_LARGE, MAX_DOWNLOAD_BYTES);

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

    res = curl_easy_perform(curl);

    // Flush and close file before checking
    fflush(fp);
    fclose(fp);

    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_off_t cl = 0;
    int cl_response = curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &cl);

    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_ERROR(mux_module, "cURL Failure: %s", curl_easy_strerror(res));
        remove(tmp_path);
        free(tmp_path);
        download_finished(-3);
        return -3;
    }

    // Verify HTTP status
    if (response_code < 200 || response_code >= 300) {
        LOG_ERROR(mux_module, "Unexpected HTTP Status: %ld", response_code);
        remove(tmp_path);
        free(tmp_path);
        download_finished(-4);
        return -4;
    }

    // Verify file is not empty
    if (cl_response != CURLE_OK || cl <= 0) {
        LOG_ERROR(mux_module, "No data downloaded...");
        remove(tmp_path);
        free(tmp_path);
        download_finished(-5);
        return -5;
    }

    remove(output_path);
    int copy_ok = (copy_file(tmp_path, output_path) == 0);
    remove(tmp_path);

    if (!copy_ok) {
        LOG_ERROR(mux_module, "Failed to finalise download");
        remove(output_path);
        free(tmp_path);
        download_finished(-6);
        return -6;
    }

    free(tmp_path);

    LOG_SUCCESS(mux_module, "Download finished (%.0ld bytes)", cl);
    download_finished(0);

    return 0;
}

static void *download_thread(void *arg) {
    download_args_t *args = (download_args_t *) arg;

    download_file(args->url, args->save_path);

    free(args->url);
    free(args->save_path);
    free(args);

    return NULL;
}

void initiate_download(const char *url, const char *output_path, int showProgress, char *message) {
    download_finish_result = INT_MIN;

    if (showProgress) show_progress_bar(message);

    download_args_t *args = mux_malloc(sizeof(*args));

    args->url = mux_strdup(url);
    args->save_path = mux_strdup(output_path);

    pthread_t tid;
    pthread_create(&tid, NULL, download_thread, args);
    pthread_detach(tid);
}
