#include <dlfcn.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GLES2/gl2.h>
#include "../../common/log.h"
#include "../common/common.h"
#include "../common/alpha.h"
#include "../common/anchor.h"
#include "../common/scale.h"
#include "../overlay/battery.h"
#include "../overlay/bright.h"
#include "../overlay/volume.h"
#include "../hook.h"
#include "gles.h"

static int fb_cached_w = 0;
static int fb_cached_h = 0;

static GLuint gles_prog = 0;

static GLint gles_a_pos = -1;
static GLint gles_a_uv = -1;
static GLint gles_u_tex = -1;
static GLint gles_u_alpha = -1;

static int prog_attempted = 0;
static int prog_ready = 0;

static GLuint overlay_tex = 0;
static int overlay_tex_w = 0;
static int overlay_tex_h = 0;
static int overlay_tex_attempted = 0;
static int overlay_tex_ready = 0;

static gl_vtx_t vtx_main[4];
static int vtx_main_valid = 0;

static SDL_GLContext last_ctx = NULL;

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

static void destroy_overlay_tex(void) {
    if (overlay_tex) {
        glDeleteTextures(1, &overlay_tex);
        overlay_tex = 0;
    }

    overlay_tex_w = 0;
    overlay_tex_h = 0;
    overlay_tex_ready = 0;
    overlay_tex_attempted = 0;

    vtx_main_valid = 0;
}

static void on_context_changed(void) {
    destroy_overlay_tex();
    destroy_overlay();

    battery_gles_attempted = 0;
    battery_gles_ready = 0;
    if (battery_gles_tex) {
        glDeleteTextures(1, &battery_gles_tex);
        battery_gles_tex = 0;
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
    if (prog_ready) return;
    if (prog_attempted) return;
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

static void upload_texture_rgba(SDL_Surface *rgba, GLuint *out_tex) {
    GLuint t = 0;

    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);

    *out_tex = t;
}

static void ensure_overlay_tex(SDL_Window *window) {
    if (overlay_tex_ready) return;
    if (overlay_tex_attempted) return;
    overlay_tex_attempted = 1;

    if (!window) return;

    if (!load_overlay_common(&GLES_RESOLVER, window, overlay_path)) return;

    SDL_Surface *raw = IMG_Load(overlay_path);
    if (!raw) {
        LOG_ERROR("stage", "GL overlay image load failed: %s", IMG_GetError());
        return;
    }

    SDL_Surface *rgba = raw;
    if (raw->format->format != SDL_PIXELFORMAT_RGBA32) {
        rgba = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(raw);
        raw = NULL;
        if (!rgba) {
            LOG_ERROR("stage", "SDL_ConvertSurfaceFormat failed: %s", SDL_GetError());
            return;
        }
    }

    overlay_tex_w = rgba->w;
    overlay_tex_h = rgba->h;

    upload_texture_rgba(rgba, &overlay_tex);
    if (!overlay_tex) {
        SDL_FreeSurface(rgba);
        return;
    }

    SDL_FreeSurface(rgba);

    overlay_tex_ready = 1;
    vtx_main_valid = 0;
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

static void update_geometry_caches(void) {
    int overlay_anchor = get_anchor_cached(&overlay_anchor_cache);
    if (overlay_anchor != overlay_anchor_cached) {
        overlay_anchor_cached = overlay_anchor;
        vtx_main_valid = 0;
    }

    int overlay_scale = get_scale_cached(&overlay_scale_cache);
    if (overlay_scale != overlay_scale_cached) {
        overlay_scale_cached = overlay_scale;
        vtx_main_valid = 0;
    }

    int battery_anchor = get_anchor_cached(&battery_anchor_cache);
    if (battery_anchor != battery_anchor_cached) {
        battery_anchor_cached = battery_anchor;
        vtx_battery_valid = 0;
    }

    int battery_scale = get_scale_cached(&battery_scale_cache);
    if (battery_scale != battery_scale_cached) {
        battery_scale_cached = battery_scale;
        vtx_battery_valid = 0;
    }

    int bright_anchor = get_anchor_cached(&bright_anchor_cache);
    if (bright_anchor != bright_anchor_cached) {
        bright_anchor_cached = bright_anchor;
        vtx_bright_valid = 0;
    }

    int bright_scale = get_scale_cached(&bright_scale_cache);
    if (bright_scale != bright_scale_cached) {
        bright_scale_cached = bright_scale;
        vtx_bright_valid = 0;
    }

    int volume_anchor = get_anchor_cached(&volume_anchor_cache);
    if (volume_anchor != volume_anchor_cached) {
        volume_anchor_cached = volume_anchor;
        vtx_volume_valid = 0;
    }

    int volume_scale = get_scale_cached(&volume_scale_cache);
    if (volume_scale != volume_scale_cached) {
        volume_scale_cached = volume_scale;
        vtx_volume_valid = 0;
    }
}

static void stage_draw(int fb_w, int fb_h) {
    ensure_program();

    // TODO: For future reference add all independent overlay layers here
    gl_battery_overlay_init();
    gl_bright_overlay_init();

    // Keep the general overlay separate!
    ensure_overlay_tex(render_window);

    update_geometry_caches();

    if (overlay_tex_ready && !vtx_main_valid) {
        build_quad_ndc(vtx_main, overlay_tex_w, overlay_tex_h, fb_w, fb_h,
                       overlay_anchor_cached, overlay_scale_cached);
        vtx_main_valid = 1;
    }

    if (battery_gles_ready && !vtx_battery_valid) {
        build_quad_ndc(vtx_battery, battery_gles_w, battery_gles_h, fb_w, fb_h,
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

    // Draw main overlay if present
    if (overlay_tex_ready && vtx_main_valid) {
        draw_quad(overlay_tex, vtx_main, get_alpha_cached(&overlay_alpha_cache));
    }

    // Draw battery overlay if activated
    if (battery_gles_ready && vtx_battery_valid) {
        draw_quad(battery_gles_tex, vtx_battery, get_alpha_cached(&battery_alpha_cache));
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

    render_window = window;
    ensure_context(window);

    // TODO: For future reference add the layer refresh states here
    // Like the audio and brightness when we get around to doing it
    // Update battery overlay status
    battery_overlay_update();
    bright_overlay_update();
    volume_overlay_update();

    int nw = 0;
    int nh = 0;
    SDL_GL_GetDrawableSize(window, &nw, &nh);

    if (nw != fb_cached_w || nh != fb_cached_h) {
        fb_cached_w = nw;
        fb_cached_h = nh;
        vtx_main_valid = 0;
        vtx_battery_valid = 0;
    }

    if (fb_cached_w > 0 && fb_cached_h > 0) stage_draw(fb_cached_w, fb_cached_h);
    real_SDL_GL_SwapWindow(window);
}
