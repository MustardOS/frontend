#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <strings.h>
#include <openssl/evp.h>

static const unsigned char mustardos_integrity_public_key[32] = {
    0x9c, 0xb7, 0x14, 0xfe, 0x38, 0x40, 0x52, 0xf4, 0x7b, 0x56, 0x68, 0x9f, 0x9b, 0x7e, 0xbb, 0x82,
    0x57, 0x30, 0x59, 0xd5, 0x9b, 0x1d, 0xbb, 0x8d, 0x25, 0xb7, 0x68, 0x01, 0x4c, 0x26, 0xaf, 0x7a,
};

static const unsigned char mustardos_core_public_key[32] = {
    0xcb, 0x77, 0xed, 0x50, 0xea, 0xf8, 0xb7, 0xbd, 0x8c, 0x94, 0x94, 0x36, 0x81, 0x45, 0x3a, 0xba,
    0x62, 0x74, 0xca, 0xf8, 0xae, 0x55, 0x4f, 0x96, 0xc3, 0x52, 0x84, 0x4d, 0xb9, 0xc2, 0x4a, 0x5b,
};

static unsigned char *read_file(const char *path, size_t *size) {
    struct stat info;
    if (stat(path, &info) != 0 || info.st_size < 0) return NULL;

    FILE *file = fopen(path, "rb");
    if (!file) return NULL;

    const size_t length = (size_t) info.st_size;
    unsigned char *data = malloc(length ? length : 1);
    if (!data) {
        fclose(file);
        return NULL;
    }

    if (length && fread(data, 1, length, file) != length) {
        free(data);
        fclose(file);
        return NULL;
    }

    fclose(file);
    *size = length;
    return data;
}

static int verify_signature(const unsigned char *public_key, const char *manifest_path, const char *signature_path) {
    size_t manifest_size = 0;
    size_t signature_size = 0;
    unsigned char *manifest = read_file(manifest_path, &manifest_size);
    unsigned char *signature = read_file(signature_path, &signature_size);
    if (!manifest || !signature || signature_size != 64) {
        fprintf(stderr, "Unable to read a valid MustardOS manifest or signature: %s\n", strerror(errno));
        free(manifest);
        free(signature);
        return 2;
    }

    EVP_PKEY *key = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, public_key, 32);
    EVP_MD_CTX *context = EVP_MD_CTX_new();

    int valid = 0;
    if (key && context && EVP_DigestVerifyInit(context, NULL, NULL, NULL, key) == 1) {
        valid = EVP_DigestVerify(context, signature, signature_size, manifest, manifest_size) == 1;
    }

    EVP_MD_CTX_free(context);
    EVP_PKEY_free(key);
    free(manifest);
    free(signature);

    if (!valid) {
        fprintf(stderr, "MustardOS signature verification failed\n");
        return 1;
    }

    return 0;
}

static int verify_sha256(const char *path, const char *expected) {
    if (strlen(expected) != 64) {
        fprintf(stderr, "Expected a SHA-256 digest of 64 hexadecimal characters\n");
        return 2;
    }

    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Unable to read %s: %s\n", path, strerror(errno));
        return 2;
    }

    EVP_MD_CTX *context = EVP_MD_CTX_new();
    int okay = context && EVP_DigestInit_ex(context, EVP_sha256(), NULL) == 1;

    unsigned char buffer[65536];
    size_t count;
    while (okay && (count = fread(buffer, 1, sizeof(buffer), file)) > 0)
        okay = EVP_DigestUpdate(context, buffer, count) == 1;

    if (ferror(file)) okay = 0;
    fclose(file);

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_size = 0;
    if (okay) okay = EVP_DigestFinal_ex(context, digest, &digest_size) == 1 && digest_size == 32;
    EVP_MD_CTX_free(context);

    if (!okay) {
        fprintf(stderr, "Unable to hash %s\n", path);
        return 2;
    }

    char actual[65];
    for (unsigned int i = 0; i < digest_size; i++)
        snprintf(actual + i * 2, 3, "%02x", digest[i]);

    if (strcasecmp(actual, expected) != 0) {
        fprintf(stderr, "SHA-256 mismatch for %s\n", path);
        return 1;
    }

    return 0;
}

int main(const int argc, char **argv) {
    if (argc == 3) return verify_signature(mustardos_integrity_public_key, argv[1], argv[2]);
    if (argc == 4 && strcmp(argv[1], "--core") == 0)
        return verify_signature(mustardos_core_public_key, argv[2], argv[3]);
    if (argc == 4 && strcmp(argv[1], "--sha256") == 0) return verify_sha256(argv[2], argv[3]);

    fprintf(stderr, "Usage: %s MANIFEST SIGNATURE\n", argv[0]);
    fprintf(stderr, "       %s --core MANIFEST SIGNATURE\n", argv[0]);
    fprintf(stderr, "       %s --sha256 FILE DIGEST\n", argv[0]);
    return 2;
}
