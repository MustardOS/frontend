#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <openssl/evp.h>

static const unsigned char mustardos_integrity_public_key[32] = {
    0x9c, 0xb7, 0x14, 0xfe, 0x38, 0x40, 0x52, 0xf4, 0x7b, 0x56, 0x68, 0x9f, 0x9b, 0x7e, 0xbb, 0x82,
    0x57, 0x30, 0x59, 0xd5, 0x9b, 0x1d, 0xbb, 0x8d, 0x25, 0xb7, 0x68, 0x01, 0x4c, 0x26, 0xaf, 0x7a,
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

int main(const int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s MANIFEST SIGNATURE\n", argv[0]);
        return 2;
    }

    size_t manifest_size = 0;
    size_t signature_size = 0;
    unsigned char *manifest = read_file(argv[1], &manifest_size);
    unsigned char *signature = read_file(argv[2], &signature_size);
    if (!manifest || !signature || signature_size != 64) {
        fprintf(stderr, "Unable to read a valid MustardOS integrity manifest or signature: %s\n", strerror(errno));
        free(manifest);
        free(signature);
        return 2;
    }

    EVP_PKEY *key = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_ED25519, NULL, mustardos_integrity_public_key, sizeof(mustardos_integrity_public_key)
    );
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
        fprintf(stderr, "MustardOS integrity signature verification failed\n");
        return 1;
    }

    return 0;
}
