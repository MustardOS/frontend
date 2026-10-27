#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libchdr/chd.h>
#include <rc_hash.h>
#include "chd_reader.h"

#define CHD_TRACK_LIMIT   99
#define CHD_TRACK_PADDING 4

typedef struct {
    uint32_t number;
    uint32_t frames;
    uint32_t padded_frames;
    uint32_t logical_frame;
    uint32_t first_sector;
    char type[32];
} chd_track;

typedef struct {
    chd_file *file;
    uint8_t *hunk;
    uint32_t hunk_bytes;
    uint32_t unit_bytes;
    uint32_t frames_per_hunk;
    uint32_t payload_bytes;
    uint32_t payload_offset;
    uint32_t logical_frame;
    uint32_t first_sector;
    uint32_t track_frames;
    int32_t loaded_hunk;
    int swap_audio;
} chd_track_handle;

static int load_hunk(chd_track_handle *handle, uint32_t hunk);

static int extension_is_chd(const char *path) {
    const char *extension = path ? strrchr(path, '.') : NULL;
    if (!extension || strlen(extension) != 4) return 0;
    return tolower((unsigned char) extension[1]) == 'c' && tolower((unsigned char) extension[2]) == 'h'
           && tolower((unsigned char) extension[3]) == 'd';
}

static void field_value(const char *metadata, const char *key, char *value, const size_t value_size) {
    value[0] = '\0';
    const size_t key_length = strlen(key);
    const char *field = metadata;
    while (field && *field) {
        while (*field == ' ')
            field++;
        if (strncmp(field, key, key_length) == 0 && field[key_length] == ':') {
            const char *start = field + key_length + 1;
            const char *end = strchr(start, ' ');
            const size_t length = end ? (size_t) (end - start) : strlen(start);
            if (length > 0 && length < value_size) {
                memcpy(value, start, length);
                value[length] = '\0';
            }
            return;
        }
        field = strchr(field, ' ');
    }
}

static uint32_t number_value(const char *metadata, const char *key) {
    char value[24];
    field_value(metadata, key, value, sizeof(value));
    if (!value[0]) return 0;
    char *end = NULL;
    const unsigned long number = strtoul(value, &end, 10);
    return end && *end == '\0' && number <= UINT32_MAX ? (uint32_t) number : 0;
}

static int read_track(chd_file *file, const uint32_t index, chd_track *track) {
    char metadata[256] = {0};
    uint32_t metadata_size = 0;
    if (chd_get_metadata(file, CDROM_TRACK_METADATA2_TAG, index, metadata, sizeof(metadata), &metadata_size, NULL, NULL)
            != CHDERR_NONE
        && chd_get_metadata(
               file, CDROM_TRACK_METADATA_TAG, index, metadata, sizeof(metadata), &metadata_size, NULL, NULL
           ) != CHDERR_NONE
        && chd_get_metadata(
               file, GDROM_TRACK_METADATA_TAG, index, metadata, sizeof(metadata), &metadata_size, NULL, NULL
           ) != CHDERR_NONE) {
        return 0;
    }
    if (!metadata_size) return 0;
    metadata[sizeof(metadata) - 1] = '\0';

    memset(track, 0, sizeof(*track));
    track->number = number_value(metadata, "TRACK");
    track->frames = number_value(metadata, "FRAMES");
    field_value(metadata, "TYPE", track->type, sizeof(track->type));
    track->padded_frames =
        track->frames + ((CHD_TRACK_PADDING - track->frames % CHD_TRACK_PADDING) % CHD_TRACK_PADDING);
    return track->number > 0 && track->frames > 0 && track->type[0];
}

static int select_track(chd_file *file, const uint32_t requested, chd_track *selected) {
    chd_track current;
    chd_track largest = {0};
    chd_track last = {0};
    uint32_t logical_frame = 0;
    uint32_t first_sector = 0;

    for (uint32_t index = 0; index < CHD_TRACK_LIMIT && read_track(file, index, &current); index++) {
        current.logical_frame = logical_frame;
        current.first_sector = first_sector;
        logical_frame += current.padded_frames;
        first_sector += current.frames;
        last = current;

        if (requested == current.number || (requested == RC_HASH_CDTRACK_FIRST_DATA && strcmp(current.type, "AUDIO"))) {
            *selected = current;
            return 1;
        }
        if (requested == RC_HASH_CDTRACK_LARGEST && strcmp(current.type, "AUDIO")
            && (!largest.number || current.frames > largest.frames)) {
            largest = current;
        }
    }

    if (requested == RC_HASH_CDTRACK_LAST && last.number) {
        *selected = last;
        return 1;
    }
    if (requested == RC_HASH_CDTRACK_LARGEST && largest.number) {
        *selected = largest;
        return 1;
    }
    return 0;
}

static void *open_chd_track(const char *path, const uint32_t requested) {
    chd_file *file = NULL;
    if (chd_open(path, CHD_OPEN_READ, NULL, &file) != CHDERR_NONE) return NULL;

    chd_track track;
    if (!select_track(file, requested, &track)) {
        chd_close(file);
        return NULL;
    }

    const chd_header *header = chd_get_header(file);
    if (!header || !header->hunkbytes || !header->unitbytes || header->hunkbytes % header->unitbytes != 0) {
        chd_close(file);
        return NULL;
    }

    chd_track_handle *handle = calloc(1, sizeof(*handle));
    if (!handle) {
        chd_close(file);
        return NULL;
    }
    handle->hunk = malloc(header->hunkbytes);
    if (!handle->hunk) {
        free(handle);
        chd_close(file);
        return NULL;
    }

    handle->file = file;
    handle->hunk_bytes = header->hunkbytes;
    handle->unit_bytes = header->unitbytes;
    handle->frames_per_hunk = header->hunkbytes / header->unitbytes;
    handle->logical_frame = track.logical_frame;
    handle->first_sector = track.first_sector;
    handle->track_frames = track.frames;
    handle->loaded_hunk = -1;
    if (strcmp(track.type, "MODE1_RAW") == 0) {
        handle->payload_bytes = 2048;
        handle->payload_offset = 16;
    } else if (strcmp(track.type, "MODE2_RAW") == 0) {
        handle->payload_bytes = 2048;
        handle->payload_offset = 24;
    } else if (strcmp(track.type, "MODE2") == 0 || strcmp(track.type, "MODE2_FORM_MIX") == 0) {
        handle->payload_bytes = 2048;
        handle->payload_offset = 8;
    } else if (strcmp(track.type, "MODE2_FORM2") == 0) {
        handle->payload_bytes = 2324;
    } else if (strcmp(track.type, "MODE1") == 0) {
        handle->payload_bytes = 2048;
    } else if (strcmp(track.type, "AUDIO") == 0) {
        handle->payload_bytes = 2352;
        handle->swap_audio = 1;
    } else {
        handle->payload_bytes = 2048;
    }
    if (!handle->payload_bytes || handle->payload_offset > handle->unit_bytes
        || handle->payload_bytes > handle->unit_bytes - handle->payload_offset) {
        free(handle->hunk);
        free(handle);
        chd_close(file);
        return NULL;
    }
    if ((strcmp(track.type, "MODE1_RAW") == 0 || strcmp(track.type, "MODE2_RAW") == 0) && track.frames > 16) {
        const uint32_t frame = handle->logical_frame + 16;
        if (load_hunk(handle, frame / handle->frames_per_hunk)) {
            const uint8_t *sector = handle->hunk + (size_t) (frame % handle->frames_per_hunk) * handle->unit_bytes;
            if (handle->unit_bytes >= 30 && memcmp(sector + 25, "CD001", 5) == 0)
                handle->payload_offset = 24;
            else if (handle->unit_bytes >= 22 && memcmp(sector + 17, "CD001", 5) == 0)
                handle->payload_offset = 16;
        }
    }
    return handle;
}

static int load_hunk(chd_track_handle *handle, const uint32_t hunk) {
    if (handle->loaded_hunk == (int32_t) hunk) return 1;
    if (chd_read(handle->file, hunk, handle->hunk) != CHDERR_NONE) return 0;
    if (handle->swap_audio) {
        for (uint32_t index = 0; index + 1 < handle->hunk_bytes; index += 2) {
            const uint8_t byte = handle->hunk[index];
            handle->hunk[index] = handle->hunk[index + 1];
            handle->hunk[index + 1] = byte;
        }
    }
    handle->loaded_hunk = (int32_t) hunk;
    return 1;
}

static size_t read_chd_sector(void *opaque, const uint32_t sector, void *buffer, size_t requested_bytes) {
    chd_track_handle *handle = opaque;
    if (!handle || sector < handle->first_sector) return 0;

    uint64_t relative_sector = (uint64_t) sector - handle->first_sector;
    if (relative_sector >= handle->track_frames) return 0;

    const size_t expected = requested_bytes;
    uint8_t *output = buffer;
    while (requested_bytes && relative_sector < handle->track_frames) {
        const uint32_t frame = handle->logical_frame + (uint32_t) relative_sector;
        const uint32_t hunk = frame / handle->frames_per_hunk;
        const uint32_t unit = frame % handle->frames_per_hunk;
        size_t amount = handle->payload_bytes;
        if (amount > requested_bytes) amount = requested_bytes;
        if (!load_hunk(handle, hunk)) return expected - requested_bytes;
        memcpy(output, handle->hunk + (size_t) unit * handle->unit_bytes + handle->payload_offset, amount);
        output += amount;
        requested_bytes -= amount;
        relative_sector++;
    }
    return expected - requested_bytes;
}

static void close_chd_track(void *opaque) {
    chd_track_handle *handle = opaque;
    if (!handle) return;
    chd_close(handle->file);
    free(handle->hunk);
    free(handle);
}

static uint32_t first_chd_sector(void *opaque) {
    const chd_track_handle *handle = opaque;
    return handle ? handle->first_sector : 0;
}

static void *open_track_direct(const char *path, const uint32_t track) {
    return extension_is_chd(path) ? open_chd_track(path, track) : NULL;
}

static void *open_track_iterator(const char *path, const uint32_t track, const rc_hash_iterator_t *iterator) {
    rc_hash_cdreader_t *callbacks = (rc_hash_cdreader_t *) &iterator->callbacks.cdreader;
    if (extension_is_chd(path)) {
        callbacks->open_track = open_track_direct;
        callbacks->open_track_iterator = open_track_iterator;
        callbacks->read_sector = read_chd_sector;
        callbacks->close_track = close_chd_track;
        callbacks->first_track_sector = first_chd_sector;
        return open_chd_track(path, track);
    }

    rc_hash_get_default_cdreader(callbacks);
    return callbacks->open_track_iterator ? callbacks->open_track_iterator(path, track, iterator) : NULL;
}

void cheevo_chd_reader_callbacks(rc_hash_cdreader_t *callbacks) {
    if (!callbacks) return;
    rc_hash_get_default_cdreader(callbacks);
    callbacks->open_track = open_track_direct;
    callbacks->open_track_iterator = open_track_iterator;
}
