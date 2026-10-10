#pragma once

#include <stdint.h>

#define MUGL_MAGIC     0x4C47554DU
#define MUGL_VERSION   2U
#define MUGL_RING_SIZE (16U * 1024U * 1024U)
#define MUGL_RESP_SIZE (8U * 1024U * 1024U)
#define MUGL_HEADER    4096U
#define MUGL_SHM_SIZE  (MUGL_HEADER + MUGL_RESP_SIZE + MUGL_RING_SIZE)
#define MUGL_SHM_FD    3
#define MUGL_STR_SIZE  2048U

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t ready;
    uint32_t failed;
    uint32_t width;
    uint32_t height;
    uint32_t server_pid;
    uint32_t client_pid;
    volatile uint32_t wpos;
    volatile uint32_t rpos;
    volatile uint32_t reader_sleeping;
    volatile uint32_t writer_sleeping;
    volatile uint32_t resp_seq;
    volatile uint32_t resp_len;
    volatile uint32_t swap_seq;
    volatile uint32_t server_exited;
    char error[256];
} mugl_header;

typedef struct {
    uint32_t op;
    uint32_t len;
} mugl_msg;

#define MUGL_HDR(base)  ((mugl_header *) (base))
#define MUGL_RESP(base) ((uint8_t *) (base) + MUGL_HEADER)
#define MUGL_RING(base) ((uint8_t *) (base) + MUGL_HEADER + MUGL_RESP_SIZE)

enum {
    MUGL_OP_HELLO = 1,
    MUGL_OP_SWAP,
    MUGL_OP_SWAP_INTERVAL,
    MUGL_OP_BYE,
    MUGL_OP_SYNC,
    MUGL_OP_GET_STRING,
    MUGL_OP_GET_V,
    MUGL_OP_GET_OBJ_V,
    MUGL_OP_GEN,
    MUGL_OP_DELETE,
    MUGL_OP_BUFFER_DATA,
    MUGL_OP_BUFFER_SUB_DATA,
    MUGL_OP_TEX_IMAGE_2D,
    MUGL_OP_TEX_SUB_IMAGE_2D,
    MUGL_OP_COMPRESSED_TEX_IMAGE_2D,
    MUGL_OP_COMPRESSED_TEX_SUB_IMAGE_2D,
    MUGL_OP_READ_PIXELS,
    MUGL_OP_SHADER_SOURCE,
    MUGL_OP_GET_SHADER_SOURCE,
    MUGL_OP_GET_INFO_LOG,
    MUGL_OP_GET_ACTIVE,
    MUGL_OP_GET_ATTACHED_SHADERS,
    MUGL_OP_BIND_ATTRIB_LOCATION,
    MUGL_OP_GET_LOCATION,
    MUGL_OP_UNIFORM_V,
    MUGL_OP_UNIFORM_MATRIX_V,
    MUGL_OP_VERTEX_ATTRIB_V,
    MUGL_OP_VERTEX_ATTRIB_POINTER,
    MUGL_OP_DRAW,
    MUGL_OP_PROGRAM_INFO,
    MUGL_OP_SHADER_PRECISION,
    MUGL_OP_SHADER_BINARY,
    MUGL_OP_DISCARD_FRAMEBUFFER,
    MUGL_OP_GEN_VERTEX_ARRAYS,
    MUGL_OP_DELETE_VERTEX_ARRAYS,
    MUGL_OP_BIND_VERTEX_ARRAY,
    MUGL_OP_IS_VERTEX_ARRAY,
    MUGL_OP_GENERATED_BASE = 256
};

#define MUGL_MAX_ATTRIBS  16U
#define MUGL_DRAW_WORDS   10U
#define MUGL_ATTRIB_WORDS 7U

enum { MUGL_DRAW_ELEMENTS = 1, MUGL_DRAW_INDEX_BUFFER = 2 };

enum { MUGL_GEN_BUFFERS = 1, MUGL_GEN_TEXTURES, MUGL_GEN_FRAMEBUFFERS, MUGL_GEN_RENDERBUFFERS };

enum { MUGL_GET_BOOLEAN = 1, MUGL_GET_INTEGER, MUGL_GET_FLOAT };

enum {
    MUGL_OBJ_BUFFER_PARAM = 1,
    MUGL_OBJ_FRAMEBUFFER_ATTACHMENT,
    MUGL_OBJ_PROGRAM,
    MUGL_OBJ_RENDERBUFFER,
    MUGL_OBJ_SHADER,
    MUGL_OBJ_TEX_PARAM_F,
    MUGL_OBJ_TEX_PARAM_I,
    MUGL_OBJ_UNIFORM_F,
    MUGL_OBJ_UNIFORM_I,
    MUGL_OBJ_VERTEX_ATTRIB_F,
    MUGL_OBJ_VERTEX_ATTRIB_I
};

enum { MUGL_LOG_SHADER = 1, MUGL_LOG_PROGRAM };

enum { MUGL_ACTIVE_ATTRIB = 1, MUGL_ACTIVE_UNIFORM };

enum { MUGL_LOC_ATTRIB = 1, MUGL_LOC_UNIFORM };
