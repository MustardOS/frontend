#include <common/runtime/remote.h>

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int remote_control_open(remote_control *control, const char *directory, const char *fifo) {
    if (!control || !directory || !fifo) return 0;
    if (control->reader >= 0) return 1;

    mkdir(directory, 0755);
    unlink(fifo);
    if (mkfifo(fifo, 0600) != 0) return 0;

    control->reader = open(fifo, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (control->reader < 0) return 0;

    control->keeper = open(fifo, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    control->length = 0;
    return 1;
}

void remote_control_close(remote_control *control, const char *fifo) {
    if (!control) return;
    if (control->reader >= 0) close(control->reader);
    if (control->keeper >= 0) close(control->keeper);
    control->reader = -1;
    control->keeper = -1;
    control->length = 0;
    if (fifo) unlink(fifo);
}

int remote_control_read_line(remote_control *control, char *line, const size_t size) {
    if (!control || control->reader < 0 || !line || !size) return 0;

    for (;;) {
        char *newline = memchr(control->buffer, '\n', control->length);
        if (newline) {
            const size_t length = (size_t) (newline - control->buffer);
            const size_t copied = length < size - 1 ? length : size - 1;
            memcpy(line, control->buffer, copied);
            line[copied] = '\0';

            const size_t used = length + 1;
            memmove(control->buffer, control->buffer + used, control->length - used);
            control->length -= used;
            return 1;
        }

        if (control->length == sizeof(control->buffer)) control->length = 0;
        const ssize_t received =
            read(control->reader, control->buffer + control->length, sizeof(control->buffer) - control->length);
        if (received <= 0) return 0;
        control->length += (size_t) received;
    }
}

void remote_json_text(FILE *file, const char *key, const char *value) {
    fprintf(file, "\"%s\":\"", key);
    for (const unsigned char *cursor = (const unsigned char *) (value ? value : ""); *cursor; cursor++) {
        if (*cursor == '"' || *cursor == '\\')
            fprintf(file, "\\%c", *cursor);
        else if (*cursor < 0x20)
            fprintf(file, "\\u%04x", *cursor);
        else
            fputc(*cursor, file);
    }
    fputs("\",", file);
}
