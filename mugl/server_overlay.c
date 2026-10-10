#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "server.h"
#include "server_gl.h"

#define OVERLAY_FLAG  "/run/muos/mugl-overlay"
#define FLAG_INTERVAL 30
#define GLYPH_W       5
#define GLYPH_H       7
#define CELL_W        6
#define CELL_H        8
#define SCALE         3
#define MARGIN        8
#define PADDING       6
#define MAX_TEXT      64
#define UPDATE_NS     500000000ULL

static const char glyph_chars[] = "0123456789. FPSRAMEGLCU%#";

static const uint8_t glyph_rows[][GLYPH_H] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}, {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}, {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}, {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}, {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F},
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}, {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}, {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03},
};

#define GLYPH_COUNT ((int) (sizeof(glyph_chars) - 1))
#define SOLID_GLYPH (GLYPH_COUNT - 1)

static const char *const vertex_source = "attribute vec2 pos; attribute vec2 uv; varying vec2 v_uv;"
                                         "void main() { v_uv = uv; gl_Position = vec4(pos, 0.0, 1.0); }";

static const char *const fragment_source =
    "precision mediump float; varying vec2 v_uv; uniform sampler2D atlas; uniform vec4 colour;"
    "void main() { gl_FragColor = vec4(colour.rgb, colour.a * texture2D(atlas, v_uv).a); }";

static PFNGLGENVERTEXARRAYSOESPROC gen_vao;
static PFNGLBINDVERTEXARRAYOESPROC bind_vao;
static int ready;
static int failed;
static GLuint program;
static GLuint atlas;
static GLuint vao;
static GLuint vbo;
static GLint colour_loc;

static int forced = -1;
static int flagged;
static unsigned flag_countdown;

static uint64_t window_start;
static uint64_t window_work;
static unsigned window_frames;
static unsigned long window_ticks;
static char text[MAX_TEXT] = "FPS";

static uint64_t monotonic_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t) t.tv_sec * 1000000000ULL + (uint64_t) t.tv_nsec;
}

int server_overlay_active(void) {
    if (forced == -1) forced = getenv("MUGL_OVERLAY") != NULL;
    if (forced) return 1;
    if (flag_countdown == 0) {
        flagged = access(OVERLAY_FLAG, F_OK) == 0;
        flag_countdown = FLAG_INTERVAL;
    }
    flag_countdown--;
    return flagged;
}

static GLuint compile(GLenum type, const char *source) {
    const GLuint shader = p_glCreateShader(type);
    p_glShaderSource(shader, 1, &source, NULL);
    p_glCompileShader(shader);
    GLint ok = 0;
    p_glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    return ok ? shader : 0;
}

static void build_atlas(void) {
    const int width = CELL_W * GLYPH_COUNT;
    uint8_t texels[CELL_H][CELL_W * GLYPH_COUNT][4];
    memset(texels, 0, sizeof(texels));

    for (int g = 0; g < GLYPH_COUNT; g++) {
        for (int y = 0; y < CELL_H; y++) {
            for (int x = 0; x < CELL_W; x++) {
                int on;
                if (g == SOLID_GLYPH)
                    on = 1;
                else
                    on = y < GLYPH_H && x < GLYPH_W && (glyph_rows[g][y] >> (GLYPH_W - 1 - x)) & 1;
                uint8_t *t = texels[y][g * CELL_W + x];
                t[0] = t[1] = t[2] = 255;
                t[3] = on ? 255 : 0;
            }
        }
    }

    GLint alignment = 4;
    p_glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    p_glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    p_glGenTextures(1, &atlas);
    p_glBindTexture(GL_TEXTURE_2D, atlas);
    p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    p_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    p_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, CELL_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels);
    p_glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
}

static int setup(void) {
    gen_vao = (PFNGLGENVERTEXARRAYSOESPROC) SDL_GL_GetProcAddress("glGenVertexArraysOES");
    bind_vao = (PFNGLBINDVERTEXARRAYOESPROC) SDL_GL_GetProcAddress("glBindVertexArrayOES");
    if (!gen_vao || !bind_vao) return 0;

    const GLuint vs = compile(GL_VERTEX_SHADER, vertex_source);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, fragment_source);
    if (!vs || !fs) return 0;

    program = p_glCreateProgram();
    p_glAttachShader(program, vs);
    p_glAttachShader(program, fs);
    p_glBindAttribLocation(program, 0, "pos");
    p_glBindAttribLocation(program, 1, "uv");
    p_glLinkProgram(program);
    p_glDeleteShader(vs);
    p_glDeleteShader(fs);
    GLint linked = 0;
    p_glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) return 0;

    p_glUseProgram(program);
    p_glUniform1i(p_glGetUniformLocation(program, "atlas"), 0);
    colour_loc = p_glGetUniformLocation(program, "colour");

    build_atlas();

    gen_vao(1, &vao);
    bind_vao(vao);
    p_glGenBuffers(1, &vbo);
    p_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    p_glEnableVertexAttribArray(0);
    p_glEnableVertexAttribArray(1);
    p_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (const void *) 0);
    p_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (const void *) 8);
    return 1;
}

static int glyph_index(char c) {
    const char *at = c ? strchr(glyph_chars, c) : NULL;
    return at ? (int) (at - glyph_chars) : (int) (strchr(glyph_chars, ' ') - glyph_chars);
}

static GLfloat *
push_quad(GLfloat *v, float x0, float y0, float x1, float y1, int glyph, uint32_t width, uint32_t height) {
    const float atlas_w = (float) (CELL_W * GLYPH_COUNT);
    const float u0 = (float) (glyph * CELL_W) / atlas_w;
    const float u1 = (float) (glyph * CELL_W + (glyph == SOLID_GLYPH ? CELL_W : GLYPH_W)) / atlas_w;
    const float v1 = glyph == SOLID_GLYPH ? 1.0f : (float) GLYPH_H / (float) CELL_H;

    const float nx0 = x0 / (float) width * 2.0f - 1.0f;
    const float nx1 = x1 / (float) width * 2.0f - 1.0f;
    const float ny0 = 1.0f - y0 / (float) height * 2.0f;
    const float ny1 = 1.0f - y1 / (float) height * 2.0f;

    const GLfloat quad[6][4] = {
        {nx0, ny0, u0, 0.0f}, {nx1, ny0, u1, 0.0f}, {nx1, ny1, u1, v1},
        {nx0, ny0, u0, 0.0f}, {nx1, ny1, u1, v1},   {nx0, ny1, u0, v1},
    };
    memcpy(v, quad, sizeof(quad));
    return v + 24;
}

static void update_text(uint64_t work_ns) {
    const uint64_t now = monotonic_ns();
    if (!window_start) {
        window_start = now;
        window_ticks = server_client_ticks();
    }
    window_frames++;
    window_work += work_ns;

    const uint64_t elapsed = now - window_start;
    if (elapsed < UPDATE_NS) return;

    const unsigned long client_ticks = server_client_ticks();
    const double seconds = (double) elapsed / 1e9;
    const double fps = (double) window_frames / seconds;
    const double frame_ms = fps > 0 ? 1000.0 / fps : 0.0;
    const double gl_ms = (double) window_work / (double) window_frames / 1e6;
    const long hz = sysconf(_SC_CLK_TCK);
    const double cpu = hz > 0 ? (double) (client_ticks - window_ticks) / (double) hz / seconds * 100.0 : 0.0;

    snprintf(text, sizeof(text), "FPS %.1f  FRAME %.1f  GL %.1f MS  CPU %.0f%%", fps, frame_ms, gl_ms, cpu);
    window_start = now;
    window_work = 0;
    window_frames = 0;
    window_ticks = client_ticks;
}

void server_overlay_draw(uint32_t width, uint32_t height, uint64_t work_ns) {
    if (failed || !width || !height) return;

    GLint saved_program = 0, saved_array = 0, saved_vao = 0, saved_active = 0, saved_texture = 0;
    GLint saved_framebuffer = 0, saved_viewport[4] = {0, 0, 0, 0};
    GLint saved_blend[4] = {0, 0, 0, 0}, saved_equation[2] = {0, 0};
    GLboolean saved_mask[4] = {1, 1, 1, 1};
    static const GLenum caps[] = {GL_BLEND, GL_DEPTH_TEST, GL_STENCIL_TEST, GL_SCISSOR_TEST, GL_CULL_FACE};
    GLboolean saved_caps[5];

    p_glGetIntegerv(GL_CURRENT_PROGRAM, &saved_program);
    p_glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &saved_array);
    p_glGetIntegerv(GL_VERTEX_ARRAY_BINDING_OES, &saved_vao);
    p_glGetIntegerv(GL_ACTIVE_TEXTURE, &saved_active);
    p_glGetIntegerv(GL_FRAMEBUFFER_BINDING, &saved_framebuffer);
    p_glGetIntegerv(GL_VIEWPORT, saved_viewport);
    p_glGetIntegerv(GL_BLEND_SRC_RGB, &saved_blend[0]);
    p_glGetIntegerv(GL_BLEND_DST_RGB, &saved_blend[1]);
    p_glGetIntegerv(GL_BLEND_SRC_ALPHA, &saved_blend[2]);
    p_glGetIntegerv(GL_BLEND_DST_ALPHA, &saved_blend[3]);
    p_glGetIntegerv(GL_BLEND_EQUATION_RGB, &saved_equation[0]);
    p_glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &saved_equation[1]);
    p_glGetBooleanv(GL_COLOR_WRITEMASK, saved_mask);
    for (int i = 0; i < 5; i++)
        saved_caps[i] = p_glIsEnabled(caps[i]);
    p_glActiveTexture(GL_TEXTURE0);
    p_glGetIntegerv(GL_TEXTURE_BINDING_2D, &saved_texture);

    if (!ready) {
        ready = 1;
        if (!setup()) {
            failed = 1;
            fprintf(stderr, "mugl-server: the performance overlay could not be set up\n");
        }
    }

    if (!failed) {
        update_text(work_ns);

        const size_t length = strlen(text);
        const float glyph_w = (float) (CELL_W * SCALE);
        const float x = (float) MARGIN;
        const float y = (float) MARGIN;
        const float box_w = (float) length * glyph_w - (float) SCALE + PADDING * 2.0f;
        const float box_h = (float) (GLYPH_H * SCALE) + PADDING * 2.0f;

        static GLfloat vertices[(MAX_TEXT + 1) * 24];
        GLfloat *v = push_quad(vertices, x, y, x + box_w, y + box_h, SOLID_GLYPH, width, height);
        const size_t first_glyph = (size_t) (v - vertices) / 4U;
        for (size_t i = 0; i < length; i++) {
            const float gx = x + PADDING + (float) i * glyph_w;
            const float gy = y + PADDING;
            v = push_quad(
                v, gx, gy, gx + (float) (GLYPH_W * SCALE), gy + (float) (GLYPH_H * SCALE), glyph_index(text[i]), width,
                height
            );
        }

        if (saved_framebuffer) p_glBindFramebuffer(GL_FRAMEBUFFER, 0);
        p_glViewport(0, 0, (GLsizei) width, (GLsizei) height);
        for (int i = 1; i < 5; i++)
            p_glDisable(caps[i]);
        p_glEnable(GL_BLEND);
        p_glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        p_glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        p_glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        p_glUseProgram(program);
        bind_vao(vao);
        p_glBindBuffer(GL_ARRAY_BUFFER, vbo);
        p_glBufferData(
            GL_ARRAY_BUFFER, (GLsizeiptr) ((size_t) (v - vertices) * sizeof(GLfloat)), vertices, GL_STREAM_DRAW
        );
        p_glBindTexture(GL_TEXTURE_2D, atlas);

        p_glUniform4f(colour_loc, 0.0f, 0.0f, 0.0f, 0.6f);
        p_glDrawArrays(GL_TRIANGLES, 0, (GLsizei) first_glyph);
        p_glUniform4f(colour_loc, 1.0f, 0.85f, 0.2f, 1.0f);
        p_glDrawArrays(GL_TRIANGLES, (GLint) first_glyph, (GLsizei) ((size_t) (v - vertices) / 4U - first_glyph));
    }

    if (bind_vao) bind_vao((GLuint) saved_vao);
    p_glBindBuffer(GL_ARRAY_BUFFER, (GLuint) saved_array);
    p_glBindTexture(GL_TEXTURE_2D, (GLuint) saved_texture);
    p_glActiveTexture((GLenum) saved_active);
    p_glUseProgram((GLuint) saved_program);
    if (saved_framebuffer) p_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint) saved_framebuffer);
    p_glViewport(saved_viewport[0], saved_viewport[1], saved_viewport[2], saved_viewport[3]);
    p_glBlendFuncSeparate(
        (GLenum) saved_blend[0], (GLenum) saved_blend[1], (GLenum) saved_blend[2], (GLenum) saved_blend[3]
    );
    p_glBlendEquationSeparate((GLenum) saved_equation[0], (GLenum) saved_equation[1]);
    p_glColorMask(saved_mask[0], saved_mask[1], saved_mask[2], saved_mask[3]);
    for (int i = 0; i < 5; i++) {
        if (saved_caps[i])
            p_glEnable(caps[i]);
        else
            p_glDisable(caps[i]);
    }
}
