#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <SDL2/SDL.h>
#include "../../common/fileio.h"
#include "../../common/init.h"
#include "../../common/language.h"
#include "../../common/log.h"
#include "../../common/options.h"
#include "../../common/randname.h"
#include "../cheevo/cheevo.h"
#include "../core/core.h"
#include "../core/perf.h"
#include "../core/muxretro.h"
#include "../core/paths.h"
#include "../core/runahead.h"
#include "../input/rumble.h"
#include "../settings/settings.h"
#include "../ui/cheats.h"
#include "../ui/options.h"
#include "../video/hw_render.h"
#include "netplay.h"

#define NETPLAY_PROTOCOL               4U
#define NETPLAY_HEADER_SIZE            32U
#define NETPLAY_CONTROL_CAP            4096U
#define NETPLAY_STATE_CAP              (128U * 1024U * 1024U)
#define NETPLAY_IO_TIMEOUT_MS          30000U
#define NETPLAY_DIGEST_INTERVAL        300U
#define NETPLAY_DIGEST_INTERVAL_MAX    3600U
#define NETPLAY_DIGEST_BUDGET_MS       4.0
#define NETPLAY_CLIENT_CAPACITY        3U
#define NETPLAY_HASH_CACHE             RETRO_HSH_PATH "/netplay"
#define NETPLAY_SETTINGS_DIR           RETRO_SET_PATH "/netplay"
#define NETPLAY_HOST_NAME_FILE         NETPLAY_SETTINGS_DIR "/host-name"
#define NETPLAY_MODE_FILE              NETPLAY_SETTINGS_DIR "/play-mode"
#define NETPLAY_SLOTS_FILE             NETPLAY_SETTINGS_DIR "/client-slots"
#define NETPLAY_PACKET_CAP             (64U * 1024U)
#define NETPLAY_PACKET_QUEUE_CAP       32U
#define NETPLAY_MENU_PAUSE_LEAD_FRAMES 12U
#define NETPLAY_INPUT_HISTORY_CAPACITY 256U
#define NETPLAY_INPUT_SEND_BUDGET      8U
#define NETPLAY_RECEIVE_DRAIN_BUDGET   32U

enum {
    netplay_message_pair = 1,
    netplay_message_hello,
    netplay_message_state,
    netplay_message_ready,
    netplay_message_input,
    netplay_message_ping,
    netplay_message_pong,
    netplay_message_menu_state,
    netplay_message_menu_pause,
    netplay_message_digest,
    netplay_message_disconnect,
    netplay_message_netpacket,
    netplay_message_delay
};

typedef struct {
    uint8_t *data;
    size_t size;
} netplay_packet;

typedef struct {
    uint8_t core_hash[SHA256_DIGEST_LENGTH];
    uint8_t content_hash[SHA256_DIGEST_LENGTH];
    uint8_t option_hash[SHA256_DIGEST_LENGTH];
    char core_name[64];
    char core_version[32];
} netplay_manifest;

typedef struct {
    pthread_t thread;
    int running;
    int socket_fd;
    SSL *ssl;
    uint32_t tx_sequence;
    uint32_t rx_sequence;
    int local_confirmed;
    int peer_confirmed;
    int pair_sent;
    int hello_sent;
    int hello_received;
    int compatible;
    int ready_sent;
    int ready_received;
    int tx_state_pending;
    uint8_t *rx_state;
    size_t rx_state_size;
    uint64_t rx_state_frame;
    int rx_state_pending;
    uint32_t ping_nonce;
    uint32_t ping_sent_at;
    uint32_t next_ping_at;
    unsigned owner_port;
    char pairing_code[8];
    uint64_t sent_input_frame[NETPLAY_PORT_COUNT];
    int sent_input_valid[NETPLAY_PORT_COUNT];
    uint64_t sent_digest_generation;
    uint64_t sent_delay_generation;
    int menu_open;
    int menu_state_received;
    int sent_menu_state_valid;
    uint64_t sent_menu_state_generation;
    uint64_t sent_menu_pause_generation;
    int wake_fd;
    uint8_t rx_control[NETPLAY_CONTROL_CAP];
} netplay_peer;

typedef struct {
    uint64_t frame;
    netplay_pad_state input;
    int valid;
} netplay_input_history;

typedef struct {
    pthread_mutex_t mutex;
    atomic_int stop;
    atomic_int disconnecting;
    SSL_CTX *tls;
    X509 *certificate;
    EVP_PKEY *private_key;
    pthread_t accept_thread;
    int accept_running;
    int listen_fd;
    netplay_peer peers[NETPLAY_CLIENT_CAPACITY];
    unsigned peer_count;
    netplay_status status;
    netplay_role role;
    netplay_info public_info;
    char core_path[PATH_MAX];
    char content_path[PATH_MAX];
    char join_address[256];
    char host_name[NETPLAY_HOST_NAME_SIZE];
    netplay_mode host_mode;
    netplay_mode mode;
    unsigned host_slots;
    uint16_t port;
    uint64_t session_id;
    netplay_manifest manifest;
    int manifest_ready;
    netplay_input_history local_history[NETPLAY_INPUT_HISTORY_CAPACITY];
    netplay_input_history remote_history[NETPLAY_PORT_COUNT][NETPLAY_INPUT_HISTORY_CAPACITY];
    uint64_t local_input_frame;
    uint64_t local_input_generation;
    uint64_t remote_input_generation[NETPLAY_PORT_COUNT];
    uint64_t remote_input_frame[NETPLAY_PORT_COUNT];
    uint64_t frame;
    struct retro_netpacket_callback netpacket;
    int netpacket_available;
    int netpacket_started;
    netplay_packet outgoing_packets[NETPLAY_PACKET_QUEUE_CAP];
    unsigned outgoing_head;
    unsigned outgoing_count;
    netplay_packet incoming_packets[NETPLAY_PACKET_QUEUE_CAP];
    unsigned incoming_head;
    unsigned incoming_count;
    int sync_state_sent;
    uint8_t *sync_state;
    size_t sync_state_size;
    uint64_t sync_state_frame;
    unsigned sync_state_refs;
    pthread_t discovery_thread;
    int discovery_running;
    atomic_int discovery_stop;
    netplay_discovered_host discovered[8];
    unsigned discovered_count;
    int digest_due;
    uint64_t digest_send_generation;
    uint64_t delay_send_generation;
    uint8_t delay_send_value;
    uint64_t delay_send_frame;
    int delay_change_pending;
    uint8_t delay_change_value;
    uint64_t delay_change_frame;
    int local_menu_open;
    uint64_t menu_state_generation;
    int menu_pause_requested;
    int menu_pause_value;
    uint64_t menu_pause_frame;
    uint64_t menu_pause_generation;
    uint64_t local_digest_frame;
    uint64_t digest_next_frame;
    unsigned digest_interval;
    double digest_serialise_ms;
    uint8_t local_digest[SHA256_DIGEST_LENGTH];
    int remote_digest_pending[NETPLAY_PORT_COUNT];
    uint64_t remote_digest_frame[NETPLAY_PORT_COUNT];
    uint8_t remote_digest[NETPLAY_PORT_COUNT][SHA256_DIGEST_LENGTH];
    unsigned resynchronisations;
    uint8_t *digest_state;
    size_t digest_state_capacity;
    pthread_t digest_thread;
    int digest_thread_running;
    pthread_mutex_t digest_mutex;
    pthread_cond_t digest_wake;
    atomic_int digest_stop;
    int digest_job_pending;
    int digest_job_busy;
    int digest_ready;
    uint64_t digest_job_frame;
    uint64_t digest_ready_frame;
    uint8_t digest_ready_value[SHA256_DIGEST_LENGTH];
    uint8_t *digest_hash_state;
    size_t digest_hash_size;
    size_t digest_hash_capacity;
    int failure_announced;
} netplay_context;

static netplay_context netplay;
static int netplay_initialised;
static atomic_int netplay_fast_status = ATOMIC_VAR_INIT(netplay_status_idle);
static struct retro_netpacket_callback pending_netpacket;
static int pending_netpacket_available;

static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t) data[0] << 8 | data[1];
}

static uint32_t read_u32(const uint8_t *data) {
    return (uint32_t) data[0] << 24 | (uint32_t) data[1] << 16 | (uint32_t) data[2] << 8 | data[3];
}

static uint64_t read_u64(const uint8_t *data) {
    return (uint64_t) read_u32(data) << 32 | read_u32(data + 4);
}

static void write_u16(uint8_t *data, const uint16_t value) {
    data[0] = (uint8_t) (value >> 8);
    data[1] = (uint8_t) value;
}

static void write_u32(uint8_t *data, const uint32_t value) {
    data[0] = (uint8_t) (value >> 24);
    data[1] = (uint8_t) (value >> 16);
    data[2] = (uint8_t) (value >> 8);
    data[3] = (uint8_t) value;
}

static void write_u64(uint8_t *data, const uint64_t value) {
    write_u32(data, (uint32_t) (value >> 32));
    write_u32(data + 4, (uint32_t) value);
}

static void set_status(const netplay_status status) {
    pthread_mutex_lock(&netplay.mutex);
    const netplay_status previous = netplay.status;
    netplay.status = status;
    netplay.public_info.status = status;
    pthread_mutex_unlock(&netplay.mutex);
    atomic_store(&netplay_fast_status, status);
    if (previous != status) LOG_INFO(mux_module, "netplay: %s", netplay_status_name(status));
}

static void set_failure(const char *message) {
    pthread_mutex_lock(&netplay.mutex);
    if (netplay.status == netplay_status_failed) {
        pthread_mutex_unlock(&netplay.mutex);
        return;
    }
    netplay.failure_announced = 0;
    netplay.status = netplay_status_failed;
    netplay.public_info.status = netplay_status_failed;
    snprintf(
        netplay.public_info.failure, sizeof(netplay.public_info.failure), "%s",
        message ? message : lang.muxretro.netplay.failed
    );
    pthread_mutex_unlock(&netplay.mutex);
    atomic_store(&netplay_fast_status, netplay_status_failed);
    LOG_WARN(mux_module, "netplay: %s", message ? message : lang.muxretro.netplay.failed);
}

static int normalise_host_name(const char *source, char output[NETPLAY_HOST_NAME_SIZE]) {
    if (!source) source = "";
    while (*source == ' ')
        source++;
    size_t length = strlen(source);
    while (length && source[length - 1] == ' ')
        length--;
    if (!length) return randname_generate_with_separator(output, NETPLAY_HOST_NAME_SIZE, " ");
    if (length >= NETPLAY_HOST_NAME_SIZE) return -1;
    for (size_t index = 0; index < length; index++) {
        const unsigned char character = (unsigned char) source[index];
        if (character < 32 || character == 127) return -1;
    }
    snprintf(output, NETPLAY_HOST_NAME_SIZE, "%.*s", (int) length, source);
    return 0;
}

static int host_name_read(const char *path, char output[NETPLAY_HOST_NAME_SIZE]) {
    const int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return -1;
    struct stat file_stat;
    char stored[NETPLAY_HOST_NAME_SIZE] = {0};
    const ssize_t count = read(fd, stored, sizeof(stored) - 1);
    const int valid = fstat(fd, &file_stat) == 0 && S_ISREG(file_stat.st_mode) && file_stat.st_nlink == 1
                      && file_stat.st_size >= 0 && file_stat.st_size < (off_t) sizeof(stored)
                      && count == file_stat.st_size;
    close(fd);
    const char *content = stored;
    while (*content == ' ')
        content++;
    return valid && *content && normalise_host_name(stored, output) == 0 ? 0 : -1;
}

static int host_name_save(const char *name) {
    create_directories(NETPLAY_SETTINGS_DIR, 0);
    const int directory = open(NETPLAY_SETTINGS_DIR, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (directory < 0) return -1;
    unlinkat(directory, ".host-name.tmp", 0);
    const int fd = openat(directory, ".host-name.tmp", O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd < 0) {
        close(directory);
        return -1;
    }
    const size_t length = strlen(name);
    size_t offset = 0;
    while (offset < length) {
        const ssize_t written = write(fd, name + offset, length - offset);
        if (written > 0) {
            offset += (size_t) written;
        } else if (written < 0 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
    int okay = offset == length && fsync(fd) == 0;
    if (close(fd) != 0) okay = 0;
    if (!okay || renameat(directory, ".host-name.tmp", directory, "host-name") != 0) {
        unlinkat(directory, ".host-name.tmp", 0);
        close(directory);
        return -1;
    }
    fsync(directory);
    close(directory);
    return 0;
}

static void host_name_load(void) {
    if (host_name_read(NETPLAY_HOST_NAME_FILE, netplay.host_name) == 0) return;
    if (randname_generate_with_separator(netplay.host_name, sizeof(netplay.host_name), " ") == 0)
        host_name_save(netplay.host_name);
}

static void host_session_settings_load(void) {
    const int mode = cfg_read_int(NETPLAY_MODE_FILE, netplay_mode_separate);
    const int slots = cfg_read_int(NETPLAY_SLOTS_FILE, 1);
    netplay.host_mode = mode == netplay_mode_play_together ? netplay_mode_play_together : netplay_mode_separate;
    netplay.host_slots = slots >= 1 && slots <= (int) NETPLAY_CLIENT_CAPACITY ? (unsigned) slots : 1U;
}

static int hash_file(const char *path, uint8_t digest[SHA256_DIGEST_LENGTH]) {
    FILE *file = fopen(path, "rb");
    if (!file) return -1;

    EVP_MD_CTX *context = EVP_MD_CTX_new();
    uint8_t *buffer = malloc(256U * 1024U);
    int okay = context && buffer && EVP_DigestInit_ex(context, EVP_sha256(), NULL) == 1;

    while (okay) {
        const size_t count = fread(buffer, 1, 256U * 1024U, file);
        if (count && EVP_DigestUpdate(context, buffer, count) != 1) okay = 0;
        if (count < 256U * 1024U) {
            if (ferror(file)) okay = 0;
            break;
        }
        if (atomic_load(&netplay.stop)) okay = 0;
    }

    unsigned digest_size = 0;
    if (okay && EVP_DigestFinal_ex(context, digest, &digest_size) != 1) okay = 0;
    if (digest_size != SHA256_DIGEST_LENGTH) okay = 0;

    free(buffer);
    EVP_MD_CTX_free(context);
    fclose(file);
    return okay ? 0 : -1;
}

static void digest_hex(const uint8_t digest[SHA256_DIGEST_LENGTH], char output[SHA256_DIGEST_LENGTH * 2 + 1]) {
    static const char alphabet[] = "0123456789abcdef";
    for (unsigned index = 0; index < SHA256_DIGEST_LENGTH; index++) {
        output[index * 2] = alphabet[digest[index] >> 4];
        output[index * 2 + 1] = alphabet[digest[index] & 15];
    }
    output[SHA256_DIGEST_LENGTH * 2] = '\0';
}

static int hash_content(const char *path, uint8_t digest[SHA256_DIGEST_LENGTH]) {
    struct stat before;
    if (stat(path, &before) != 0 || !S_ISREG(before.st_mode)) return -1;

    uint8_t path_digest[SHA256_DIGEST_LENGTH];
    SHA256((const uint8_t *) path, strlen(path), path_digest);
    char name[SHA256_DIGEST_LENGTH * 2 + 1];
    digest_hex(path_digest, name);
    char cache_path[PATH_MAX];
    snprintf(cache_path, sizeof(cache_path), "%s/%s.bin", NETPLAY_HASH_CACHE, name);

    uint8_t record[56];
    const int cache = open(cache_path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (cache >= 0) {
        struct stat cache_stat;
        const ssize_t count = read(cache, record, sizeof(record));
        const int valid = fstat(cache, &cache_stat) == 0 && S_ISREG(cache_stat.st_mode)
                          && cache_stat.st_size == (off_t) sizeof(record) && count == (ssize_t) sizeof(record)
                          && memcmp(record, "PKNH", 4) == 0 && record[4] == 1
                          && read_u64(record + 8) == (uint64_t) before.st_size
                          && read_u64(record + 16) == (uint64_t) before.st_mtime;
        close(cache);
        if (valid) {
            memcpy(digest, record + 24, SHA256_DIGEST_LENGTH);
            return 0;
        }
    }

    if (hash_file(path, digest) != 0) return -1;
    struct stat after;
    if (stat(path, &after) != 0 || before.st_size != after.st_size || before.st_mtime != after.st_mtime) return -1;

    memset(record, 0, sizeof(record));
    memcpy(record, "PKNH", 4);
    record[4] = 1;
    write_u64(record + 8, (uint64_t) after.st_size);
    write_u64(record + 16, (uint64_t) after.st_mtime);
    memcpy(record + 24, digest, SHA256_DIGEST_LENGTH);
    create_directories(NETPLAY_HASH_CACHE, 0);

    char temporary[PATH_MAX];
    snprintf(temporary, sizeof(temporary), "%s.tmp", cache_path);
    const int output = open(temporary, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (output >= 0) {
        const int written = write(output, record, sizeof(record)) == (ssize_t) sizeof(record) && fsync(output) == 0;
        close(output);
        if (written) {
            if (rename(temporary, cache_path) != 0) unlink(temporary);
        } else {
            unlink(temporary);
        }
    }
    return 0;
}

static void hash_options(uint8_t digest[SHA256_DIGEST_LENGTH]) {
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    if (!context || EVP_DigestInit_ex(context, EVP_sha256(), NULL) != 1) {
        memset(digest, 0, SHA256_DIGEST_LENGTH);
        EVP_MD_CTX_free(context);
        return;
    }

    for (int index = 0; index < options_count; index++) {
        const struct core_option_entry *option = &options_list[index];
        EVP_DigestUpdate(context, option->key, strlen(option->key) + 1);
        if (option->current_index >= 0 && option->current_index < option->value_count)
            EVP_DigestUpdate(
                context, option->values[option->current_index], strlen(option->values[option->current_index]) + 1
            );
    }

    for (unsigned port = 0; port < NETPLAY_PORT_COUNT; port++) {
        uint8_t device[4];
        write_u32(device, (uint32_t) session_settings.port_device_id[port]);
        EVP_DigestUpdate(context, device, sizeof(device));
    }
    EVP_DigestUpdate(context, core_active_patches, strlen(core_active_patches) + 1);

    unsigned digest_size = 0;
    EVP_DigestFinal_ex(context, digest, &digest_size);
    EVP_MD_CTX_free(context);
}

static int prepare_manifest(void) {
    pthread_mutex_lock(&netplay.mutex);
    const int ready = netplay.manifest_ready;
    pthread_mutex_unlock(&netplay.mutex);
    if (ready) return 0;

    netplay_manifest manifest = {0};
    pthread_mutex_lock(&netplay.mutex);
    snprintf(manifest.core_name, sizeof(manifest.core_name), "%s", netplay.manifest.core_name);
    snprintf(manifest.core_version, sizeof(manifest.core_version), "%s", netplay.manifest.core_version);
    memcpy(manifest.option_hash, netplay.manifest.option_hash, sizeof(manifest.option_hash));
    pthread_mutex_unlock(&netplay.mutex);

    if (hash_file(netplay.core_path, manifest.core_hash) != 0
        || hash_content(netplay.content_path, manifest.content_hash) != 0) {
        set_failure(lang.muxretro.netplay.identity_failed);
        return -1;
    }

    pthread_mutex_lock(&netplay.mutex);
    netplay.manifest = manifest;
    netplay.manifest_ready = 1;
    pthread_mutex_unlock(&netplay.mutex);
    return 0;
}

static int tls_verify(int okay, X509_STORE_CTX *store) {
    (void) okay;
    (void) store;
    return 1;
}

static int tls_create(void) {
    EVP_PKEY_CTX *key_context = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    EVP_PKEY *key = NULL;
    if (!key_context || EVP_PKEY_keygen_init(key_context) <= 0
        || EVP_PKEY_CTX_set_rsa_keygen_bits(key_context, 2048) <= 0 || EVP_PKEY_keygen(key_context, &key) <= 0) {
        EVP_PKEY_CTX_free(key_context);
        return -1;
    }
    EVP_PKEY_CTX_free(key_context);

    X509 *certificate = X509_new();
    if (!certificate) {
        EVP_PKEY_free(key);
        return -1;
    }

    uint8_t serial_bytes[8];
    if (RAND_bytes(serial_bytes, sizeof(serial_bytes)) != 1) {
        X509_free(certificate);
        EVP_PKEY_free(key);
        return -1;
    }
    ASN1_INTEGER_set(X509_get_serialNumber(certificate), (long) (read_u64(serial_bytes) & 0x7fffffff));
    X509_set_version(certificate, 2);
    X509_gmtime_adj(X509_get_notBefore(certificate), -60);
    X509_gmtime_adj(X509_get_notAfter(certificate), 24 * 60 * 60);
    X509_set_pubkey(certificate, key);
    X509_NAME *name = X509_get_subject_name(certificate);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char *) "Pickles Network Play", -1, -1, 0);
    X509_set_issuer_name(certificate, name);
    if (X509_sign(certificate, key, EVP_sha256()) <= 0) {
        X509_free(certificate);
        EVP_PKEY_free(key);
        return -1;
    }

    SSL_CTX *tls = SSL_CTX_new(TLS_method());
    if (!tls || SSL_CTX_set_min_proto_version(tls, TLS1_3_VERSION) != 1
        || SSL_CTX_use_certificate(tls, certificate) != 1 || SSL_CTX_use_PrivateKey(tls, key) != 1) {
        SSL_CTX_free(tls);
        X509_free(certificate);
        EVP_PKEY_free(key);
        return -1;
    }

    SSL_CTX_set_verify(tls, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, tls_verify);
    SSL_CTX_set_options(tls, SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION);
    netplay.tls = tls;
    netplay.certificate = certificate;
    netplay.private_key = key;
    return 0;
}

static int set_nonblocking(const int fd) {
    const int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0 ? 0 : -1;
}

static int wait_socket(const int fd, const short events, const uint32_t timeout_ms) {
    struct pollfd poll_fd = {fd, events, 0};
    const int result = poll(&poll_fd, 1, (int) timeout_ms);
    if (result <= 0) return result;
    if (poll_fd.revents & events) return 1;
    if (poll_fd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        errno = ECONNRESET;
        return -1;
    }
    return 0;
}

static int wait_socket_or_wake(const netplay_peer *peer, const short events, const uint32_t timeout_ms) {
    if (peer->wake_fd < 0) return wait_socket(peer->socket_fd, events, timeout_ms);

    struct pollfd poll_fds[2] = {{peer->socket_fd, events, 0}, {peer->wake_fd, POLLIN, 0}};
    const int result = poll(poll_fds, 2, (int) timeout_ms);
    if (result <= 0) return result;

    if (poll_fds[1].revents & POLLIN) {
        uint64_t drained;
        while (read(peer->wake_fd, &drained, sizeof(drained)) > 0) {
        }
    }

    if (poll_fds[0].revents & events) return 1;
    if (poll_fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
        errno = ECONNRESET;
        return -1;
    }

    return 0;
}

static int peer_read_ready(const netplay_peer *peer, const uint32_t timeout_ms) {
    uint8_t byte;
    for (int attempt = 0; attempt < 3; attempt++) {
        const int result = SSL_peek(peer->ssl, &byte, 1);
        if (result > 0) return 1;
        const int error = SSL_get_error(peer->ssl, result);
        if (error == SSL_ERROR_SYSCALL && errno == EINTR) return 0;
        if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE) {
            if (!errno) errno = error == SSL_ERROR_ZERO_RETURN ? ECONNRESET : EPROTO;
            return -1;
        }
        const int ready =
            wait_socket_or_wake(peer, error == SSL_ERROR_WANT_READ ? POLLIN : POLLOUT, attempt ? 0 : timeout_ms);
        if (ready <= 0) return ready;
    }
    return 0;
}

static int tls_handshake(SSL *ssl, const int fd, const int server) {
    const uint32_t deadline = SDL_GetTicks() + NETPLAY_IO_TIMEOUT_MS;
    for (;;) {
        const int result = server ? SSL_accept(ssl) : SSL_connect(ssl);
        if (result == 1) return 0;
        const int error = SSL_get_error(ssl, result);
        if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE) return -1;
        if (SDL_TICKS_PASSED(SDL_GetTicks(), deadline) || atomic_load(&netplay.stop)) return -1;
        wait_socket(fd, error == SSL_ERROR_WANT_READ ? POLLIN : POLLOUT, 50);
    }
}

static int ssl_transfer(SSL *ssl, const int fd, uint8_t *data, const size_t size, const int writing) {
    size_t offset = 0;
    const uint32_t deadline = SDL_GetTicks() + NETPLAY_IO_TIMEOUT_MS;
    while (offset < size) {
        const int chunk = size - offset > INT_MAX ? INT_MAX : (int) (size - offset);
        const int result = writing ? SSL_write(ssl, data + offset, chunk) : SSL_read(ssl, data + offset, chunk);
        if (result > 0) {
            offset += (size_t) result;
            continue;
        }

        const int error = SSL_get_error(ssl, result);
        if (error == SSL_ERROR_SYSCALL && errno == EINTR) continue;
        if (error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE) {
            if (!errno) errno = error == SSL_ERROR_ZERO_RETURN ? ECONNRESET : EPROTO;
            return -1;
        }
        if (SDL_TICKS_PASSED(SDL_GetTicks(), deadline)) {
            errno = ETIMEDOUT;
            return -1;
        }
        if (atomic_load(&netplay.stop)) {
            errno = ECANCELED;
            return -1;
        }
        if (wait_socket(fd, error == SSL_ERROR_WANT_READ ? POLLIN : POLLOUT, 20) < 0) return -1;
    }
    return 0;
}

static int
send_message(netplay_peer *peer, const uint8_t type, const uint64_t frame, const void *payload, const uint32_t size) {
    uint8_t header[NETPLAY_HEADER_SIZE] = {0};
    memcpy(header, "PKNP", 4);
    header[4] = NETPLAY_PROTOCOL;
    header[5] = type;
    write_u32(header + 8, size);
    write_u32(header + 12, ++peer->tx_sequence);
    write_u64(header + 16, frame);
    write_u64(header + 24, netplay.session_id);

    if (size <= NETPLAY_CONTROL_CAP) {
        uint8_t packet[NETPLAY_HEADER_SIZE + NETPLAY_CONTROL_CAP];
        memcpy(packet, header, sizeof(header));
        if (size) memcpy(packet + sizeof(header), payload, size);
        return ssl_transfer(peer->ssl, peer->socket_fd, packet, sizeof(header) + size, 1);
    }

    if (ssl_transfer(peer->ssl, peer->socket_fd, header, sizeof(header), 1) != 0) return -1;
    return size ? ssl_transfer(peer->ssl, peer->socket_fd, (uint8_t *) payload, size, 1) : 0;
}

static int build_hello(const netplay_peer *peer, uint8_t *payload, const size_t size) {
    if (size < 166) return -1;
    memset(payload, 0, size);
    write_u16(payload, NETPLAY_PROTOCOL);
    payload[2] = (uint8_t) netplay.role;
    payload[3] = (uint8_t) (netplay.netpacket_available != 0);
    if (netplay.role == netplay_role_host) {
        payload[3] |= (uint8_t) (netplay.mode == netplay_mode_play_together ? 1U << 1 : 0);
        payload[3] |= (uint8_t) ((peer->owner_port & 3U) << 2);
        payload[3] |= (uint8_t) (((netplay.host_slots - 1U) & 3U) << 4);
    }
    memcpy(payload + 4, netplay.manifest.core_hash, SHA256_DIGEST_LENGTH);
    memcpy(payload + 36, netplay.manifest.content_hash, SHA256_DIGEST_LENGTH);
    memcpy(payload + 68, netplay.manifest.option_hash, SHA256_DIGEST_LENGTH);
    snprintf((char *) payload + 100, 64, "%s", netplay.manifest.core_name);
    snprintf((char *) payload + 164, size - 164, "%s", netplay.manifest.core_version);
    return 0;
}

static int check_hello(netplay_peer *peer, const uint8_t *payload, const size_t size) {
    if (size != 196 || read_u16(payload) != NETPLAY_PROTOCOL) return -1;
    if (payload[2] != (netplay.role == netplay_role_host ? netplay_role_client : netplay_role_host)
        || (payload[3] & 1U) != (uint8_t) (netplay.netpacket_available != 0))
        return -1;
    if (memcmp(payload + 4, netplay.manifest.core_hash, SHA256_DIGEST_LENGTH) != 0) return -2;
    if (memcmp(payload + 36, netplay.manifest.content_hash, SHA256_DIGEST_LENGTH) != 0) return -3;
    if (memcmp(payload + 68, netplay.manifest.option_hash, SHA256_DIGEST_LENGTH) != 0) return -4;
    if (strncmp((const char *) payload + 100, netplay.manifest.core_name, 64) != 0) return -2;
    if (strncmp((const char *) payload + 164, netplay.manifest.core_version, 32) != 0) return -2;
    if (netplay.role == netplay_role_client) {
        const unsigned owner_port = payload[3] >> 2 & 3U;
        const unsigned slots = (payload[3] >> 4 & 3U) + 1U;
        if (!owner_port || owner_port > slots || slots > NETPLAY_CLIENT_CAPACITY) return -1;
        peer->owner_port = owner_port;
        netplay.mode = payload[3] & (1U << 1) ? netplay_mode_play_together : netplay_mode_separate;
        netplay.public_info.mode = netplay.mode;
        netplay.public_info.local_port = owner_port;
        netplay.public_info.player_count = slots + 1U;
    }
    return 0;
}

static unsigned peer_count_with(int field);
static void refresh_pairing_info(void);

static int derive_pairing_code(netplay_peer *peer) {
    uint8_t local_digest[SHA256_DIGEST_LENGTH];
    uint8_t peer_digest[SHA256_DIGEST_LENGTH];
    unsigned local_size = 0;
    unsigned peer_size = 0;
    X509 *peer_certificate = SSL_get1_peer_certificate(peer->ssl);
    if (!peer_certificate || X509_digest(netplay.certificate, EVP_sha256(), local_digest, &local_size) != 1
        || X509_digest(peer_certificate, EVP_sha256(), peer_digest, &peer_size) != 1) {
        X509_free(peer_certificate);
        set_failure(lang.muxretro.netplay.pairing_material_failed);
        return -1;
    }
    X509_free(peer_certificate);

    uint8_t combined[SHA256_DIGEST_LENGTH * 2];
    if (memcmp(local_digest, peer_digest, SHA256_DIGEST_LENGTH) < 0) {
        memcpy(combined, local_digest, SHA256_DIGEST_LENGTH);
        memcpy(combined + SHA256_DIGEST_LENGTH, peer_digest, SHA256_DIGEST_LENGTH);
    } else {
        memcpy(combined, peer_digest, SHA256_DIGEST_LENGTH);
        memcpy(combined + SHA256_DIGEST_LENGTH, local_digest, SHA256_DIGEST_LENGTH);
    }

    uint8_t digest[SHA256_DIGEST_LENGTH];
    SHA256(combined, sizeof(combined), digest);
    const unsigned code = read_u32(digest) % 1000000U;

    pthread_mutex_lock(&netplay.mutex);
    snprintf(peer->pairing_code, sizeof(peer->pairing_code), "%06u", code);
    snprintf(netplay.public_info.pairing_code, sizeof(netplay.public_info.pairing_code), "%s", peer->pairing_code);
    netplay.public_info.pairing_local_confirmed = 0;
    netplay.public_info.pairing_peer_confirmed = 0;
    if (netplay.role == netplay_role_host)
        refresh_pairing_info();
    else {
        netplay.public_info.pairing_confirmed_count = 0;
        netplay.public_info.pairing_remaining_count = 1;
    }
    netplay.public_info.player_count = netplay.role == netplay_role_host ? netplay.host_slots + 1U : 2U;
    netplay.status = netplay_status_pairing;
    netplay.public_info.status = netplay_status_pairing;
    pthread_mutex_unlock(&netplay.mutex);
    atomic_store(&netplay_fast_status, netplay_status_pairing);
    return 0;
}

static void encode_input(uint8_t payload[12], const netplay_pad_state *input, const uint8_t owner) {
    payload[0] = owner;
    payload[1] = input->connected;
    write_u16(payload + 2, input->buttons);
    for (unsigned index = 0; index < 4; index++)
        write_u16(payload + 4 + index * 2, (uint16_t) input->axes[index]);
}

static unsigned peer_count_with(const int field) {
    unsigned count = 0;
    for (unsigned index = 0; index < netplay.peer_count; index++) {
        const netplay_peer *peer = &netplay.peers[index];
        if ((field == 0 && peer->peer_confirmed) || (field == 1 && peer->compatible)
            || (field == 2 && peer->ready_received))
            count++;
    }
    return count;
}

static void refresh_pairing_info(void) {
    const unsigned confirmed = peer_count_with(0);
    netplay.public_info.pairing_confirmed_count = confirmed;
    netplay.public_info.pairing_remaining_count = netplay.host_slots > confirmed ? netplay.host_slots - confirmed : 0;
    for (unsigned index = 0; index < netplay.peer_count; index++) {
        if (netplay.peers[index].peer_confirmed) continue;
        snprintf(
            netplay.public_info.pairing_code, sizeof(netplay.public_info.pairing_code), "%s",
            netplay.peers[index].pairing_code
        );
        return;
    }
    netplay.public_info.pairing_code[0] = '\0';
}

static void update_host_menu_pause_locked(void) {
    int requested = netplay.local_menu_open;
    for (unsigned index = 0; index < netplay.peer_count && !requested; index++)
        requested = !netplay.peers[index].menu_state_received || netplay.peers[index].menu_open;
    if (requested == netplay.menu_pause_requested) return;
    netplay.menu_pause_requested = requested;
    netplay.menu_pause_value = requested;
    netplay.menu_pause_frame = requested ? netplay.frame + NETPLAY_MENU_PAUSE_LEAD_FRAMES : netplay.frame;
    netplay.menu_pause_generation++;
}

static int receive_message(netplay_peer *peer) {
    uint8_t header[NETPLAY_HEADER_SIZE];
    if (ssl_transfer(peer->ssl, peer->socket_fd, header, sizeof(header), 0) != 0) return -1;
    if (memcmp(header, "PKNP", 4) != 0 || header[4] != NETPLAY_PROTOCOL) return -1;

    const uint8_t type = header[5];
    const uint32_t size = read_u32(header + 8);
    const uint32_t sequence = read_u32(header + 12);
    const uint64_t frame = read_u64(header + 16);
    const uint64_t session = read_u64(header + 24);
    const uint32_t cap = type == netplay_message_state       ? NETPLAY_STATE_CAP
                         : type == netplay_message_netpacket ? NETPLAY_PACKET_CAP
                                                             : NETPLAY_CONTROL_CAP;
    if (size > cap || sequence <= peer->rx_sequence || (peer->hello_received && session != netplay.session_id))
        return -1;
    peer->rx_sequence = sequence;

    const int payload_kept = type == netplay_message_state || type == netplay_message_netpacket;

    uint8_t *owned = size && payload_kept ? malloc(size) : NULL;
    uint8_t *payload = size ? (payload_kept ? owned : peer->rx_control) : NULL;
    if (size && (!payload || ssl_transfer(peer->ssl, peer->socket_fd, payload, size, 0) != 0)) {
        free(owned);
        return -1;
    }

    int result = 0;
    if (type == netplay_message_pair && size == 0) {
        int confirm_host = 0;
        pthread_mutex_lock(&netplay.mutex);
        peer->peer_confirmed = 1;
        netplay.public_info.pairing_peer_confirmed = 1;
        if (netplay.role == netplay_role_host) {
            peer->local_confirmed = 1;
            netplay.public_info.pairing_local_confirmed = 1;
            refresh_pairing_info();
            confirm_host = 1;
        }
        pthread_mutex_unlock(&netplay.mutex);
        if (confirm_host && !peer->pair_sent) {
            if (send_message(peer, netplay_message_pair, 0, NULL, 0) != 0)
                result = -1;
            else
                peer->pair_sent = 1;
        }
    } else if (type == netplay_message_hello) {
        pthread_mutex_lock(&netplay.mutex);
        const int compatible = check_hello(peer, payload, size);
        pthread_mutex_unlock(&netplay.mutex);
        if (compatible != 0) {
            set_failure(
                compatible == -2   ? lang.muxretro.netplay.core_version_mismatch
                : compatible == -3 ? lang.muxretro.netplay.content_mismatch
                                   : lang.muxretro.netplay.options_mismatch
            );
            result = -1;
        } else {
            pthread_mutex_lock(&netplay.mutex);
            if (netplay.role == netplay_role_client) netplay.session_id = session;
            peer->hello_received = 1;
            peer->compatible = 1;
            pthread_mutex_unlock(&netplay.mutex);
        }
    } else if (type == netplay_message_state && netplay.role == netplay_role_client && size > 0) {
        pthread_mutex_lock(&netplay.mutex);
        free(peer->rx_state);
        peer->rx_state = owned;
        peer->rx_state_size = size;
        peer->rx_state_frame = frame;
        peer->rx_state_pending = 1;
        netplay.status = netplay_status_synchronising;
        netplay.public_info.status = netplay_status_synchronising;
        peer->ready_received = 0;
        owned = NULL;
        pthread_mutex_unlock(&netplay.mutex);
        atomic_store(&netplay_fast_status, netplay_status_synchronising);
    } else if (type == netplay_message_ready && size == 0) {
        pthread_mutex_lock(&netplay.mutex);
        peer->ready_received = 1;
        pthread_mutex_unlock(&netplay.mutex);
        if (netplay.role == netplay_role_client) set_status(netplay_status_playing);
    } else if (type == netplay_message_input && size == 12) {
        const unsigned owner = payload[0];
        const unsigned local_port = netplay.public_info.local_port;
        const unsigned player_count = netplay.public_info.player_count;
        if ((netplay.role == netplay_role_host && owner != peer->owner_port)
            || (netplay.role == netplay_role_client && (owner >= player_count || owner == local_port))) {
            result = -1;
        } else {
            netplay_pad_state input = {0};
            input.connected = payload[1] != 0;
            input.buttons = read_u16(payload + 2);
            for (unsigned index = 0; index < 4; index++)
                input.axes[index] = (int16_t) read_u16(payload + 4 + index * 2);
            pthread_mutex_lock(&netplay.mutex);
            const int accepts_input = netplay.status == netplay_status_playing
                                      || (netplay.role == netplay_role_host
                                          && netplay.status == netplay_status_synchronising && peer->ready_sent);
            const uint64_t distance = frame > netplay.frame ? frame - netplay.frame : netplay.frame - frame;
            if (accepts_input) {
                if (distance > NETPLAY_INPUT_HISTORY_CAPACITY) {
                    result = -1;
                    errno = EPROTO;
                } else {
                    netplay.remote_history[owner][frame % NETPLAY_INPUT_HISTORY_CAPACITY] =
                        (netplay_input_history) {frame, input, 1};
                    netplay.remote_input_generation[owner]++;
                    netplay.remote_input_frame[owner] = frame;
                    netplay.public_info.frame = frame;
                }
            }
            pthread_mutex_unlock(&netplay.mutex);
        }
    } else if (type == netplay_message_ping && size == 4) {
        result = send_message(peer, netplay_message_pong, frame, payload, size);
    } else if (type == netplay_message_pong && size == 4 && read_u32(payload) == peer->ping_nonce) {
        const unsigned elapsed = SDL_GetTicks() - peer->ping_sent_at;
        pthread_mutex_lock(&netplay.mutex);
        const unsigned previous = netplay.public_info.ping_ms;
        netplay.public_info.ping_ms = elapsed;
        netplay.public_info.jitter_ms = previous > elapsed ? previous - elapsed : elapsed - previous;
        if (netplay.role == netplay_role_host && !netplay.netpacket_available) {
            unsigned delay = (elapsed / 2 + netplay.public_info.jitter_ms + 16) / 17 + 1;
            if (delay < 2) delay = 2;
            if (delay > 8) delay = 8;
            if (delay > netplay.public_info.input_delay
                && (!netplay.delay_change_pending || delay > netplay.delay_change_value)) {
                netplay.delay_send_value = (uint8_t) delay;
                netplay.delay_send_frame = netplay.frame + 30;
                netplay.delay_send_generation++;
                netplay.delay_change_value = (uint8_t) delay;
                netplay.delay_change_frame = netplay.delay_send_frame;
                netplay.delay_change_pending = 1;
            }
        }
        pthread_mutex_unlock(&netplay.mutex);
    } else if (type == netplay_message_menu_state && netplay.role == netplay_role_host && size == 1
               && payload[0] <= 1) {
        pthread_mutex_lock(&netplay.mutex);
        if (netplay.status == netplay_status_playing) {
            peer->menu_open = payload[0];
            peer->menu_state_received = 1;
            update_host_menu_pause_locked();
        }
        pthread_mutex_unlock(&netplay.mutex);
    } else if (type == netplay_message_menu_pause && netplay.role == netplay_role_client && size == 1
               && payload[0] <= 1) {
        pthread_mutex_lock(&netplay.mutex);
        if (netplay.status == netplay_status_playing) {
            netplay.menu_pause_requested = payload[0];
            netplay.menu_pause_value = payload[0];
            netplay.menu_pause_frame = frame;
        }
        pthread_mutex_unlock(&netplay.mutex);
    } else if (type == netplay_message_digest && size == SHA256_DIGEST_LENGTH) {
        pthread_mutex_lock(&netplay.mutex);
        if (netplay.status == netplay_status_playing) {
            const unsigned owner = netplay.role == netplay_role_host ? peer->owner_port : 0U;
            netplay.remote_digest_frame[owner] = frame;
            memcpy(netplay.remote_digest[owner], payload, SHA256_DIGEST_LENGTH);
            netplay.remote_digest_pending[owner] = 1;
        }
        pthread_mutex_unlock(&netplay.mutex);
    } else if (type == netplay_message_netpacket && netplay.netpacket_available && size > 0) {
        pthread_mutex_lock(&netplay.mutex);
        if (netplay.incoming_count >= NETPLAY_PACKET_QUEUE_CAP) {
            result = -1;
        } else {
            const unsigned tail = (netplay.incoming_head + netplay.incoming_count) % NETPLAY_PACKET_QUEUE_CAP;
            netplay.incoming_packets[tail] = (netplay_packet) {owned, size};
            netplay.incoming_count++;
            owned = NULL;
        }
        pthread_mutex_unlock(&netplay.mutex);
    } else if (type == netplay_message_delay && netplay.role == netplay_role_client && size == 1 && payload[0] >= 2
               && payload[0] <= 8) {
        pthread_mutex_lock(&netplay.mutex);
        netplay.delay_change_value = payload[0];
        netplay.delay_change_frame = frame;
        netplay.delay_change_pending = 1;
        pthread_mutex_unlock(&netplay.mutex);
    } else if (type == netplay_message_disconnect) {
        result = -1;
        errno = ECONNRESET;
    } else {
        result = -1;
        errno = EPROTO;
    }

    if (result != 0 && !errno) errno = EPROTO;

    free(owned);
    return result;
}

static int peer_send_pending(netplay_peer *peer) {
    if (peer->pair_sent && !peer->hello_sent && peer->peer_confirmed) {
        uint8_t hello[196];
        if (build_hello(peer, hello, sizeof(hello)) != 0
            || send_message(peer, netplay_message_hello, 0, hello, sizeof(hello)) != 0)
            return -1;
        peer->hello_sent = 1;
        pthread_mutex_lock(&netplay.mutex);
        const int ready_to_check =
            netplay.role == netplay_role_client
            || (netplay.peer_count >= netplay.host_slots && peer_count_with(0) >= netplay.host_slots);
        pthread_mutex_unlock(&netplay.mutex);
        if (ready_to_check) set_status(netplay_status_checking);
    }

    if (peer->hello_sent && peer->hello_received && peer->compatible
        && atomic_load(&netplay_fast_status) == netplay_status_checking) {
        pthread_mutex_lock(&netplay.mutex);
        const int compatible = netplay.role == netplay_role_client || peer_count_with(1) >= netplay.host_slots;
        pthread_mutex_unlock(&netplay.mutex);
        if (compatible) set_status(netplay_status_synchronising);
    }

    pthread_mutex_lock(&netplay.mutex);
    uint8_t *state = NULL;
    size_t state_size = 0;
    uint64_t state_frame = 0;
    if (peer->tx_state_pending) {
        state = netplay.sync_state;
        state_size = netplay.sync_state_size;
        state_frame = netplay.sync_state_frame;
        peer->tx_state_pending = 0;
    }
    const unsigned local_port = netplay.public_info.local_port;
    const unsigned player_count = netplay.public_info.player_count;
    const netplay_status status = netplay.status;
    struct {
        uint64_t frame;
        unsigned port;
        netplay_pad_state input;
    } pending_input[NETPLAY_INPUT_SEND_BUDGET];
    unsigned pending_input_count = 0;
    int input_history_exhausted = 0;
    if (status == netplay_status_playing && !netplay.netpacket_available) {
        for (unsigned port = 0; port < player_count && port < NETPLAY_PORT_COUNT
                                && pending_input_count < NETPLAY_INPUT_SEND_BUDGET && !input_history_exhausted;
             port++) {
            if ((netplay.role == netplay_role_client && port != local_port)
                || (netplay.role == netplay_role_host && port == peer->owner_port))
                continue;
            const uint64_t generation =
                port == local_port ? netplay.local_input_generation : netplay.remote_input_generation[port];
            const uint64_t newest = port == local_port ? netplay.local_input_frame : netplay.remote_input_frame[port];
            if (!generation) continue;
            uint64_t first = newest;
            if (peer->sent_input_valid[port]) {
                first = peer->sent_input_frame[port] + 1U;
                if (first <= newest && newest - first >= NETPLAY_INPUT_HISTORY_CAPACITY) {
                    input_history_exhausted = 1;
                    break;
                }
            } else {
                const uint64_t oldest =
                    newest >= NETPLAY_INPUT_HISTORY_CAPACITY - 1U ? newest - (NETPLAY_INPUT_HISTORY_CAPACITY - 1U) : 0U;
                for (uint64_t candidate = oldest; candidate <= newest; candidate++) {
                    const netplay_input_history *history =
                        port == local_port ? &netplay.local_history[candidate % NETPLAY_INPUT_HISTORY_CAPACITY]
                                           : &netplay.remote_history[port][candidate % NETPLAY_INPUT_HISTORY_CAPACITY];
                    if (history->valid && history->frame == candidate) {
                        first = candidate;
                        break;
                    }
                }
            }
            if (first > newest) continue;
            for (uint64_t frame = first; frame <= newest && pending_input_count < NETPLAY_INPUT_SEND_BUDGET; frame++) {
                const netplay_input_history *history =
                    port == local_port ? &netplay.local_history[frame % NETPLAY_INPUT_HISTORY_CAPACITY]
                                       : &netplay.remote_history[port][frame % NETPLAY_INPUT_HISTORY_CAPACITY];
                if (!history->valid || history->frame != frame) break;
                pending_input[pending_input_count].frame = frame;
                pending_input[pending_input_count].port = port;
                pending_input[pending_input_count].input = history->input;
                pending_input_count++;
            }
        }
    }
    const uint64_t current_frame = netplay.frame;
    const int ready_sent = peer->ready_sent;
    const int ready_received = peer->ready_received;
    if (ready_sent && !ready_received && netplay.role == netplay_role_client) peer->ready_sent = 0;
    const uint64_t digest_generation = netplay.digest_send_generation;
    const int digest_pending = digest_generation != peer->sent_digest_generation;
    const uint64_t digest_frame = netplay.local_digest_frame;
    uint8_t digest[SHA256_DIGEST_LENGTH];
    memcpy(digest, netplay.local_digest, sizeof(digest));
    const uint64_t delay_generation = netplay.delay_send_generation;
    const int delay_pending = delay_generation != peer->sent_delay_generation;
    const uint8_t delay_value = netplay.delay_send_value;
    const uint64_t delay_frame = netplay.delay_send_frame;
    const uint64_t menu_state_generation = netplay.menu_state_generation;
    const int menu_state_pending =
        netplay.role == netplay_role_client
        && (!peer->sent_menu_state_valid || menu_state_generation != peer->sent_menu_state_generation);
    const uint8_t menu_state = (uint8_t) (netplay.local_menu_open != 0);
    const uint64_t menu_pause_generation = netplay.menu_pause_generation;
    const int menu_pause_pending =
        netplay.role == netplay_role_host && menu_pause_generation != peer->sent_menu_pause_generation;
    const uint8_t menu_pause = (uint8_t) (netplay.menu_pause_value != 0);
    const uint64_t menu_pause_frame = netplay.menu_pause_frame;
    netplay_packet outgoing = {0};
    if (netplay.outgoing_count) {
        outgoing = netplay.outgoing_packets[netplay.outgoing_head];
        memset(&netplay.outgoing_packets[netplay.outgoing_head], 0, sizeof(outgoing));
        netplay.outgoing_head = (netplay.outgoing_head + 1) % NETPLAY_PACKET_QUEUE_CAP;
        netplay.outgoing_count--;
    }
    pthread_mutex_unlock(&netplay.mutex);

    if (input_history_exhausted) {
        free(outgoing.data);
        errno = ENOBUFS;
        return -1;
    }

    if (state) {
        const int sent = send_message(peer, netplay_message_state, state_frame, state, (uint32_t) state_size);
        pthread_mutex_lock(&netplay.mutex);
        if (netplay.sync_state_refs) netplay.sync_state_refs--;
        if (!netplay.sync_state_refs) {
            free(netplay.sync_state);
            netplay.sync_state = NULL;
            netplay.sync_state_size = 0;
        }
        pthread_mutex_unlock(&netplay.mutex);
        if (sent != 0) {
            free(outgoing.data);
            return -1;
        }
    }

    if (outgoing.data) {
        const int sent =
            send_message(peer, netplay_message_netpacket, current_frame, outgoing.data, (uint32_t) outgoing.size);
        free(outgoing.data);
        if (sent != 0) return -1;
    }

    if (ready_sent && !ready_received && netplay.role == netplay_role_client) {
        if (send_message(peer, netplay_message_ready, 0, NULL, 0) != 0) return -1;
    }

    if (ready_received && netplay.role == netplay_role_host && status == netplay_status_synchronising) {
        pthread_mutex_lock(&netplay.mutex);
        const int all_ready = peer_count_with(2) >= netplay.host_slots;
        pthread_mutex_unlock(&netplay.mutex);
        if (all_ready && !ready_sent) {
            if (send_message(peer, netplay_message_ready, 0, NULL, 0) != 0) return -1;
            pthread_mutex_lock(&netplay.mutex);
            peer->ready_sent = 1;
            pthread_mutex_unlock(&netplay.mutex);
        }
        if (all_ready) {
            pthread_mutex_lock(&netplay.mutex);
            unsigned sent = 0;
            for (unsigned index = 0; index < netplay.peer_count; index++)
                sent += netplay.peers[index].ready_sent != 0;
            pthread_mutex_unlock(&netplay.mutex);
            if (sent >= netplay.host_slots) set_status(netplay_status_playing);
        }
    }

    for (unsigned index = 0; index < pending_input_count; index++) {
        uint8_t payload[12];
        encode_input(payload, &pending_input[index].input, (uint8_t) pending_input[index].port);
        if (send_message(peer, netplay_message_input, pending_input[index].frame, payload, sizeof(payload)) != 0)
            return -1;
        pthread_mutex_lock(&netplay.mutex);
        if (netplay.status == netplay_status_playing) {
            peer->sent_input_frame[pending_input[index].port] = pending_input[index].frame;
            peer->sent_input_valid[pending_input[index].port] = 1;
        }
        pthread_mutex_unlock(&netplay.mutex);
    }

    if (status == netplay_status_playing && !netplay.netpacket_available && digest_pending
        && send_message(peer, netplay_message_digest, digest_frame, digest, sizeof(digest)) != 0)
        return -1;
    if (status == netplay_status_playing && !netplay.netpacket_available && digest_pending)
        peer->sent_digest_generation = digest_generation;

    if (status == netplay_status_playing && delay_pending
        && send_message(peer, netplay_message_delay, delay_frame, &delay_value, sizeof(delay_value)) != 0)
        return -1;
    if (status == netplay_status_playing && delay_pending) peer->sent_delay_generation = delay_generation;

    if (status == netplay_status_playing && menu_state_pending
        && send_message(peer, netplay_message_menu_state, current_frame, &menu_state, sizeof(menu_state)) != 0)
        return -1;
    if (status == netplay_status_playing && menu_state_pending) {
        peer->sent_menu_state_generation = menu_state_generation;
        peer->sent_menu_state_valid = 1;
    }

    if (status == netplay_status_playing && menu_pause_pending
        && send_message(peer, netplay_message_menu_pause, menu_pause_frame, &menu_pause, sizeof(menu_pause)) != 0)
        return -1;
    if (status == netplay_status_playing && menu_pause_pending)
        peer->sent_menu_pause_generation = menu_pause_generation;

    const uint32_t now = SDL_GetTicks();
    if (status == netplay_status_playing && SDL_TICKS_PASSED(now, peer->next_ping_at)) {
        if (RAND_bytes((uint8_t *) &peer->ping_nonce, sizeof(peer->ping_nonce)) != 1) return -1;
        peer->ping_sent_at = now;
        peer->next_ping_at = now + 1000;
        uint8_t payload[4];
        write_u32(payload, peer->ping_nonce);
        if (send_message(peer, netplay_message_ping, current_frame, payload, sizeof(payload)) != 0) return -1;
    }

    return 0;
}

static void *peer_thread(void *userdata) {
    netplay_peer *peer = userdata;
    if (derive_pairing_code(peer) != 0) return NULL;
    int transport_error = 0;

    while (!atomic_load(&netplay.stop)) {
        pthread_mutex_lock(&netplay.mutex);
        const int confirmed = peer->local_confirmed;
        pthread_mutex_unlock(&netplay.mutex);

        if (confirmed && !peer->pair_sent) {
            errno = 0;
            if (send_message(peer, netplay_message_pair, 0, NULL, 0) != 0) {
                transport_error = errno ? errno : EPROTO;
                break;
            }
            peer->pair_sent = 1;
        }

        int receive_failed = 0;
        for (unsigned index = 0; index < NETPLAY_RECEIVE_DRAIN_BUDGET; index++) {
            errno = 0;
            const int readable = peer_read_ready(peer, 0);
            if (readable == 0) break;
            if (readable < 0 && errno == EINTR) break;
            if (readable < 0 || receive_message(peer) != 0) {
                transport_error = errno ? errno : EPROTO;
                receive_failed = 1;
                break;
            }
        }
        if (receive_failed) break;

        errno = 0;
        if (peer_send_pending(peer) != 0) {
            transport_error = errno ? errno : EPROTO;
            break;
        }

        errno = 0;
        const int readable = peer_read_ready(peer, 4);
        if (readable > 0 && receive_message(peer) != 0) {
            transport_error = errno ? errno : EPROTO;
            break;
        }
        if (readable < 0 && errno != EINTR) {
            transport_error = errno ? errno : ECONNRESET;
            break;
        }
    }

    if (!atomic_load(&netplay.stop) && atomic_load(&netplay_fast_status) != netplay_status_failed) {
        if (transport_error == ETIMEDOUT)
            set_failure(lang.muxretro.netplay.peer_timed_out);
        else if (transport_error == ENOBUFS)
            set_failure(lang.muxretro.netplay.input_backlog);
        else if (transport_error == EPROTO)
            set_failure(lang.muxretro.netplay.protocol_error);
        else
            set_failure(lang.muxretro.netplay.peer_disconnected);
    }
    return NULL;
}

static void wake_peers(void) {
    for (unsigned index = 0; index < NETPLAY_CLIENT_CAPACITY; index++) {
        const int wake_fd = netplay.peers[index].wake_fd;
        if (wake_fd < 0) continue;

        const uint64_t one = 1;
        ssize_t written = write(wake_fd, &one, sizeof(one));
        if (written < 0 && errno == EINTR) written = write(wake_fd, &one, sizeof(one));
        (void) written;
    }
}

static int peer_start(const int socket_fd, const int server, const unsigned index) {
    if (index >= NETPLAY_CLIENT_CAPACITY) return -1;
    netplay_peer *peer = &netplay.peers[index];
    pthread_mutex_lock(&netplay.mutex);
    memset(peer, 0, sizeof(*peer));
    peer->socket_fd = socket_fd;
    peer->wake_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    peer->owner_port = server ? index + 1U : 1U;
    if (netplay.peer_count <= index) netplay.peer_count = index + 1U;
    pthread_mutex_unlock(&netplay.mutex);
    peer->ssl = SSL_new(netplay.tls);
    if (!peer->ssl) return -1;
    SSL_set_fd(peer->ssl, socket_fd);
    if (!server) SSL_set_tlsext_host_name(peer->ssl, "Pickles Network Play");
    if (tls_handshake(peer->ssl, socket_fd, server) != 0) return -1;
    peer->running = pthread_create(&peer->thread, NULL, peer_thread, peer) == 0;
    return peer->running ? 0 : -1;
}

static void send_discovery(const int socket_fd) {
    struct sockaddr_in destination = {0};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(NETPLAY_DISCOVERY_PORT);
    destination.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    uint8_t message[64] = {0};
    memcpy(message, "PKND", 4);
    message[4] = NETPLAY_PROTOCOL;
    write_u16(message + 6, netplay.port);
    pthread_mutex_lock(&netplay.mutex);
    snprintf((char *) message + 8, sizeof(message) - 8, "%s", netplay.host_name);
    pthread_mutex_unlock(&netplay.mutex);
    sendto(socket_fd, message, sizeof(message), MSG_DONTWAIT, (struct sockaddr *) &destination, sizeof(destination));
}

static void *discovery_thread(void *unused) {
    (void) unused;
    const int socket_fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (socket_fd < 0) {
        set_failure(lang.muxretro.netplay.discovery_start_failed);
        return NULL;
    }

    int one = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in address = {
        .sin_family = AF_INET, .sin_port = htons(NETPLAY_DISCOVERY_PORT), .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    if (bind(socket_fd, (struct sockaddr *) &address, sizeof(address)) != 0) {
        close(socket_fd);
        set_failure(lang.muxretro.netplay.discovery_listen_failed);
        return NULL;
    }

    const uint32_t deadline = SDL_GetTicks() + 5000;
    while (!atomic_load(&netplay.discovery_stop) && !SDL_TICKS_PASSED(SDL_GetTicks(), deadline)) {
        if (wait_socket(socket_fd, POLLIN, 100) <= 0) continue;
        uint8_t message[64];
        struct sockaddr_in source;
        socklen_t source_size = sizeof(source);
        const ssize_t count =
            recvfrom(socket_fd, message, sizeof(message), 0, (struct sockaddr *) &source, &source_size);
        if (count != (ssize_t) sizeof(message) || memcmp(message, "PKND", 4) != 0 || message[4] != NETPLAY_PROTOCOL)
            continue;

        char source_address[INET_ADDRSTRLEN];
        if (!inet_ntop(AF_INET, &source.sin_addr, source_address, sizeof(source_address))) continue;
        const uint16_t port = read_u16(message + 6);
        if (!port) continue;

        pthread_mutex_lock(&netplay.mutex);
        unsigned index = 0;
        while (index < netplay.discovered_count
               && (strcmp(netplay.discovered[index].address, source_address) != 0
                   || netplay.discovered[index].port != port))
            index++;
        if (index == netplay.discovered_count && netplay.discovered_count < 8) netplay.discovered_count++;
        if (index < 8) {
            snprintf(
                netplay.discovered[index].label, sizeof(netplay.discovered[index].label), "%.*s", 56,
                (char *) message + 8
            );
            snprintf(
                netplay.discovered[index].address, sizeof(netplay.discovered[index].address), "%s", source_address
            );
            netplay.discovered[index].port = port;
        }
        pthread_mutex_unlock(&netplay.mutex);
    }

    close(socket_fd);
    pthread_mutex_lock(&netplay.mutex);
    if (netplay.status == netplay_status_discovering) {
        netplay.status = netplay_status_idle;
        netplay.public_info.status = netplay_status_idle;
    }
    pthread_mutex_unlock(&netplay.mutex);
    if (atomic_load(&netplay_fast_status) == netplay_status_discovering)
        atomic_store(&netplay_fast_status, netplay_status_idle);
    return NULL;
}

static void *accept_thread(void *unused) {
    (void) unused;
    if (prepare_manifest() != 0) return NULL;

    const int discovery_fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    int broadcast = 1;
    if (discovery_fd >= 0) setsockopt(discovery_fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast));
    uint32_t next_announcement = 0;

    while (!atomic_load(&netplay.stop)) {
        const uint32_t now = SDL_GetTicks();
        if (discovery_fd >= 0 && SDL_TICKS_PASSED(now, next_announcement)) {
            send_discovery(discovery_fd);
            next_announcement = now + 1000;
        }

        if (wait_socket(netplay.listen_fd, POLLIN, 100) <= 0) continue;
        struct sockaddr_storage address;
        socklen_t address_size = sizeof(address);
        const int socket_fd = accept4(netplay.listen_fd, (struct sockaddr *) &address, &address_size, SOCK_CLOEXEC);
        if (socket_fd < 0) continue;

        char host[NI_MAXHOST];
        if (getnameinfo((struct sockaddr *) &address, address_size, host, sizeof(host), NULL, 0, NI_NUMERICHOST) == 0) {
            pthread_mutex_lock(&netplay.mutex);
            snprintf(netplay.public_info.peer, sizeof(netplay.public_info.peer), "%s", host);
            pthread_mutex_unlock(&netplay.mutex);
        }

        int one = 1;
        setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        set_nonblocking(socket_fd);
        pthread_mutex_lock(&netplay.mutex);
        const unsigned index = netplay.peer_count;
        pthread_mutex_unlock(&netplay.mutex);
        if (index >= netplay.host_slots || peer_start(socket_fd, 1, index) != 0) {
            close(socket_fd);
            if (index < NETPLAY_CLIENT_CAPACITY) {
                netplay.peers[index].socket_fd = -1;
                netplay.peers[index].wake_fd = -1;
            }
            set_failure(lang.muxretro.netplay.secure_connection_failed);
            break;
        }
        if (index + 1U >= netplay.host_slots) break;
    }

    if (discovery_fd >= 0) close(discovery_fd);
    return NULL;
}

static int connect_address(const char *address, const uint16_t port) {
    char port_text[8];
    snprintf(port_text, sizeof(port_text), "%u", port);
    struct addrinfo hints = {.ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM, .ai_protocol = IPPROTO_TCP};
    struct addrinfo *addresses = NULL;
    if (getaddrinfo(address, port_text, &hints, &addresses) != 0) return -1;

    int connected_fd = -1;
    for (struct addrinfo *entry = addresses; entry; entry = entry->ai_next) {
        const int fd = socket(entry->ai_family, entry->ai_socktype | SOCK_CLOEXEC, entry->ai_protocol);
        if (fd < 0) continue;
        if (set_nonblocking(fd) != 0) {
            close(fd);
            continue;
        }
        int connected = connect(fd, entry->ai_addr, entry->ai_addrlen) == 0;
        if (!connected && errno == EINPROGRESS && wait_socket(fd, POLLOUT, NETPLAY_IO_TIMEOUT_MS) > 0) {
            int socket_error = 0;
            socklen_t error_size = sizeof(socket_error);
            connected = getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_size) == 0 && socket_error == 0;
        }
        if (connected && !atomic_load(&netplay.stop)) {
            connected_fd = fd;
            break;
        }
        close(fd);
    }
    freeaddrinfo(addresses);
    return connected_fd;
}

static void *connect_thread(void *unused) {
    (void) unused;
    if (prepare_manifest() != 0) return NULL;
    const int socket_fd = connect_address(netplay.join_address, netplay.port);
    if (socket_fd < 0) {
        set_failure(lang.muxretro.netplay.host_unreachable);
        return NULL;
    }

    int one = 1;
    setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    set_nonblocking(socket_fd);
    if (peer_start(socket_fd, 0, 0) != 0) {
        close(socket_fd);
        netplay.peers[0].socket_fd = -1;
        netplay.peers[0].wake_fd = -1;
        set_failure(lang.muxretro.netplay.secure_connection_failed);
    }
    return NULL;
}

static int queue_host_state(void) {
    if (!current_core.retro_serialize_size || !current_core.retro_serialize) return -1;

    hw_render_bridge_enter_core_call();
    const size_t size = current_core.retro_serialize_size();
    if (!size || size > NETPLAY_STATE_CAP) {
        hw_render_bridge_exit_core_call();
        return -1;
    }
    uint8_t *state = malloc(size);
    const int okay = state && current_core.retro_serialize(state, size);
    hw_render_bridge_exit_core_call();
    if (!okay) {
        free(state);
        return -1;
    }

    pthread_mutex_lock(&netplay.mutex);
    if (netplay.sync_state) {
        pthread_mutex_unlock(&netplay.mutex);
        free(state);
        return -1;
    }
    netplay.sync_state = state;
    netplay.sync_state_size = size;
    netplay.sync_state_frame = netplay.frame;
    netplay.sync_state_refs = netplay.peer_count;
    memset(netplay.local_history, 0, sizeof(netplay.local_history));
    memset(netplay.remote_history, 0, sizeof(netplay.remote_history));
    netplay.local_input_generation = 0;
    netplay.local_input_frame = netplay.frame;
    memset(netplay.remote_input_generation, 0, sizeof(netplay.remote_input_generation));
    memset(netplay.remote_input_frame, 0, sizeof(netplay.remote_input_frame));
    memset(netplay.remote_digest_pending, 0, sizeof(netplay.remote_digest_pending));
    netplay.digest_due = 0;
    netplay.local_digest_frame = 0;
    for (unsigned index = 0; index < netplay.peer_count; index++) {
        memset(netplay.peers[index].sent_input_frame, 0, sizeof(netplay.peers[index].sent_input_frame));
        memset(netplay.peers[index].sent_input_valid, 0, sizeof(netplay.peers[index].sent_input_valid));
        netplay.peers[index].tx_state_pending = 1;
    }
    pthread_mutex_unlock(&netplay.mutex);
    return 0;
}

static void *digest_worker(void *argument) {
    (void) argument;

    for (;;) {
        pthread_mutex_lock(&netplay.digest_mutex);
        while (!netplay.digest_job_pending && !atomic_load(&netplay.digest_stop))
            pthread_cond_wait(&netplay.digest_wake, &netplay.digest_mutex);

        if (atomic_load(&netplay.digest_stop)) {
            pthread_mutex_unlock(&netplay.digest_mutex);
            break;
        }

        netplay.digest_job_pending = 0;
        netplay.digest_job_busy = 1;
        const size_t size = netplay.digest_hash_size;
        const uint64_t frame = netplay.digest_job_frame;
        uint8_t *state = netplay.digest_hash_state;
        pthread_mutex_unlock(&netplay.digest_mutex);

        uint8_t digest[SHA256_DIGEST_LENGTH];
        SHA256(state, size, digest);

        pthread_mutex_lock(&netplay.digest_mutex);
        memcpy(netplay.digest_ready_value, digest, sizeof(digest));
        netplay.digest_ready_frame = frame;
        netplay.digest_ready = 1;
        netplay.digest_job_busy = 0;
        pthread_mutex_unlock(&netplay.digest_mutex);
    }

    return NULL;
}

static int digest_worker_start(void) {
    if (netplay.digest_thread_running) return 0;

    pthread_mutex_init(&netplay.digest_mutex, NULL);
    pthread_cond_init(&netplay.digest_wake, NULL);
    atomic_store(&netplay.digest_stop, 0);
    netplay.digest_job_pending = 0;
    netplay.digest_job_busy = 0;
    netplay.digest_ready = 0;

    if (pthread_create(&netplay.digest_thread, NULL, digest_worker, NULL) != 0) {
        pthread_mutex_destroy(&netplay.digest_mutex);
        pthread_cond_destroy(&netplay.digest_wake);
        return -1;
    }

    netplay.digest_thread_running = 1;
    return 0;
}

static void digest_worker_stop(void) {
    if (!netplay.digest_thread_running) return;

    atomic_store(&netplay.digest_stop, 1);
    pthread_mutex_lock(&netplay.digest_mutex);
    pthread_cond_signal(&netplay.digest_wake);
    pthread_mutex_unlock(&netplay.digest_mutex);
    pthread_join(netplay.digest_thread, NULL);
    pthread_mutex_destroy(&netplay.digest_mutex);
    pthread_cond_destroy(&netplay.digest_wake);
    netplay.digest_thread_running = 0;
}

static int queue_state_digest(const uint64_t frame) {
    if (!current_core.retro_serialize_size || !current_core.retro_serialize) return -1;
    if (digest_worker_start() != 0) return -1;

    pthread_mutex_lock(&netplay.digest_mutex);
    const int busy = netplay.digest_job_pending || netplay.digest_job_busy;
    pthread_mutex_unlock(&netplay.digest_mutex);
    if (busy) return 0;

    const uint64_t digest_start = perf_begin();

    hw_render_bridge_enter_core_call();
    const size_t size = current_core.retro_serialize_size();
    if (!size || size > NETPLAY_STATE_CAP) {
        hw_render_bridge_exit_core_call();
        return -1;
    }
    if (size > netplay.digest_state_capacity) {
        uint8_t *grown = realloc(netplay.digest_state, size);
        if (!grown) {
            hw_render_bridge_exit_core_call();
            return -1;
        }
        netplay.digest_state = grown;
        netplay.digest_state_capacity = size;
    }
    const uint64_t serialise_start = SDL_GetPerformanceCounter();
    const int okay = current_core.retro_serialize(netplay.digest_state, size);
    const double serialise_ms =
        (double) (SDL_GetPerformanceCounter() - serialise_start) * 1000.0 / (double) SDL_GetPerformanceFrequency();
    hw_render_bridge_exit_core_call();
    if (!okay) return -1;

    pthread_mutex_lock(&netplay.mutex);
    netplay.digest_serialise_ms =
        netplay.digest_serialise_ms <= 0.0 ? serialise_ms : netplay.digest_serialise_ms * 0.5 + serialise_ms * 0.5;

    unsigned interval = NETPLAY_DIGEST_INTERVAL;
    if (netplay.digest_serialise_ms > NETPLAY_DIGEST_BUDGET_MS) {
        const double scale = netplay.digest_serialise_ms / NETPLAY_DIGEST_BUDGET_MS;
        const double scaled = (double) NETPLAY_DIGEST_INTERVAL * scale;
        interval = scaled >= (double) NETPLAY_DIGEST_INTERVAL_MAX ? NETPLAY_DIGEST_INTERVAL_MAX : (unsigned) scaled;
    }
    if (interval != netplay.digest_interval) {
        LOG_INFO(
            mux_module, "netplay: state digest costs %.1f ms to serialise, checking every %u frames",
            netplay.digest_serialise_ms, interval
        );
        netplay.digest_interval = interval;
    }
    netplay.digest_next_frame = netplay.frame + interval;
    pthread_mutex_unlock(&netplay.mutex);

    pthread_mutex_lock(&netplay.digest_mutex);
    uint8_t *const state = netplay.digest_state;
    const size_t capacity = netplay.digest_state_capacity;
    netplay.digest_state = netplay.digest_hash_state;
    netplay.digest_state_capacity = netplay.digest_hash_capacity;
    netplay.digest_hash_state = state;
    netplay.digest_hash_capacity = capacity;
    netplay.digest_hash_size = size;
    netplay.digest_job_frame = frame;
    netplay.digest_job_pending = 1;
    pthread_cond_signal(&netplay.digest_wake);
    pthread_mutex_unlock(&netplay.digest_mutex);

    perf_end(perf_stage_netplay_digest, digest_start);
    return 0;
}

static int apply_client_state(void) {
    pthread_mutex_lock(&netplay.mutex);
    netplay_peer *peer = &netplay.peers[0];
    uint8_t *state = peer->rx_state;
    const size_t size = peer->rx_state_size;
    const uint64_t frame = peer->rx_state_frame;
    peer->rx_state = NULL;
    peer->rx_state_size = 0;
    peer->rx_state_frame = 0;
    peer->rx_state_pending = 0;
    pthread_mutex_unlock(&netplay.mutex);
    if (!state) return -1;

    hw_render_bridge_enter_core_call();
    if (current_core.retro_serialize_size) current_core.retro_serialize_size();
    const int okay = current_core.retro_unserialize && current_core.retro_unserialize(state, size);
    hw_render_bridge_exit_core_call();
    free(state);
    if (!okay) return -1;

    audio_bridge_clear_queued();
    runahead_invalidate();
    pthread_mutex_lock(&netplay.mutex);
    netplay.frame = frame;
    netplay.public_info.frame = frame;
    memset(netplay.local_history, 0, sizeof(netplay.local_history));
    memset(netplay.remote_history, 0, sizeof(netplay.remote_history));
    netplay.local_input_generation = 0;
    netplay.local_input_frame = frame;
    memset(netplay.remote_input_generation, 0, sizeof(netplay.remote_input_generation));
    memset(netplay.remote_input_frame, 0, sizeof(netplay.remote_input_frame));
    memset(netplay.remote_digest_pending, 0, sizeof(netplay.remote_digest_pending));
    netplay.digest_due = 0;
    netplay.local_digest_frame = 0;
    memset(peer->sent_input_frame, 0, sizeof(peer->sent_input_frame));
    memset(peer->sent_input_valid, 0, sizeof(peer->sent_input_valid));
    peer->ready_sent = 1;
    pthread_mutex_unlock(&netplay.mutex);
    return 0;
}

static void RETRO_CALLCONV
netpacket_send(const int flags, const void *data, const size_t size, const uint16_t client_id) {
    (void) flags;
    if (!netplay.netpacket_started || !data || !size || size > NETPLAY_PACKET_CAP) return;
    const uint16_t peer_id = netplay.role == netplay_role_host ? 1U : 0U;
    if (client_id != RETRO_NETPACKET_BROADCAST && client_id != peer_id) return;

    uint8_t *copy = malloc(size);
    if (!copy) {
        set_failure(lang.muxretro.netplay.packet_alloc_failed);
        return;
    }
    memcpy(copy, data, size);

    pthread_mutex_lock(&netplay.mutex);
    if (netplay.outgoing_count >= NETPLAY_PACKET_QUEUE_CAP) {
        pthread_mutex_unlock(&netplay.mutex);
        free(copy);
        set_failure(lang.muxretro.netplay.packet_queue_failed);
        return;
    }
    const unsigned tail = (netplay.outgoing_head + netplay.outgoing_count) % NETPLAY_PACKET_QUEUE_CAP;
    netplay.outgoing_packets[tail] = (netplay_packet) {copy, size};
    netplay.outgoing_count++;
    pthread_mutex_unlock(&netplay.mutex);
}

static void netpacket_receive_drain(void) {
    if (!netplay.netpacket_started || !netplay.netpacket.receive) return;
    for (;;) {
        pthread_mutex_lock(&netplay.mutex);
        if (!netplay.incoming_count) {
            pthread_mutex_unlock(&netplay.mutex);
            break;
        }
        netplay_packet packet = netplay.incoming_packets[netplay.incoming_head];
        memset(&netplay.incoming_packets[netplay.incoming_head], 0, sizeof(packet));
        netplay.incoming_head = (netplay.incoming_head + 1) % NETPLAY_PACKET_QUEUE_CAP;
        netplay.incoming_count--;
        pthread_mutex_unlock(&netplay.mutex);

        netplay.netpacket.receive(packet.data, packet.size, netplay.role == netplay_role_host ? 1U : 0U);
        free(packet.data);
    }
}

static void RETRO_CALLCONV netpacket_poll_receive(void) {
    netpacket_receive_drain();
}

static int netpacket_start(void) {
    if (!netplay.netpacket_available || netplay.netpacket_started) return 0;
    netplay.netpacket_started = 1;
    const uint16_t local_id = (uint16_t) netplay.public_info.local_port;
    hw_render_bridge_enter_core_call();
    netplay.netpacket.start(local_id, netpacket_send, netpacket_poll_receive);
    int accepted = 1;
    if (netplay.role == netplay_role_host && netplay.netpacket.connected) {
        for (unsigned index = 0; index < netplay.peer_count && accepted; index++)
            accepted = netplay.netpacket.connected((uint16_t) netplay.peers[index].owner_port);
    }
    hw_render_bridge_exit_core_call();
    if (!accepted) {
        set_failure(lang.muxretro.netplay.core_rejected);
        return -1;
    }
    return 0;
}

static void netpacket_stop(void) {
    if (!netplay.netpacket_started) return;
    netplay.netpacket_started = 0;
    const struct retro_netpacket_callback callback = netplay.netpacket;
    const netplay_role role = netplay.role;
    hw_render_bridge_enter_core_call();
    if (role == netplay_role_host && callback.disconnected) {
        for (unsigned index = 0; index < netplay.peer_count; index++)
            callback.disconnected((uint16_t) netplay.peers[index].owner_port);
    }
    if (callback.stop) callback.stop();
    hw_render_bridge_exit_core_call();
}

int netplay_init(const char *core_path, const char *content_path) {
    memset(&netplay, 0, sizeof(netplay));
    pthread_mutex_init(&netplay.mutex, NULL);
    atomic_init(&netplay.stop, 0);
    atomic_init(&netplay.disconnecting, 0);
    atomic_init(&netplay.discovery_stop, 0);
    netplay_initialised = 1;
    netplay.listen_fd = -1;
    for (unsigned index = 0; index < NETPLAY_CLIENT_CAPACITY; index++) {
        netplay.peers[index].socket_fd = -1;
        netplay.peers[index].wake_fd = -1;
    }
    netplay.status = netplay_status_idle;
    netplay.public_info.status = netplay_status_idle;
    atomic_store(&netplay_fast_status, netplay_status_idle);
    netplay.public_info.input_delay = 2;
    host_name_load();
    host_session_settings_load();
    snprintf(netplay.core_path, sizeof(netplay.core_path), "%s", core_path ? core_path : "");
    snprintf(netplay.content_path, sizeof(netplay.content_path), "%s", content_path ? content_path : "");

    struct retro_system_info system = {0};
    if (current_core.retro_get_system_info) current_core.retro_get_system_info(&system);
    snprintf(
        netplay.manifest.core_name, sizeof(netplay.manifest.core_name), "%s",
        system.library_name ? system.library_name : ""
    );
    snprintf(
        netplay.manifest.core_version, sizeof(netplay.manifest.core_version), "%s",
        system.library_version ? system.library_version : ""
    );
    hash_options(netplay.manifest.option_hash);
    netplay.netpacket = pending_netpacket;
    netplay.netpacket_available = pending_netpacket_available;
    return 0;
}

int netplay_parse_address(const char *specification, char *address, const size_t address_size, uint16_t *port) {
    if (!specification || !specification[0] || !address || address_size < 2 || !port) return -1;
    *port = NETPLAY_DEFAULT_PORT;

    if (specification[0] == '[') {
        const char *closing = strchr(specification, ']');
        if (!closing || closing == specification + 1 || (closing[1] && closing[1] != ':')) return -1;
        if ((size_t) (closing - specification) > address_size) return -1;
        snprintf(address, address_size, "%.*s", (int) (closing - specification - 1), specification + 1);
        if (closing[1] == ':') {
            char *end = NULL;
            const unsigned long parsed = strtoul(closing + 2, &end, 10);
            if (!end || *end || !parsed || parsed > UINT16_MAX) return -1;
            *port = (uint16_t) parsed;
        }
    } else {
        const char *first = strchr(specification, ':');
        const char *last = strrchr(specification, ':');
        if (first && first == last) {
            if (first == specification || (size_t) (first - specification) >= address_size) return -1;
            snprintf(address, address_size, "%.*s", (int) (first - specification), specification);
            char *end = NULL;
            const unsigned long parsed = strtoul(first + 1, &end, 10);
            if (!end || *end || !parsed || parsed > UINT16_MAX) return -1;
            *port = (uint16_t) parsed;
        } else {
            if (strlen(specification) >= address_size) return -1;
            snprintf(address, address_size, "%s", specification);
        }
    }
    return address[0] ? 0 : -1;
}

void netplay_shutdown(void) {
    if (!netplay_initialised) return;
    netplay_disconnect();
    SSL_CTX_free(netplay.tls);
    X509_free(netplay.certificate);
    EVP_PKEY_free(netplay.private_key);
    netplay.tls = NULL;
    netplay.certificate = NULL;
    netplay.private_key = NULL;
    pthread_mutex_destroy(&netplay.mutex);
    netplay_initialised = 0;
}

int netplay_host(const uint16_t requested_port) {
    if (!netplay_initialised) return -1;
    if (atomic_load(&netplay_fast_status) == netplay_status_failed) netplay_disconnect();
    if (netplay_is_active()) return -1;
    if (!netplay.tls && tls_create() != 0) {
        set_failure(lang.muxretro.netplay.encryption_init_failed);
        return -1;
    }
    if (netplay.discovery_running) {
        atomic_store(&netplay.discovery_stop, 1);
        pthread_join(netplay.discovery_thread, NULL);
        netplay.discovery_running = 0;
    }
    netplay.port = requested_port ? requested_port : NETPLAY_DEFAULT_PORT;
    hash_options(netplay.manifest.option_hash);
    netplay.manifest_ready = 0;
    netplay.role = netplay_role_host;
    netplay.mode = netplay.netpacket_available ? netplay_mode_separate : netplay.host_mode;
    if (netplay.netpacket_available) netplay.host_slots = 1;
    netplay.public_info.role = netplay_role_host;
    netplay.public_info.mode = netplay.mode;
    netplay.public_info.local_port = 0;
    netplay.public_info.input_delay = netplay.netpacket_available ? 0U : 2U;
    netplay.public_info.player_count = netplay.host_slots + 1U;
    netplay.peer_count = 0;
    if (RAND_bytes((uint8_t *) &netplay.session_id, sizeof(netplay.session_id)) != 1) {
        set_failure(lang.muxretro.netplay.hosting_start_failed);
        return -1;
    }

    const int fd = socket(AF_INET6, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        set_failure(lang.muxretro.netplay.hosting_start_failed);
        return -1;
    }
    int one = 1;
    int zero = 0;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &zero, sizeof(zero));
    struct sockaddr_in6 address = {
        .sin6_family = AF_INET6, .sin6_port = htons(netplay.port), .sin6_addr = IN6ADDR_ANY_INIT
    };
    if (bind(fd, (struct sockaddr *) &address, sizeof(address)) != 0 || listen(fd, 3) != 0
        || set_nonblocking(fd) != 0) {
        close(fd);
        set_failure(lang.muxretro.netplay.hosting_start_failed);
        return -1;
    }

    netplay.listen_fd = fd;
    atomic_store(&netplay.stop, 0);
    set_status(netplay_status_hosting);
    netplay.accept_running = pthread_create(&netplay.accept_thread, NULL, accept_thread, NULL) == 0;
    if (!netplay.accept_running) {
        close(fd);
        netplay.listen_fd = -1;
        set_failure(lang.muxretro.netplay.hosting_start_failed);
        return -1;
    }
    cheevo_set_netplay_active(1);
    cheats_set_suppressed(1);
    return 0;
}

int netplay_join(const char *address, const uint16_t requested_port) {
    if (!netplay_initialised || !address || !address[0]) return -1;
    if (atomic_load(&netplay_fast_status) == netplay_status_failed) netplay_disconnect();
    if (netplay_is_active()) return -1;
    if (!netplay.tls && tls_create() != 0) {
        set_failure(lang.muxretro.netplay.encryption_init_failed);
        return -1;
    }
    if (netplay.discovery_running) {
        atomic_store(&netplay.discovery_stop, 1);
        pthread_join(netplay.discovery_thread, NULL);
        netplay.discovery_running = 0;
    }
    netplay.port = requested_port ? requested_port : NETPLAY_DEFAULT_PORT;
    hash_options(netplay.manifest.option_hash);
    netplay.manifest_ready = 0;
    netplay.role = netplay_role_client;
    netplay.mode = netplay_mode_separate;
    netplay.public_info.role = netplay_role_client;
    netplay.public_info.mode = netplay.mode;
    netplay.public_info.local_port = 1;
    netplay.public_info.input_delay = netplay.netpacket_available ? 0U : 2U;
    netplay.public_info.player_count = 2;
    snprintf(netplay.join_address, sizeof(netplay.join_address), "%s", address);
    snprintf(netplay.public_info.peer, sizeof(netplay.public_info.peer), "%s", address);
    atomic_store(&netplay.stop, 0);
    set_status(netplay_status_connecting);
    netplay.accept_running = pthread_create(&netplay.accept_thread, NULL, connect_thread, NULL) == 0;
    if (!netplay.accept_running) {
        set_failure(lang.muxretro.netplay.joining_start_failed);
        return -1;
    }
    cheevo_set_netplay_active(1);
    cheats_set_suppressed(1);
    return 0;
}

void netplay_confirm_pairing(void) {
    if (!netplay_initialised) return;
    pthread_mutex_lock(&netplay.mutex);
    if (netplay.status == netplay_status_pairing && netplay.role == netplay_role_client) {
        netplay.peers[0].local_confirmed = 1;
        netplay.public_info.pairing_local_confirmed = 1;
    }
    pthread_mutex_unlock(&netplay.mutex);
}

void netplay_disconnect(void) {
    if (!netplay_initialised) return;
    if (atomic_exchange(&netplay.disconnecting, 1)) return;
    atomic_store(&netplay.stop, 1);
    atomic_store(&netplay.discovery_stop, 1);
    pthread_mutex_lock(&netplay.mutex);
    const int listen_fd = netplay.listen_fd;
    int peer_fds[NETPLAY_CLIENT_CAPACITY];
    for (unsigned index = 0; index < NETPLAY_CLIENT_CAPACITY; index++)
        peer_fds[index] = netplay.peers[index].socket_fd;
    pthread_mutex_unlock(&netplay.mutex);
    if (listen_fd >= 0) shutdown(listen_fd, SHUT_RDWR);
    for (unsigned index = 0; index < NETPLAY_CLIENT_CAPACITY; index++)
        if (peer_fds[index] >= 0) shutdown(peer_fds[index], SHUT_RDWR);
    if (netplay.accept_running) pthread_join(netplay.accept_thread, NULL);
    if (netplay.discovery_running) pthread_join(netplay.discovery_thread, NULL);
    for (unsigned index = 0; index < NETPLAY_CLIENT_CAPACITY; index++)
        if (netplay.peers[index].running) pthread_join(netplay.peers[index].thread, NULL);
    netpacket_stop();

    for (unsigned index = 0; index < NETPLAY_CLIENT_CAPACITY; index++) {
        netplay_peer *peer = &netplay.peers[index];
        if (peer->ssl) SSL_free(peer->ssl);
        if (peer->socket_fd >= 0) close(peer->socket_fd);
        if (peer->wake_fd >= 0) close(peer->wake_fd);
        peer->wake_fd = -1;
        free(peer->rx_state);
    }
    if (netplay.listen_fd >= 0) close(netplay.listen_fd);
    digest_worker_stop();
    free(netplay.digest_state);
    free(netplay.digest_hash_state);
    free(netplay.sync_state);
    for (unsigned index = 0; index < NETPLAY_PACKET_QUEUE_CAP; index++) {
        free(netplay.outgoing_packets[index].data);
        free(netplay.incoming_packets[index].data);
        memset(&netplay.outgoing_packets[index], 0, sizeof(netplay.outgoing_packets[index]));
        memset(&netplay.incoming_packets[index], 0, sizeof(netplay.incoming_packets[index]));
    }
    memset(netplay.peers, 0, sizeof(netplay.peers));
    for (unsigned index = 0; index < NETPLAY_CLIENT_CAPACITY; index++) {
        netplay.peers[index].socket_fd = -1;
        netplay.peers[index].wake_fd = -1;
    }
    netplay.peer_count = 0;
    netplay.listen_fd = -1;
    netplay.accept_running = 0;
    netplay.discovery_running = 0;
    netplay.status = netplay_status_idle;
    memset(&netplay.public_info, 0, sizeof(netplay.public_info));
    netplay.public_info.status = netplay_status_idle;
    atomic_store(&netplay_fast_status, netplay_status_idle);
    netplay.public_info.input_delay = 2;
    netplay.role = netplay_role_none;
    netplay.mode = netplay_mode_separate;
    netplay.session_id = 0;
    netplay.sync_state_sent = 0;
    netplay.sync_state = NULL;
    netplay.sync_state_size = 0;
    netplay.sync_state_refs = 0;
    netplay.local_input_generation = 0;
    netplay.local_input_frame = 0;
    netplay.frame = 0;
    memset(netplay.remote_input_generation, 0, sizeof(netplay.remote_input_generation));
    memset(netplay.remote_input_frame, 0, sizeof(netplay.remote_input_frame));
    memset(netplay.local_history, 0, sizeof(netplay.local_history));
    memset(netplay.remote_history, 0, sizeof(netplay.remote_history));
    netplay.digest_due = 0;
    netplay.digest_interval = hw_render_bridge_active() ? NETPLAY_DIGEST_INTERVAL * 4 : NETPLAY_DIGEST_INTERVAL;
    netplay.digest_next_frame = 0;
    netplay.digest_serialise_ms = 0.0;
    netplay.digest_send_generation = 0;
    netplay.delay_send_generation = 0;
    netplay.delay_change_pending = 0;
    netplay.local_menu_open = 0;
    netplay.menu_state_generation = 0;
    netplay.menu_pause_requested = 0;
    netplay.menu_pause_value = 0;
    netplay.menu_pause_frame = 0;
    netplay.menu_pause_generation = 0;
    memset(netplay.remote_digest_pending, 0, sizeof(netplay.remote_digest_pending));
    netplay.local_digest_frame = 0;
    memset(netplay.remote_digest_frame, 0, sizeof(netplay.remote_digest_frame));
    netplay.resynchronisations = 0;
    netplay.failure_announced = 0;
    netplay.outgoing_head = 0;
    netplay.outgoing_count = 0;
    netplay.incoming_head = 0;
    netplay.incoming_count = 0;
    netplay.digest_state = NULL;
    netplay.digest_state_capacity = 0;
    netplay.digest_hash_state = NULL;
    netplay.digest_hash_capacity = 0;
    netplay.digest_hash_size = 0;
    netplay.digest_job_pending = 0;
    netplay.digest_job_busy = 0;
    netplay.digest_ready = 0;
    netplay.digest_ready_frame = 0;
    netplay.digest_job_frame = 0;
    input_bridge_set_netplay_state(0, 0);
    cheevo_set_netplay_active(0);
    cheats_set_suppressed(cheevo_restricted());
    atomic_store(&netplay.stop, 0);
    atomic_store(&netplay.discovery_stop, 0);
    atomic_store(&netplay.disconnecting, 0);
}

int netplay_discover(void) {
    if (!netplay_initialised) return -1;
    if (atomic_load(&netplay_fast_status) == netplay_status_failed) netplay_disconnect();
    if (netplay_is_active()) return -1;
    if (netplay.discovery_running) {
        atomic_store(&netplay.discovery_stop, 1);
        pthread_join(netplay.discovery_thread, NULL);
        netplay.discovery_running = 0;
    }
    pthread_mutex_lock(&netplay.mutex);
    netplay.discovered_count = 0;
    memset(netplay.discovered, 0, sizeof(netplay.discovered));
    netplay.status = netplay_status_discovering;
    netplay.public_info.status = netplay_status_discovering;
    atomic_store(&netplay_fast_status, netplay_status_discovering);
    pthread_mutex_unlock(&netplay.mutex);
    atomic_store(&netplay.discovery_stop, 0);
    netplay.discovery_running = pthread_create(&netplay.discovery_thread, NULL, discovery_thread, NULL) == 0;
    if (!netplay.discovery_running) {
        set_failure(lang.muxretro.netplay.discovery_start_failed);
        return -1;
    }
    return 0;
}

unsigned netplay_discovered_count(void) {
    if (!netplay_initialised) return 0;
    pthread_mutex_lock(&netplay.mutex);
    const unsigned count = netplay.discovered_count;
    pthread_mutex_unlock(&netplay.mutex);
    return count;
}

int netplay_discovered_get(const unsigned index, netplay_discovered_host *host) {
    if (!netplay_initialised || !host) return -1;
    pthread_mutex_lock(&netplay.mutex);
    const int okay = index < netplay.discovered_count;
    if (okay) *host = netplay.discovered[index];
    pthread_mutex_unlock(&netplay.mutex);
    return okay ? 0 : -1;
}

int netplay_join_discovered(const unsigned index) {
    netplay_discovered_host host;
    if (netplay_discovered_get(index, &host) != 0) return -1;
    atomic_store(&netplay.discovery_stop, 1);
    if (netplay.discovery_running) pthread_join(netplay.discovery_thread, NULL);
    netplay.discovery_running = 0;
    pthread_mutex_lock(&netplay.mutex);
    netplay.status = netplay_status_idle;
    netplay.public_info.status = netplay_status_idle;
    atomic_store(&netplay_fast_status, netplay_status_idle);
    pthread_mutex_unlock(&netplay.mutex);
    return netplay_join(host.address, host.port);
}

void netplay_get_host_name(char *name, const size_t size) {
    if (!name || !size) return;
    if (!netplay_initialised) {
        if (randname_generate_with_separator(name, size, " ") != 0) name[0] = '\0';
        return;
    }
    pthread_mutex_lock(&netplay.mutex);
    snprintf(name, size, "%s", netplay.host_name);
    pthread_mutex_unlock(&netplay.mutex);
}

int netplay_set_host_name(const char *name) {
    if (!netplay_initialised) return -1;
    char normalised[NETPLAY_HOST_NAME_SIZE];
    if (normalise_host_name(name, normalised) != 0 || host_name_save(normalised) != 0) return -1;
    pthread_mutex_lock(&netplay.mutex);
    snprintf(netplay.host_name, sizeof(netplay.host_name), "%s", normalised);
    pthread_mutex_unlock(&netplay.mutex);
    return 0;
}

netplay_mode netplay_get_host_mode(void) {
    if (!netplay_initialised || netplay.netpacket_available) return netplay_mode_separate;
    pthread_mutex_lock(&netplay.mutex);
    const netplay_mode mode = netplay.host_mode;
    pthread_mutex_unlock(&netplay.mutex);
    return mode;
}

int netplay_set_host_mode(const netplay_mode mode) {
    if (!netplay_initialised || netplay_is_active() || netplay.netpacket_available
        || (mode != netplay_mode_separate && mode != netplay_mode_play_together))
        return -1;
    create_directories(NETPLAY_SETTINGS_DIR, 0);
    write_text_to_file_atomic(NETPLAY_MODE_FILE, INT, (int) mode);
    pthread_mutex_lock(&netplay.mutex);
    netplay.host_mode = mode;
    pthread_mutex_unlock(&netplay.mutex);
    return 0;
}

int netplay_play_together_available(void) {
    return netplay_initialised && !netplay.netpacket_available;
}

unsigned netplay_get_host_slots(void) {
    if (!netplay_initialised || netplay.netpacket_available) return 1;
    pthread_mutex_lock(&netplay.mutex);
    const unsigned slots = netplay.host_slots;
    pthread_mutex_unlock(&netplay.mutex);
    return slots;
}

int netplay_set_host_slots(const unsigned slots) {
    if (!netplay_initialised || netplay_is_active() || (netplay.netpacket_available && slots != 1) || slots < 1
        || slots > NETPLAY_CLIENT_CAPACITY)
        return -1;
    create_directories(NETPLAY_SETTINGS_DIR, 0);
    write_text_to_file_atomic(NETPLAY_SLOTS_FILE, INT, (int) slots);
    pthread_mutex_lock(&netplay.mutex);
    netplay.host_slots = slots;
    pthread_mutex_unlock(&netplay.mutex);
    return 0;
}

unsigned netplay_get_host_slot_limit(void) {
    return netplay_initialised && !netplay.netpacket_available ? NETPLAY_CLIENT_CAPACITY : 1U;
}

void netplay_tick(void) {
    if (!netplay_initialised || atomic_load(&netplay_fast_status) == netplay_status_idle) return;
    const int local_menu_open = pause_menu_is_active();
    pthread_mutex_lock(&netplay.mutex);
    if (local_menu_open != netplay.local_menu_open) {
        netplay.local_menu_open = local_menu_open;
        if (netplay.role == netplay_role_host)
            update_host_menu_pause_locked();
        else if (netplay.role == netplay_role_client)
            netplay.menu_state_generation++;
    }
    const netplay_status status = netplay.status;
    const netplay_role role = netplay.role;
    if (role == netplay_role_host) update_host_menu_pause_locked();
    const unsigned player_count = netplay.public_info.player_count;
    const netplay_mode mode = netplay.mode;
    const int routes_input = !netplay.netpacket_available;
    const int state_pending = netplay.peers[0].rx_state_pending;
    int state_queued = 0;
    state_queued = netplay.sync_state != NULL;
    const int sync_state_sent = netplay.sync_state_sent;
    const int digest_due = netplay.digest_due;
    char failure[sizeof(netplay.public_info.failure)] = "";
    if (status == netplay_status_failed && !netplay.failure_announced) {
        netplay.failure_announced = 1;
        snprintf(failure, sizeof(failure), "%s", netplay.public_info.failure);
    }
    pthread_mutex_unlock(&netplay.mutex);

    input_bridge_set_netplay_state(
        routes_input && mode == netplay_mode_play_together ? 1U : player_count, routes_input
    );

    if (failure[0]) {
        pause_menu_show_toast_timed(failure, 5000);
        netplay_disconnect();
        return;
    }

    if (status == netplay_status_synchronising && netplay.netpacket_available && !sync_state_sent) {
        if (netpacket_start() != 0) return;
        pthread_mutex_lock(&netplay.mutex);
        if (role == netplay_role_client) netplay.peers[0].ready_sent = 1;
        netplay.sync_state_sent = 1;
        pthread_mutex_unlock(&netplay.mutex);
    } else if (status == netplay_status_synchronising && role == netplay_role_host && !state_queued
               && !sync_state_sent) {
        if (queue_host_state() != 0) {
            set_failure(lang.muxretro.netplay.state_create_failed);
        } else {
            pthread_mutex_lock(&netplay.mutex);
            netplay.sync_state_sent = 1;
            pthread_mutex_unlock(&netplay.mutex);
        }
    } else if (status == netplay_status_synchronising && role == netplay_role_client && state_pending) {
        if (apply_client_state() != 0) set_failure(lang.muxretro.netplay.state_rejected);
    }

    if (status == netplay_status_playing && digest_due) {
        pthread_mutex_lock(&netplay.mutex);
        const uint64_t digest_frame = netplay.frame;
        netplay.digest_due = 0;
        pthread_mutex_unlock(&netplay.mutex);

        if (queue_state_digest(digest_frame) != 0) {
            set_failure(lang.muxretro.netplay.digest_create_failed);
            return;
        }
    }

    if (status == netplay_status_playing && netplay.digest_thread_running) {
        pthread_mutex_lock(&netplay.digest_mutex);
        const int ready = netplay.digest_ready;
        uint8_t digest[SHA256_DIGEST_LENGTH];
        uint64_t frame = 0;
        if (ready) {
            memcpy(digest, netplay.digest_ready_value, sizeof(digest));
            frame = netplay.digest_ready_frame;
            netplay.digest_ready = 0;
        }
        pthread_mutex_unlock(&netplay.digest_mutex);

        if (ready) {
            pthread_mutex_lock(&netplay.mutex);
            memcpy(netplay.local_digest, digest, sizeof(digest));
            netplay.local_digest_frame = frame;
            netplay.digest_send_generation++;
            pthread_mutex_unlock(&netplay.mutex);
        }
    }

    int repeated_mismatch = 0;
    if (status == netplay_status_playing) {
        pthread_mutex_lock(&netplay.mutex);
        for (unsigned port = 0; port < NETPLAY_PORT_COUNT; port++) {
            if (!netplay.remote_digest_pending[port] || netplay.local_digest_frame != netplay.remote_digest_frame[port])
                continue;
            const int mismatch = memcmp(netplay.local_digest, netplay.remote_digest[port], SHA256_DIGEST_LENGTH) != 0;
            netplay.remote_digest_pending[port] = 0;
            if (mismatch && netplay.role == netplay_role_host) {
                if (netplay.resynchronisations == 0) {
                    LOG_WARN(
                        mux_module, "netplay: state mismatch at frame %llu from player %u; resynchronising",
                        (unsigned long long) netplay.local_digest_frame, port + 1U
                    );
                    netplay.resynchronisations = 1;
                    netplay.status = netplay_status_synchronising;
                    netplay.public_info.status = netplay_status_synchronising;
                    atomic_store(&netplay_fast_status, netplay_status_synchronising);
                    netplay.sync_state_sent = 0;
                    netplay.digest_due = 0;
                    netplay.local_digest_frame = 0;
                    for (unsigned index = 0; index < netplay.peer_count; index++) {
                        netplay.peers[index].ready_received = 0;
                        netplay.peers[index].ready_sent = 0;
                    }
                    memset(netplay.remote_digest_pending, 0, sizeof(netplay.remote_digest_pending));
                    break;
                }
                LOG_WARN(
                    mux_module, "netplay: repeated state mismatch at frame %llu from player %u",
                    (unsigned long long) netplay.local_digest_frame, port + 1U
                );
                repeated_mismatch = 1;
            }
        }
        pthread_mutex_unlock(&netplay.mutex);
    }
    if (repeated_mismatch) set_failure(lang.muxretro.netplay.mismatch_disconnected);
}

static int16_t merge_axis(const int16_t first, const int16_t second) {
    const int first_magnitude = first < 0 ? -(int) first : (int) first;
    const int second_magnitude = second < 0 ? -(int) second : (int) second;
    if (second_magnitude > first_magnitude) return second;
    if (second_magnitude < first_magnitude) return first;
    return second > first ? second : first;
}

static void merge_pad(netplay_pad_state *target, const netplay_pad_state *source) {
    target->buttons |= source->buttons;
    for (unsigned index = 0; index < 4; index++)
        target->axes[index] = merge_axis(target->axes[index], source->axes[index]);
    target->connected |= source->connected;
}

static void normalise_merged_directions(netplay_pad_state *state) {
    const uint16_t vertical = (uint16_t) ((1U << RETRO_DEVICE_ID_JOYPAD_UP) | (1U << RETRO_DEVICE_ID_JOYPAD_DOWN));
    const uint16_t horizontal = (uint16_t) ((1U << RETRO_DEVICE_ID_JOYPAD_LEFT) | (1U << RETRO_DEVICE_ID_JOYPAD_RIGHT));
    if ((state->buttons & vertical) == vertical) state->buttons &= (uint16_t) ~vertical;
    if ((state->buttons & horizontal) == horizontal) state->buttons &= (uint16_t) ~horizontal;
}

int netplay_before_frame(void) {
    if (netplay_menu_paused()) return 0;
    if (netplay.netpacket_available) {
        netpacket_receive_drain();
        if (netplay.netpacket.poll) netplay.netpacket.poll();
        return 1;
    }

    netplay_pad_state physical;
    netplay_input_get_local(&physical);
    if (pause_menu_is_active()) memset(&physical, 0, sizeof(physical));
    pthread_mutex_lock(&netplay.mutex);
    const uint64_t frame = netplay.frame;
    if (netplay.delay_change_pending && frame >= netplay.delay_change_frame) {
        netplay.public_info.input_delay = netplay.delay_change_value;
        netplay.delay_change_pending = 0;
    }
    const unsigned delay = netplay.public_info.input_delay;
    const unsigned local_port = netplay.public_info.local_port;
    const unsigned player_count = netplay.public_info.player_count;
    const netplay_mode mode = netplay.mode;
    netplay_input_history *current = &netplay.local_history[frame % NETPLAY_INPUT_HISTORY_CAPACITY];
    int input_published = 0;
    if (!current->valid || current->frame != frame) {
        netplay.local_input_frame = frame;
        netplay.local_input_generation++;
        *current = (netplay_input_history) {frame, physical, 1};
        input_published = 1;
    }

    netplay_pad_state delayed_local = {0};
    netplay_pad_state delayed_remote[NETPLAY_PORT_COUNT] = {0};
    if (frame >= delay) {
        const uint64_t target = frame - delay;
        const netplay_input_history *local = &netplay.local_history[target % NETPLAY_INPUT_HISTORY_CAPACITY];
        if (!local->valid || local->frame != target) {
            pthread_mutex_unlock(&netplay.mutex);
            if (input_published) wake_peers();
            return 0;
        }
        delayed_local = local->input;
        for (unsigned port = 0; port < NETPLAY_PORT_COUNT; port++) {
            if (port == local_port || port >= player_count) continue;
            const netplay_input_history *remote =
                &netplay.remote_history[port][target % NETPLAY_INPUT_HISTORY_CAPACITY];
            if (!remote->valid || remote->frame != target) {
                pthread_mutex_unlock(&netplay.mutex);
                if (input_published) wake_peers();
                return 0;
            }
            delayed_remote[port] = remote->input;
        }
    }
    pthread_mutex_unlock(&netplay.mutex);
    if (input_published) wake_peers();

    if (mode == netplay_mode_play_together) {
        netplay_pad_state merged = delayed_local;
        netplay_pad_state empty = {0};
        for (unsigned port = 0; port < player_count && port < NETPLAY_PORT_COUNT; port++) {
            if (port != local_port) merge_pad(&merged, &delayed_remote[port]);
        }
        normalise_merged_directions(&merged);
        netplay_input_set_port(0, &merged);
        for (unsigned port = 1; port < NETPLAY_PORT_COUNT; port++)
            netplay_input_set_port(port, &empty);
    } else {
        for (unsigned port = 0; port < player_count && port < NETPLAY_PORT_COUNT; port++)
            netplay_input_set_port(port, port == local_port ? &delayed_local : &delayed_remote[port]);
    }
    return 1;
}

void netplay_after_frame(void) {
    pthread_mutex_lock(&netplay.mutex);
    netplay.frame++;
    netplay.public_info.frame = netplay.frame;
    if (!netplay.digest_interval) netplay.digest_interval = NETPLAY_DIGEST_INTERVAL;
    if (!netplay.digest_next_frame) netplay.digest_next_frame = netplay.digest_interval;
    if (!netplay.netpacket_available && netplay.frame >= netplay.digest_next_frame) {
        netplay.digest_due = 1;
        netplay.digest_next_frame = netplay.frame + netplay.digest_interval;
    }
    pthread_mutex_unlock(&netplay.mutex);
}

int netplay_is_active(void) {
    if (!netplay_initialised) return 0;
    return atomic_load(&netplay_fast_status) != netplay_status_idle;
}

int netplay_is_playing(void) {
    if (!netplay_initialised) return 0;
    return atomic_load(&netplay_fast_status) == netplay_status_playing;
}

int netplay_menu_paused(void) {
    if (!netplay_initialised || atomic_load(&netplay_fast_status) != netplay_status_playing) return 0;
    pthread_mutex_lock(&netplay.mutex);
    const int paused = netplay.menu_pause_value && netplay.frame >= netplay.menu_pause_frame;
    pthread_mutex_unlock(&netplay.mutex);
    return paused;
}

int netplay_blocks_core(void) {
    if (!netplay_initialised) return 0;
    const netplay_status status = (netplay_status) atomic_load(&netplay_fast_status);
    return status == netplay_status_connecting || status == netplay_status_pairing || status == netplay_status_checking
           || status == netplay_status_synchronising || status == netplay_status_reconnecting
           || status == netplay_status_failed;
}

void netplay_get_info(netplay_info *info) {
    if (!info) return;
    if (!netplay_initialised) {
        memset(info, 0, sizeof(*info));
        info->status = netplay_status_failed;
        snprintf(info->failure, sizeof(info->failure), "%s", lang.muxretro.netplay.secure_unavailable);
        return;
    }
    pthread_mutex_lock(&netplay.mutex);
    *info = netplay.public_info;
    pthread_mutex_unlock(&netplay.mutex);
}

const char *netplay_status_name(const netplay_status status) {
    switch (status) {
        case netplay_status_idle:
            return lang.muxretro.netplay.not_connected;
        case netplay_status_discovering:
            return lang.muxretro.netplay.status_discovering;
        case netplay_status_hosting:
            return lang.muxretro.netplay.status_hosting;
        case netplay_status_connecting:
            return lang.muxretro.netplay.status_connecting;
        case netplay_status_pairing:
            return lang.muxretro.netplay.status_pairing;
        case netplay_status_checking:
            return lang.muxretro.netplay.status_checking;
        case netplay_status_synchronising:
            return lang.muxretro.netplay.status_synchronising;
        case netplay_status_playing:
            return lang.muxretro.netplay.status_connected;
        case netplay_status_reconnecting:
            return lang.muxretro.netplay.status_reconnecting;
        case netplay_status_failed:
            return lang.muxretro.netplay.status_failed;
        default:
            return lang.generic.unknown;
    }
}

int netplay_get_client_index(unsigned *index) {
    if (!index || !netplay_is_active()) return 0;
    *index = netplay.public_info.local_port;
    return 1;
}

void netplay_set_netpacket_interface(const struct retro_netpacket_callback *callback) {
    if (!netplay_initialised) {
        if (callback) {
            pending_netpacket = *callback;
            pending_netpacket_available = callback->start && callback->receive;
        } else {
            memset(&pending_netpacket, 0, sizeof(pending_netpacket));
            pending_netpacket_available = 0;
        }
        return;
    }
    pthread_mutex_lock(&netplay.mutex);
    if (callback) {
        netplay.netpacket = *callback;
        netplay.netpacket_available = callback->start && callback->receive;
    } else {
        memset(&netplay.netpacket, 0, sizeof(netplay.netpacket));
        netplay.netpacket_available = 0;
    }
    pthread_mutex_unlock(&netplay.mutex);
}
