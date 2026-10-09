#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/futex.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include "client.h"

#define SPIN_LIMIT 4000
#define WAIT_NS    50000000L

extern char **environ;

static pthread_mutex_t lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
static pthread_mutex_t connect_lock = PTHREAD_MUTEX_INITIALIZER;
static uint8_t *base;
static mugl_header *hdr;
static uint8_t *ring;
static uint8_t *resp;
static pid_t server_pid;
static uint32_t msg_left;
static uint32_t msg_pad;
static uint32_t msg_resp_seq;
static uint32_t swaps_sent;
static uint32_t pending_error;

static long futex(volatile uint32_t *addr, int op, uint32_t val, const struct timespec *ts) {
    return syscall(SYS_futex, addr, op, val, ts, NULL, 0);
}

static void server_gone(void) {
    fprintf(stderr, "mugl: the GL server has stopped%s%s\n", hdr && hdr->error[0] ? ": " : "", hdr ? hdr->error : "");
    _exit(1);
}

static void check_server(void) {
    if (hdr->server_exited || (server_pid > 0 && kill(server_pid, 0) != 0 && errno == ESRCH)) server_gone();
}

static const char *server_path(void) {
    const char *path = getenv("MUGL_SERVER");
    return path && *path ? path : "/opt/muos/frontend/mugl-server";
}

static char **server_environment(void) {
    size_t count = 0;
    while (environ[count]) count++;

    char **env = calloc(count + 1, sizeof(char *));
    if (!env) return NULL;

    size_t out = 0;
    for (size_t i = 0; i < count; i++) {
        if (strncmp(environ[i], "LD_PRELOAD=", 11) == 0) continue;
        if (strncmp(environ[i], "LD_LIBRARY_PATH=", 16) == 0) continue;
        env[out++] = environ[i];
    }
    env[out] = NULL;
    return env;
}

static int spawn_server(int fd) {
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, fd, MUGL_SHM_FD);

    char fd_arg[16];
    snprintf(fd_arg, sizeof(fd_arg), "%d", MUGL_SHM_FD);
    char *argv[] = {(char *) server_path(), fd_arg, NULL};

    char **env = server_environment();
    const int result = posix_spawn(&server_pid, argv[0], &actions, NULL, argv, env ? env : environ);
    free(env);
    posix_spawn_file_actions_destroy(&actions);

    if (result != 0) {
        fprintf(stderr, "mugl: could not start %s: %s\n", argv[0], strerror(result));
        return 0;
    }
    return 1;
}

int mugl_connect(void) {
    if (hdr) return 1;

    pthread_mutex_lock(&connect_lock);
    if (hdr) {
        pthread_mutex_unlock(&connect_lock);
        return 1;
    }

    int ok = 0;
    const int fd = memfd_create("mugl", 0);
    if (fd < 0 || ftruncate(fd, MUGL_SHM_SIZE) != 0) goto out;

    uint8_t *map = mmap(NULL, MUGL_SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) goto out;

    mugl_header *h = MUGL_HDR(map);
    h->magic = MUGL_MAGIC;
    h->version = MUGL_VERSION;
    h->client_pid = (uint32_t) getpid();

    if (!spawn_server(fd)) {
        munmap(map, MUGL_SHM_SIZE);
        goto out;
    }

    const struct timespec ts = {0, WAIT_NS};
    for (int tries = 0; tries < 200; tries++) {
        if (__atomic_load_n(&h->ready, __ATOMIC_ACQUIRE)) break;
        if (h->failed || kill(server_pid, 0) != 0) break;
        futex(&h->ready, FUTEX_WAIT, 0, &ts);
    }

    if (!__atomic_load_n(&h->ready, __ATOMIC_ACQUIRE) || h->failed) {
        fprintf(stderr, "mugl: GL server did not start%s%s\n", h->error[0] ? ": " : "", h->error);
        kill(server_pid, SIGTERM);
        munmap(map, MUGL_SHM_SIZE);
        goto out;
    }

    base = map;
    ring = MUGL_RING(map);
    resp = MUGL_RESP(map);
    __atomic_store_n(&hdr, h, __ATOMIC_RELEASE);
    ok = 1;

out:
    if (fd >= 0) close(fd);
    pthread_mutex_unlock(&connect_lock);
    return ok;
}

int mugl_connected(void) {
    return hdr != NULL;
}

uint32_t mugl_width(void) {
    return hdr ? hdr->width : 0;
}

uint32_t mugl_height(void) {
    return hdr ? hdr->height : 0;
}

void mugl_lock(void) {
    pthread_mutex_lock(&lock);
}

void mugl_unlock(void) {
    pthread_mutex_unlock(&lock);
}

static void wake_reader(void) {
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    if (__atomic_load_n(&hdr->reader_sleeping, __ATOMIC_RELAXED)) futex(&hdr->wpos, FUTEX_WAKE, 1, NULL);
}

static uint32_t wait_space(void) {
    int spins = 0;
    for (;;) {
        const uint32_t used = hdr->wpos - __atomic_load_n(&hdr->rpos, __ATOMIC_ACQUIRE);
        const uint32_t space = MUGL_RING_SIZE - used;
        if (space) return space;

        if (++spins < SPIN_LIMIT) continue;

        const uint32_t seen = __atomic_load_n(&hdr->rpos, __ATOMIC_ACQUIRE);
        __atomic_store_n(&hdr->writer_sleeping, 1, __ATOMIC_SEQ_CST);
        wake_reader();
        if (hdr->wpos - __atomic_load_n(&hdr->rpos, __ATOMIC_ACQUIRE) == MUGL_RING_SIZE) {
            const struct timespec ts = {0, WAIT_NS};
            futex(&hdr->rpos, FUTEX_WAIT, seen, &ts);
            check_server();
        }
        __atomic_store_n(&hdr->writer_sleeping, 0, __ATOMIC_RELAXED);
        spins = 0;
    }
}

static void ring_write(const void *data, uint32_t len) {
    const uint8_t *src = data;
    while (len) {
        const uint32_t space = wait_space();
        const uint32_t w = hdr->wpos;
        const uint32_t offset = w & (MUGL_RING_SIZE - 1);
        uint32_t chunk = MUGL_RING_SIZE - offset;
        if (chunk > space) chunk = space;
        if (chunk > len) chunk = len;

        if (src) {
            memcpy(ring + offset, src, chunk);
            src += chunk;
        } else {
            memset(ring + offset, 0, chunk);
        }

        __atomic_store_n(&hdr->wpos, w + chunk, __ATOMIC_RELEASE);
        len -= chunk;
    }
}

void mugl_msg_begin(uint32_t op, uint32_t len) {
    pthread_mutex_lock(&lock);
    if (!hdr && !mugl_connect()) server_gone();
    msg_resp_seq = __atomic_load_n(&hdr->resp_seq, __ATOMIC_ACQUIRE);

    const mugl_msg msg = {op, len};
    ring_write(&msg, sizeof(msg));
    msg_left = len;
    msg_pad = (4U - (len & 3U)) & 3U;
}

void mugl_msg_put(const void *data, uint32_t len) {
    if (len > msg_left) len = msg_left;
    ring_write(data, len);
    msg_left -= len;
}

static void msg_finish(void) {
    if (msg_left) ring_write(NULL, msg_left);
    if (msg_pad) ring_write(NULL, msg_pad);
    msg_left = 0;
    msg_pad = 0;
    wake_reader();
}

void mugl_msg_end(void) {
    msg_finish();
    pthread_mutex_unlock(&lock);
}

const uint8_t *mugl_msg_wait(uint32_t *len) {
    const uint32_t seq = msg_resp_seq;
    msg_finish();

    int spins = 0;
    while (__atomic_load_n(&hdr->resp_seq, __ATOMIC_ACQUIRE) == seq) {
        if (++spins < SPIN_LIMIT) continue;
        const struct timespec ts = {0, WAIT_NS};
        futex(&hdr->resp_seq, FUTEX_WAIT, seq, &ts);
        check_server();
        spins = 0;
    }

    if (len) *len = hdr->resp_len;
    return resp;
}

void mugl_send(uint32_t op, const uint32_t *args, uint32_t count) {
    mugl_msg_begin(op, count * 4U);
    if (count) mugl_msg_put(args, count * 4U);
    mugl_msg_end();
}

uint32_t mugl_call_u32(uint32_t op, const uint32_t *args, uint32_t count) {
    mugl_msg_begin(op, count * 4U);
    if (count) mugl_msg_put(args, count * 4U);
    uint32_t len = 0;
    const uint8_t *r = mugl_msg_wait(&len);
    uint32_t value = 0;
    if (len >= 4) memcpy(&value, r, 4);
    pthread_mutex_unlock(&lock);
    return value;
}

void mugl_swap(void) {
    mugl_msg_begin(MUGL_OP_SWAP, 0);
    const uint32_t sent = ++swaps_sent;
    mugl_msg_end();

    int spins = 0;
    for (;;) {
        const uint32_t done = __atomic_load_n(&hdr->swap_seq, __ATOMIC_ACQUIRE);
        if ((int32_t) (sent - done) <= 1) break;
        if (++spins < SPIN_LIMIT) continue;
        const struct timespec ts = {0, WAIT_NS};
        futex(&hdr->swap_seq, FUTEX_WAIT, done, &ts);
        check_server();
        spins = 0;
    }
}

void mugl_set_error(uint32_t error) {
    if (!pending_error) pending_error = error;
}

uint32_t mugl_take_error(void) {
    const uint32_t error = pending_error;
    pending_error = 0;
    return error;
}
