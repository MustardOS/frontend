#include <common/platform/media.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common/storage/fileio.h>

static long seen_serial = -1;

int media_background_active(void) {
    const int pid = read_line_int_from(MEDIA_BACKGROUND_PID, 1);
    return pid > 0 && pid != (int) getpid() && (kill(pid, 0) == 0 || errno == EPERM);
}

int media_track_changed(char *text, const size_t size) {
    if (!text || !size) return 0;
    text[0] = '\0';

    FILE *file = fopen(MEDIA_BACKGROUND_TRACK, "r");
    if (!file) {
        if (seen_serial < 0) seen_serial = 0;
        return 0;
    }

    char line[512];
    long serial = 0;
    char title[512] = "";
    char artist[512] = "";
    if (fgets(line, sizeof(line), file)) serial = strtol(line, NULL, 10);
    if (fgets(title, sizeof(title), file)) title[strcspn(title, "\r\n")] = '\0';
    if (fgets(artist, sizeof(artist), file)) artist[strcspn(artist, "\r\n")] = '\0';
    fclose(file);

    const long previous = seen_serial;
    seen_serial = serial;
    if (previous < 0 || serial == previous || !title[0] || !media_background_active()) return 0;

    if (artist[0])
        snprintf(text, size, "%s - %s", title, artist);
    else
        snprintf(text, size, "%s", title);
    return 1;
}
