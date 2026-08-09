#include <string.h>
#include <SDL2/SDL.h>
#include "../../common/function_pointer.h"
#include "../../common/log.h"
#include "gl_dispatch.h"

static gl_dispatch_t dispatch;
static SDL_GLContext dispatch_context;
static unsigned dispatch_capabilities;

const gl_dispatch_t *gl_dispatch_acquire(const char *consumer, const unsigned capabilities) {
    const SDL_GLContext context = SDL_GL_GetCurrentContext();
    if (context != dispatch_context) {
        memset(&dispatch, 0, sizeof(dispatch));
        dispatch_context = context;
        dispatch_capabilities = 0;
    }
    if ((dispatch_capabilities & capabilities) == capabilities) return &dispatch;

#define LOAD_REQUIRED(NAME)                                                                                            \
    do {                                                                                                               \
        if (!dispatch.NAME) MUOS_FUNCTION_ASSIGN(dispatch.NAME, SDL_GL_GetProcAddress("gl" #NAME));                  \
        if (!dispatch.NAME) {                                                                                          \
            LOG_ERROR("muxretro", "%s: failed to resolve GL function gl%s", consumer, #NAME);                       \
            return NULL;                                                                                               \
        }                                                                                                              \
    } while (0)

    if ((capabilities & gl_dispatch_colour) && !(dispatch_capabilities & gl_dispatch_colour)) {
        LOAD_REQUIRED(CreateShader);
        LOAD_REQUIRED(ShaderSource);
        LOAD_REQUIRED(CompileShader);
        LOAD_REQUIRED(GetShaderiv);
        LOAD_REQUIRED(GetShaderInfoLog);
        LOAD_REQUIRED(DeleteShader);
        LOAD_REQUIRED(CreateProgram);
        LOAD_REQUIRED(AttachShader);
        LOAD_REQUIRED(LinkProgram);
        LOAD_REQUIRED(DeleteProgram);
        LOAD_REQUIRED(GetProgramiv);
        LOAD_REQUIRED(GetProgramInfoLog);
        LOAD_REQUIRED(GetAttribLocation);
        LOAD_REQUIRED(GetUniformLocation);
        LOAD_REQUIRED(UseProgram);
        LOAD_REQUIRED(VertexAttribPointer);
        LOAD_REQUIRED(EnableVertexAttribArray);
        LOAD_REQUIRED(DisableVertexAttribArray);
        LOAD_REQUIRED(Uniform1i);
        LOAD_REQUIRED(Uniform1f);
        LOAD_REQUIRED(Uniform2f);
        LOAD_REQUIRED(UniformMatrix3fv);
        LOAD_REQUIRED(DrawArrays);
        LOAD_REQUIRED(BindBuffer);
        LOAD_REQUIRED(ActiveTexture);
        LOAD_REQUIRED(BindAttribLocation);
        LOAD_REQUIRED(Enable);
        LOAD_REQUIRED(Disable);
        LOAD_REQUIRED(Viewport);
        LOAD_REQUIRED(GetIntegerv);
        LOAD_REQUIRED(IsEnabled);
        dispatch_capabilities |= gl_dispatch_colour;
    }

    if ((capabilities & gl_dispatch_hardware) && !(dispatch_capabilities & gl_dispatch_hardware)) {
        LOAD_REQUIRED(CreateShader);
        LOAD_REQUIRED(ShaderSource);
        LOAD_REQUIRED(CompileShader);
        LOAD_REQUIRED(GetShaderiv);
        LOAD_REQUIRED(GetShaderInfoLog);
        LOAD_REQUIRED(DeleteShader);
        LOAD_REQUIRED(CreateProgram);
        LOAD_REQUIRED(AttachShader);
        LOAD_REQUIRED(LinkProgram);
        LOAD_REQUIRED(DeleteProgram);
        LOAD_REQUIRED(GetProgramiv);
        LOAD_REQUIRED(GetProgramInfoLog);
        LOAD_REQUIRED(GetAttribLocation);
        LOAD_REQUIRED(GetUniformLocation);
        LOAD_REQUIRED(UseProgram);
        LOAD_REQUIRED(VertexAttribPointer);
        LOAD_REQUIRED(EnableVertexAttribArray);
        LOAD_REQUIRED(DisableVertexAttribArray);
        LOAD_REQUIRED(Uniform1i);
        LOAD_REQUIRED(DrawArrays);
        LOAD_REQUIRED(BindBuffer);
        LOAD_REQUIRED(ActiveTexture);
        LOAD_REQUIRED(Enable);
        LOAD_REQUIRED(Disable);
        LOAD_REQUIRED(Viewport);
        LOAD_REQUIRED(ClearColor);
        LOAD_REQUIRED(Clear);
        LOAD_REQUIRED(ColorMask);
        LOAD_REQUIRED(DepthMask);
        LOAD_REQUIRED(GetIntegerv);
        LOAD_REQUIRED(GetBooleanv);
        LOAD_REQUIRED(GetFloatv);
        LOAD_REQUIRED(IsEnabled);
        LOAD_REQUIRED(GetVertexAttribiv);
        LOAD_REQUIRED(BlendFuncSeparate);
        LOAD_REQUIRED(BlendEquationSeparate);
        LOAD_REQUIRED(FrontFace);
        LOAD_REQUIRED(CullFace);
        LOAD_REQUIRED(PixelStorei);
        LOAD_REQUIRED(Scissor);
        LOAD_REQUIRED(GenFramebuffers);
        LOAD_REQUIRED(DeleteFramebuffers);
        LOAD_REQUIRED(BindFramebuffer);
        LOAD_REQUIRED(FramebufferTexture2D);
        LOAD_REQUIRED(FramebufferRenderbuffer);
        LOAD_REQUIRED(CheckFramebufferStatus);
        LOAD_REQUIRED(GenRenderbuffers);
        LOAD_REQUIRED(DeleteRenderbuffers);
        LOAD_REQUIRED(BindRenderbuffer);
        LOAD_REQUIRED(RenderbufferStorage);
        LOAD_REQUIRED(GenTextures);
        LOAD_REQUIRED(DeleteTextures);
        LOAD_REQUIRED(BindTexture);
        LOAD_REQUIRED(TexImage2D);
        LOAD_REQUIRED(TexParameteri);
        LOAD_REQUIRED(IsTexture);
        LOAD_REQUIRED(GetString);
        LOAD_REQUIRED(Flush);
        dispatch_capabilities |= gl_dispatch_hardware;
    }

#undef LOAD_REQUIRED

#define LOAD_OPTIONAL(NAME)                                                                                            \
    do {                                                                                                               \
        if (!dispatch.NAME) MUOS_FUNCTION_ASSIGN(dispatch.NAME, SDL_GL_GetProcAddress("gl" #NAME));                  \
    } while (0)
    if (capabilities & gl_dispatch_hardware) {
        LOAD_OPTIONAL(BindVertexArray);
        LOAD_OPTIONAL(BindSampler);
        LOAD_OPTIONAL(BindTransformFeedback);
    }
#undef LOAD_OPTIONAL

    return &dispatch;
}
