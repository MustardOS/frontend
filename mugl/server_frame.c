#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <GLES2/gl2.h>
#include "frame.h"
#include "server.h"
#include "server_gl.h"

static mugl_frame_header *shared;
static size_t shared_size;

void server_read_frame(uint8_t *dst, int width, int height) {
    GLint framebuffer = 0;
    GLint alignment = 4;
    p_glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    p_glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
    if (framebuffer) p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    p_glPixelStorei(GL_PACK_ALIGNMENT, 1);
    p_glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, dst);
    p_glPixelStorei(GL_PACK_ALIGNMENT, alignment);
    if (framebuffer) p_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) framebuffer);
}

void server_frame_open(uint32_t width, uint32_t height) {
    if (!width || !height || access("/run/muos", W_OK) != 0) return;

    const int fd = open(MUGL_FRAME_PATH, O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) return;

    const size_t size = MUGL_FRAME_DATA + (size_t) width * height * 4U;
    void *map = MAP_FAILED;
    if (ftruncate(fd, (off_t) size) == 0) map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (map == MAP_FAILED) return;

    shared = map;
    shared_size = size;
    shared->server_pid = (uint32_t) getpid();
    shared->width = width;
    shared->height = height;
    __atomic_store_n(&shared->magic, MUGL_FRAME_MAGIC, __ATOMIC_RELEASE);
}

void server_frame_publish(void) {
    if (!shared) return;
    const uint32_t request = __atomic_load_n(&shared->request, __ATOMIC_ACQUIRE);
    if (request == shared->served) return;

    __atomic_add_fetch(&shared->seq, 1, __ATOMIC_ACQ_REL);
    server_read_frame((uint8_t *) shared + MUGL_FRAME_DATA, (int) shared->width, (int) shared->height);
    __atomic_add_fetch(&shared->seq, 1, __ATOMIC_ACQ_REL);
    __atomic_store_n(&shared->served, request, __ATOMIC_RELEASE);
}

void server_frame_close(void) {
    if (!shared) return;
    const int ours = shared->server_pid == (uint32_t) getpid();
    munmap(shared, shared_size);
    shared = NULL;
    if (ours) unlink(MUGL_FRAME_PATH);
}

void server_frame_forget(void) {
    if (shared && shared->server_pid == (uint32_t) getpid()) unlink(MUGL_FRAME_PATH);
}
