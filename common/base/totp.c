#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <common/base/totp.h>

enum {
    sha1_block_size = 64,
    sha1_digest_size = 20,
    totp_counter_size = 8,
};

_Static_assert(TOTP_SECRET_SIZE <= sha1_block_size, "TOTP secret must fit in one SHA-1 block");

static int make_parent_directories(const char *path) {
    char working[PATH_MAX];
    if ((size_t) snprintf(working, sizeof(working), "%s", path) >= sizeof(working)) return 0;

    char *leaf = strrchr(working, '/');
    if (!leaf || leaf == working) return 1;
    *leaf = '\0';

    for (char *p = working + 1; *p; ++p) {
        if (*p != '/') continue;
        *p = '\0';
        if (mkdir(working, 0755) < 0 && errno != EEXIST) return 0;
        *p = '/';
    }
    return mkdir(working, 0755) == 0 || errno == EEXIST;
}

int64_t totp_window(const int64_t when) {
    return when / TOTP_STEP;
}

int totp_remaining(const int64_t when) {
    return (int) (TOTP_STEP - when % TOTP_STEP);
}

struct sha1_state {
    uint32_t hash[5];
    uint64_t length;
    unsigned char block[sha1_block_size];
    size_t pending;
};

static uint32_t sha1_rotate(const uint32_t value, const unsigned bits) {
    return (value << bits) | (value >> (32 - bits));
}

static void sha1_compress(struct sha1_state *state, const unsigned char *block) {
    uint32_t words[80];

    for (int i = 0; i < 16; ++i)
        words[i] = (uint32_t) block[i * 4] << 24 | (uint32_t) block[i * 4 + 1] << 16 | (uint32_t) block[i * 4 + 2] << 8
                   | (uint32_t) block[i * 4 + 3];
    for (int i = 16; i < 80; ++i)
        words[i] = sha1_rotate(words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16], 1);

    uint32_t a = state->hash[0], b = state->hash[1], c = state->hash[2];
    uint32_t d = state->hash[3], e = state->hash[4];

    for (int i = 0; i < 80; ++i) {
        uint32_t function;
        uint32_t constant;

        if (i < 20) {
            function = (b & c) | (~b & d);
            constant = 0x5a827999;
        } else if (i < 40) {
            function = b ^ c ^ d;
            constant = 0x6ed9eba1;
        } else if (i < 60) {
            function = (b & c) | (b & d) | (c & d);
            constant = 0x8f1bbcdc;
        } else {
            function = b ^ c ^ d;
            constant = 0xca62c1d6;
        }

        const uint32_t next = sha1_rotate(a, 5) + function + e + constant + words[i];
        e = d;
        d = c;
        c = sha1_rotate(b, 30);
        b = a;
        a = next;
    }

    state->hash[0] += a;
    state->hash[1] += b;
    state->hash[2] += c;
    state->hash[3] += d;
    state->hash[4] += e;
}

static void sha1_init(struct sha1_state *state) {
    state->hash[0] = 0x67452301;
    state->hash[1] = 0xefcdab89;
    state->hash[2] = 0x98badcfe;
    state->hash[3] = 0x10325476;
    state->hash[4] = 0xc3d2e1f0;
    state->length = 0;
    state->pending = 0;
}

static void sha1_update(struct sha1_state *state, const unsigned char *data, size_t length) {
    state->length += length;

    while (length) {
        const size_t room = sha1_block_size - state->pending;
        const size_t take = length < room ? length : room;

        memcpy(state->block + state->pending, data, take);
        state->pending += take;
        data += take;
        length -= take;

        if (state->pending == sha1_block_size) {
            sha1_compress(state, state->block);
            state->pending = 0;
        }
    }
}

static void sha1_final(struct sha1_state *state, unsigned char *digest) {
    const uint64_t bits = state->length * 8;
    const unsigned char padding = 0x80;
    const unsigned char zero = 0x00;

    sha1_update(state, &padding, 1);
    while (state->pending != 56)
        sha1_update(state, &zero, 1);

    unsigned char tail[8];
    for (int i = 0; i < 8; ++i)
        tail[i] = (unsigned char) (bits >> (56 - i * 8));
    sha1_update(state, tail, 8);

    for (int i = 0; i < 5; ++i) {
        digest[i * 4] = (unsigned char) (state->hash[i] >> 24);
        digest[i * 4 + 1] = (unsigned char) (state->hash[i] >> 16);
        digest[i * 4 + 2] = (unsigned char) (state->hash[i] >> 8);
        digest[i * 4 + 3] = (unsigned char) state->hash[i];
    }
}

static void totp_hmac_sha1(
    const unsigned char key[static TOTP_SECRET_SIZE], const unsigned char message[static totp_counter_size],
    unsigned char digest[static sha1_digest_size]
) {
    unsigned char padded[sha1_block_size];
    unsigned char inner[sha1_digest_size];
    struct sha1_state state;

    memset(padded, 0, sizeof(padded));
    memcpy(padded, key, TOTP_SECRET_SIZE);

    unsigned char block[sha1_block_size];
    for (size_t i = 0; i < sizeof(block); ++i)
        block[i] = padded[i] ^ 0x36;

    sha1_init(&state);
    sha1_update(&state, block, sizeof(block));
    sha1_update(&state, message, totp_counter_size);
    sha1_final(&state, inner);

    for (size_t i = 0; i < sizeof(block); ++i)
        block[i] = padded[i] ^ 0x5c;

    sha1_init(&state);
    sha1_update(&state, block, sizeof(block));
    sha1_update(&state, inner, sizeof(inner));
    sha1_final(&state, digest);
}

void totp_code(const unsigned char *secret, const int64_t window, char *out, const size_t out_size) {
    unsigned char counter[totp_counter_size];
    unsigned char digest[sha1_digest_size];

    for (int i = 0; i < totp_counter_size; ++i)
        counter[i] = (unsigned char) ((uint64_t) window >> ((totp_counter_size - 1 - i) * 8));
    totp_hmac_sha1(secret, counter, digest);

    const int offset = digest[sha1_digest_size - 1] & 0x0f;
    const uint32_t truncated = ((uint32_t) (digest[offset] & 0x7f) << 24) | ((uint32_t) digest[offset + 1] << 16)
                               | ((uint32_t) digest[offset + 2] << 8) | (uint32_t) digest[offset + 3];

    unsigned divisor = 1;
    for (int i = 0; i < TOTP_DIGITS; ++i)
        divisor *= 10;
    snprintf(out, out_size, "%0*u", TOTP_DIGITS, truncated % divisor);
}

/* Loads the device secret, minting one on first run. Kept outside the web root and
   readable only by root so it never travels over the wire. */
int totp_secret_load(const char *path, unsigned char *secret) {
    int descriptor = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);

    if (descriptor >= 0) {
        const ssize_t got = read(descriptor, secret, TOTP_SECRET_SIZE);
        close(descriptor);
        if (got == (ssize_t) TOTP_SECRET_SIZE) return 1;
    }

    const int random = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (random < 0) return 0;

    const ssize_t got = read(random, secret, TOTP_SECRET_SIZE);
    close(random);
    if (got != (ssize_t) TOTP_SECRET_SIZE) return 0;

    make_parent_directories(path);

    descriptor = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (descriptor < 0) return 0;

    const ssize_t written = write(descriptor, secret, TOTP_SECRET_SIZE);
    close(descriptor);
    return written == (ssize_t) TOTP_SECRET_SIZE;
}

int totp_matches(const unsigned char *secret, const char *candidate) {
    if (!candidate || strlen(candidate) != TOTP_DIGITS) return 0;

    const int64_t window = totp_window(time(NULL));
    int accepted = 0;

    for (int64_t offset = -TOTP_SKEW; offset <= TOTP_SKEW; ++offset) {
        char expected[16];
        totp_code(secret, window + offset, expected, sizeof(expected));

        unsigned difference = 0;
        for (int i = 0; i < TOTP_DIGITS; ++i)
            difference |= (unsigned) (expected[i] ^ candidate[i]);

        accepted |= difference == 0;
    }
    return accepted;
}

int totp_secret_read(const char *path, unsigned char *secret) {
    const int descriptor = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (descriptor < 0) return 0;

    const ssize_t got = read(descriptor, secret, TOTP_SECRET_SIZE);
    close(descriptor);
    return got == (ssize_t) TOTP_SECRET_SIZE;
}
