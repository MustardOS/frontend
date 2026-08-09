#pragma once

#include <GLES2/gl2.h>

typedef struct gl_dispatch {
    GLuint(GL_APIENTRY *CreateShader)(GLenum type);
    void(GL_APIENTRY *ShaderSource)(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length);
    void(GL_APIENTRY *CompileShader)(GLuint shader);
    void(GL_APIENTRY *GetShaderiv)(GLuint shader, GLenum pname, GLint *params);
    void(GL_APIENTRY *GetShaderInfoLog)(GLuint shader, GLsizei size, GLsizei *length, GLchar *log);
    void(GL_APIENTRY *DeleteShader)(GLuint shader);
    GLuint(GL_APIENTRY *CreateProgram)(void);
    void(GL_APIENTRY *AttachShader)(GLuint program, GLuint shader);
    void(GL_APIENTRY *LinkProgram)(GLuint program);
    void(GL_APIENTRY *DeleteProgram)(GLuint program);
    void(GL_APIENTRY *GetProgramiv)(GLuint program, GLenum pname, GLint *params);
    void(GL_APIENTRY *GetProgramInfoLog)(GLuint program, GLsizei size, GLsizei *length, GLchar *log);
    GLint(GL_APIENTRY *GetAttribLocation)(GLuint program, const GLchar *name);
    GLint(GL_APIENTRY *GetUniformLocation)(GLuint program, const GLchar *name);
    void(GL_APIENTRY *UseProgram)(GLuint program);
    void(GL_APIENTRY *VertexAttribPointer)(
        GLuint index, GLint size, GLenum type, GLboolean normalised, GLsizei stride, const void *pointer
    );
    void(GL_APIENTRY *EnableVertexAttribArray)(GLuint index);
    void(GL_APIENTRY *DisableVertexAttribArray)(GLuint index);
    void(GL_APIENTRY *Uniform1i)(GLint location, GLint value);
    void(GL_APIENTRY *Uniform1f)(GLint location, GLfloat value);
    void(GL_APIENTRY *Uniform2f)(GLint location, GLfloat x, GLfloat y);
    void(GL_APIENTRY *UniformMatrix3fv)(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void(GL_APIENTRY *DrawArrays)(GLenum mode, GLint first, GLsizei count);
    void(GL_APIENTRY *BindBuffer)(GLenum target, GLuint buffer);
    void(GL_APIENTRY *ActiveTexture)(GLenum texture);
    void(GL_APIENTRY *BindAttribLocation)(GLuint program, GLuint index, const GLchar *name);
    void(GL_APIENTRY *Enable)(GLenum capability);
    void(GL_APIENTRY *Disable)(GLenum capability);
    void(GL_APIENTRY *Viewport)(GLint x, GLint y, GLsizei width, GLsizei height);
    void(GL_APIENTRY *ClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
    void(GL_APIENTRY *Clear)(GLbitfield mask);
    void(GL_APIENTRY *ColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
    void(GL_APIENTRY *DepthMask)(GLboolean enabled);
    void(GL_APIENTRY *GetIntegerv)(GLenum name, GLint *values);
    void(GL_APIENTRY *GetBooleanv)(GLenum name, GLboolean *values);
    void(GL_APIENTRY *GetFloatv)(GLenum name, GLfloat *values);
    GLboolean(GL_APIENTRY *IsEnabled)(GLenum capability);
    void(GL_APIENTRY *GetVertexAttribiv)(GLuint index, GLenum name, GLint *values);
    void(GL_APIENTRY *BlendFuncSeparate)(GLenum src_rgb, GLenum dst_rgb, GLenum src_alpha, GLenum dst_alpha);
    void(GL_APIENTRY *BlendEquationSeparate)(GLenum rgb, GLenum alpha);
    void(GL_APIENTRY *FrontFace)(GLenum mode);
    void(GL_APIENTRY *CullFace)(GLenum mode);
    void(GL_APIENTRY *PixelStorei)(GLenum name, GLint value);
    void(GL_APIENTRY *Scissor)(GLint x, GLint y, GLsizei width, GLsizei height);
    void(GL_APIENTRY *GenFramebuffers)(GLsizei count, GLuint *framebuffers);
    void(GL_APIENTRY *DeleteFramebuffers)(GLsizei count, const GLuint *framebuffers);
    void(GL_APIENTRY *BindFramebuffer)(GLenum target, GLuint framebuffer);
    void(GL_APIENTRY *FramebufferTexture2D)(
        GLenum target, GLenum attachment, GLenum texture_target, GLuint texture, GLint level
    );
    void(GL_APIENTRY *FramebufferRenderbuffer)(
        GLenum target, GLenum attachment, GLenum renderbuffer_target, GLuint renderbuffer
    );
    GLenum(GL_APIENTRY *CheckFramebufferStatus)(GLenum target);
    void(GL_APIENTRY *GenRenderbuffers)(GLsizei count, GLuint *renderbuffers);
    void(GL_APIENTRY *DeleteRenderbuffers)(GLsizei count, const GLuint *renderbuffers);
    void(GL_APIENTRY *BindRenderbuffer)(GLenum target, GLuint renderbuffer);
    void(GL_APIENTRY *RenderbufferStorage)(GLenum target, GLenum format, GLsizei width, GLsizei height);
    void(GL_APIENTRY *GenTextures)(GLsizei count, GLuint *textures);
    void(GL_APIENTRY *DeleteTextures)(GLsizei count, const GLuint *textures);
    void(GL_APIENTRY *BindTexture)(GLenum target, GLuint texture);
    void(GL_APIENTRY *TexImage2D)(
        GLenum target, GLint level, GLint internal_format, GLsizei width, GLsizei height, GLint border, GLenum format,
        GLenum type, const void *pixels
    );
    void(GL_APIENTRY *TexParameteri)(GLenum target, GLenum name, GLint value);
    GLboolean(GL_APIENTRY *IsTexture)(GLuint texture);
    const GLubyte *(GL_APIENTRY *GetString)(GLenum name);
    void(GL_APIENTRY *Flush)(void);
    void(GL_APIENTRY *BindVertexArray)(GLuint array);
    void(GL_APIENTRY *BindSampler)(GLuint unit, GLuint sampler);
    void(GL_APIENTRY *BindTransformFeedback)(GLenum target, GLuint id);
} gl_dispatch_t;

typedef enum gl_dispatch_capability {
    gl_dispatch_colour = 1 << 0,
    gl_dispatch_hardware = 1 << 1
} gl_dispatch_capability;

const gl_dispatch_t *gl_dispatch_acquire(const char *consumer, unsigned capabilities);
