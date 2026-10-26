#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>
#include <zstd.h>

#define RZIP_HEADER_SIZE 20U
#define RZIP_CHUNK_LIMIT (64U * 1024U * 1024U)
#define STATE_SIZE_LIMIT (512U * 1024U * 1024U)
#define PKST_HEADER_SIZE 32U

struct payload {
    uint8_t *data;
    size_t size;
};

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t) data[0] | (uint32_t) data[1] << 8 | (uint32_t) data[2] << 16 | (uint32_t) data[3] << 24;
}

static uint64_t read_u64(const uint8_t *data) {
    return (uint64_t) read_u32(data) | (uint64_t) read_u32(data + 4) << 32;
}

static void write_u16(uint8_t *data, const uint16_t value) {
    data[0] = (uint8_t) value;
    data[1] = (uint8_t) (value >> 8);
}

static void write_u32(uint8_t *data, const uint32_t value) {
    data[0] = (uint8_t) value;
    data[1] = (uint8_t) (value >> 8);
    data[2] = (uint8_t) (value >> 16);
    data[3] = (uint8_t) (value >> 24);
}

static int read_exact(FILE *file, void *data, const size_t size) {
    return !size || fread(data, 1, size, file) == size;
}

static int load_raw(FILE *file, const struct stat *info, struct payload *out) {
    if (info->st_size <= 0 || (uint64_t) info->st_size > STATE_SIZE_LIMIT) return 0;
    out->size = (size_t) info->st_size;
    out->data = malloc(out->size);
    return out->data && fseek(file, 0, SEEK_SET) == 0 && read_exact(file, out->data, out->size);
}

static int load_rzip(FILE *file, const uint8_t header[RZIP_HEADER_SIZE], struct payload *out) {
    const uint8_t version = header[6];
    const uint32_t chunk_size = read_u32(header + 8);
    const uint64_t total = read_u64(header + 12);
    if ((version != 1 && version != 2) || !chunk_size || chunk_size > RZIP_CHUNK_LIMIT || !total
        || total > STATE_SIZE_LIMIT)
        return 0;

    uint8_t *decoded = malloc((size_t) total);
    uint8_t *compressed = NULL;
    size_t compressed_capacity = 0;
    size_t offset = 0;
    int ok = decoded != NULL;

    while (ok && offset < total) {
        uint8_t length_data[4];
        if (!read_exact(file, length_data, sizeof(length_data))) {
            ok = 0;
            break;
        }
        const uint32_t compressed_size = read_u32(length_data);
        if (!compressed_size || compressed_size > RZIP_CHUNK_LIMIT) {
            ok = 0;
            break;
        }
        if (compressed_capacity < compressed_size) {
            uint8_t *grown = realloc(compressed, compressed_size);
            if (!grown) {
                ok = 0;
                break;
            }
            compressed = grown;
            compressed_capacity = compressed_size;
        }
        if (!read_exact(file, compressed, compressed_size)) {
            ok = 0;
            break;
        }

        const size_t expected = total - offset < chunk_size ? (size_t) (total - offset) : chunk_size;
        size_t produced = 0;
        if (version == 1) {
            uLongf destination_size = expected;
            if (uncompress(decoded + offset, &destination_size, compressed, compressed_size) != Z_OK)
                ok = 0;
            else
                produced = destination_size;
        } else {
            const size_t destination_size = ZSTD_decompress(decoded + offset, expected, compressed, compressed_size);
            if (ZSTD_isError(destination_size))
                ok = 0;
            else
                produced = destination_size;
        }
        if (ok && produced != expected) ok = 0;
        offset += produced;
    }

    free(compressed);
    if (!ok || offset != total) {
        free(decoded);
        return 0;
    }
    out->data = decoded;
    out->size = (size_t) total;
    return 1;
}

static int load_payload(const char *path, struct payload *out) {
    memset(out, 0, sizeof(*out));
    FILE *file = fopen(path, "rb");
    if (!file) return 0;

    struct stat info = {0};
    uint8_t header[RZIP_HEADER_SIZE];
    if (fstat(fileno(file), &info) < 0) {
        fclose(file);
        return 0;
    }
    const int header_read = read_exact(file, header, sizeof(header));
    int ok;
    if (header_read && memcmp(header, "#RZIPv", 6) == 0 && header[7] == '#')
        ok = load_rzip(file, header, out);
    else
        ok = load_raw(file, &info, out);
    fclose(file);

    if (!ok) {
        free(out->data);
        memset(out, 0, sizeof(*out));
    }
    return ok;
}

static int core_payload(const struct payload *input, const uint8_t **data, size_t *size) {
    if (input->size >= PKST_HEADER_SIZE && memcmp(input->data, "PKST", 4) == 0) {
        const uint32_t core_size = read_u32(input->data + 8);
        const uint32_t cheevo_size = read_u32(input->data + 12);
        if ((uint64_t) PKST_HEADER_SIZE + core_size + cheevo_size != input->size) return 0;
        *data = input->data + PKST_HEADER_SIZE;
        *size = core_size;
        return core_size > 0;
    }

    if (input->size >= 8 && memcmp(input->data, "RASTATE", 7) == 0) {
        if (input->data[7] != 1) return 0;
        size_t offset = 8;
        while (offset + 8 <= input->size) {
            const uint8_t *header = input->data + offset;
            const uint32_t block_size = read_u32(header + 4);
            offset += 8;
            if (block_size > input->size - offset) return 0;
            if (memcmp(header, "MEM ", 4) == 0) {
                *data = input->data + offset;
                *size = block_size;
                return block_size > 0;
            }
            if (memcmp(header, "END ", 4) == 0) break;
            const size_t aligned = ((size_t) block_size + 7U) & ~(size_t) 7U;
            if (aligned > input->size - offset) return 0;
            offset += aligned;
        }
        return 0;
    }

    *data = input->data;
    *size = input->size;
    return input->size > 0;
}

static int write_all(const int descriptor, const void *data, const size_t size) {
    const uint8_t *bytes = data;
    size_t offset = 0;
    while (offset < size) {
        const ssize_t written = write(descriptor, bytes + offset, size - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return 0;
        offset += (size_t) written;
    }
    return 1;
}

static int
write_atomic(const char *path, const void *header, const size_t header_size, const void *data, const size_t size) {
    char temporary[4096];
    const int temporary_length = snprintf(temporary, sizeof(temporary), "%s.tmp.%ld", path, (long) getpid());
    if (temporary_length < 1 || (size_t) temporary_length >= sizeof(temporary)) return 0;
    const int descriptor = open(temporary, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (descriptor < 0) return 0;
    int ok = write_all(descriptor, header, header_size) && write_all(descriptor, data, size) && fsync(descriptor) == 0;
    if (close(descriptor) < 0) ok = 0;
    if (ok && rename(temporary, path) < 0) ok = 0;
    if (!ok) unlink(temporary);
    return ok;
}

static int extract(const char *source, const char *target) {
    struct payload payload;
    if (!load_payload(source, &payload)) return 0;
    const int ok = write_atomic(target, NULL, 0, payload.data, payload.size);
    free(payload.data);
    return ok;
}

static int convert(const char *source, const char *target) {
    struct payload payload;
    if (!load_payload(source, &payload)) return 0;

    const uint8_t *core;
    size_t core_size;
    int ok = core_payload(&payload, &core, &core_size) && core_size <= UINT32_MAX;
    uint8_t header[PKST_HEADER_SIZE] = {0};
    if (ok) {
        memcpy(header, "PKST", 4);
        write_u16(header + 4, 1);
        write_u32(header + 8, (uint32_t) core_size);
        write_u32(header + 16, crc32(0, core, (uInt) core_size));
        ok = write_atomic(target, header, sizeof(header), core, core_size);
    }
    free(payload.data);
    return ok;
}

int main(const int argc, char **argv) {
    if (argc != 4 || (strcmp(argv[1], "extract") != 0 && strcmp(argv[1], "convert") != 0)) {
        fprintf(stderr, "Usage: mustate extract|convert SOURCE TARGET\n");
        return 2;
    }

    const int ok = strcmp(argv[1], "extract") == 0 ? extract(argv[2], argv[3]) : convert(argv[2], argv[3]);
    if (!ok) fprintf(stderr, "mustate: could not %s %s\n", argv[1], argv[2]);
    return ok ? 0 : 1;
}
