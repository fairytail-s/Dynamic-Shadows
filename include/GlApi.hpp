#pragma once

#include <cstdint>

namespace shadow::glapi {

using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLfloat = float;
using GLchar = char;

constexpr GLenum GL_READ_FRAMEBUFFER = 0x8CA8, GL_DRAW_FRAMEBUFFER = 0x8CA9;
constexpr GLenum GL_FRAMEBUFFER = 0x8D40, GL_DRAW_FRAMEBUFFER_BINDING = 0x8CA6, GL_READ_FRAMEBUFFER_BINDING = 0x8CAA, GL_RENDERBUFFER = 0x8D41;
constexpr GLenum GL_RENDERBUFFER_BINDING = 0x8CA7, GL_RENDERBUFFER_WIDTH = 0x8D42, GL_RENDERBUFFER_HEIGHT = 0x8D43;
constexpr GLenum GL_RENDERBUFFER_INTERNAL_FORMAT = 0x8D44, GL_RENDERBUFFER_SAMPLES = 0x8CAB;
constexpr GLenum GL_DEPTH_ATTACHMENT = 0x8D00, GL_DEPTH_STENCIL_ATTACHMENT = 0x821A;
constexpr GLenum GL_FRAMEBUFFER_COMPLETE = 0x8CD5;

constexpr GLenum GL_TEXTURE_2D = 0x0DE1, GL_TEXTURE0 = 0x84C0, GL_ACTIVE_TEXTURE = 0x84E0, GL_TEXTURE_BINDING_2D = 0x8069;
constexpr GLenum GL_SAMPLER_BINDING = 0x8919;
constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801, GL_TEXTURE_MAG_FILTER = 0x2800, GL_TEXTURE_WRAP_S = 0x2802, GL_TEXTURE_WRAP_T = 0x2803;
constexpr GLint GL_NEAREST = 0x2600, GL_LINEAR = 0x2601, GL_CLAMP_TO_EDGE = 0x812F;
constexpr GLenum GL_TEXTURE_WIDTH = 0x1000, GL_TEXTURE_HEIGHT = 0x1001, GL_TEXTURE_INTERNAL_FORMAT = 0x1003;

constexpr GLenum GL_DEPTH24_STENCIL8 = 0x88F0, GL_DEPTH32F_STENCIL8 = 0x8CAD;
constexpr GLbitfield GL_DEPTH_BUFFER_BIT = 0x0100, GL_COLOR_BUFFER_BIT = 0x4000;
constexpr GLenum GL_COLOR_CLEAR_VALUE = 0x0C22, GL_EXTENSIONS = 0x1F03;

constexpr GLenum GL_SCISSOR_TEST = 0x0C11, GL_DEPTH_TEST = 0x0B71, GL_BLEND = 0x0BE2, GL_CULL_FACE = 0x0B44, GL_STENCIL_TEST = 0x0B90;
constexpr GLenum GL_DITHER = 0x0BD0, GL_RASTERIZER_DISCARD = 0x8C89, GL_DEPTH_WRITEMASK = 0x0B72, GL_COLOR_WRITEMASK = 0x0C23;
constexpr GLenum GL_VIEWPORT = 0x0BA2, GL_CURRENT_PROGRAM = 0x8B8D, GL_VERTEX_ARRAY_BINDING = 0x85B5;

constexpr GLenum GL_TRIANGLES = 0x0004, GL_VERTEX_SHADER = 0x8B31, GL_FRAGMENT_SHADER = 0x8B30;
constexpr GLenum GL_COMPILE_STATUS = 0x8B81, GL_LINK_STATUS = 0x8B82;

constexpr GLenum GL_TEXTURE_COMPARE_MODE = 0x884C, GL_TEXTURE_COMPARE_FUNC = 0x884D, GL_COMPARE_REF_TO_TEXTURE = 0x884E, GL_LEQUAL = 0x0203;
constexpr GLenum GL_DEPTH_RANGE = 0x0B70, GL_GREATER = 0x0204, GL_DEPTH_FUNC = 0x0B74, GL_LESS = 0x0201, GL_DEPTH_CLEAR_VALUE = 0x0B73, GL_DEPTH_COMPONENT24 = 0x81A6;

constexpr GLenum GL_BACK = 0x0405, GL_DEPTH = 0x1802, GL_STENCIL = 0x1801, GL_SAMPLES = 0x80A9;

constexpr GLenum GL_BLEND_SRC_RGB = 0x80C9, GL_BLEND_DST_RGB = 0x80C8, GL_BLEND_SRC_ALPHA = 0x80CB, GL_BLEND_DST_ALPHA = 0x80CA;
constexpr GLenum GL_BLEND_EQUATION_RGB = 0x8009, GL_BLEND_EQUATION_ALPHA = 0x883D;
constexpr GLenum GL_SRC_ALPHA = 0x0302, GL_ONE_MINUS_SRC_ALPHA = 0x0303, GL_ZERO = 0, GL_ONE = 1, GL_FUNC_ADD = 0x8006;
constexpr GLenum GL_ATTACHMENT_RED_SIZE = 0x8212, GL_ATTACHMENT_ALPHA_SIZE = 0x8215, GL_ATTACHMENT_DEPTH_SIZE = 0x8216,
                 GL_ATTACHMENT_STENCIL_SIZE = 0x8217;

struct Gl {
    void (*BindFramebuffer)(GLenum, GLuint);
    void (*FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
    void (*DrawElements)(GLenum, GLsizei, GLenum, const void*);
    void (*DrawArrays)(GLenum, GLint, GLsizei);
    void (*DrawElementsInstanced)(GLenum, GLsizei, GLenum, const void*, GLsizei);
    void (*DrawArraysInstanced)(GLenum, GLint, GLsizei, GLsizei);
    void (*GenTextures)(GLsizei, GLuint*);
    void (*DeleteTextures)(GLsizei, const GLuint*);
    void (*BindTexture)(GLenum, GLuint);
    void (*TexParameteri)(GLenum, GLenum, GLint);
    void (*TexStorage2D)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
    void (*ActiveTexture)(GLenum);
    void (*GenFramebuffers)(GLsizei, GLuint*);
    void (*BlitFramebuffer)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
    GLenum (*CheckFramebufferStatus)(GLenum);
    void (*GenVertexArrays)(GLsizei, GLuint*);
    void (*BindVertexArray)(GLuint);
    GLuint (*CreateShader)(GLenum);
    void (*ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
    void (*CompileShader)(GLuint);
    void (*GetShaderiv)(GLuint, GLenum, GLint*);
    GLuint (*CreateProgram)();
    void (*AttachShader)(GLuint, GLuint);
    void (*LinkProgram)(GLuint);
    void (*GetProgramiv)(GLuint, GLenum, GLint*);
    void (*DeleteProgram)(GLuint);
    void (*DeleteShader)(GLuint);
    GLint (*GetUniformLocation)(GLuint, const GLchar*);
    void (*UseProgram)(GLuint);
    void (*Uniform1i)(GLint, GLint);
    void (*Uniform1f)(GLint, GLfloat);
    void (*ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
    const unsigned char* (*GetString)(GLenum);
    void (*Uniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat);
    void (*GetIntegerv)(GLenum, GLint*);
    void (*GetBooleanv)(GLenum, GLboolean*);
    GLboolean (*IsEnabled)(GLenum);
    void (*Enable)(GLenum);
    void (*Disable)(GLenum);
    void (*DepthMask)(GLboolean);
    void (*ColorMask)(GLboolean, GLboolean, GLboolean, GLboolean);
    void (*Viewport)(GLint, GLint, GLsizei, GLsizei);
    GLenum (*GetError)();
    void (*BindSampler)(GLuint, GLuint);
    void (*GenSamplers)(GLsizei, GLuint*);
    void (*SamplerParameteri)(GLuint, GLenum, GLint);
    void (*GetTexLevelParameteriv)(GLenum, GLint, GLenum, GLint*);
    void (*GetRenderbufferParameteriv)(GLenum, GLenum, GLint*);
    void (*Clear)(GLbitfield);
    void (*UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
    void (*UniformMatrix3fv)(GLint, GLsizei, GLboolean, const GLfloat*);
    void (*BlendFuncSeparate)(GLenum, GLenum, GLenum, GLenum);
    void (*BlendEquationSeparate)(GLenum, GLenum);
    void (*DrawBuffers)(GLsizei, const GLenum*);
    void (*BindRenderbuffer)(GLenum, GLuint);
    void (*GetFramebufferAttachmentParameteriv)(GLenum, GLenum, GLenum, GLint*);
    void (*ClearDepthf)(GLfloat);
    void (*GetFloatv)(GLenum, GLfloat*);
    void (*DepthFunc)(GLenum);
    void (*DepthRangef)(GLfloat, GLfloat);
};
inline Gl real{};

}
