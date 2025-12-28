#include <stdio.h>
#include <string.h>

#include "xxhash/xxhash.h"
#include "verify.h"

static int file_xxh64(const char *path, char out[17]) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    XXH64_state_t *state = XXH64_createState();
    if (!state) {
        fclose(f);
        return -1;
    }

    XXH64_reset(state, 0);

    unsigned char buf[8192];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) XXH64_update(state, buf, n);

    fclose(f);

    XXH64_hash_t h = XXH64_digest(state);
    XXH64_freeState(state);

    snprintf(out, 17, "%016llx", (unsigned long long) h);

    return 0;
}

int script_hash_check(void) {
    int modified = 0;
    char hash[17];

    for (size_t i = 0; i < sizeof(int_scripts) / sizeof(int_scripts[0]); i++) {
        if (file_xxh64(int_scripts[i].path, hash) != 0 || strcmp(hash, int_scripts[i].hash) != 0) {
            modified++;
        }
    }

    return modified > 0 ? 1 : 0;
}
