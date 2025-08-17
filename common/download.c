#include "download.h"

bool cancel_download = false;
bool download_in_progress = false;

static bool showProgressUpdates = false;
static int last_update = 0;
static lv_timer_t *timer_update_progress;
static char download_message[MAX_BUFFER_SIZE];

typedef struct {
    const char *url;
    const char *save_path;
} download_args_t;

typedef struct {
    int percent;
} progress_data_t;

// For writing data to a file
static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream);
}

static void (*download_finish_cb)(int) = NULL;

void set_download_callbacks(void (*callback)(int)) {
    download_finish_cb = callback;
}

static void update_progress() {
    if (showProgressUpdates) {
        lv_obj_set_style_opa(ui_pnlDownload, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_bar_set_value(ui_barDownload, last_update, LV_ANIM_OFF);

        char buf[32];
        snprintf(buf, sizeof(buf), "%s: %d%%", download_message, last_update);
        lv_label_set_text(ui_lblDownload, buf);
        lv_obj_move_foreground(ui_pnlDownload);
    }

    if (cancel_download || last_update == 100) {
        download_in_progress = false;
        printf("Deleting timer\n");
    }

    if (!download_in_progress) {
        lv_obj_set_style_opa(ui_pnlDownload, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        if (timer_update_progress) {
            lv_timer_del(timer_update_progress);
            timer_update_progress = NULL;
        }
    }
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
        if (last_update != percent) {
            last_update = percent;
            printf("Progress: %d\n", percent);

            progress_data_t *data = malloc(sizeof(progress_data_t));
            data->percent = percent;
        }
    }

    return 0;
}

static void download_finished(int result) {
    download_in_progress = false;
    if (download_finish_cb) download_finish_cb(result);
}

int download_file(const char *url, const char *output_path) {
    last_update = 0;
    cancel_download = false;
    download_in_progress = true;

    if (showProgressUpdates) {
        lv_label_set_text(ui_lblDownload, "");
        lv_bar_set_value(ui_barDownload, last_update, LV_ANIM_OFF);
        lv_obj_set_style_opa(ui_pnlDownload, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    timer_update_progress = lv_timer_create(update_progress, TIMER_REFRESH, NULL);

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
    fclose(fp);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        download_finished(-3);
        remove(output_path);
        return -3;
    }

    printf("Download Finished\n");
    download_finished(0);
    return 0;
}

static void *download_thread(void *arg) {
    download_args_t *args = (download_args_t *) arg;

    download_file(args->url, args->save_path);  // Your existing download function

    free(args);  // Clean up if dynamically allocated
    return NULL;
}

void initiate_download(const char *url, const char *output_path, bool showProgress, char *message) {
    snprintf(download_message, sizeof(download_message), "%s", message);

    showProgressUpdates = showProgress;
    download_args_t *args = malloc(sizeof(download_args_t));

    args->url = url;
    args->save_path = output_path;

    pthread_t tid;
    pthread_create(&tid, NULL, download_thread, args);
}
