#include <unistd.h>
#include "common.h"
#include "download.h"

bool cancel_download = false;
bool download_in_progress = false;

typedef struct {
    char *url;
    char *save_path;
} download_args_t;

typedef struct {
    int percent;
} progress_data_t;

// For writing data to a file
static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written * size;
}

static void (*download_finish_cb)(int) = NULL;

void set_download_callbacks(void (*callback)(int)) {
    download_finish_cb = callback;
}

// Progress callback (called by libcurl)
static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
                             curl_off_t ultotal, curl_off_t ulnow) {
    if (cancel_download) {
        printf("Cancelling download\n");
        return 1;
    }

    if (dltotal > 0) {
        int percent = (int) ((dlnow * 100) / dltotal);
        if (progress_bar_value != percent) {
            progress_bar_value = percent;

            progress_data_t *data = malloc(sizeof(progress_data_t));
            data->percent = percent;
        }
    }

    return 0;
}

static void download_finished(int result) {
    printf("Download finished with result: %d\n", result);

    if (result == 0) {
        progress_bar_value = 100;
        usleep(500000); // 0.5 seconds
    }

    hide_progress_bar();
    download_in_progress = false;

    if (download_finish_cb) download_finish_cb(result);
}

int download_file(const char *url, const char *output_path) {
    progress_bar_value = 0;
    cancel_download = false;
    download_in_progress = true;

    CURL *curl;
    FILE *fp;
    CURLcode res;

    curl = curl_easy_init();
    if (!curl) {
        download_finished(-1);
        return -1;
    }

    fp = fopen(output_path, "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        download_finished(-2);
        return -2;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    // Enable progress function
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);

    // Optional: follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    res = curl_easy_perform(curl);

    // flush and close file before checking
    fflush(fp);
    fclose(fp);

    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_off_t cl = 0;
    int cl_response = curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &cl);

    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        download_finished(-3);
        remove(output_path);
        return -3;
    }

    // Verify HTTP status
    if (response_code < 200 || response_code >= 300) {
        fprintf(stderr, "Unexpected HTTP status: %ld\n", response_code);
        remove(output_path);
        download_finished(-4);
        return -4;
    }

    // Verify file is not empty
    if (cl_response != CURLE_OK || cl <= 0) {
        fprintf(stderr, "No data downloaded\n");
        remove(output_path);
        download_finished(-5);
        return -5;
    }

    printf("Download Finished (%.0ld bytes)\n", cl);
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

void initiate_download(const char *url, const char *output_path, bool showProgress, char *message) {
    if (showProgress) show_progress_bar(message);

    download_args_t *args = malloc(sizeof(*args));

    args->url = strdup(url);
    args->save_path = strdup(output_path);

    pthread_t tid;
    pthread_create(&tid, NULL, download_thread, args);
    pthread_detach(tid);
}
