#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../common/fileio.h"
#include "../../common/init.h"
#include "../../common/log.h"
#include "../../common/miniz/miniz.h"
#include "patch.h"

enum bps_mode { source_read = 0, target_read, source_copy, target_copy };

enum patch_error {
    patch_unknown = 0,
    patch_success,
    patch_patch_too_small,
    patch_patch_invalid_header,
    patch_patch_invalid,
    patch_source_too_small,
    patch_target_alloc_failed,
    patch_source_invalid,
    patch_target_invalid,
    patch_source_checksum_invalid,
    patch_target_checksum_invalid,
    patch_patch_checksum_invalid
};

struct bps_data {
    const uint8_t *modify_data;
    const uint8_t *source_data;
    uint8_t *target_data;
    size_t modify_length;
    size_t source_length;
    size_t target_length;
    size_t modify_offset;
    size_t source_offset;
    size_t target_offset;
    size_t output_offset;
    uint32_t modify_checksum;
    uint32_t source_checksum;
    uint32_t target_checksum;
};

struct ups_data {
    const uint8_t *patch_data;
    const uint8_t *source_data;
    uint8_t *target_data;
    unsigned patch_length;
    unsigned source_length;
    unsigned target_length;
    unsigned patch_offset;
    unsigned source_offset;
    unsigned target_offset;
    uint32_t patch_checksum;
    uint32_t source_checksum;
    uint32_t target_checksum;
};

typedef enum patch_error (*patch_func_t)(const uint8_t *, uint64_t, const uint8_t *, uint64_t, uint8_t **, uint64_t *);

static uint8_t bps_read(struct bps_data *bps) {
    const uint8_t data = bps->modify_data[bps->modify_offset++];
    bps->modify_checksum = ~(mz_crc32(~bps->modify_checksum, &data, 1));
    return data;
}

static uint64_t bps_decode(struct bps_data *bps) {
    uint64_t data = 0, shift = 1;

    for (;;) {
        const uint8_t x = bps_read(bps);
        data += (x & 0x7f) * shift;
        if (x & 0x80) break;
        shift <<= 7;
        data += shift;
    }

    return data;
}

static enum patch_error bps_apply_patch(
    const uint8_t *modify_data, const uint64_t modify_length, const uint8_t *source_data, const uint64_t source_length,
    uint8_t **target_data, uint64_t *target_length
) {
    if (modify_length < 19) return patch_patch_too_small;

    struct bps_data bps = {
        .modify_data = modify_data,
        .source_data = source_data,
        .target_data = *target_data,
        .modify_length = modify_length,
        .source_length = source_length,
        .target_length = *target_length,
        .modify_checksum = ~0u,
        .target_checksum = ~0u,
    };

    if (bps_read(&bps) != 'B' || bps_read(&bps) != 'P' || bps_read(&bps) != 'S' || bps_read(&bps) != '1')
        return patch_patch_invalid_header;

    size_t modify_source_size, modify_target_size, modify_markup_size;
    {
        const uint64_t raw_source = bps_decode(&bps);
        const uint64_t raw_target = bps_decode(&bps);
        const uint64_t raw_markup = bps_decode(&bps);

        if (raw_markup > modify_length - bps.modify_offset) return patch_patch_invalid;
        if (raw_target > SIZE_MAX) return patch_target_alloc_failed;
        if (raw_source > SIZE_MAX) return patch_source_too_small;

        modify_source_size = (size_t) raw_source;
        modify_target_size = (size_t) raw_target;
        modify_markup_size = (size_t) raw_markup;
    }

    for (size_t i = 0; i < modify_markup_size; i++)
        bps_read(&bps);

    if (modify_source_size > bps.source_length) return patch_source_too_small;

    if (modify_target_size > bps.target_length) {
        uint8_t *prov = malloc(modify_target_size);
        if (!prov) return patch_target_alloc_failed;

        free(*target_data);
        bps.target_data = prov;
        *target_data = prov;
        bps.target_length = modify_target_size;
    }

    while (bps.modify_offset < bps.modify_length - 12) {
        size_t len = bps_decode(&bps);
        const unsigned mode = len & 3;
        len = (len >> 2) + 1;

        if (bps.output_offset >= bps.target_length || len > bps.target_length - bps.output_offset)
            return patch_target_invalid;

        switch (mode) {
            case source_read:
                if (bps.output_offset + len > bps.source_length) return patch_source_invalid;
                while (len--) {
                    const uint8_t data = bps.source_data[bps.output_offset];
                    bps.target_data[bps.output_offset++] = data;
                    bps.target_checksum = ~(mz_crc32(~bps.target_checksum, &data, 1));
                }
                break;

            case target_read:
                if (bps.modify_offset + len > bps.modify_length - 12) return patch_patch_invalid;
                while (len--) {
                    const uint8_t data = bps_read(&bps);
                    bps.target_data[bps.output_offset++] = data;
                    bps.target_checksum = ~(mz_crc32(~bps.target_checksum, &data, 1));
                }
                break;

            case source_copy:
            case target_copy: {
                int64_t offset = (int64_t) bps_decode(&bps);
                const int negative = offset & 1;
                offset >>= 1;
                if (negative) offset = -offset;

                if (mode == source_copy) {
                    bps.source_offset = (size_t) ((int64_t) bps.source_offset + offset);
                    if (bps.source_offset > bps.source_length || len > bps.source_length - bps.source_offset)
                        return patch_source_invalid;
                    while (len--) {
                        const uint8_t data = bps.source_data[bps.source_offset++];
                        bps.target_data[bps.output_offset++] = data;
                        bps.target_checksum = ~(mz_crc32(~bps.target_checksum, &data, 1));
                    }
                } else {
                    bps.target_offset = (size_t) ((int64_t) bps.target_offset + offset);
                    if (bps.target_offset > bps.target_length || len > bps.target_length - bps.target_offset)
                        return patch_target_invalid;
                    while (len--) {
                        const uint8_t data = bps.target_data[bps.target_offset++];
                        bps.target_data[bps.output_offset++] = data;
                        bps.target_checksum = ~(mz_crc32(~bps.target_checksum, &data, 1));
                    }
                }
                break;
            }
        }
    }

    uint32_t modify_source_checksum = 0, modify_target_checksum = 0, modify_modify_checksum = 0;
    for (int i = 0; i < 32; i += 8)
        modify_source_checksum |= (uint32_t) bps_read(&bps) << i;
    for (int i = 0; i < 32; i += 8)
        modify_target_checksum |= (uint32_t) bps_read(&bps) << i;

    const uint32_t checksum = ~bps.modify_checksum;
    for (int i = 0; i < 32; i += 8)
        modify_modify_checksum |= (uint32_t) bps_read(&bps) << i;

    bps.source_checksum = (uint32_t) mz_crc32(0, bps.source_data, bps.source_length);
    bps.target_checksum = ~bps.target_checksum;

    if (bps.source_checksum != modify_source_checksum) return patch_source_checksum_invalid;
    if (bps.target_checksum != modify_target_checksum) return patch_target_checksum_invalid;
    if (checksum != modify_modify_checksum) return patch_patch_checksum_invalid;

    *target_length = modify_target_size;
    return patch_success;
}

static uint8_t ups_patch_read(struct ups_data *data) {
    if (data->patch_offset < data->patch_length) {
        const uint8_t n = data->patch_data[data->patch_offset++];
        data->patch_checksum = ~(mz_crc32(~data->patch_checksum, &n, 1));
        return n;
    }
    return 0;
}

static uint8_t ups_source_read(struct ups_data *data) {
    if (data->source_offset < data->source_length) {
        const uint8_t n = data->source_data[data->source_offset++];
        data->source_checksum = ~(mz_crc32(~data->source_checksum, &n, 1));
        return n;
    }
    return 0;
}

static void ups_target_write(struct ups_data *data, const uint8_t n) {
    if (data->target_offset < data->target_length) {
        data->target_data[data->target_offset] = n;
        data->target_checksum = ~(mz_crc32(~data->target_checksum, &n, 1));
    }
    data->target_offset++;
}

static uint64_t ups_decode(struct ups_data *data) {
    uint64_t offset = 0, shift = 1;

    for (;;) {
        const uint8_t x = ups_patch_read(data);
        offset += (x & 0x7f) * shift;
        if (x & 0x80) break;
        shift <<= 7;
        offset += shift;
    }

    return offset;
}

static enum patch_error ups_apply_patch(
    const uint8_t *patchdata, const uint64_t patchlength, const uint8_t *sourcedata, const uint64_t sourcelength,
    uint8_t **targetdata, uint64_t *targetlength
) {
    if (patchlength < 18) return patch_patch_invalid;

    struct ups_data data = {
        .patch_data = patchdata,
        .source_data = sourcedata,
        .target_data = *targetdata,
        .patch_length = (unsigned) patchlength,
        .source_length = (unsigned) sourcelength,
        .target_length = (unsigned) *targetlength,
        .patch_checksum = ~0u,
        .source_checksum = ~0u,
        .target_checksum = ~0u,
    };

    if (ups_patch_read(&data) != 'U' || ups_patch_read(&data) != 'P' || ups_patch_read(&data) != 'S'
        || ups_patch_read(&data) != '1')
        return patch_patch_invalid;

    const unsigned source_read_length = (unsigned) ups_decode(&data);
    const unsigned target_read_length = (unsigned) ups_decode(&data);

    if (data.source_length != source_read_length && data.source_length != target_read_length)
        return patch_source_invalid;

    *targetlength = data.source_length == source_read_length ? target_read_length : source_read_length;

    if (data.target_length < *targetlength) {
        uint8_t *prov = malloc(*targetlength);
        if (!prov) return patch_target_alloc_failed;
        free(*targetdata);
        *targetdata = prov;
        data.target_data = prov;
    }

    data.target_length = (unsigned) *targetlength;

    while (data.patch_offset < data.patch_length - 12) {
        unsigned len = (unsigned) ups_decode(&data);
        while (len--)
            ups_target_write(&data, ups_source_read(&data));

        for (;;) {
            const uint8_t patch_xor = ups_patch_read(&data);
            ups_target_write(&data, patch_xor ^ ups_source_read(&data));
            if (patch_xor == 0) break;
        }
    }

    while (data.source_offset < data.source_length)
        ups_target_write(&data, ups_source_read(&data));
    while (data.target_offset < data.target_length)
        ups_target_write(&data, ups_source_read(&data));

    uint32_t source_read_checksum = 0, target_read_checksum = 0, patch_read_checksum = 0;
    for (int i = 0; i < 4; i++)
        source_read_checksum |= (uint32_t) ups_patch_read(&data) << (i * 8);
    for (int i = 0; i < 4; i++)
        target_read_checksum |= (uint32_t) ups_patch_read(&data) << (i * 8);

    const uint32_t patch_result_checksum = ~data.patch_checksum;
    data.source_checksum = ~data.source_checksum;
    data.target_checksum = ~data.target_checksum;

    for (int i = 0; i < 4; i++)
        patch_read_checksum |= (uint32_t) ups_patch_read(&data) << (i * 8);

    if (patch_result_checksum != patch_read_checksum) return patch_patch_invalid;

    if (data.source_checksum == source_read_checksum && data.source_length == source_read_length) {
        if (data.target_checksum == target_read_checksum && data.target_length == target_read_length)
            return patch_success;
        return patch_target_invalid;
    }
    if (data.source_checksum == target_read_checksum && data.source_length == target_read_length) {
        if (data.target_checksum == source_read_checksum && data.target_length == source_read_length)
            return patch_success;
        return patch_target_invalid;
    }

    return patch_source_invalid;
}

static enum patch_error ips_alloc_targetdata(
    const uint8_t *patchdata, const uint64_t patchlen, const uint64_t sourcelength, uint8_t **targetdata,
    uint64_t *targetlength
) {
    uint32_t offset = 5;
    *targetlength = sourcelength;

    while (offset <= patchlen - 3) {
        uint32_t address = (uint32_t) patchdata[offset++] << 16;
        address |= (uint32_t) patchdata[offset++] << 8;
        address |= patchdata[offset++];

        if (address == 0x454f46) {
            if (offset == patchlen || offset == patchlen - 3) {
                if (offset == patchlen - 3) {
                    uint32_t size = (uint32_t) patchdata[offset++] << 16;
                    size |= (uint32_t) patchdata[offset++] << 8;
                    size |= patchdata[offset++];
                    *targetlength = size;
                }

                uint8_t *prov = malloc((size_t) *targetlength);
                if (!prov) return patch_target_alloc_failed;
                free(*targetdata);
                *targetdata = prov;
                return patch_success;
            }
        }

        if (offset > patchlen - 2) break;

        size_t len = (size_t) patchdata[offset++] << 8;
        len |= patchdata[offset++];

        if (len) {
            if (offset > patchlen - len) break;
            while (len--) {
                address++;
                offset++;
            }
        } else {
            if (offset > patchlen - 3) break;
            len = (size_t) patchdata[offset++] << 8;
            len |= patchdata[offset++];
            if (len == 0) break;
            while (len--)
                address++;
            offset++;
        }

        if (address > *targetlength) *targetlength = address;
    }

    return patch_patch_invalid;
}

static enum patch_error ips_apply_patch(
    const uint8_t *patchdata, const uint64_t patchlen, const uint8_t *sourcedata, const uint64_t sourcelength,
    uint8_t **targetdata, uint64_t *targetlength
) {
    if (patchlen < 8 || patchdata[0] != 'P' || patchdata[1] != 'A' || patchdata[2] != 'T' || patchdata[3] != 'C'
        || patchdata[4] != 'H')
        return patch_patch_invalid;

    const enum patch_error err = ips_alloc_targetdata(patchdata, patchlen, sourcelength, targetdata, targetlength);
    if (err != patch_success) return err;

    memcpy(*targetdata, sourcedata, sourcelength);

    uint32_t offset = 5;
    while (offset <= patchlen - 3) {

        uint32_t address = (uint32_t) patchdata[offset++] << 16;
        address |= (uint32_t) patchdata[offset++] << 8;
        address |= patchdata[offset++];

        if (address == 0x454f46 && (offset == patchlen || offset == patchlen - 3)) return patch_success;

        if (offset > patchlen - 2) break;

        size_t len = (size_t) patchdata[offset++] << 8;
        len |= patchdata[offset++];

        if (len) {
            if (offset > patchlen - len) break;
            while (len--)
                (*targetdata)[address++] = patchdata[offset++];
        } else {
            if (offset > patchlen - 3) break;
            len = (size_t) patchdata[offset++] << 8;
            len |= patchdata[offset++];
            if (len == 0) break;
            while (len--)
                (*targetdata)[address++] = patchdata[offset];
            offset++;
        }
    }

    return patch_patch_invalid;
}

static int read_whole_file(const char *path, uint8_t **out_data, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        return 0;
    }

    uint8_t *data = malloc((size_t) size);
    if (!data || fread(data, 1, (size_t) size, f) != (size_t) size) {
        fclose(f);
        free(data);
        return 0;
    }
    fclose(f);

    *out_data = data;
    *out_size = (size_t) size;
    return 1;
}

static int try_one_patch(
    const char *path, const char *desc, const patch_func_t func, uint8_t **data, uint64_t *size, char *patch_list,
    const size_t patch_list_size, int *applied_count
) {
    if (!file_exist(path)) return 0;

    uint8_t *patch_data = NULL;
    size_t patch_size = 0;
    if (!read_whole_file(path, &patch_data, &patch_size)) return 1;

    uint8_t *target_data = NULL;
    uint64_t target_size = 0;
    const enum patch_error err = func(patch_data, patch_size, *data, *size, &target_data, &target_size);
    free(patch_data);

    if (err != patch_success) {
        LOG_ERROR(mux_module, "%s patch failed to apply (error %d): %s", desc, (int) err, path);
        free(target_data);
        return 1;
    }

    free(*data);
    *data = target_data;
    *size = target_size;
    (*applied_count)++;

    LOG_SUCCESS(mux_module, "Applied %s patch: %s", desc, path);

    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    const size_t used = strlen(patch_list);
    snprintf(patch_list + used, patch_list_size - used, "%s%s", used ? ", " : "", name);

    return 1;
}

static void patch_stem(const char *base_path, char *stem) {
    snprintf(stem, PATH_MAX, "%s", base_path);

    const char *last_slash = strrchr(stem, '/');
    char *last_dot = strrchr(last_slash ? last_slash : stem, '.');
    if (last_dot) *last_dot = '\0';
}

int patch_exists(const char *base_path) {
    char stem[PATH_MAX];
    patch_stem(base_path, stem);

    char name[PATH_MAX + 4];
    snprintf(name, sizeof(name), "%s.ips", stem);
    if (file_exist(name)) return 1;
    snprintf(name, sizeof(name), "%s.bps", stem);
    if (file_exist(name)) return 1;
    snprintf(name, sizeof(name), "%s.ups", stem);
    return file_exist(name);
}

int patch_apply(const char *base_path, void **data, size_t *size, char *patch_list, const size_t patch_list_size) {
    char stem[PATH_MAX];
    patch_stem(base_path, stem);

    char name_ips[PATH_MAX + 8];
    snprintf(name_ips, sizeof(name_ips), "%s.ips", stem);

    char name_bps[PATH_MAX + 8];
    snprintf(name_bps, sizeof(name_bps), "%s.bps", stem);

    char name_ups[PATH_MAX + 8];
    snprintf(name_ups, sizeof(name_ups), "%s.ups", stem);

    uint8_t *cur_data = *data;
    uint64_t cur_size = *size;
    int applied = 0;

    const int base_claimed =
        try_one_patch(name_ips, "IPS", ips_apply_patch, &cur_data, &cur_size, patch_list, patch_list_size, &applied)
        || try_one_patch(name_bps, "BPS", bps_apply_patch, &cur_data, &cur_size, patch_list, patch_list_size, &applied)
        || try_one_patch(name_ups, "UPS", ups_apply_patch, &cur_data, &cur_size, patch_list, patch_list_size, &applied);

    if (base_claimed) {
        const size_t ips_len = strlen(name_ips);
        const size_t bps_len = strlen(name_bps);
        const size_t ups_len = strlen(name_ups);

        for (int index = 1; index < 10; index++) {
            const char index_char = (char) ('0' + index);
            name_ips[ips_len] = index_char;
            name_ips[ips_len + 1] = '\0';
            name_bps[bps_len] = index_char;
            name_bps[bps_len + 1] = '\0';
            name_ups[ups_len] = index_char;
            name_ups[ups_len + 1] = '\0';

            const int claimed =
                try_one_patch(
                    name_ips, "IPS", ips_apply_patch, &cur_data, &cur_size, patch_list, patch_list_size, &applied
                )
                || try_one_patch(
                    name_bps, "BPS", bps_apply_patch, &cur_data, &cur_size, patch_list, patch_list_size, &applied
                )
                || try_one_patch(
                    name_ups, "UPS", ups_apply_patch, &cur_data, &cur_size, patch_list, patch_list_size, &applied
                );
            if (!claimed) break;
        }
    }

    *data = cur_data;
    *size = cur_size;

    return applied;
}
