#include <unistd.h>
#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#include "../../common/log.h"
#include "../common/common.h"
#include "../../common/inotify.h"
#include "../common/alpha.h"
#include "../common/anchor.h"
#include "../common/scale.h"
#include "../overlay/base.h"
#include "../overlay/battery.h"
#include "../overlay/bright.h"
#include "../overlay/volume.h"
#include "../hook.h"

static int fb_cached_w = 0;
static int fb_cached_h = 0;

static GLuint gles_prog = 0;

static GLint gles_a_pos = -1;
static GLint gles_a_uv = -1;
static GLint gles_u_tex = -1;
static GLint gles_u_alpha = -1;

static int prog_attempted = 0;
static int prog_ready = 0;

static SDL_GLContext last_ctx = NULL;

typedef struct {
    GLint program;
    GLint active_tex;
    GLint tex_binding;
    GLint viewport[4];

    GLboolean blend;
    GLint blend_src_rgb;
    GLint blend_dst_rgb;
    GLint blend_eq_rgb;
} gles_state_t;

static const char *vs_src =
        "attribute vec2 a_pos;"
        "attribute vec2 a_uv;"
        "varying vec2 v_uv;"
        "void main(){"
        "    gl_Position = vec4(a_pos, 0.0, 1.0);"
        "    v_uv = a_uv;"
        "}";

static const char *fs_src =
        "precision mediump float;"
        "uniform sampler2D u_tex;"
        "uniform float u_alpha;"
        "varying vec2 v_uv;"
        "void main(){"
        "    gl_FragColor = texture2D(u_tex, v_uv) * vec4(1.0, 1.0, 1.0, u_alpha);"
        "}";

static void build_quad_ndc(gl_vtx_t out[4], int tex_w, int tex_h, int fb_w, int fb_h, int anchor, int scale) {
    float draw_w = (float) tex_w;
    float draw_h = (float) tex_h;

    if (draw_w < 1.0f) draw_w = 1.0f;
    if (draw_h < 1.0f) draw_h = 1.0f;

    if (fb_w < 1) fb_w = 1;
    if (fb_h < 1) fb_h = 1;

    if (scale == SCALE_FIT) {
        const float sx = (float) fb_w / draw_w;
        const float sy = (float) fb_h / draw_h;
        const float s = (sx < sy) ? sx : sy;
        draw_w *= s;
        draw_h *= s;
    } else if (scale == SCALE_STRETCH) {
        draw_w = (float) fb_w;
        draw_h = (float) fb_h;
    }

    if (draw_w < 1.0f) draw_w = 1.0f;
    if (draw_h < 1.0f) draw_h = 1.0f;

    const float w_ndc = (draw_w * (2.0f / (float) fb_w));
    const float h_ndc = (draw_h * (2.0f / (float) fb_h));

    float x0, x1, y0, y1;

    switch (anchor) {
        case ANCHOR_TOP_LEFT:
            x0 = -1.0f;
            x1 = x0 + w_ndc;
            y0 = 1.0f;
            y1 = y0 - h_ndc;
            break;
        case ANCHOR_TOP_MIDDLE:
            x0 = -w_ndc * 0.5f;
            x1 = w_ndc * 0.5f;
            y0 = 1.0f;
            y1 = y0 - h_ndc;
            break;
        case ANCHOR_TOP_RIGHT:
            x1 = 1.0f;
            x0 = x1 - w_ndc;
            y0 = 1.0f;
            y1 = y0 - h_ndc;
            break;
        case ANCHOR_CENTRE_LEFT:
            x0 = -1.0f;
            x1 = x0 + w_ndc;
            y0 = h_ndc * 0.5f;
            y1 = -h_ndc * 0.5f;
            break;
        case ANCHOR_CENTRE_MIDDLE:
            x0 = -w_ndc * 0.5f;
            x1 = w_ndc * 0.5f;
            y0 = h_ndc * 0.5f;
            y1 = -h_ndc * 0.5f;
            break;
        case ANCHOR_CENTRE_RIGHT:
            x1 = 1.0f;
            x0 = x1 - w_ndc;
            y0 = h_ndc * 0.5f;
            y1 = -h_ndc * 0.5f;
            break;
        case ANCHOR_BOTTOM_LEFT:
            x0 = -1.0f;
            x1 = x0 + w_ndc;
            y1 = -1.0f;
            y0 = y1 + h_ndc;
            break;
        case ANCHOR_BOTTOM_MIDDLE:
            x0 = -w_ndc * 0.5f;
            x1 = w_ndc * 0.5f;
            y1 = -1.0f;
            y0 = y1 + h_ndc;
            break;
        case ANCHOR_BOTTOM_RIGHT:
            x1 = 1.0f;
            x0 = x1 - w_ndc;
            y1 = -1.0f;
            y0 = y1 + h_ndc;
            break;
        default:
            x0 = -w_ndc * 0.5f;
            x1 = w_ndc * 0.5f;
            y0 = h_ndc * 0.5f;
            y1 = -h_ndc * 0.5f;
            break;
    }

    out[0].x = x0;
    out[0].y = y0;
    out[0].u = 0.0f;
    out[0].v = 0.0f;
    out[1].x = x0;
    out[1].y = y1;
    out[1].u = 0.0f;
    out[1].v = 1.0f;
    out[2].x = x1;
    out[2].y = y0;
    out[2].u = 1.0f;
    out[2].v = 0.0f;
    out[3].x = x1;
    out[3].y = y1;
    out[3].u = 1.0f;
    out[3].v = 1.0f;
}

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, NULL);
    glCompileShader(sh);

    GLint ok = GL_FALSE;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);

    if (ok != GL_TRUE) {
        char log_buffer[512];
        GLsizei n = 0;
        glGetShaderInfoLog(sh, (GLsizei) sizeof(log_buffer), &n, log_buffer);
        LOG_ERROR("stage", "GL shader compile failed: %.*s", (int) n, log_buffer);
        glDeleteShader(sh);
        return 0;
    }

    return sh;
}

static GLuint link_program(GLuint vs, GLuint fs) {
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);

    GLint ok = GL_FALSE;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log_buffer[512];
        GLsizei n = 0;
        glGetProgramInfoLog(p, (GLsizei) sizeof(log_buffer), &n, log_buffer);
        LOG_ERROR("stage", "GL program link failed: %.*s", (int) n, log_buffer);
        glDeleteProgram(p);
        return 0;
    }

    return p;
}

static void save_gles_state(gles_state_t *st) {
    glGetIntegerv(GL_CURRENT_PROGRAM, &st->program);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &st->active_tex);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &st->tex_binding);
    glGetIntegerv(GL_VIEWPORT, st->viewport);

    st->blend = glIsEnabled(GL_BLEND);
    glGetIntegerv(GL_BLEND_SRC_RGB, &st->blend_src_rgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &st->blend_dst_rgb);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &st->blend_eq_rgb);
}

static void restore_gles_state(const gles_state_t *st) {
    if (st->blend) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);

    glBlendEquation(st->blend_eq_rgb);
    glBlendFunc(st->blend_src_rgb, st->blend_dst_rgb);

    glUseProgram(st->program);
    glActiveTexture(st->active_tex);
    glBindTexture(GL_TEXTURE_2D, (GLuint) st->tex_binding);
    glViewport(st->viewport[0], st->viewport[1], st->viewport[2], st->viewport[3]);
}

static void destroy_overlay(void) {
    if (gles_prog) {
        glDeleteProgram(gles_prog);
        gles_prog = 0;
    }

    gles_a_pos = -1;
    gles_a_uv = -1;
    gles_u_tex = -1;
    gles_u_alpha = -1;

    prog_ready = 0;
    prog_attempted = 0;
}

static void on_context_changed(void) {
    destroy_base_gles();
    destroy_overlay();

    base_nop_last = -1;
    vtx_base_valid = 0;

    battery_preload_gles_done = 0;
    battery_disabled_gles = 0;
    for (int i = 0; i < INDICATOR_STEPS; i++) {
        if (battery_gles_tex[i]) {
            glDeleteTextures(1, &battery_gles_tex[i]);
            battery_gles_tex[i] = 0;
        }
    }
    vtx_battery_valid = 0;

    bright_preload_gles_done = 0;
    bright_disabled_gles = 0;
    for (int i = 0; i < INDICATOR_STEPS; i++) {
        if (bright_gles_tex[i]) {
            glDeleteTextures(1, &bright_gles_tex[i]);
            bright_gles_tex[i] = 0;
        }
    }
    vtx_bright_valid = 0;

    volume_preload_gles_done = 0;
    volume_disabled_gles = 0;
    for (int i = 0; i < INDICATOR_STEPS; i++) {
        if (volume_gles_tex[i]) {
            glDeleteTextures(1, &volume_gles_tex[i]);
            volume_gles_tex[i] = 0;
        }
    }
    vtx_volume_valid = 0;
}

static void ensure_context(SDL_Window *window) {
    if (!window) return;

    SDL_GLContext ctx = SDL_GL_GetCurrentContext();
    if (!ctx) return;

    if (last_ctx != ctx) {
        last_ctx = ctx;
        on_context_changed();
    }
}

static void ensure_program(void) {
    if (prog_ready || prog_attempted) return;
    prog_attempted = 1;

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);

    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return;
    }

    gles_prog = link_program(vs, fs);

    glDeleteShader(vs);
    glDeleteShader(fs);

    if (!gles_prog) return;

    gles_a_pos = glGetAttribLocation(gles_prog, "a_pos");
    gles_a_uv = glGetAttribLocation(gles_prog, "a_uv");
    gles_u_tex = glGetUniformLocation(gles_prog, "u_tex");
    gles_u_alpha = glGetUniformLocation(gles_prog, "u_alpha");

    if (gles_a_pos < 0 || gles_a_uv < 0 || gles_u_tex < 0) {
        LOG_ERROR("stage", "GL locations missing (pos=%d uv=%d tex=%d)", gles_a_pos, gles_a_uv, gles_u_tex);
        destroy_overlay();
        return;
    }

    if (gles_u_alpha < 0) LOG_WARN("stage", "u_alpha uniform not found... alpha disabled!");

    prog_ready = 1;
}

static void draw_quad(GLuint tex, const gl_vtx_t vtx[4], float alpha) {
    if (!prog_ready) return;
    if (!tex) return;

    glUseProgram(gles_prog);

    glActiveTexture(GL_TEXTURE0);
    glUniform1i(gles_u_tex, 0);

    if (gles_u_alpha >= 0) glUniform1f(gles_u_alpha, alpha);

    glBindTexture(GL_TEXTURE_2D, tex);

    glEnableVertexAttribArray((GLuint) gles_a_pos);
    glEnableVertexAttribArray((GLuint) gles_a_uv);

    glVertexAttribPointer((GLuint) gles_a_pos, 2, GL_FLOAT, GL_FALSE, (GLsizei) sizeof(gl_vtx_t), &vtx[0].x);
    glVertexAttribPointer((GLuint) gles_a_uv, 2, GL_FLOAT, GL_FALSE, (GLsizei) sizeof(gl_vtx_t), &vtx[0].u);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray((GLuint) gles_a_pos);
    glDisableVertexAttribArray((GLuint) gles_a_uv);
}

#define UPDATE_GEOM_CACHE(LAYER, TYPE)                           \
    do {                                                         \
        int gc = get_##TYPE##_cached(&(LAYER##_##TYPE##_cache)); \
        if (gc != LAYER##_##TYPE##_cached) {                     \
            LAYER##_##TYPE##_cached = gc;                        \
            vtx_##LAYER##_valid = 0;                             \
        }                                                        \
    } while (0)

static void update_geometry_caches(void) {
    UPDATE_GEOM_CACHE(base, anchor);
    UPDATE_GEOM_CACHE(base, scale);

    UPDATE_GEOM_CACHE(battery, anchor);
    UPDATE_GEOM_CACHE(battery, scale);

    UPDATE_GEOM_CACHE(bright, anchor);
    UPDATE_GEOM_CACHE(bright, scale);

    UPDATE_GEOM_CACHE(volume, anchor);
    UPDATE_GEOM_CACHE(volume, scale);
}

static void stage_draw(int fb_w, int fb_h) {
    ensure_program();

    // TODO: For future reference add all independent overlay layers here
    gl_battery_overlay_init();

    const int base_disabled = ino_proc ? base_overlay_disabled_cached : (access(BASE_OVERLAY_NOP, F_OK) == 0);
    if (base_disabled != base_nop_last) {
        if (base_disabled) destroy_base_gles();

        vtx_base_valid = 0;
        base_nop_last = base_disabled;
    }

    // Only init base when truly enabled...
    if (!base_disabled) gl_base_overlay_init(render_window);

    update_geometry_caches();

    if (!base_disabled && base_gles_ready && !vtx_base_valid) {
        build_quad_ndc(vtx_base, base_gles_w, base_gles_h, fb_w, fb_h,
                       base_anchor_cached, base_scale_cached);
        vtx_base_valid = 1;
    }

    int battery_step = battery_last_step;
    if (battery_step >= 0 && battery_step < INDICATOR_STEPS && battery_gles_tex[battery_step] && !vtx_battery_valid) {
        build_quad_ndc(vtx_battery, battery_gles_w[battery_step], battery_gles_h[battery_step], fb_w, fb_h,
                       battery_anchor_cached, battery_scale_cached);
        vtx_battery_valid = 1;
    }

    if (bright_is_visible()) {
        int step = bright_last_step;
        if (step >= 0 && step < INDICATOR_STEPS && bright_gles_tex[step] && !vtx_bright_valid) {
            build_quad_ndc(vtx_bright, bright_gles_w[step], bright_gles_h[step],
                           fb_w, fb_h, bright_anchor_cached, bright_scale_cached);
            vtx_bright_valid = 1;
        }
    }

    if (volume_is_visible()) {
        int step = volume_last_step;
        if (step >= 0 && step < INDICATOR_STEPS && volume_gles_tex[step] && !vtx_volume_valid) {
            build_quad_ndc(vtx_volume, volume_gles_w[step], volume_gles_h[step],
                           fb_w, fb_h, volume_anchor_cached, volume_scale_cached);
            vtx_volume_valid = 1;
        }
    }

    gles_state_t st;
    save_gles_state(&st);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glViewport(0, 0, fb_w, fb_h);

    // Draw base overlay if present (and not disabled)
    if (!base_disabled && base_gles_ready && vtx_base_valid) {
        draw_quad(base_gles_tex, vtx_base, get_alpha_cached(&overlay_alpha_cache));
    }

    // Draw battery overlay if activated
    if (battery_step >= 0 && battery_step < INDICATOR_STEPS && battery_gles_tex[battery_step] && vtx_battery_valid) {
        draw_quad(battery_gles_tex[battery_step], vtx_battery, get_alpha_cached(&battery_alpha_cache));
    }

    if (bright_is_visible()) {
        gl_bright_overlay_init();

        int step = bright_last_step;
        if (step >= 0 && step < INDICATOR_STEPS && bright_gles_tex[step] && vtx_bright_valid) {
            draw_quad(bright_gles_tex[step], vtx_bright, get_alpha_cached(&bright_alpha_cache));
        }
    }

    if (volume_is_visible()) {
        gl_volume_overlay_init();

        int step = volume_last_step;
        if (step >= 0 && step < INDICATOR_STEPS && volume_gles_tex[step] && vtx_volume_valid) {
            draw_quad(volume_gles_tex[step], vtx_volume, get_alpha_cached(&volume_alpha_cache));
        }
    }

    restore_gles_state(&st);
}

void SDL_GL_SwapWindow(SDL_Window *window) {
    if (is_overlay_disabled()) {
        if (real_SDL_GL_SwapWindow) real_SDL_GL_SwapWindow(window);
        return;
    }

    if (!real_SDL_GL_SwapWindow) return;

    base_inotify_check();
    if (ino_proc) inotify_check(ino_proc);

    render_window = window;
    ensure_context(window);

    battery_overlay_update();
    bright_overlay_update();
    volume_overlay_update();

    int nw = 0;
    int nh = 0;
    SDL_GL_GetDrawableSize(window, &nw, &nh);

    if (nw != fb_cached_w || nh != fb_cached_h) {
        fb_cached_w = nw;
        fb_cached_h = nh;
        vtx_base_valid = 0;
        vtx_battery_valid = 0;
        vtx_bright_valid = 0;
        vtx_volume_valid = 0;
    }

    if (fb_cached_w > 0 && fb_cached_h > 0) stage_draw(fb_cached_w, fb_cached_h);
    real_SDL_GL_SwapWindow(window);
}
