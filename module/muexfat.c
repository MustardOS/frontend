#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define ENTRY_SIZE   32
#define FAT_END      0xFFFFFFFFU
#define FAT_BAD      0xFFFFFFF7U
#define MAX_DEPTH    64
#define FAT_CACHE    4096
#define MAX_DIR_SIZE (256U * 1024U * 1024U)
#define DIR_PIECE    4096

#define TYPE_END       0x00
#define TYPE_IN_USE    0x80
#define TYPE_SECONDARY 0x40
#define TYPE_BITMAP    0x81
#define TYPE_UPCASE    0x82
#define TYPE_LABEL     0x83
#define TYPE_FILE      0x85
#define TYPE_GUID      0xA0
#define TYPE_PADDING   0xA1
#define TYPE_ACL       0xA2
#define TYPE_STREAM    0xC0
#define TYPE_NAME      0xC1
#define TYPE_BENIGN    0x20

#define FLAG_ALLOCATION 0x01
#define FLAG_NO_CHAIN   0x02
#define ATTR_DIRECTORY  0x10

typedef struct {
    int fd;
    uint32_t sector_size;
    uint64_t cluster_size;
    uint64_t fat_offset;
    uint64_t heap_offset;
    uint32_t cluster_count;
    uint32_t root_cluster;
    uint8_t *used;
    uint32_t bitmap_cluster;
    uint32_t *bitmap_chain;
    uint64_t bitmap_clusters;
    uint64_t fat_cache_base;
    uint8_t fat_cache[FAT_CACHE];
    int fat_cache_valid;
} volume;

static int fail(const char *message) {
    fprintf(stderr, "muexfat: %s\n", message);
    return -1;
}

static uint16_t le16(const uint8_t *p) {
    return (uint16_t) (p[0] | p[1] << 8);
}

static uint32_t le32(const uint8_t *p) {
    return (uint32_t) p[0] | (uint32_t) p[1] << 8 | (uint32_t) p[2] << 16 | (uint32_t) p[3] << 24;
}

static uint64_t le64(const uint8_t *p) {
    return (uint64_t) le32(p) | (uint64_t) le32(p + 4) << 32;
}

static int read_at(const volume *v, const uint64_t offset, void *buffer, const size_t size) {
    size_t done = 0;
    while (done < size) {
        const ssize_t got = pread(v->fd, (uint8_t *) buffer + done, size - done, (off_t) (offset + done));
        if (got <= 0) return -1;
        done += (size_t) got;
    }
    return 0;
}

static int write_at(const volume *v, const uint64_t offset, const void *buffer, const size_t size) {
    size_t done = 0;
    while (done < size) {
        const ssize_t put = pwrite(v->fd, (const uint8_t *) buffer + done, size - done, (off_t) (offset + done));
        if (put <= 0) return -1;
        done += (size_t) put;
    }
    return 0;
}

static int valid_cluster(const volume *v, const uint32_t cluster) {
    return cluster >= 2 && cluster - 2 < v->cluster_count;
}

static uint64_t cluster_offset(const volume *v, const uint32_t cluster) {
    return v->heap_offset + (uint64_t) (cluster - 2) * v->cluster_size;
}

static int fat_next(volume *v, const uint32_t cluster, uint32_t *next) {
    const uint64_t offset = v->fat_offset + (uint64_t) cluster * 4;
    const uint64_t base = offset & ~(uint64_t) (FAT_CACHE - 1);

    if (!v->fat_cache_valid || v->fat_cache_base != base) {
        if (read_at(v, base, v->fat_cache, FAT_CACHE) != 0) return fail("could not read the FAT");
        v->fat_cache_base = base;
        v->fat_cache_valid = 1;
    }

    *next = le32(v->fat_cache + (offset - base));
    return 0;
}

static int mark(volume *v, const uint32_t cluster) {
    if (!valid_cluster(v, cluster)) return fail("a chain points outside the cluster heap");

    const uint32_t index = cluster - 2;
    if (v->used[index / 8] & (1U << (index % 8))) return fail("a cluster belongs to more than one chain");

    v->used[index / 8] |= (uint8_t) (1U << (index % 8));
    return 0;
}

static uint64_t clusters_for(const volume *v, const uint64_t length) {
    return (length + v->cluster_size - 1) / v->cluster_size;
}

static int
claim(volume *v, const uint32_t first, const uint64_t length, const int no_chain, uint32_t **chain, uint64_t *count) {
    const uint64_t needed = clusters_for(v, length);
    if (count) *count = 0;
    if (!first) return needed ? fail("allocated data has no first cluster") : 0;
    if (needed > v->cluster_count) return fail("a length is larger than the volume");

    uint64_t total = 0;
    uint32_t *list = NULL;
    uint64_t capacity = 0;
    uint32_t cluster = first;

    for (;;) {
        if (no_chain && total == needed) break;
        if (mark(v, cluster) != 0) {
            free(list);
            return -1;
        }

        if (chain) {
            if (total == capacity) {
                capacity = capacity ? capacity * 2 : 64;
                uint32_t *grown = realloc(list, capacity * sizeof(*list));
                if (!grown) {
                    free(list);
                    return fail("out of memory");
                }
                list = grown;
            }
            list[total] = cluster;
        }
        total++;

        if (no_chain) {
            cluster++;
            continue;
        }

        uint32_t next;
        if (fat_next(v, cluster, &next) != 0) {
            free(list);
            return -1;
        }
        if (next == FAT_END) break;
        if (next == FAT_BAD || !valid_cluster(v, next) || total > v->cluster_count) {
            free(list);
            return fail("a FAT chain is broken");
        }
        cluster = next;
    }

    if (total < needed) {
        free(list);
        return fail("a FAT chain is shorter than its data");
    }

    if (chain) *chain = list;
    if (count) *count = total;
    return 0;
}

static int read_chain(
    const volume *v, const uint32_t *chain, const uint64_t count, const int directory, uint8_t **data, uint64_t *size
) {
    const uint64_t total = count * v->cluster_size;
    if (total > MAX_DIR_SIZE) return fail("a directory is too large");

    *data = malloc(total ? total : 1);
    if (!*data) return fail("out of memory");

    const uint64_t piece = v->cluster_size < DIR_PIECE ? v->cluster_size : DIR_PIECE;
    *size = 0;
    while (*size < total) {
        const uint64_t cluster = *size / v->cluster_size;
        const uint64_t within = *size % v->cluster_size;
        if (read_at(v, cluster_offset(v, chain[cluster]) + within, *data + *size, piece) != 0) {
            free(*data);
            return fail("could not read a directory");
        }

        int ended = 0;
        for (uint64_t at = *size; directory && !ended && at < *size + piece; at += ENTRY_SIZE)
            ended = (*data)[at] == TYPE_END;
        *size += piece;
        if (ended) break;
    }
    return 0;
}

static int walk(volume *v, const uint32_t *chain, uint64_t count, int depth);

static int walk_entries(volume *v, const uint8_t *data, const uint64_t size, const int depth) {
    for (uint64_t at = 0; at + ENTRY_SIZE <= size; at += ENTRY_SIZE) {
        const uint8_t *entry = data + at;
        const uint8_t type = entry[0];

        if (type == TYPE_END) return 0;
        if (!(type & TYPE_IN_USE)) continue;

        switch (type) {
            case TYPE_BITMAP:
                if (depth != 0 || v->bitmap_cluster) return fail("unexpected allocation bitmap entry");
                v->bitmap_cluster = le32(entry + 20);
                if (claim(v, v->bitmap_cluster, le64(entry + 24), 0, &v->bitmap_chain, &v->bitmap_clusters) != 0)
                    return -1;
                continue;
            case TYPE_UPCASE:
                if (claim(v, le32(entry + 20), le64(entry + 24), 0, NULL, NULL) != 0) return -1;
                continue;
            case TYPE_LABEL:
            case TYPE_GUID:
            case TYPE_PADDING:
            case TYPE_ACL:
                continue;
            case TYPE_FILE:
                break;
            default:
                if (type & TYPE_SECONDARY) return fail("a secondary entry has no file entry");
                if (!(type & TYPE_BENIGN)) return fail("unknown critical directory entry");
                if ((le16(entry + 2) & FLAG_ALLOCATION)
                    && claim(v, le32(entry + 20), le64(entry + 24), le16(entry + 2) & FLAG_NO_CHAIN, NULL, NULL) != 0)
                    return -1;
                continue;
        }

        const unsigned secondaries = entry[1];
        const int directory = (le16(entry + 4) & ATTR_DIRECTORY) != 0;
        if (secondaries < 2 || at + (uint64_t) (secondaries + 1) * ENTRY_SIZE > size)
            return fail("a file entry set is incomplete");

        for (unsigned s = 1; s <= secondaries; s++) {
            const uint8_t *second = entry + s * ENTRY_SIZE;
            if ((second[0] & (TYPE_IN_USE | TYPE_SECONDARY)) != (TYPE_IN_USE | TYPE_SECONDARY)
                || (s == 1 && second[0] != TYPE_STREAM)
                || (second[0] != TYPE_STREAM && second[0] != TYPE_NAME && !(second[0] & TYPE_BENIGN)))
                return fail("a file entry set is damaged");
            if (!(second[1] & FLAG_ALLOCATION)) continue;

            const int stream = s == 1;
            uint32_t *chain = NULL;
            uint64_t count = 0;
            if (claim(
                    v, le32(second + 20), le64(second + 24), second[1] & FLAG_NO_CHAIN,
                    stream && directory ? &chain : NULL, &count
                )
                != 0)
                return -1;

            if (stream && directory) {
                const int result = walk(v, chain, count, depth + 1);
                free(chain);
                if (result != 0) return -1;
            }
        }

        at += (uint64_t) secondaries * ENTRY_SIZE;
    }
    return 0;
}

static int walk(volume *v, const uint32_t *chain, const uint64_t count, const int depth) {
    if (depth > MAX_DEPTH) return fail("directories are nested too deeply");

    uint8_t *data;
    uint64_t size;
    if (read_chain(v, chain, count, 1, &data, &size) != 0) return -1;

    const int result = walk_entries(v, data, size, depth);
    free(data);
    return result;
}

static int boot_checksum_ok(const volume *v) {
    uint8_t *region = malloc((size_t) v->sector_size * 12);
    if (!region || read_at(v, 0, region, (size_t) v->sector_size * 12) != 0) {
        free(region);
        return 0;
    }

    uint32_t sum = 0;
    for (size_t i = 0; i < (size_t) v->sector_size * 11; i++) {
        if (i == 106 || i == 107 || i == 112) continue;
        sum = ((sum & 1) ? 0x80000000U : 0) + (sum >> 1) + region[i];
    }

    const uint32_t stored = le32(region + (size_t) v->sector_size * 11);
    free(region);
    return sum == stored;
}

static int load(volume *v) {
    uint8_t boot[512];
    if (read_at(v, 0, boot, sizeof(boot)) != 0) return fail("could not read the boot sector");
    if (memcmp(boot + 3, "EXFAT   ", 8) != 0) return fail("not an exFAT volume");

    const uint8_t sector_shift = boot[108];
    const uint8_t cluster_shift = boot[109];
    if (sector_shift < 9 || sector_shift > 12 || cluster_shift > 25 - sector_shift)
        return fail("unsupported sector or cluster size");
    if (boot[110] != 1) return fail("volumes with more than one FAT are not supported");

    v->sector_size = 1U << sector_shift;
    v->cluster_size = (uint64_t) v->sector_size << cluster_shift;
    v->fat_offset = (uint64_t) le32(boot + 80) * v->sector_size;
    v->heap_offset = (uint64_t) le32(boot + 88) * v->sector_size;
    v->cluster_count = le32(boot + 92);
    v->root_cluster = le32(boot + 96);

    if (!v->cluster_count || !valid_cluster(v, v->root_cluster)) return fail("the boot sector is damaged");
    if (!boot_checksum_ok(v)) return fail("the boot region checksum does not match");

    v->used = calloc(v->cluster_count / 8 + 1, 1);
    return v->used ? 0 : fail("out of memory");
}

static int loop_backed(const char *path) {
    char real[PATH_MAX];
    if (!realpath(path, real)) return 1;

    DIR *loops = opendir("/sys/block");
    if (!loops) return 1;

    int found = 0;
    const struct dirent *entry;
    while (!found && (entry = readdir(loops))) {
        if (strncmp(entry->d_name, "loop", 4) != 0) continue;

        char backing[PATH_MAX + 64];
        snprintf(backing, sizeof(backing), "/sys/block/%s/loop/backing_file", entry->d_name);
        FILE *file = fopen(backing, "r");
        if (!file) continue;

        char target[PATH_MAX];
        if (fgets(target, sizeof(target), file)) {
            target[strcspn(target, "\n")] = '\0';
            found = strcmp(target, real) == 0;
        }
        fclose(file);
    }

    closedir(loops);
    return found;
}

static int open_volume(const char *path, const int reclaim) {
    struct stat info;
    if (stat(path, &info) != 0) return -1;
    if (!reclaim) return open(path, O_RDONLY | O_CLOEXEC);

    if (S_ISBLK(info.st_mode)) return open(path, O_RDWR | O_EXCL | O_CLOEXEC);
    if (!S_ISREG(info.st_mode) || loop_backed(path)) {
        errno = EBUSY;
        return -1;
    }
    return open(path, O_RDWR | O_CLOEXEC);
}

int main(const int argc, char *argv[]) {
    if (argc != 3 || (strcmp(argv[1], "check") != 0 && strcmp(argv[1], "reclaim") != 0)) {
        fprintf(stderr, "Usage: %s <check|reclaim> DEVICE\n", argv[0]);
        return 2;
    }

    const int reclaim = strcmp(argv[1], "reclaim") == 0;

    volume v = {0};
    v.fd = open_volume(argv[2], reclaim);
    if (v.fd < 0) {
        fprintf(stderr, "muexfat: %s: %s\n", argv[2], errno == EBUSY ? "the volume is in use" : strerror(errno));
        return 1;
    }

    uint32_t *root = NULL;
    uint64_t root_count = 0;
    if (load(&v) != 0 || claim(&v, v.root_cluster, 0, 0, &root, &root_count) != 0
        || walk(&v, root, root_count, 0) != 0) {
        free(root);
        close(v.fd);
        return 1;
    }
    free(root);

    if (!v.bitmap_cluster) {
        fail("no allocation bitmap was found");
        close(v.fd);
        return 1;
    }

    uint8_t *bitmap;
    uint64_t bitmap_size;
    if (read_chain(&v, v.bitmap_chain, v.bitmap_clusters, 0, &bitmap, &bitmap_size) != 0) return 1;

    uint64_t marked = 0, in_use = 0, leaked = 0;
    for (uint32_t i = 0; i < v.cluster_count; i++) {
        const int on_disk = (bitmap[i / 8] >> (i % 8)) & 1;
        const int needed = (v.used[i / 8] >> (i % 8)) & 1;
        marked += (uint64_t) on_disk;
        in_use += (uint64_t) needed;
        if (needed && !on_disk) {
            fprintf(stderr, "muexfat: cluster %u is in use but marked free, leaving the volume alone\n", i + 2);
            return 1;
        }
        if (on_disk && !needed) {
            bitmap[i / 8] &= (uint8_t) ~(1U << (i % 8));
            leaked++;
        }
    }

    printf(
        "Clusters: %u of %llu KiB, marked %llu, in use %llu, unused %llu (%llu MiB)\n", v.cluster_count,
        (unsigned long long) (v.cluster_size / 1024), (unsigned long long) marked, (unsigned long long) in_use,
        (unsigned long long) leaked, (unsigned long long) (leaked * v.cluster_size / (1024 * 1024))
    );

    int status = 0;
    if (reclaim && leaked) {
        for (uint64_t i = 0; i < v.bitmap_clusters && status == 0; i++) {
            if (write_at(&v, cluster_offset(&v, v.bitmap_chain[i]), bitmap + i * v.cluster_size, v.cluster_size) != 0)
                status = fail("could not write the allocation bitmap");
        }

        const uint8_t percent = (uint8_t) (in_use * 100 / v.cluster_count);
        if (status == 0 && write_at(&v, 112, &percent, 1) != 0) status = fail("could not update the volume usage");
        if (status == 0 && fsync(v.fd) != 0) status = fail("could not flush the volume");
        if (status == 0) printf("Reclaimed %llu MiB\n", (unsigned long long) (leaked * v.cluster_size / (1024 * 1024)));
    }

    free(bitmap);
    free(v.bitmap_chain);
    free(v.used);
    close(v.fd);
    return status ? 1 : 0;
}
