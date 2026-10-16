#include "DrawBatch.hpp"
#include "Config.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <dlfcn.h>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace shadow::batch {
namespace {
void drainErrors() { for (unsigned i = 0; i < 16 && real.GetError() != 0; ++i) {} }
using GLintptr = std::intptr_t;
using GLsizeiptr = std::intptr_t;
void (*depthClearObserver)() = nullptr;
struct Api {
    void (*VertexAttrib1f)(GLuint index, GLfloat v0){};
    void (*VertexAttrib1fv)(GLuint index, const GLfloat* values){};
    void (*VertexAttrib2f)(GLuint index, GLfloat v0, GLfloat v1){};
    void (*VertexAttrib2fv)(GLuint index, const GLfloat* values){};
    void (*VertexAttrib3f)(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2){};
    void (*VertexAttrib3fv)(GLuint index, const GLfloat* values){};
    void (*VertexAttrib4f)(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3){};
    void (*VertexAttrib4fv)(GLuint index, const GLfloat* values){};
    void (*VertexAttribI4i)(GLuint index, GLint x, GLint y, GLint z, GLint w){};
    void (*VertexAttribI4iv)(GLuint index, const GLint* values){};
    void (*VertexAttribI4ui)(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w){};
    void (*VertexAttribI4uiv)(GLuint index, const GLuint* values){};
    void (*TexParameteriv)(GLenum target, GLenum pname, const GLint* values){};
    void (*TexParameterIiv)(GLenum target, GLenum pname, const GLint* values){};
    void (*TexParameterIuiv)(GLenum target, GLenum pname, const GLuint* values){};
    void (*SamplerParameteriv)(GLuint sampler, GLenum pname, const GLint* values){};
    void (*SamplerParameterIiv)(GLuint sampler, GLenum pname, const GLint* values){};
    void (*SamplerParameterIuiv)(GLuint sampler, GLenum pname, const GLuint* values){};
    void (*GetActiveUniformBlockiv)(GLuint, GLuint, GLenum, GLint*){};
    void (*GetIntegeri_v)(GLenum, GLuint, GLint*){};
    void (*GetInteger64i_v)(GLenum, GLuint, std::int64_t*){};
    void (*UniformBlockBinding)(GLuint, GLuint, GLuint){};
    void (*GetVertexAttribiv)(GLuint, GLenum, GLint*){};
    void (*GetVertexAttribPointerv)(GLuint, GLenum, void**){};
    void (*InvalidateSubFramebuffer)(GLenum, GLsizei, const GLenum*, GLint, GLint, GLsizei, GLsizei){};
    void (*MultiDrawElementsIndirect)(GLenum, GLenum, const void*, GLsizei, GLsizei){};
    void (*MultiDrawArraysIndirect)(GLenum, const void*, GLsizei, GLsizei){};
    void (*DrawElementsIndirect)(GLenum, GLenum, const void*){};
    void (*DrawArraysIndirect)(GLenum, const void*){};
    void (*CopyImageSubData)(GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei){};
    void (*GetActiveUniform)(GLuint, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, GLchar*){};
    void (*GetUniformfv)(GLuint, GLint, GLfloat*){};
    void (*GetUniformiv)(GLuint, GLint, GLint*){};
    void (*GetUniformuiv)(GLuint, GLint, GLuint*){};
    void (*CullFace)(GLenum){};
    void (*FrontFace)(GLenum){};
    void (*PolygonOffset)(GLfloat, GLfloat){};
    void (*BindBuffer)(GLenum, GLuint){};
    void (*BindBufferBase)(GLenum, GLuint, GLuint){};
    void (*BindBufferRange)(GLenum, GLuint, GLuint, GLintptr, GLsizeiptr){};
    void (*GenSamplers)(GLsizei, GLuint*){};
    void (*SamplerParameteri)(GLuint, GLenum, GLint){};
    void (*DeleteProgram)(GLuint){};
    void (*Uniform1fv)(GLint, GLsizei, const GLfloat*){};
    void (*Uniform1f)(GLint, GLfloat){};
    void (*Uniform2fv)(GLint, GLsizei, const GLfloat*){};
    void (*Uniform2f)(GLint, GLfloat, GLfloat){};
    void (*Uniform3fv)(GLint, GLsizei, const GLfloat*){};
    void (*Uniform3f)(GLint, GLfloat, GLfloat, GLfloat){};
    void (*Uniform4fv)(GLint, GLsizei, const GLfloat*){};
    void (*Uniform4f)(GLint, GLfloat, GLfloat, GLfloat, GLfloat){};
    void (*Uniform1iv)(GLint, GLsizei, const GLint*){};
    void (*Uniform1i)(GLint, GLint){};
    void (*Uniform2iv)(GLint, GLsizei, const GLint*){};
    void (*Uniform2i)(GLint, GLint, GLint){};
    void (*Uniform3iv)(GLint, GLsizei, const GLint*){};
    void (*Uniform3i)(GLint, GLint, GLint, GLint){};
    void (*Uniform4iv)(GLint, GLsizei, const GLint*){};
    void (*Uniform4i)(GLint, GLint, GLint, GLint, GLint){};
    void (*Uniform1uiv)(GLint, GLsizei, const GLuint*){};
    void (*Uniform1ui)(GLint, GLuint){};
    void (*Uniform2uiv)(GLint, GLsizei, const GLuint*){};
    void (*Uniform2ui)(GLint, GLuint, GLuint){};
    void (*Uniform3uiv)(GLint, GLsizei, const GLuint*){};
    void (*Uniform3ui)(GLint, GLuint, GLuint, GLuint){};
    void (*Uniform4uiv)(GLint, GLsizei, const GLuint*){};
    void (*Uniform4ui)(GLint, GLuint, GLuint, GLuint, GLuint){};
    void (*UniformMatrix2fv)(GLint, GLsizei, GLboolean, const GLfloat*){};
    void (*UniformMatrix3fv)(GLint, GLsizei, GLboolean, const GLfloat*){};
    void (*UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*){};
    void (*UniformMatrix2x3fv)(GLint, GLsizei, GLboolean, const GLfloat*){};
    void (*UniformMatrix3x2fv)(GLint, GLsizei, GLboolean, const GLfloat*){};
    void (*UniformMatrix2x4fv)(GLint, GLsizei, GLboolean, const GLfloat*){};
    void (*UniformMatrix4x2fv)(GLint, GLsizei, GLboolean, const GLfloat*){};
    void (*UniformMatrix3x4fv)(GLint, GLsizei, GLboolean, const GLfloat*){};
    void (*UniformMatrix4x3fv)(GLint, GLsizei, GLboolean, const GLfloat*){};
    void (*ActiveTexture)(GLenum){};
    void (*BindTexture)(GLenum, GLuint){};
    void (*BindSampler)(GLuint, GLuint){};
    void (*BindVertexArray)(GLuint){};
    void (*BufferData)(GLenum, GLsizeiptr, const void*, GLenum){};
    void (*BufferSubData)(GLenum, GLintptr, GLsizeiptr, const void*){};
    void (*CopyBufferSubData)(GLenum, GLenum, GLintptr, GLintptr, GLsizeiptr){};
    void* (*MapBufferRange)(GLenum, GLintptr, GLsizeiptr, GLbitfield){};
    void (*FlushMappedBufferRange)(GLenum, GLintptr, GLsizeiptr){};
    GLboolean (*UnmapBuffer)(GLenum){};
    void (*DeleteBuffers)(GLsizei, const GLuint*){};
    void (*DeleteTextures)(GLsizei, const GLuint*){};
    void (*DeleteSamplers)(GLsizei, const GLuint*){};
    void (*SamplerParameterf)(GLuint, GLenum, GLfloat){};
    void (*SamplerParameterfv)(GLuint, GLenum, const GLfloat*){};
    void (*TexParameteri)(GLenum, GLenum, GLint){};
    void (*TexParameterf)(GLenum, GLenum, GLfloat){};
    void (*TexParameterfv)(GLenum, GLenum, const GLfloat*){};
    void (*TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*){};
    void (*TexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*){};
    void (*TexStorage2D)(GLenum, GLsizei, GLenum, GLsizei, GLsizei){};
    void (*TexImage3D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*){};
    void (*TexSubImage3D)(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLenum, const void*){};
    void (*TexStorage3D)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei){};
    void (*CompressedTexImage2D)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void*){};
    void (*CompressedTexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const void*){};
    void (*CompressedTexImage3D)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, GLsizei, const void*){};
    void (*CompressedTexSubImage3D)(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const void*){};
    void (*CopyTexSubImage2D)(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei){};
    void (*GenerateMipmap)(GLenum){};
    void (*LinkProgram)(GLuint){};
    void (*ProgramBinary)(GLuint, GLenum, const void*, GLsizei){};
    void (*DeleteFramebuffers)(GLsizei, const GLuint*){};
    void (*FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint){};
    void (*FramebufferTextureLayer)(GLenum, GLenum, GLuint, GLint, GLint){};
    void (*FramebufferRenderbuffer)(GLenum, GLenum, GLenum, GLuint){};
    void (*RenderbufferStorage)(GLenum, GLenum, GLsizei, GLsizei){};
    void (*RenderbufferStorageMultisample)(GLenum, GLsizei, GLenum, GLsizei, GLsizei){};
    void (*DeleteRenderbuffers)(GLsizei, const GLuint*){};
    void (*ClearBufferfv)(GLenum, GLint, const GLfloat*){};
    void (*ClearBufferiv)(GLenum, GLint, const GLint*){};
    void (*ClearBufferuiv)(GLenum, GLint, const GLuint*){};
    void (*ClearBufferfi)(GLenum, GLint, GLfloat, GLint){};
    void (*InvalidateFramebuffer)(GLenum, GLsizei, const GLenum*){};
    void (*BlitFramebuffer)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum){};
    void (*DispatchCompute)(GLuint, GLuint, GLuint){};
    void (*DispatchComputeIndirect)(GLintptr){};
    void (*MemoryBarrier)(GLbitfield){};
    void (*BindImageTexture)(GLuint, GLuint, GLint, GLboolean, GLint, GLenum, GLenum){};
    void (*BeginQuery)(GLenum, GLuint){};
    void (*EndQuery)(GLenum){};
    void (*BeginTransformFeedback)(GLenum){};
    void (*EndTransformFeedback)(){};
    void (*EnableVertexAttribArray)(GLuint){};
    void (*DisableVertexAttribArray)(GLuint){};
    void (*VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*){};
    void (*VertexAttribIPointer)(GLuint, GLint, GLenum, GLsizei, const void*){};
    void (*VertexAttribDivisor)(GLuint, GLuint){};
    void (*DeleteVertexArrays)(GLsizei, const GLuint*){};
    void (*DepthRangef)(GLfloat, GLfloat){};
} api;
std::atomic<void*> origVertexAttrib1f{nullptr};
std::atomic<void*> origVertexAttrib1fv{nullptr};
std::atomic<void*> origVertexAttrib2f{nullptr};
std::atomic<void*> origVertexAttrib2fv{nullptr};
std::atomic<void*> origVertexAttrib3f{nullptr};
std::atomic<void*> origVertexAttrib3fv{nullptr};
std::atomic<void*> origVertexAttrib4f{nullptr};
std::atomic<void*> origVertexAttrib4fv{nullptr};
std::atomic<void*> origVertexAttribI4i{nullptr};
std::atomic<void*> origVertexAttribI4iv{nullptr};
std::atomic<void*> origVertexAttribI4ui{nullptr};
std::atomic<void*> origVertexAttribI4uiv{nullptr};
std::atomic<void*> origTexParameteriv{nullptr};
std::atomic<void*> origTexParameterIiv{nullptr};
std::atomic<void*> origTexParameterIuiv{nullptr};
std::atomic<void*> origSamplerParameteriv{nullptr};
std::atomic<void*> origSamplerParameterIiv{nullptr};
std::atomic<void*> origSamplerParameterIuiv{nullptr};
std::atomic<void*> origUniformBlockBinding{nullptr};
std::atomic<void*> origInvalidateSubFramebuffer{nullptr};
std::atomic<void*> origMultiDrawElementsIndirect{nullptr};
std::atomic<void*> origMultiDrawArraysIndirect{nullptr};
std::atomic<void*> origDrawElementsIndirect{nullptr};
std::atomic<void*> origDrawArraysIndirect{nullptr};
std::atomic<void*> origCopyImageSubData{nullptr};
std::atomic<void*> origCullFace{nullptr};
std::atomic<void*> origFrontFace{nullptr};
std::atomic<void*> origPolygonOffset{nullptr};
std::atomic<void*> origBindBuffer{nullptr};
std::atomic<void*> origBindBufferBase{nullptr};
std::atomic<void*> origBindBufferRange{nullptr};
std::atomic<void*> origSamplerParameteri{nullptr};
std::atomic<void*> origDeleteProgram{nullptr};
std::atomic<void*> origUniform1fv{nullptr};
std::atomic<void*> origUniform1f{nullptr};
std::atomic<void*> origUniform2fv{nullptr};
std::atomic<void*> origUniform2f{nullptr};
std::atomic<void*> origUniform3fv{nullptr};
std::atomic<void*> origUniform3f{nullptr};
std::atomic<void*> origUniform4fv{nullptr};
std::atomic<void*> origUniform4f{nullptr};
std::atomic<void*> origUniform1iv{nullptr};
std::atomic<void*> origUniform1i{nullptr};
std::atomic<void*> origUniform2iv{nullptr};
std::atomic<void*> origUniform2i{nullptr};
std::atomic<void*> origUniform3iv{nullptr};
std::atomic<void*> origUniform3i{nullptr};
std::atomic<void*> origUniform4iv{nullptr};
std::atomic<void*> origUniform4i{nullptr};
std::atomic<void*> origUniform1uiv{nullptr};
std::atomic<void*> origUniform1ui{nullptr};
std::atomic<void*> origUniform2uiv{nullptr};
std::atomic<void*> origUniform2ui{nullptr};
std::atomic<void*> origUniform3uiv{nullptr};
std::atomic<void*> origUniform3ui{nullptr};
std::atomic<void*> origUniform4uiv{nullptr};
std::atomic<void*> origUniform4ui{nullptr};
std::atomic<void*> origUniformMatrix2fv{nullptr};
std::atomic<void*> origUniformMatrix3fv{nullptr};
std::atomic<void*> origUniformMatrix2x3fv{nullptr};
std::atomic<void*> origUniformMatrix3x2fv{nullptr};
std::atomic<void*> origUniformMatrix2x4fv{nullptr};
std::atomic<void*> origUniformMatrix4x2fv{nullptr};
std::atomic<void*> origUniformMatrix3x4fv{nullptr};
std::atomic<void*> origUniformMatrix4x3fv{nullptr};
std::atomic<void*> origActiveTexture{nullptr};
std::atomic<void*> origBindTexture{nullptr};
std::atomic<void*> origBindSampler{nullptr};
std::atomic<void*> origBindVertexArray{nullptr};
std::atomic<void*> origBufferData{nullptr};
std::atomic<void*> origBufferSubData{nullptr};
std::atomic<void*> origCopyBufferSubData{nullptr};
std::atomic<void*> origMapBufferRange{nullptr};
std::atomic<void*> origFlushMappedBufferRange{nullptr};
std::atomic<void*> origUnmapBuffer{nullptr};
std::atomic<void*> origDeleteBuffers{nullptr};
std::atomic<void*> origDeleteTextures{nullptr};
std::atomic<void*> origDeleteSamplers{nullptr};
std::atomic<void*> origSamplerParameterf{nullptr};
std::atomic<void*> origSamplerParameterfv{nullptr};
std::atomic<void*> origTexParameteri{nullptr};
std::atomic<void*> origTexParameterf{nullptr};
std::atomic<void*> origTexParameterfv{nullptr};
std::atomic<void*> origTexImage2D{nullptr};
std::atomic<void*> origTexSubImage2D{nullptr};
std::atomic<void*> origTexStorage2D{nullptr};
std::atomic<void*> origTexImage3D{nullptr};
std::atomic<void*> origTexSubImage3D{nullptr};
std::atomic<void*> origTexStorage3D{nullptr};
std::atomic<void*> origCompressedTexImage2D{nullptr};
std::atomic<void*> origCompressedTexSubImage2D{nullptr};
std::atomic<void*> origCompressedTexImage3D{nullptr};
std::atomic<void*> origCompressedTexSubImage3D{nullptr};
std::atomic<void*> origCopyTexSubImage2D{nullptr};
std::atomic<void*> origGenerateMipmap{nullptr};
std::atomic<void*> origLinkProgram{nullptr};
std::atomic<void*> origProgramBinary{nullptr};
std::atomic<void*> origDeleteFramebuffers{nullptr};
std::atomic<void*> origFramebufferTexture2D{nullptr};
std::atomic<void*> origFramebufferTextureLayer{nullptr};
std::atomic<void*> origFramebufferRenderbuffer{nullptr};
std::atomic<void*> origRenderbufferStorage{nullptr};
std::atomic<void*> origRenderbufferStorageMultisample{nullptr};
std::atomic<void*> origDeleteRenderbuffers{nullptr};
std::atomic<void*> origClearBufferfv{nullptr};
std::atomic<void*> origClearBufferiv{nullptr};
std::atomic<void*> origClearBufferuiv{nullptr};
std::atomic<void*> origClearBufferfi{nullptr};
std::atomic<void*> origInvalidateFramebuffer{nullptr};
std::atomic<void*> origBlitFramebuffer{nullptr};
std::atomic<void*> origDispatchCompute{nullptr};
std::atomic<void*> origDispatchComputeIndirect{nullptr};
std::atomic<void*> origMemoryBarrier{nullptr};
std::atomic<void*> origBindImageTexture{nullptr};
std::atomic<void*> origBeginQuery{nullptr};
std::atomic<void*> origEndQuery{nullptr};
std::atomic<void*> origBeginTransformFeedback{nullptr};
std::atomic<void*> origEndTransformFeedback{nullptr};
std::atomic<void*> origEnableVertexAttribArray{nullptr};
std::atomic<void*> origDisableVertexAttribArray{nullptr};
std::atomic<void*> origVertexAttribPointer{nullptr};
std::atomic<void*> origVertexAttribIPointer{nullptr};
std::atomic<void*> origVertexAttribDivisor{nullptr};
std::atomic<void*> origDeleteVertexArrays{nullptr};
std::atomic<void*> origDepthRangef{nullptr};
template<class F> F forward(std::atomic<void*>& original, F fallback) {
    void* p = original.load(std::memory_order_acquire);
    return p ? reinterpret_cast<F>(p) : fallback;
}
constexpr GLenum kActiveUniforms = 0x8B86, kActiveUniformMaxLength = 0x8B87;
constexpr GLenum kCullFaceMode = 0x0B45, kFrontFace = 0x0B46, kPolygonOffset = 0x8037;
constexpr GLenum kPolygonFactor = 0x8038, kPolygonUnits = 0x2A00;
constexpr std::size_t kMemoryLimit = 8 * 1024 * 1024;
constexpr std::array<GLenum, 4> kTargets{GL_TEXTURE_2D, 0x806F, 0x8513, 0x8C1A};
constexpr std::array<GLenum, 4> kTextureQueries{GL_TEXTURE_BINDING_2D, 0x806A, 0x8514, 0x8C1D};

enum class Kind { F1, F2, F3, F4, I1, I2, I3, I4, U1, U2, U3, U4, M2, M3, M4, M2x3, M3x2, M2x4, M4x2, M3x4, M4x3 };

int components(Kind kind) {
    const int k = static_cast<int>(kind);
    if (k < 12) return k % 4 + 1;
    constexpr int sizes[]{4, 9, 16, 6, 6, 8, 8, 12, 12};
    return sizes[k - 12];
}

void upload(Kind kind, GLint location, GLsizei count, const void* data) {
    const auto* f = static_cast<const GLfloat*>(data);
    const auto* i = static_cast<const GLint*>(data);
    const auto* u = static_cast<const GLuint*>(data);
    switch (kind) {
        case Kind::F1: api.Uniform1fv(location, count, f); break;
        case Kind::F2: api.Uniform2fv(location, count, f); break;
        case Kind::F3: api.Uniform3fv(location, count, f); break;
        case Kind::F4: api.Uniform4fv(location, count, f); break;
        case Kind::I1: api.Uniform1iv(location, count, i); break;
        case Kind::I2: api.Uniform2iv(location, count, i); break;
        case Kind::I3: api.Uniform3iv(location, count, i); break;
        case Kind::I4: api.Uniform4iv(location, count, i); break;
        case Kind::U1: api.Uniform1uiv(location, count, u); break;
        case Kind::U2: api.Uniform2uiv(location, count, u); break;
        case Kind::U3: api.Uniform3uiv(location, count, u); break;
        case Kind::U4: api.Uniform4uiv(location, count, u); break;
        case Kind::M2: api.UniformMatrix2fv(location, count, 0, f); break;
        case Kind::M3: api.UniformMatrix3fv(location, count, 0, f); break;
        case Kind::M4: api.UniformMatrix4fv(location, count, 0, f); break;
        case Kind::M2x3: api.UniformMatrix2x3fv(location, count, 0, f); break;
        case Kind::M3x2: api.UniformMatrix3x2fv(location, count, 0, f); break;
        case Kind::M2x4: api.UniformMatrix2x4fv(location, count, 0, f); break;
        case Kind::M4x2: api.UniformMatrix4x2fv(location, count, 0, f); break;
        case Kind::M3x4: api.UniformMatrix3x4fv(location, count, 0, f); break;
        case Kind::M4x3: api.UniformMatrix4x3fv(location, count, 0, f); break;
    }
}

GLenum samplerTarget(GLenum type) {
    switch (type) {
        case 0x8B5E: case 0x8B62: case 0x8DCA: case 0x8DD2: return GL_TEXTURE_2D;
        case 0x8B5F: case 0x8DCB: case 0x8DD3: return 0x806F;
        case 0x8B60: case 0x8DC5: case 0x8DCC: case 0x8DD4: return 0x8513;
        case 0x8DC1: case 0x8DC4: case 0x8DCF: case 0x8DD7: return 0x8C1A;
        default: return 0;
    }
}

bool uniformKind(GLenum type, Kind& kind) {
    if (samplerTarget(type)) { kind = Kind::I1; return true; }
    switch (type) {
        case 0x1406: kind = Kind::F1; return true;
        case 0x8B50: kind = Kind::F2; return true;
        case 0x8B51: kind = Kind::F3; return true;
        case 0x8B52: kind = Kind::F4; return true;
        case 0x1404: case 0x8B56: kind = Kind::I1; return true;
        case 0x8B53: case 0x8B57: kind = Kind::I2; return true;
        case 0x8B54: case 0x8B58: kind = Kind::I3; return true;
        case 0x8B55: case 0x8B59: kind = Kind::I4; return true;
        case 0x1405: kind = Kind::U1; return true;
        case 0x8DC6: kind = Kind::U2; return true;
        case 0x8DC7: kind = Kind::U3; return true;
        case 0x8DC8: kind = Kind::U4; return true;
        case 0x8B5A: kind = Kind::M2; return true;
        case 0x8B5B: kind = Kind::M3; return true;
        case 0x8B5C: kind = Kind::M4; return true;
        case 0x8B65: kind = Kind::M2x3; return true;
        case 0x8B67: kind = Kind::M3x2; return true;
        case 0x8B66: kind = Kind::M2x4; return true;
        case 0x8B69: kind = Kind::M4x2; return true;
        case 0x8B68: kind = Kind::M3x4; return true;
        case 0x8B6A: kind = Kind::M4x3; return true;
        default: return false;
    }
}

struct Uniform {
    GLint location{-1};
    GLenum type{};
    Kind kind{};
    int count{};
    GLenum sampler{};
    bool camera{};
    std::vector<std::uint32_t> values, replayValues;
    std::uint64_t replaySerial{};
};
struct UniformLocation { std::size_t index{}; int element{}; };
struct Program {
    bool initialized{}, supported{true};
    std::array<GLint, 5> cameraLocations{-1, -1, -1, -1, -1};
    std::array<UniformLocation, 5> cameraSlots{};
    std::vector<GLuint> uniformBuffers, uniformBlockBindings;
    bool blocksReady{};
    std::uint64_t serial{};
    std::vector<Uniform> uniforms;
    std::unordered_map<GLint, UniformLocation> locations;
};
struct TextureUnit {
    std::array<GLuint, 4> targets{};
    unsigned known{};
    GLuint sampler{};
    bool haveSampler{};
};
struct Pipeline {
    bool cull{}, offset{};
    GLenum cullFace{0x0405}, frontFace{0x0901};
    float factor{}, units{};
    float depthRange[2]{0.0f, 1.0f};
};
struct UniformCommand { GLuint program{}; UniformLocation slot; GLsizei count{}; std::size_t offset{}; };
struct TextureCommand { unsigned unit{}, target{}; GLuint texture{}, sampler{}; };
struct DrawCommand {
    DrawCall draw;
    GLuint program{}, vao{};
    Pipeline pipeline;
    std::array<MatrixUpload, 5> matrices;
    std::array<UniformLocation, 5> matrixSlots;
    int matrixCount{};
    unsigned mask{};
};
enum class CommandKind { Uniform, Texture, Indexed, Draw };
struct Command { CommandKind kind{}; std::size_t index{}; };

struct AttributeFormat {
    GLint size{};
    GLenum type{};
    GLboolean normalized{};
    GLsizei stride{};
    const void* pointer{};
    GLuint buffer{};
    bool integer{};
    bool operator==(const AttributeFormat&) const = default;
};
struct AttributeState {
    AttributeFormat format;
    GLuint divisor{};
    bool enabled{}, haveFormat{}, haveDivisor{}, haveEnabled{};
};
struct Layout {
    std::array<AttributeState, 32> attributes;
    GLuint element{};
    bool haveElement{};
};
struct FrozenAttribute {
    GLuint index{};
    AttributeFormat format;
    GLuint divisor{};
    bool operator==(const FrozenAttribute&) const = default;
};
struct VertexKey {
    GLuint element{};
    std::vector<FrozenAttribute> attributes;
    bool operator==(const VertexKey&) const = default;
};
struct VertexHash {
    std::size_t operator()(const VertexKey& key) const {
        std::size_t hash = key.element;
        auto mix = [&](std::uintptr_t value) { hash ^= value + std::uintptr_t{0x9E3779B185EBCA87ull} + (hash << 6) + (hash >> 2); };
        for (const auto& attribute : key.attributes) {
            mix(attribute.index); mix(attribute.divisor);
            mix(static_cast<std::uintptr_t>(attribute.format.size)); mix(attribute.format.type); mix(attribute.format.normalized);
            mix(static_cast<std::uintptr_t>(attribute.format.stride)); mix(reinterpret_cast<std::uintptr_t>(attribute.format.pointer));
            mix(attribute.format.buffer); mix(attribute.format.integer);
        }
        return hash;
    }
};
struct OwnedVertexArray { GLuint name{}; std::uint64_t usedFrame{}; };
struct IndexedBuffer { GLuint buffer{}; GLintptr offset{}; GLsizeiptr size{}; bool range{}; bool operator==(const IndexedBuffer&) const = default; };
struct IndexedCommand { GLuint index{}; IndexedBuffer binding; };
struct Parameter { std::array<std::uint32_t, 4> words{}; unsigned count{}, encoding{}; bool operator==(const Parameter&) const = default; };
struct ParameterKey { GLuint object{}; GLenum target{}, name{}; bool sampler{}; bool operator==(const ParameterKey&) const = default; };
struct ParameterHash {
    std::size_t operator()(const ParameterKey& key) const {
        return (std::size_t(key.object) * 0x9E3779B1u) ^ (std::size_t(key.target) << 5) ^ (std::size_t(key.name) << 17) ^ key.sampler;
    }
};
struct Live {
    GLuint program{}, vao{}, arrayBuffer{};
    bool haveVao{}, haveArrayBuffer{}, havePipeline{}, flushing{};
    std::unordered_map<GLuint, Layout> layouts;
    std::unordered_map<GLuint, std::vector<std::size_t>> pendingVaoDraws;
    std::vector<GLuint> pendingVaoSources;
    VertexKey scratchVertexKey;
    std::unordered_map<VertexKey, OwnedVertexArray, VertexHash> vertexCache;
    std::vector<GLuint> temporaryVaos;
    std::unordered_map<std::uint64_t, IndexedBuffer> indexedBuffers;
    std::unordered_map<GLuint, IndexedBuffer> recordedUbos, replayUbos;
    std::vector<IndexedCommand> indexedCommands;
    std::unordered_map<ParameterKey, Parameter, ParameterHash> parameters;
    std::uint64_t completedFrames{};
    int maxAttributes{};
    bool freezeHealthy{true};
    unsigned cloneChecks{};
    int active{-1};
    unsigned queryDepth{};
    bool transformFeedback{}, healthy{true};
    unsigned errorChecks{};
    Pipeline pipeline;
    std::unordered_map<GLuint, Program> programs;
    std::unordered_map<GLuint, unsigned> generations;
    std::vector<TextureUnit> textures;
    std::vector<TextureUnit> recorded, finalTextures, replayTextures;
    Target target;
    std::uint64_t serial{1};
    std::vector<GLuint> usedPrograms;
    unsigned cascadeMask{};
    std::vector<std::pair<unsigned, unsigned>> touched;
    std::vector<Command> commands;
    std::vector<UniformCommand> uniforms;
    std::vector<TextureCommand> bindings;
    std::vector<DrawCommand> draws;
    std::vector<std::uint32_t> words;
};
Live& live() { static thread_local Live state; return state; }

int activeUnit() {
    auto& s = live();
    if (s.active < 0) {
        GLint unit = 0;
        real.GetIntegerv(GL_ACTIVE_TEXTURE, &unit);
        s.active = unit - static_cast<GLint>(GL_TEXTURE0);
    }
    return s.active;
}
void ensureUnits(unsigned unit) {
    auto& s = live();
    if (unit >= 256) return;
    if (s.textures.size() <= unit) s.textures.resize(unit + 1);
    if (s.recorded.size() <= unit) s.recorded.resize(unit + 1);
}
int targetIndex(GLenum target) {
    for (int i = 0; i < 4; ++i) if (kTargets[i] == target) return i;
    return -1;
}
void trackTexture(GLenum target, GLuint texture) {
    const int index = targetIndex(target);
    const int unit = activeUnit();
    if (index < 0 || unit < 0 || unit >= 256) return;
    ensureUnits(static_cast<unsigned>(unit));
    auto& binding = live().textures[unit];
    binding.targets[index] = texture;
    binding.known |= 1u << index;
}
void trackSampler(unsigned unit, GLuint sampler) {
    if (unit >= 256) return;
    ensureUnits(unit);
    auto& binding = live().textures[unit];
    binding.sampler = sampler;
    binding.haveSampler = true;
}
TextureUnit textureUnit(unsigned unit, unsigned target) {
    ensureUnits(unit);
    auto& s = live();
    auto& binding = s.textures[unit];
    if (!(binding.known & (1u << target)) || !binding.haveSampler) {
        const int active = activeUnit();
        real.ActiveTexture(GL_TEXTURE0 + unit);
        GLint value = 0;
        if (!(binding.known & (1u << target))) {
            real.GetIntegerv(kTextureQueries[target], &value);
            binding.targets[target] = static_cast<GLuint>(value);
            binding.known |= 1u << target;
        }
        if (!binding.haveSampler) {
            real.GetIntegerv(GL_SAMPLER_BINDING, &value);
            binding.sampler = static_cast<GLuint>(value);
            binding.haveSampler = true;
        }
        real.ActiveTexture(GL_TEXTURE0 + active);
    }
    return binding;
}
GLuint vertexArray() {
    auto& s = live();
    if (!s.haveVao) {
        GLint value = 0;
        real.GetIntegerv(GL_VERTEX_ARRAY_BINDING, &value);
        s.vao = static_cast<GLuint>(value);
        s.haveVao = true;
    }
    return s.vao;
}
bool queuedVao() {
    auto& s = live();
    const auto found = s.pendingVaoDraws.find(vertexArray());
    return found != s.pendingVaoDraws.end() && !found->second.empty();
}

GLuint arrayBuffer();

bool readVertexKey(GLuint sourceVao, VertexKey& key) {
    auto& state = live();
    auto& layout = state.layouts[sourceVao];
    if (!state.maxAttributes) {
        GLint count = 0;
        real.GetIntegerv(0x8869, &count);
        if (count < 1 || count > 32) return false;
        state.maxAttributes = count;
    }
    if (!layout.haveElement) {
        GLint buffer = 0;
        real.GetIntegerv(0x8895, &buffer);
        layout.element = static_cast<GLuint>(buffer);
        layout.haveElement = true;
    }
    key.element = layout.element;
    key.attributes.clear();
    key.attributes.reserve(static_cast<std::size_t>(state.maxAttributes));
    for (GLuint index = 0; index < static_cast<GLuint>(state.maxAttributes); ++index) {
        auto& attribute = layout.attributes[index];
        GLint value = 0;
        if (!attribute.haveEnabled) {
            api.GetVertexAttribiv(index, 0x8622, &value);
            attribute.enabled = value != 0;
            attribute.haveEnabled = true;
        }
        if (!attribute.enabled) continue;
        if (!attribute.haveFormat) {
            api.GetVertexAttribiv(index, 0x8623, &attribute.format.size);
            api.GetVertexAttribiv(index, 0x8624, &attribute.format.stride);
            api.GetVertexAttribiv(index, 0x8625, &value); attribute.format.type = static_cast<GLenum>(value);
            api.GetVertexAttribiv(index, 0x886A, &value); attribute.format.normalized = value ? 1 : 0;
            api.GetVertexAttribiv(index, 0x889F, &value); attribute.format.buffer = static_cast<GLuint>(value);
            api.GetVertexAttribiv(index, 0x88FD, &value); attribute.format.integer = value != 0;
            void* pointer = nullptr;
            api.GetVertexAttribPointerv(index, 0x8645, &pointer);
            attribute.format.pointer = pointer;
            attribute.haveFormat = true;
        }
        if (!attribute.haveDivisor) {
            api.GetVertexAttribiv(index, 0x88FE, &value);
            attribute.divisor = static_cast<GLuint>(value);
            attribute.haveDivisor = true;
        }
        if (!attribute.format.buffer) return false;
        key.attributes.push_back({index, attribute.format, attribute.divisor});
    }
    return !key.attributes.empty();
}

GLuint cloneVertexArray(const VertexKey& key) {
    auto& state = live();
    auto found = state.vertexCache.find(key);
    if (found != state.vertexCache.end()) {
        found->second.usedFrame = state.completedFrames;

        return found->second.name;
    }
    const bool checking = state.cloneChecks < 4;
    if (checking) drainErrors();
    const GLuint savedVao = vertexArray(), savedArray = arrayBuffer();
    GLuint name = 0;
    real.GenVertexArrays(1, &name);
    if (!name) return 0;
    real.BindVertexArray(name);
    api.BindBuffer(0x8893, key.element);
    for (const auto& attribute : key.attributes) {
        const auto& format = attribute.format;
        api.BindBuffer(0x8892, format.buffer);
        if (format.integer) api.VertexAttribIPointer(attribute.index, format.size, format.type, format.stride, format.pointer);
        else api.VertexAttribPointer(attribute.index, format.size, format.type, format.normalized, format.stride, format.pointer);
        api.EnableVertexAttribArray(attribute.index);
        api.VertexAttribDivisor(attribute.index, attribute.divisor);
    }
    real.BindVertexArray(savedVao);
    api.BindBuffer(0x8892, savedArray);
    if (checking) {
        ++state.cloneChecks;
        const GLenum error = real.GetError();
        drainErrors();
        if (error) {
            api.DeleteVertexArrays(1, &name);
            state.freezeHealthy = false;

            return 0;
        }
    }

    if (state.vertexCache.size() >= 4096) {
        for (auto it = state.vertexCache.begin(); it != state.vertexCache.end(); ++it) {
            if (it->second.usedFrame >= state.completedFrames) continue;
            api.DeleteVertexArrays(1, &it->second.name);
            state.vertexCache.erase(it);
            break;
        }
    }
    if (state.vertexCache.size() < 4096) state.vertexCache.emplace(key, OwnedVertexArray{name, state.completedFrames});
    else state.temporaryVaos.push_back(name);
    return name;
}

void preserveQueuedVao(GLuint sourceVao) {
    auto& state = live();
    auto pending = state.pendingVaoDraws.find(sourceVao);
    if (pending == state.pendingVaoDraws.end() || pending->second.empty()) return;
    if (!config().layoutSnapshots.load(std::memory_order_relaxed) || !state.freezeHealthy || !api.GetVertexAttribiv || !api.GetVertexAttribPointerv || !api.DeleteVertexArrays) {
        flush();
        return;
    }
    const GLuint savedVao = vertexArray();
    if (savedVao != sourceVao) real.BindVertexArray(sourceVao);
    auto& key = state.scratchVertexKey;
    const bool valid = readVertexKey(sourceVao, key);
    if (savedVao != sourceVao) real.BindVertexArray(savedVao);
    const GLuint frozen = valid ? cloneVertexArray(key) : 0;
    if (!frozen) {
        flush();
        return;
    }
    for (const auto index : pending->second) state.draws[index].vao = frozen;
    pending->second.clear();
}

void invalidateVertexBuffers(GLsizei count, const GLuint* buffers) {
    auto& state = live();
    std::unordered_set<GLuint> removed;
    for (GLsizei i = 0; i < count; ++i) removed.insert(buffers[i]);
    for (auto it = state.vertexCache.begin(); it != state.vertexCache.end();) {
        bool remove = removed.contains(it->first.element);
        for (const auto& attribute : it->first.attributes) if (removed.contains(attribute.format.buffer)) { remove = true; break; }
        if (remove) {
            api.DeleteVertexArrays(1, &it->second.name);
            it = state.vertexCache.erase(it);
        } else ++it;
    }
    state.indexedBuffers.clear();
}

bool unchangedIndexed(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size, bool range) {
    auto& bindings = live().indexedBuffers;
    const std::uint64_t key = (std::uint64_t(target) << 32) | index;
    const IndexedBuffer value{buffer, offset, size, range};
    auto found = bindings.find(key);
    const bool unchanged = found != bindings.end() && found->second == value;
    bindings[key] = value;
    return unchanged;
}

void parameterChange(GLuint object, GLenum target, GLenum pname, const void* values, unsigned count, unsigned encoding, bool sampler) {
    if (!values || count > 4) { flush(); return; }
    auto& parameters = live().parameters;
    const ParameterKey key{object, target, pname, sampler};
    Parameter value;
    value.count = count;
    value.encoding = encoding;
    std::memcpy(value.words.data(), values, count * 4);
    auto found = parameters.find(key);
    if (found != parameters.end() && found->second == value) return;
    flush();
    if (pname >= 0x8E42 && pname <= 0x8E46) {
        for (auto it = parameters.begin(); it != parameters.end();) {
            const auto& other = it->first;
            if (other.object == object && other.target == target && other.sampler == sampler && other.name >= 0x8E42 && other.name <= 0x8E46)
                it = parameters.erase(it);
            else ++it;
        }
    }
    parameters[key] = value;
}

GLuint boundTexture(GLenum target) {
    const int index = targetIndex(target), unit = activeUnit();
    if (index < 0 || unit < 0 || unit >= 256) return 0;
    return textureUnit(static_cast<unsigned>(unit), static_cast<unsigned>(index)).targets[index];
}

void forgetParameters(GLsizei count, const GLuint* objects, bool samplers) {
    auto& parameters = live().parameters;
    for (auto it = parameters.begin(); it != parameters.end();) {
        bool remove = false;
        if (it->first.sampler == samplers) for (GLsizei i = 0; i < count; ++i) if (it->first.object == objects[i]) { remove = true; break; }
        if (remove) it = parameters.erase(it);
        else ++it;
    }
}

Pipeline pipeline() {
    auto& s = live();
    if (!s.havePipeline) {
        s.pipeline.cull = real.IsEnabled(GL_CULL_FACE) != 0;
        s.pipeline.offset = real.IsEnabled(kPolygonOffset) != 0;
        GLint value = 0;
        real.GetIntegerv(kCullFaceMode, &value);
        s.pipeline.cullFace = static_cast<GLenum>(value);
        real.GetIntegerv(kFrontFace, &value);
        s.pipeline.frontFace = static_cast<GLenum>(value);
        real.GetFloatv(kPolygonFactor, &s.pipeline.factor);
        real.GetFloatv(kPolygonUnits, &s.pipeline.units);
        s.havePipeline = true;
    }
    return s.pipeline;
}

bool captureUbos() { return api.GetActiveUniformBlockiv && api.GetIntegeri_v && api.GetInteger64i_v && api.BindBufferBase && api.BindBufferRange && api.UniformBlockBinding; }

IndexedBuffer currentUbo(GLuint index) {
    auto& bindings = live().indexedBuffers;
    const std::uint64_t key = (std::uint64_t{0x8A11} << 32) | index;
    const auto found = bindings.find(key);
    if (found != bindings.end()) return found->second;
    GLint buffer = 0;
    std::int64_t offset = 0, size = 0;
    api.GetIntegeri_v(0x8A28, index, &buffer);
    if (buffer) {
        api.GetInteger64i_v(0x8A29, index, &offset);
        api.GetInteger64i_v(0x8A2A, index, &size);
    }
    const IndexedBuffer binding{static_cast<GLuint>(buffer), static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), buffer != 0 && size > 0};
    bindings.emplace(key, binding);
    return binding;
}

void bindUbo(GLuint index, const IndexedBuffer& binding) {
    if (binding.buffer && binding.range) api.BindBufferRange(0x8A11, index, binding.buffer, binding.offset, binding.size);
    else api.BindBufferBase(0x8A11, index, binding.buffer);
}

bool replayUbo(GLuint index, const IndexedBuffer& binding) {
    auto& bindings = live().replayUbos;
    auto found = bindings.find(index);
    if (found == bindings.end()) found = bindings.emplace(index, currentUbo(index)).first;
    if (found->second == binding) return false;
    bindUbo(index, binding);
    found->second = binding;
    return true;
}

void primeBlocks(GLuint program, Program& state) {
    if (state.blocksReady) return;
    state.blocksReady = true;
    state.uniformBuffers.clear();
    state.uniformBlockBindings.clear();
    if (!captureUbos()) return;
    GLint count = 0;
    real.GetProgramiv(program, 0x8A36, &count);
    if (count < 0 || count > 256) { state.supported = false; return; }
    state.uniformBlockBindings.resize(static_cast<std::size_t>(count));
    for (GLint block = 0; block < count; ++block) {
        GLint binding = 0;
        api.GetActiveUniformBlockiv(program, static_cast<GLuint>(block), 0x8A3F, &binding);
        if (binding < 0) { state.supported = false; return; }
        const auto index = static_cast<GLuint>(binding);
        state.uniformBlockBindings[static_cast<std::size_t>(block)] = index;
        if (std::find(state.uniformBuffers.begin(), state.uniformBuffers.end(), index) == state.uniformBuffers.end()) state.uniformBuffers.push_back(index);
    }
}

Program& primeProgram(GLuint program) {
    auto& p = live().programs[program];
    if (p.initialized) { primeBlocks(program, p); return p; }
    p.initialized = true;
    primeBlocks(program, p);
    if (!p.supported) return p;
    GLint count = 0, maxLength = 0;
    real.GetProgramiv(program, kActiveUniforms, &count);
    real.GetProgramiv(program, kActiveUniformMaxLength, &maxLength);
    if (count < 0 || count > 4096 || maxLength < 0 || maxLength > 65536) {
        p.supported = false;

        return p;
    }
    std::vector<char> name(static_cast<std::size_t>(std::max(1, maxLength)));
    for (GLint i = 0; i < count; ++i) {
        GLsizei length = 0;
        GLint arraySize = 0;
        GLenum type = 0;
        api.GetActiveUniform(program, static_cast<GLuint>(i), static_cast<GLsizei>(name.size()), &length, &arraySize, &type, name.data());
        std::string uniformName(name.data(), static_cast<std::size_t>(length));
        const GLint location = real.GetUniformLocation(program, uniformName.c_str());
        if (location < 0) continue;
        Uniform u;
        u.location = location;
        u.type = type;
        u.count = arraySize;
        u.sampler = samplerTarget(type);
        std::string cameraName = uniformName;
        if (cameraName.ends_with("[0]")) cameraName.resize(cameraName.size() - 3);
        u.camera = arraySize == 1 && (cameraName == "u_view" || cameraName == "u_proj" || cameraName == "u_viewProj" ||
                                     cameraName == "u_modelView" || cameraName == "u_modelViewProj");
        if (arraySize <= 0 || arraySize > 4096 || !uniformKind(type, u.kind)) {
            p.supported = false;
            break;
        }
        const int width = components(u.kind);
        u.values.resize(static_cast<std::size_t>(arraySize * width));
        const auto marker = uniformName.find("[0]");
        for (int element = 0; element < arraySize; ++element) {
            std::string elementName = uniformName;
            if (arraySize > 1) {
                if (marker == std::string::npos) elementName += "[" + std::to_string(element) + "]";
                else elementName.replace(marker, 3, "[" + std::to_string(element) + "]");
            }
            const GLint elementLocation = real.GetUniformLocation(program, elementName.c_str());
            if (elementLocation != location + element) {
                p.supported = false;
                break;
            }
            void* output = u.values.data() + element * width;
            const int k = static_cast<int>(u.kind);
            if (k >= 4 && k < 8) api.GetUniformiv(program, elementLocation, static_cast<GLint*>(output));
            else if (k >= 8 && k < 12) api.GetUniformuiv(program, elementLocation, static_cast<GLuint*>(output));
            else api.GetUniformfv(program, elementLocation, static_cast<GLfloat*>(output));
            p.locations.emplace(elementLocation, UniformLocation{p.uniforms.size(), element});
        }
        if (!p.supported) break;
        p.uniforms.push_back(std::move(u));
    }

    return p;
}

std::size_t queuedBytes() {
    const auto& s = live();
    return s.words.size() * 4 + s.draws.size() * sizeof(DrawCommand) + s.uniforms.size() * sizeof(UniformCommand) +
           s.bindings.size() * sizeof(TextureCommand) + s.indexedCommands.size() * sizeof(IndexedCommand) + s.commands.size() * sizeof(Command);
}
void appendUniform(GLuint program, UniformLocation slot, Kind kind, GLsizei count, const void* values) {
    auto& s = live();
    const std::size_t length = static_cast<std::size_t>(count) * components(kind);
    if (!length || length * 4 > kMemoryLimit) return;
    const std::size_t offset = s.words.size();
    s.words.resize(offset + length);
    std::memcpy(s.words.data() + offset, values, length * 4);
    s.commands.push_back({CommandKind::Uniform, s.uniforms.size()});
    s.uniforms.push_back({program, slot, count, offset});
}
void remember(Kind kind, GLint location, GLsizei count, const void* values) {
    if (location < 0 || count <= 0 || !values) return;
    auto& s = live();
    auto found = s.programs.find(s.program);
    if (found == s.programs.end() || !found->second.initialized || !found->second.supported) return;
    auto& p = found->second;
    const auto slot = p.locations.find(location);
    if (slot == p.locations.end()) return;
    auto& u = p.uniforms[slot->second.index];
    std::vector<std::uint32_t> converted;
    if (u.kind != kind && (u.type == 0x8B56 || (u.type >= 0x8B57 && u.type <= 0x8B59)) && components(kind) == components(u.kind) && static_cast<int>(kind) < 12) {
        count = std::min(count, u.count - slot->second.element);
        converted.resize(static_cast<std::size_t>(count) * components(kind));
        for (std::size_t i = 0; i < converted.size(); ++i) {
            if (static_cast<int>(kind) < 4) converted[i] = static_cast<const GLfloat*>(values)[i] != 0.0f;
            else if (static_cast<int>(kind) < 8) converted[i] = static_cast<const GLint*>(values)[i] != 0;
            else converted[i] = static_cast<const GLuint*>(values)[i] != 0;
        }
        kind = u.kind;
        values = converted.data();
    }
    if (u.kind != kind) {
        flush();
        p.supported = false;

        return;
    }
    count = std::min(count, u.count - slot->second.element);
    if (count <= 0) return;
    const std::size_t size = static_cast<std::size_t>(count) * components(kind) * 4;
    auto* destination = u.values.data() + slot->second.element * components(kind);
    if (std::memcmp(destination, values, size) == 0) return;
    if (!u.camera && p.serial == s.serial && !s.draws.empty()) {
        if (queuedBytes() + size > kMemoryLimit) flush();
        if (p.serial == s.serial) appendUniform(s.program, slot->second, kind, count, values);
    }
    std::memcpy(destination, values, size);
}

bool updateReplay(Uniform& uniform, int element, GLsizei count, const void* values) {
    const auto offset = static_cast<std::size_t>(element) * components(uniform.kind);
    const auto bytes = static_cast<std::size_t>(count) * components(uniform.kind) * sizeof(std::uint32_t);
    const auto serial = live().serial;
    const auto& current = uniform.replaySerial == serial ? uniform.replayValues : uniform.values;
    if (std::memcmp(current.data() + offset, values, bytes) == 0) return false;
    if (uniform.replaySerial != serial) {
        uniform.replayValues.assign(uniform.values.begin(), uniform.values.end());
        uniform.replaySerial = serial;
    }
    std::memcpy(uniform.replayValues.data() + offset, values, bytes);
    return true;
}

void issue(const DrawCall& draw) {
    switch (draw.kind) {
        case DrawKind::Elements: real.DrawElements(draw.mode, draw.count, draw.type, draw.indices); break;
        case DrawKind::Arrays: real.DrawArrays(draw.mode, draw.first, draw.count); break;
        case DrawKind::ElementsInstanced: real.DrawElementsInstanced(draw.mode, draw.count, draw.type, draw.indices, draw.instances); break;
        case DrawKind::ArraysInstanced: real.DrawArraysInstanced(draw.mode, draw.first, draw.count, draw.instances); break;
    }
}
void enabled(GLenum cap, bool value) { if (value) real.Enable(cap); else real.Disable(cap); }
struct Saved {
    GLint framebuffer{}, program{}, vao{}, active{}, viewport[4]{}, depthFunc{}, cullFace{}, frontFace{};
    GLboolean depthTest{}, blend{}, scissor{}, stencil{}, cull{}, discard{}, offset{}, depthMask{}, colorMask[4]{};
    float depthRange[2]{}, factor{}, units{};
};
Saved save() {
    Saved s;
    real.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &s.framebuffer);
    real.GetIntegerv(GL_CURRENT_PROGRAM, &s.program);
    real.GetIntegerv(GL_VERTEX_ARRAY_BINDING, &s.vao);
    real.GetIntegerv(GL_ACTIVE_TEXTURE, &s.active);
    real.GetIntegerv(GL_VIEWPORT, s.viewport);
    real.GetIntegerv(GL_DEPTH_FUNC, &s.depthFunc);
    real.GetIntegerv(kCullFaceMode, &s.cullFace);
    real.GetIntegerv(kFrontFace, &s.frontFace);
    real.GetFloatv(GL_DEPTH_RANGE, s.depthRange);
    real.GetFloatv(kPolygonFactor, &s.factor);
    real.GetFloatv(kPolygonUnits, &s.units);
    s.depthTest = real.IsEnabled(GL_DEPTH_TEST);
    s.blend = real.IsEnabled(GL_BLEND);
    s.scissor = real.IsEnabled(GL_SCISSOR_TEST);
    s.stencil = real.IsEnabled(GL_STENCIL_TEST);
    s.cull = real.IsEnabled(GL_CULL_FACE);
    s.discard = real.IsEnabled(GL_RASTERIZER_DISCARD);
    s.offset = real.IsEnabled(kPolygonOffset);
    real.GetBooleanv(GL_DEPTH_WRITEMASK, &s.depthMask);
    real.GetBooleanv(GL_COLOR_WRITEMASK, s.colorMask);
    return s;
}
void restore(const Saved& s) {
    real.BindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(s.framebuffer));
    real.Viewport(s.viewport[0], s.viewport[1], s.viewport[2], s.viewport[3]);
    real.UseProgram(static_cast<GLuint>(s.program));
    real.BindVertexArray(static_cast<GLuint>(s.vao));
    real.ActiveTexture(static_cast<GLenum>(s.active));
    real.DepthFunc(static_cast<GLenum>(s.depthFunc));
    real.DepthRangef(s.depthRange[0], s.depthRange[1]);
    real.DepthMask(s.depthMask);
    real.ColorMask(s.colorMask[0], s.colorMask[1], s.colorMask[2], s.colorMask[3]);
    api.CullFace(static_cast<GLenum>(s.cullFace));
    api.FrontFace(static_cast<GLenum>(s.frontFace));
    api.PolygonOffset(s.factor, s.units);
    enabled(GL_DEPTH_TEST, s.depthTest);
    enabled(GL_BLEND, s.blend);
    enabled(GL_SCISSOR_TEST, s.scissor);
    enabled(GL_STENCIL_TEST, s.stencil);
    enabled(GL_CULL_FACE, s.cull);
    enabled(GL_RASTERIZER_DISCARD, s.discard);
    enabled(kPolygonOffset, s.offset);
}

void hookUniform1fv(GLint location, GLsizei count, const GLfloat* value) {
    remember(Kind::F1, location, count, value);
    forward(origUniform1fv, api.Uniform1fv)(location, count, value);
}
void hookUniform1f(GLint location, GLfloat v0) {
    const GLfloat values[] = {v0};
    remember(Kind::F1, location, 1, values);
    forward(origUniform1f, api.Uniform1f)(location, v0);
}
void hookUniform2fv(GLint location, GLsizei count, const GLfloat* value) {
    remember(Kind::F2, location, count, value);
    forward(origUniform2fv, api.Uniform2fv)(location, count, value);
}
void hookUniform2f(GLint location, GLfloat v0, GLfloat v1) {
    const GLfloat values[] = {v0, v1};
    remember(Kind::F2, location, 1, values);
    forward(origUniform2f, api.Uniform2f)(location, v0, v1);
}
void hookUniform3fv(GLint location, GLsizei count, const GLfloat* value) {
    remember(Kind::F3, location, count, value);
    forward(origUniform3fv, api.Uniform3fv)(location, count, value);
}
void hookUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    const GLfloat values[] = {v0, v1, v2};
    remember(Kind::F3, location, 1, values);
    forward(origUniform3f, api.Uniform3f)(location, v0, v1, v2);
}
void hookUniform4fv(GLint location, GLsizei count, const GLfloat* value) {
    remember(Kind::F4, location, count, value);
    forward(origUniform4fv, api.Uniform4fv)(location, count, value);
}
void hookUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    const GLfloat values[] = {v0, v1, v2, v3};
    remember(Kind::F4, location, 1, values);
    forward(origUniform4f, api.Uniform4f)(location, v0, v1, v2, v3);
}
void hookUniform1iv(GLint location, GLsizei count, const GLint* value) {
    remember(Kind::I1, location, count, value);
    forward(origUniform1iv, api.Uniform1iv)(location, count, value);
}
void hookUniform1i(GLint location, GLint v0) {
    const GLint values[] = {v0};
    remember(Kind::I1, location, 1, values);
    forward(origUniform1i, api.Uniform1i)(location, v0);
}
void hookUniform2iv(GLint location, GLsizei count, const GLint* value) {
    remember(Kind::I2, location, count, value);
    forward(origUniform2iv, api.Uniform2iv)(location, count, value);
}
void hookUniform2i(GLint location, GLint v0, GLint v1) {
    const GLint values[] = {v0, v1};
    remember(Kind::I2, location, 1, values);
    forward(origUniform2i, api.Uniform2i)(location, v0, v1);
}
void hookUniform3iv(GLint location, GLsizei count, const GLint* value) {
    remember(Kind::I3, location, count, value);
    forward(origUniform3iv, api.Uniform3iv)(location, count, value);
}
void hookUniform3i(GLint location, GLint v0, GLint v1, GLint v2) {
    const GLint values[] = {v0, v1, v2};
    remember(Kind::I3, location, 1, values);
    forward(origUniform3i, api.Uniform3i)(location, v0, v1, v2);
}
void hookUniform4iv(GLint location, GLsizei count, const GLint* value) {
    remember(Kind::I4, location, count, value);
    forward(origUniform4iv, api.Uniform4iv)(location, count, value);
}
void hookUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    const GLint values[] = {v0, v1, v2, v3};
    remember(Kind::I4, location, 1, values);
    forward(origUniform4i, api.Uniform4i)(location, v0, v1, v2, v3);
}
void hookUniform1uiv(GLint location, GLsizei count, const GLuint* value) {
    remember(Kind::U1, location, count, value);
    forward(origUniform1uiv, api.Uniform1uiv)(location, count, value);
}
void hookUniform1ui(GLint location, GLuint v0) {
    const GLuint values[] = {v0};
    remember(Kind::U1, location, 1, values);
    forward(origUniform1ui, api.Uniform1ui)(location, v0);
}
void hookUniform2uiv(GLint location, GLsizei count, const GLuint* value) {
    remember(Kind::U2, location, count, value);
    forward(origUniform2uiv, api.Uniform2uiv)(location, count, value);
}
void hookUniform2ui(GLint location, GLuint v0, GLuint v1) {
    const GLuint values[] = {v0, v1};
    remember(Kind::U2, location, 1, values);
    forward(origUniform2ui, api.Uniform2ui)(location, v0, v1);
}
void hookUniform3uiv(GLint location, GLsizei count, const GLuint* value) {
    remember(Kind::U3, location, count, value);
    forward(origUniform3uiv, api.Uniform3uiv)(location, count, value);
}
void hookUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2) {
    const GLuint values[] = {v0, v1, v2};
    remember(Kind::U3, location, 1, values);
    forward(origUniform3ui, api.Uniform3ui)(location, v0, v1, v2);
}
void hookUniform4uiv(GLint location, GLsizei count, const GLuint* value) {
    remember(Kind::U4, location, count, value);
    forward(origUniform4uiv, api.Uniform4uiv)(location, count, value);
}
void hookUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    const GLuint values[] = {v0, v1, v2, v3};
    remember(Kind::U4, location, 1, values);
    forward(origUniform4ui, api.Uniform4ui)(location, v0, v1, v2, v3);
}
void hookUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    if (!transpose) remember(Kind::M2, location, count, value);
    forward(origUniformMatrix2fv, api.UniformMatrix2fv)(location, count, transpose, value);
}
void hookUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    if (!transpose) remember(Kind::M3, location, count, value);
    forward(origUniformMatrix3fv, api.UniformMatrix3fv)(location, count, transpose, value);
}
void hookUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    if (!transpose) remember(Kind::M2x3, location, count, value);
    forward(origUniformMatrix2x3fv, api.UniformMatrix2x3fv)(location, count, transpose, value);
}
void hookUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    if (!transpose) remember(Kind::M3x2, location, count, value);
    forward(origUniformMatrix3x2fv, api.UniformMatrix3x2fv)(location, count, transpose, value);
}
void hookUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    if (!transpose) remember(Kind::M2x4, location, count, value);
    forward(origUniformMatrix2x4fv, api.UniformMatrix2x4fv)(location, count, transpose, value);
}
void hookUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    if (!transpose) remember(Kind::M4x2, location, count, value);
    forward(origUniformMatrix4x2fv, api.UniformMatrix4x2fv)(location, count, transpose, value);
}
void hookUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    if (!transpose) remember(Kind::M3x4, location, count, value);
    forward(origUniformMatrix3x4fv, api.UniformMatrix3x4fv)(location, count, transpose, value);
}
void hookUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    if (!transpose) remember(Kind::M4x3, location, count, value);
    forward(origUniformMatrix4x3fv, api.UniformMatrix4x3fv)(location, count, transpose, value);
}
void hookActiveTexture(GLenum texture) {
    live().active = static_cast<int>(texture - GL_TEXTURE0);
    forward(origActiveTexture, api.ActiveTexture)(texture);
}
void hookBindTexture(GLenum target, GLuint texture) {
    trackTexture(target, texture);
    forward(origBindTexture, api.BindTexture)(target, texture);
}
void hookBindSampler(GLuint unit, GLuint sampler) {
    trackSampler(unit, sampler);
    forward(origBindSampler, api.BindSampler)(unit, sampler);
}
void hookBindVertexArray(GLuint array) {
    live().vao = array; live().haveVao = true;
    forward(origBindVertexArray, api.BindVertexArray)(array);
}
void hookCullFace(GLenum mode) {
    live().pipeline.cullFace = mode;
    forward(origCullFace, api.CullFace)(mode);
}
void hookFrontFace(GLenum mode) {
    live().pipeline.frontFace = mode;
    forward(origFrontFace, api.FrontFace)(mode);
}
void hookPolygonOffset(GLfloat factor, GLfloat units) {
    live().pipeline.factor = factor; live().pipeline.units = units;
    forward(origPolygonOffset, api.PolygonOffset)(factor, units);
}
void hookBindBuffer(GLenum target, GLuint buffer) {
    auto& s = live();
    if (target == 0x8893) {
        auto& layout = s.layouts[vertexArray()];
        if ((!layout.haveElement || layout.element != buffer) && queuedVao()) preserveQueuedVao(vertexArray());
        layout.element = buffer;
        layout.haveElement = true;
    } else if (target == 0x8892) {
        s.arrayBuffer = buffer;
        s.haveArrayBuffer = true;
    }
    forward(origBindBuffer, api.BindBuffer)(target, buffer);
}

void hookDepthRangef(GLfloat nearValue, GLfloat farValue) {
    live().pipeline.depthRange[0] = nearValue;
    live().pipeline.depthRange[1] = farValue;
    forward(origDepthRangef, api.DepthRangef)(nearValue, farValue);
}
void hookBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
    flush();

    forward(origBufferData, api.BufferData)(target, size, data, usage);
}
void hookBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
    flush();

    forward(origBufferSubData, api.BufferSubData)(target, offset, size, data);
}
void hookCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
    flush();

    forward(origCopyBufferSubData, api.CopyBufferSubData)(readTarget, writeTarget, readOffset, writeOffset, size);
}
void* hookMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access) {
    flush();

    auto result = forward(origMapBufferRange, api.MapBufferRange)(target, offset, length, access);

    return result;
}
void hookFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) {
    flush();

    forward(origFlushMappedBufferRange, api.FlushMappedBufferRange)(target, offset, length);
}
GLboolean hookUnmapBuffer(GLenum target) {
    flush();

    auto result = forward(origUnmapBuffer, api.UnmapBuffer)(target);

    return result;
}
void hookDeleteBuffers(GLsizei count, const GLuint* buffers) {
    flush();
    invalidateVertexBuffers(count, buffers);
    forward(origDeleteBuffers, api.DeleteBuffers)(count, buffers);

    auto& state = live();
    for (GLsizei i = 0; i < count; ++i) {
        if (state.arrayBuffer == buffers[i]) state.haveArrayBuffer = false;
        for (auto& entry : state.layouts) {
            auto& layout = entry.second;
            if (layout.element == buffers[i]) layout.haveElement = false;
            for (auto& attribute : layout.attributes) if (attribute.format.buffer == buffers[i]) attribute.haveFormat = false;
        }
    }
}
void hookBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    if (!unchangedIndexed(target, index, buffer, 0, 0, false) && (target != 0x8A11 || !captureUbos())) flush();

    forward(origBindBufferBase, api.BindBufferBase)(target, index, buffer);
}
void hookBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
    if (!unchangedIndexed(target, index, buffer, offset, size, true) && (target != 0x8A11 || !captureUbos())) flush();

    forward(origBindBufferRange, api.BindBufferRange)(target, index, buffer, offset, size);
}
void hookDeleteTextures(GLsizei count, const GLuint* textures) {
    flush();
    forgetParameters(count, textures, false);

    forward(origDeleteTextures, api.DeleteTextures)(count, textures);
    for (auto& unit : live().textures) for (auto& binding : unit.targets) for (GLsizei i = 0; i < count; ++i) if (binding == textures[i]) binding = 0;
}
void hookDeleteSamplers(GLsizei count, const GLuint* samplers) {
    flush();
    forgetParameters(count, samplers, true);

    forward(origDeleteSamplers, api.DeleteSamplers)(count, samplers);
    for (auto& unit : live().textures) for (GLsizei i = 0; i < count; ++i) if (unit.sampler == samplers[i]) unit.sampler = 0;
}
void hookSamplerParameteri(GLuint sampler, GLenum pname, GLint param) {
    parameterChange(sampler, 0, pname, &param, 1, true, true);

    forward(origSamplerParameteri, api.SamplerParameteri)(sampler, pname, param);
}
void hookSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param) {
    parameterChange(sampler, 0, pname, &param, 1, false, true);

    forward(origSamplerParameterf, api.SamplerParameterf)(sampler, pname, param);
}
void hookSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat* params) {
    parameterChange(sampler, 0, pname, params, pname == 0x1004 ? 4 : 1, false, true);

    forward(origSamplerParameterfv, api.SamplerParameterfv)(sampler, pname, params);
}
void hookTexParameteri(GLenum target, GLenum pname, GLint param) {
    if (targetIndex(target) < 0) flush();
    else parameterChange(boundTexture(target), target, pname, &param, 1, true, false);

    forward(origTexParameteri, api.TexParameteri)(target, pname, param);
}
void hookTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    if (targetIndex(target) < 0) flush();
    else parameterChange(boundTexture(target), target, pname, &param, 1, false, false);

    forward(origTexParameterf, api.TexParameterf)(target, pname, param);
}
void hookTexParameterfv(GLenum target, GLenum pname, const GLfloat* params) {
    if (targetIndex(target) < 0) flush();
    else parameterChange(boundTexture(target), target, pname, params, pname == 0x1004 || pname == 0x8E46 ? 4 : 1, false, false);

    forward(origTexParameterfv, api.TexParameterfv)(target, pname, params);
}
void hookTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels) {
    flush();

    forward(origTexImage2D, api.TexImage2D)(target, level, internalformat, width, height, border, format, type, pixels);
}
void hookTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels) {
    flush();

    forward(origTexSubImage2D, api.TexSubImage2D)(target, level, xoffset, yoffset, width, height, format, type, pixels);
}
void hookTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
    flush();

    forward(origTexStorage2D, api.TexStorage2D)(target, levels, internalformat, width, height);
}
void hookTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* pixels) {
    flush();

    forward(origTexImage3D, api.TexImage3D)(target, level, internalformat, width, height, depth, border, format, type, pixels);
}
void hookTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels) {
    flush();

    forward(origTexSubImage3D, api.TexSubImage3D)(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}
void hookTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
    flush();

    forward(origTexStorage3D, api.TexStorage3D)(target, levels, internalformat, width, height, depth);
}
void hookCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void* data) {
    flush();

    forward(origCompressedTexImage2D, api.CompressedTexImage2D)(target, level, internalformat, width, height, border, imageSize, data);
}
void hookCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void* data) {
    flush();

    forward(origCompressedTexSubImage2D, api.CompressedTexSubImage2D)(target, level, xoffset, yoffset, width, height, format, imageSize, data);
}
void hookCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void* data) {
    flush();

    forward(origCompressedTexImage3D, api.CompressedTexImage3D)(target, level, internalformat, width, height, depth, border, imageSize, data);
}
void hookCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void* data) {
    flush();

    forward(origCompressedTexSubImage3D, api.CompressedTexSubImage3D)(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize, data);
}
void hookCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    flush();

    forward(origCopyTexSubImage2D, api.CopyTexSubImage2D)(target, level, xoffset, yoffset, x, y, width, height);
}
void hookGenerateMipmap(GLenum target) {
    flush();

    forward(origGenerateMipmap, api.GenerateMipmap)(target);
}
void hookDeleteProgram(GLuint program) {
    flush();
    live().programs.erase(program); ++live().generations[program];
    forward(origDeleteProgram, api.DeleteProgram)(program);
}
void hookLinkProgram(GLuint program) {
    flush();
    live().programs.erase(program); ++live().generations[program];
    forward(origLinkProgram, api.LinkProgram)(program);
}
void hookProgramBinary(GLuint program, GLenum format, const void* binary, GLsizei length) {
    flush();
    live().programs.erase(program); ++live().generations[program];
    forward(origProgramBinary, api.ProgramBinary)(program, format, binary, length);
}
void hookDeleteFramebuffers(GLsizei count, const GLuint* framebuffers) {
    flush();

    forward(origDeleteFramebuffers, api.DeleteFramebuffers)(count, framebuffers);
}
void hookFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
    flush();

    forward(origFramebufferTexture2D, api.FramebufferTexture2D)(target, attachment, textarget, texture, level);
}
void hookFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer) {
    flush();

    forward(origFramebufferTextureLayer, api.FramebufferTextureLayer)(target, attachment, texture, level, layer);
}
void hookFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
    flush();

    forward(origFramebufferRenderbuffer, api.FramebufferRenderbuffer)(target, attachment, renderbuffertarget, renderbuffer);
}
void hookRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
    flush();

    forward(origRenderbufferStorage, api.RenderbufferStorage)(target, internalformat, width, height);
}
void hookRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) {
    flush();

    forward(origRenderbufferStorageMultisample, api.RenderbufferStorageMultisample)(target, samples, internalformat, width, height);
}
void hookDeleteRenderbuffers(GLsizei count, const GLuint* renderbuffers) {
    flush();

    forward(origDeleteRenderbuffers, api.DeleteRenderbuffers)(count, renderbuffers);
}
void hookClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat* value) {
    if (buffer == GL_DEPTH && depthClearObserver) depthClearObserver();
    flush();

    forward(origClearBufferfv, api.ClearBufferfv)(buffer, drawbuffer, value);
}
void hookClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint* value) {
    flush();

    forward(origClearBufferiv, api.ClearBufferiv)(buffer, drawbuffer, value);
}
void hookClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint* value) {
    flush();

    forward(origClearBufferuiv, api.ClearBufferuiv)(buffer, drawbuffer, value);
}
void hookClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
    if (buffer == 0x84F9 && depthClearObserver) depthClearObserver();
    flush();

    forward(origClearBufferfi, api.ClearBufferfi)(buffer, drawbuffer, depth, stencil);
}
void hookInvalidateFramebuffer(GLenum target, GLsizei count, const GLenum* attachments) {
    if (depthClearObserver) for (GLsizei i = 0; i < count; ++i) {
        if (attachments[i] == GL_DEPTH || attachments[i] == GL_DEPTH_ATTACHMENT || attachments[i] == GL_DEPTH_STENCIL_ATTACHMENT) { depthClearObserver(); break; }
    }
    flush();

    forward(origInvalidateFramebuffer, api.InvalidateFramebuffer)(target, count, attachments);
}
void hookBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
    flush();

    forward(origBlitFramebuffer, api.BlitFramebuffer)(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}
void hookDispatchCompute(GLuint x, GLuint y, GLuint z) {
    flush();

    forward(origDispatchCompute, api.DispatchCompute)(x, y, z);
}
void hookDispatchComputeIndirect(GLintptr offset) {
    flush();

    forward(origDispatchComputeIndirect, api.DispatchComputeIndirect)(offset);
}
void hookMemoryBarrier(GLbitfield barriers) {
    flush();

    forward(origMemoryBarrier, api.MemoryBarrier)(barriers);
}
void hookBindImageTexture(GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format) {
    flush();

    forward(origBindImageTexture, api.BindImageTexture)(unit, texture, level, layered, layer, access, format);
}
void hookBeginQuery(GLenum target, GLuint id) {
    flush();

    forward(origBeginQuery, api.BeginQuery)(target, id);
    ++live().queryDepth;
}
void hookEndQuery(GLenum target) {
    flush();

    forward(origEndQuery, api.EndQuery)(target);
    if (live().queryDepth) --live().queryDepth;
}
void hookBeginTransformFeedback(GLenum mode) {
    flush();

    forward(origBeginTransformFeedback, api.BeginTransformFeedback)(mode);
    live().transformFeedback = true;
}
void hookEndTransformFeedback() {
    flush();

    forward(origEndTransformFeedback, api.EndTransformFeedback)();
    live().transformFeedback = false;
}
GLuint arrayBuffer() {
    auto& s = live();
    if (!s.haveArrayBuffer) {
        GLint value = 0;
        real.GetIntegerv(0x8894, &value);
        s.arrayBuffer = static_cast<GLuint>(value);
        s.haveArrayBuffer = true;
    }
    return s.arrayBuffer;
}
void attributeEnabled(GLuint index, bool enabledValue) {
    if (index >= 32) { flush(); return; }
    auto& attribute = live().layouts[vertexArray()].attributes[index];
    if ((!attribute.haveEnabled || attribute.enabled != enabledValue) && queuedVao()) preserveQueuedVao(vertexArray());
    attribute.enabled = enabledValue;
    attribute.haveEnabled = true;
}
void attributeFormat(GLuint index, const AttributeFormat& format) {
    if (index >= 32) { flush(); return; }
    auto& attribute = live().layouts[vertexArray()].attributes[index];
    if ((!attribute.haveFormat || !(attribute.format == format)) && queuedVao()) preserveQueuedVao(vertexArray());
    attribute.format = format;
    attribute.haveFormat = true;
}
void hookEnableVertexAttribArray(GLuint index) {
    attributeEnabled(index, true);
    forward(origEnableVertexAttribArray, api.EnableVertexAttribArray)(index);
}
void hookDisableVertexAttribArray(GLuint index) {
    attributeEnabled(index, false);
    forward(origDisableVertexAttribArray, api.DisableVertexAttribArray)(index);
}
void hookVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) {
    attributeFormat(index, {size, type, normalized, stride, pointer, arrayBuffer(), false});
    forward(origVertexAttribPointer, api.VertexAttribPointer)(index, size, type, normalized, stride, pointer);
}
void hookVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer) {
    attributeFormat(index, {size, type, 0, stride, pointer, arrayBuffer(), true});
    forward(origVertexAttribIPointer, api.VertexAttribIPointer)(index, size, type, stride, pointer);
}
void hookVertexAttribDivisor(GLuint index, GLuint divisor) {
    if (index >= 32) {
        flush();
        forward(origVertexAttribDivisor, api.VertexAttribDivisor)(index, divisor);
        return;
    }
    auto& attribute = live().layouts[vertexArray()].attributes[index];
    if ((!attribute.haveDivisor || attribute.divisor != divisor) && queuedVao()) preserveQueuedVao(vertexArray());
    attribute.divisor = divisor;
    attribute.haveDivisor = true;
    forward(origVertexAttribDivisor, api.VertexAttribDivisor)(index, divisor);
}
void hookDeleteVertexArrays(GLsizei count, const GLuint* arrays) {
    for (GLsizei i = 0; i < count; ++i) preserveQueuedVao(arrays[i]);
    forward(origDeleteVertexArrays, api.DeleteVertexArrays)(count, arrays);
    for (GLsizei i = 0; i < count; ++i) {
        live().layouts.erase(arrays[i]);
        live().pendingVaoDraws.erase(arrays[i]);
    }
    live().haveVao = false;
}
void hookCopyImageSubData(GLuint source, GLenum sourceTarget, GLint sourceLevel, GLint sourceX, GLint sourceY, GLint sourceZ, GLuint destination, GLenum destinationTarget, GLint destinationLevel, GLint destinationX, GLint destinationY, GLint destinationZ, GLsizei width, GLsizei height, GLsizei depth) {
    flush();
    forward(origCopyImageSubData, api.CopyImageSubData)(source, sourceTarget, sourceLevel, sourceX, sourceY, sourceZ, destination, destinationTarget, destinationLevel, destinationX, destinationY, destinationZ, width, height, depth);
}
void hookDrawArraysIndirect(GLenum mode, const void* indirect) {
    flush();
    forward(origDrawArraysIndirect, api.DrawArraysIndirect)(mode, indirect);
}
void hookDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect) {
    flush();
    forward(origDrawElementsIndirect, api.DrawElementsIndirect)(mode, type, indirect);
}
void hookMultiDrawArraysIndirect(GLenum mode, const void* indirect, GLsizei drawCount, GLsizei stride) {
    flush();
    forward(origMultiDrawArraysIndirect, api.MultiDrawArraysIndirect)(mode, indirect, drawCount, stride);
}
void hookMultiDrawElementsIndirect(GLenum mode, GLenum type, const void* indirect, GLsizei drawCount, GLsizei stride) {
    flush();
    forward(origMultiDrawElementsIndirect, api.MultiDrawElementsIndirect)(mode, type, indirect, drawCount, stride);
}
void hookInvalidateSubFramebuffer(GLenum target, GLsizei count, const GLenum* attachments, GLint x, GLint y, GLsizei width, GLsizei height) {
    if (depthClearObserver) for (GLsizei i = 0; i < count; ++i) {
        if (attachments[i] == GL_DEPTH || attachments[i] == GL_DEPTH_ATTACHMENT || attachments[i] == GL_DEPTH_STENCIL_ATTACHMENT) { depthClearObserver(); break; }
    }
    flush();
    forward(origInvalidateSubFramebuffer, api.InvalidateSubFramebuffer)(target, count, attachments, x, y, width, height);
}
void hookVertexAttrib1f(GLuint index, GLfloat v0) {
    flush();
    forward(origVertexAttrib1f, api.VertexAttrib1f)(index, v0);
}
void hookVertexAttrib1fv(GLuint index, const GLfloat* values) {
    flush();
    forward(origVertexAttrib1fv, api.VertexAttrib1fv)(index, values);
}
void hookVertexAttrib2f(GLuint index, GLfloat v0, GLfloat v1) {
    flush();
    forward(origVertexAttrib2f, api.VertexAttrib2f)(index, v0, v1);
}
void hookVertexAttrib2fv(GLuint index, const GLfloat* values) {
    flush();
    forward(origVertexAttrib2fv, api.VertexAttrib2fv)(index, values);
}
void hookVertexAttrib3f(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2) {
    flush();
    forward(origVertexAttrib3f, api.VertexAttrib3f)(index, v0, v1, v2);
}
void hookVertexAttrib3fv(GLuint index, const GLfloat* values) {
    flush();
    forward(origVertexAttrib3fv, api.VertexAttrib3fv)(index, values);
}
void hookVertexAttrib4f(GLuint index, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    flush();
    forward(origVertexAttrib4f, api.VertexAttrib4f)(index, v0, v1, v2, v3);
}
void hookVertexAttrib4fv(GLuint index, const GLfloat* values) {
    flush();
    forward(origVertexAttrib4fv, api.VertexAttrib4fv)(index, values);
}
void hookVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w) {
    flush();
    forward(origVertexAttribI4i, api.VertexAttribI4i)(index, x, y, z, w);
}
void hookVertexAttribI4iv(GLuint index, const GLint* values) {
    flush();
    forward(origVertexAttribI4iv, api.VertexAttribI4iv)(index, values);
}
void hookVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) {
    flush();
    forward(origVertexAttribI4ui, api.VertexAttribI4ui)(index, x, y, z, w);
}
void hookVertexAttribI4uiv(GLuint index, const GLuint* values) {
    flush();
    forward(origVertexAttribI4uiv, api.VertexAttribI4uiv)(index, values);
}
void hookTexParameteriv(GLenum target, GLenum pname, const GLint* values) {
    if (targetIndex(target) < 0) flush();
    else parameterChange(boundTexture(target), target, pname, values, pname == 0x1004 || pname == 0x8E46 ? 4 : 1, 1, false);
    forward(origTexParameteriv, api.TexParameteriv)(target, pname, values);
}
void hookTexParameterIiv(GLenum target, GLenum pname, const GLint* values) {
    if (targetIndex(target) < 0) flush();
    else parameterChange(boundTexture(target), target, pname, values, pname == 0x1004 || pname == 0x8E46 ? 4 : 1, 2, false);
    forward(origTexParameterIiv, api.TexParameterIiv)(target, pname, values);
}
void hookTexParameterIuiv(GLenum target, GLenum pname, const GLuint* values) {
    if (targetIndex(target) < 0) flush();
    else parameterChange(boundTexture(target), target, pname, values, pname == 0x1004 || pname == 0x8E46 ? 4 : 1, 3, false);
    forward(origTexParameterIuiv, api.TexParameterIuiv)(target, pname, values);
}
void hookSamplerParameteriv(GLuint sampler, GLenum pname, const GLint* values) {
    parameterChange(sampler, 0, pname, values, pname == 0x1004 ? 4 : 1, 1, true);
    forward(origSamplerParameteriv, api.SamplerParameteriv)(sampler, pname, values);
}
void hookSamplerParameterIiv(GLuint sampler, GLenum pname, const GLint* values) {
    parameterChange(sampler, 0, pname, values, pname == 0x1004 ? 4 : 1, 2, true);
    forward(origSamplerParameterIiv, api.SamplerParameterIiv)(sampler, pname, values);
}
void hookSamplerParameterIuiv(GLuint sampler, GLenum pname, const GLuint* values) {
    parameterChange(sampler, 0, pname, values, pname == 0x1004 ? 4 : 1, 3, true);
    forward(origSamplerParameterIuiv, api.SamplerParameterIuiv)(sampler, pname, values);
}
void hookUniformBlockBinding(GLuint program, GLuint block, GLuint binding) {
    auto& state = live();
    auto found = state.programs.find(program);
    const bool same = found != state.programs.end() && found->second.blocksReady &&
        block < found->second.uniformBlockBindings.size() && found->second.uniformBlockBindings[block] == binding;
    if (!same && found != state.programs.end() && found->second.serial == state.serial) flush();
    forward(origUniformBlockBinding, api.UniformBlockBinding)(program, block, binding);
    if (!same && found != state.programs.end()) found->second.blocksReady = false;
}
const Hook kHooks[] = {
    {"glUniformBlockBinding", reinterpret_cast<void*>(&hookUniformBlockBinding), &origUniformBlockBinding},
    {"glVertexAttrib1f", reinterpret_cast<void*>(&hookVertexAttrib1f), &origVertexAttrib1f},
    {"glVertexAttrib1fv", reinterpret_cast<void*>(&hookVertexAttrib1fv), &origVertexAttrib1fv},
    {"glVertexAttrib2f", reinterpret_cast<void*>(&hookVertexAttrib2f), &origVertexAttrib2f},
    {"glVertexAttrib2fv", reinterpret_cast<void*>(&hookVertexAttrib2fv), &origVertexAttrib2fv},
    {"glVertexAttrib3f", reinterpret_cast<void*>(&hookVertexAttrib3f), &origVertexAttrib3f},
    {"glVertexAttrib3fv", reinterpret_cast<void*>(&hookVertexAttrib3fv), &origVertexAttrib3fv},
    {"glVertexAttrib4f", reinterpret_cast<void*>(&hookVertexAttrib4f), &origVertexAttrib4f},
    {"glVertexAttrib4fv", reinterpret_cast<void*>(&hookVertexAttrib4fv), &origVertexAttrib4fv},
    {"glVertexAttribI4i", reinterpret_cast<void*>(&hookVertexAttribI4i), &origVertexAttribI4i},
    {"glVertexAttribI4iv", reinterpret_cast<void*>(&hookVertexAttribI4iv), &origVertexAttribI4iv},
    {"glVertexAttribI4ui", reinterpret_cast<void*>(&hookVertexAttribI4ui), &origVertexAttribI4ui},
    {"glVertexAttribI4uiv", reinterpret_cast<void*>(&hookVertexAttribI4uiv), &origVertexAttribI4uiv},
    {"glTexParameteriv", reinterpret_cast<void*>(&hookTexParameteriv), &origTexParameteriv},
    {"glTexParameterIiv", reinterpret_cast<void*>(&hookTexParameterIiv), &origTexParameterIiv},
    {"glTexParameterIuiv", reinterpret_cast<void*>(&hookTexParameterIuiv), &origTexParameterIuiv},
    {"glSamplerParameteriv", reinterpret_cast<void*>(&hookSamplerParameteriv), &origSamplerParameteriv},
    {"glSamplerParameterIiv", reinterpret_cast<void*>(&hookSamplerParameterIiv), &origSamplerParameterIiv},
    {"glSamplerParameterIuiv", reinterpret_cast<void*>(&hookSamplerParameterIuiv), &origSamplerParameterIuiv},
    {"glInvalidateSubFramebuffer", reinterpret_cast<void*>(&hookInvalidateSubFramebuffer), &origInvalidateSubFramebuffer},
    {"glMultiDrawElementsIndirect", reinterpret_cast<void*>(&hookMultiDrawElementsIndirect), &origMultiDrawElementsIndirect},
    {"glMultiDrawArraysIndirect", reinterpret_cast<void*>(&hookMultiDrawArraysIndirect), &origMultiDrawArraysIndirect},
    {"glDrawElementsIndirect", reinterpret_cast<void*>(&hookDrawElementsIndirect), &origDrawElementsIndirect},
    {"glDrawArraysIndirect", reinterpret_cast<void*>(&hookDrawArraysIndirect), &origDrawArraysIndirect},
    {"glCopyImageSubData", reinterpret_cast<void*>(&hookCopyImageSubData), &origCopyImageSubData},
    {"glUniform1fv", reinterpret_cast<void*>(&hookUniform1fv), &origUniform1fv},
    {"glUniform1f", reinterpret_cast<void*>(&hookUniform1f), &origUniform1f},
    {"glUniform2fv", reinterpret_cast<void*>(&hookUniform2fv), &origUniform2fv},
    {"glUniform2f", reinterpret_cast<void*>(&hookUniform2f), &origUniform2f},
    {"glUniform3fv", reinterpret_cast<void*>(&hookUniform3fv), &origUniform3fv},
    {"glUniform3f", reinterpret_cast<void*>(&hookUniform3f), &origUniform3f},
    {"glUniform4fv", reinterpret_cast<void*>(&hookUniform4fv), &origUniform4fv},
    {"glUniform4f", reinterpret_cast<void*>(&hookUniform4f), &origUniform4f},
    {"glUniform1iv", reinterpret_cast<void*>(&hookUniform1iv), &origUniform1iv},
    {"glUniform1i", reinterpret_cast<void*>(&hookUniform1i), &origUniform1i},
    {"glUniform2iv", reinterpret_cast<void*>(&hookUniform2iv), &origUniform2iv},
    {"glUniform2i", reinterpret_cast<void*>(&hookUniform2i), &origUniform2i},
    {"glUniform3iv", reinterpret_cast<void*>(&hookUniform3iv), &origUniform3iv},
    {"glUniform3i", reinterpret_cast<void*>(&hookUniform3i), &origUniform3i},
    {"glUniform4iv", reinterpret_cast<void*>(&hookUniform4iv), &origUniform4iv},
    {"glUniform4i", reinterpret_cast<void*>(&hookUniform4i), &origUniform4i},
    {"glUniform1uiv", reinterpret_cast<void*>(&hookUniform1uiv), &origUniform1uiv},
    {"glUniform1ui", reinterpret_cast<void*>(&hookUniform1ui), &origUniform1ui},
    {"glUniform2uiv", reinterpret_cast<void*>(&hookUniform2uiv), &origUniform2uiv},
    {"glUniform2ui", reinterpret_cast<void*>(&hookUniform2ui), &origUniform2ui},
    {"glUniform3uiv", reinterpret_cast<void*>(&hookUniform3uiv), &origUniform3uiv},
    {"glUniform3ui", reinterpret_cast<void*>(&hookUniform3ui), &origUniform3ui},
    {"glUniform4uiv", reinterpret_cast<void*>(&hookUniform4uiv), &origUniform4uiv},
    {"glUniform4ui", reinterpret_cast<void*>(&hookUniform4ui), &origUniform4ui},
    {"glUniformMatrix2fv", reinterpret_cast<void*>(&hookUniformMatrix2fv), &origUniformMatrix2fv},
    {"glUniformMatrix3fv", reinterpret_cast<void*>(&hookUniformMatrix3fv), &origUniformMatrix3fv},
    {"glUniformMatrix2x3fv", reinterpret_cast<void*>(&hookUniformMatrix2x3fv), &origUniformMatrix2x3fv},
    {"glUniformMatrix3x2fv", reinterpret_cast<void*>(&hookUniformMatrix3x2fv), &origUniformMatrix3x2fv},
    {"glUniformMatrix2x4fv", reinterpret_cast<void*>(&hookUniformMatrix2x4fv), &origUniformMatrix2x4fv},
    {"glUniformMatrix4x2fv", reinterpret_cast<void*>(&hookUniformMatrix4x2fv), &origUniformMatrix4x2fv},
    {"glUniformMatrix3x4fv", reinterpret_cast<void*>(&hookUniformMatrix3x4fv), &origUniformMatrix3x4fv},
    {"glUniformMatrix4x3fv", reinterpret_cast<void*>(&hookUniformMatrix4x3fv), &origUniformMatrix4x3fv},
    {"glActiveTexture", reinterpret_cast<void*>(&hookActiveTexture), &origActiveTexture},
    {"glBindTexture", reinterpret_cast<void*>(&hookBindTexture), &origBindTexture},
    {"glBindSampler", reinterpret_cast<void*>(&hookBindSampler), &origBindSampler},
    {"glBindVertexArray", reinterpret_cast<void*>(&hookBindVertexArray), &origBindVertexArray},
    {"glCullFace", reinterpret_cast<void*>(&hookCullFace), &origCullFace},
    {"glFrontFace", reinterpret_cast<void*>(&hookFrontFace), &origFrontFace},
    {"glPolygonOffset", reinterpret_cast<void*>(&hookPolygonOffset), &origPolygonOffset},
    {"glBindBuffer", reinterpret_cast<void*>(&hookBindBuffer), &origBindBuffer},
    {"glDepthRangef", reinterpret_cast<void*>(&hookDepthRangef), &origDepthRangef},
    {"glBufferData", reinterpret_cast<void*>(&hookBufferData), &origBufferData},
    {"glBufferSubData", reinterpret_cast<void*>(&hookBufferSubData), &origBufferSubData},
    {"glCopyBufferSubData", reinterpret_cast<void*>(&hookCopyBufferSubData), &origCopyBufferSubData},
    {"glMapBufferRange", reinterpret_cast<void*>(&hookMapBufferRange), &origMapBufferRange},
    {"glFlushMappedBufferRange", reinterpret_cast<void*>(&hookFlushMappedBufferRange), &origFlushMappedBufferRange},
    {"glUnmapBuffer", reinterpret_cast<void*>(&hookUnmapBuffer), &origUnmapBuffer},
    {"glDeleteBuffers", reinterpret_cast<void*>(&hookDeleteBuffers), &origDeleteBuffers},
    {"glBindBufferBase", reinterpret_cast<void*>(&hookBindBufferBase), &origBindBufferBase},
    {"glBindBufferRange", reinterpret_cast<void*>(&hookBindBufferRange), &origBindBufferRange},
    {"glDeleteTextures", reinterpret_cast<void*>(&hookDeleteTextures), &origDeleteTextures},
    {"glDeleteSamplers", reinterpret_cast<void*>(&hookDeleteSamplers), &origDeleteSamplers},
    {"glSamplerParameteri", reinterpret_cast<void*>(&hookSamplerParameteri), &origSamplerParameteri},
    {"glSamplerParameterf", reinterpret_cast<void*>(&hookSamplerParameterf), &origSamplerParameterf},
    {"glSamplerParameterfv", reinterpret_cast<void*>(&hookSamplerParameterfv), &origSamplerParameterfv},
    {"glTexParameteri", reinterpret_cast<void*>(&hookTexParameteri), &origTexParameteri},
    {"glTexParameterf", reinterpret_cast<void*>(&hookTexParameterf), &origTexParameterf},
    {"glTexParameterfv", reinterpret_cast<void*>(&hookTexParameterfv), &origTexParameterfv},
    {"glTexImage2D", reinterpret_cast<void*>(&hookTexImage2D), &origTexImage2D},
    {"glTexSubImage2D", reinterpret_cast<void*>(&hookTexSubImage2D), &origTexSubImage2D},
    {"glTexStorage2D", reinterpret_cast<void*>(&hookTexStorage2D), &origTexStorage2D},
    {"glTexImage3D", reinterpret_cast<void*>(&hookTexImage3D), &origTexImage3D},
    {"glTexSubImage3D", reinterpret_cast<void*>(&hookTexSubImage3D), &origTexSubImage3D},
    {"glTexStorage3D", reinterpret_cast<void*>(&hookTexStorage3D), &origTexStorage3D},
    {"glCompressedTexImage2D", reinterpret_cast<void*>(&hookCompressedTexImage2D), &origCompressedTexImage2D},
    {"glCompressedTexSubImage2D", reinterpret_cast<void*>(&hookCompressedTexSubImage2D), &origCompressedTexSubImage2D},
    {"glCompressedTexImage3D", reinterpret_cast<void*>(&hookCompressedTexImage3D), &origCompressedTexImage3D},
    {"glCompressedTexSubImage3D", reinterpret_cast<void*>(&hookCompressedTexSubImage3D), &origCompressedTexSubImage3D},
    {"glCopyTexSubImage2D", reinterpret_cast<void*>(&hookCopyTexSubImage2D), &origCopyTexSubImage2D},
    {"glGenerateMipmap", reinterpret_cast<void*>(&hookGenerateMipmap), &origGenerateMipmap},
    {"glDeleteProgram", reinterpret_cast<void*>(&hookDeleteProgram), &origDeleteProgram},
    {"glLinkProgram", reinterpret_cast<void*>(&hookLinkProgram), &origLinkProgram},
    {"glProgramBinary", reinterpret_cast<void*>(&hookProgramBinary), &origProgramBinary},
    {"glDeleteFramebuffers", reinterpret_cast<void*>(&hookDeleteFramebuffers), &origDeleteFramebuffers},
    {"glFramebufferTexture2D", reinterpret_cast<void*>(&hookFramebufferTexture2D), &origFramebufferTexture2D},
    {"glFramebufferTextureLayer", reinterpret_cast<void*>(&hookFramebufferTextureLayer), &origFramebufferTextureLayer},
    {"glFramebufferRenderbuffer", reinterpret_cast<void*>(&hookFramebufferRenderbuffer), &origFramebufferRenderbuffer},
    {"glRenderbufferStorage", reinterpret_cast<void*>(&hookRenderbufferStorage), &origRenderbufferStorage},
    {"glRenderbufferStorageMultisample", reinterpret_cast<void*>(&hookRenderbufferStorageMultisample), &origRenderbufferStorageMultisample},
    {"glDeleteRenderbuffers", reinterpret_cast<void*>(&hookDeleteRenderbuffers), &origDeleteRenderbuffers},
    {"glClearBufferfv", reinterpret_cast<void*>(&hookClearBufferfv), &origClearBufferfv},
    {"glClearBufferiv", reinterpret_cast<void*>(&hookClearBufferiv), &origClearBufferiv},
    {"glClearBufferuiv", reinterpret_cast<void*>(&hookClearBufferuiv), &origClearBufferuiv},
    {"glClearBufferfi", reinterpret_cast<void*>(&hookClearBufferfi), &origClearBufferfi},
    {"glInvalidateFramebuffer", reinterpret_cast<void*>(&hookInvalidateFramebuffer), &origInvalidateFramebuffer},
    {"glBlitFramebuffer", reinterpret_cast<void*>(&hookBlitFramebuffer), &origBlitFramebuffer},
    {"glDispatchCompute", reinterpret_cast<void*>(&hookDispatchCompute), &origDispatchCompute},
    {"glDispatchComputeIndirect", reinterpret_cast<void*>(&hookDispatchComputeIndirect), &origDispatchComputeIndirect},
    {"glMemoryBarrier", reinterpret_cast<void*>(&hookMemoryBarrier), &origMemoryBarrier},
    {"glBindImageTexture", reinterpret_cast<void*>(&hookBindImageTexture), &origBindImageTexture},
    {"glBeginQuery", reinterpret_cast<void*>(&hookBeginQuery), &origBeginQuery},
    {"glEndQuery", reinterpret_cast<void*>(&hookEndQuery), &origEndQuery},
    {"glBeginTransformFeedback", reinterpret_cast<void*>(&hookBeginTransformFeedback), &origBeginTransformFeedback},
    {"glEndTransformFeedback", reinterpret_cast<void*>(&hookEndTransformFeedback), &origEndTransformFeedback},
    {"glEnableVertexAttribArray", reinterpret_cast<void*>(&hookEnableVertexAttribArray), &origEnableVertexAttribArray},
    {"glDisableVertexAttribArray", reinterpret_cast<void*>(&hookDisableVertexAttribArray), &origDisableVertexAttribArray},
    {"glVertexAttribPointer", reinterpret_cast<void*>(&hookVertexAttribPointer), &origVertexAttribPointer},
    {"glVertexAttribIPointer", reinterpret_cast<void*>(&hookVertexAttribIPointer), &origVertexAttribIPointer},
    {"glVertexAttribDivisor", reinterpret_cast<void*>(&hookVertexAttribDivisor), &origVertexAttribDivisor},
    {"glDeleteVertexArrays", reinterpret_cast<void*>(&hookDeleteVertexArrays), &origDeleteVertexArrays},
};
}
bool loadApi(void* gles) {
    bool complete = true;
    api.GetActiveUniformBlockiv = reinterpret_cast<decltype(api.GetActiveUniformBlockiv)>(::dlsym(gles, "glGetActiveUniformBlockiv"));
    api.GetIntegeri_v = reinterpret_cast<decltype(api.GetIntegeri_v)>(::dlsym(gles, "glGetIntegeri_v"));
    api.GetInteger64i_v = reinterpret_cast<decltype(api.GetInteger64i_v)>(::dlsym(gles, "glGetInteger64i_v"));
    api.UniformBlockBinding = reinterpret_cast<decltype(api.UniformBlockBinding)>(::dlsym(gles, "glUniformBlockBinding"));
    api.VertexAttrib1f = reinterpret_cast<decltype(api.VertexAttrib1f)>(::dlsym(gles, "glVertexAttrib1f"));
    api.VertexAttrib1fv = reinterpret_cast<decltype(api.VertexAttrib1fv)>(::dlsym(gles, "glVertexAttrib1fv"));
    api.VertexAttrib2f = reinterpret_cast<decltype(api.VertexAttrib2f)>(::dlsym(gles, "glVertexAttrib2f"));
    api.VertexAttrib2fv = reinterpret_cast<decltype(api.VertexAttrib2fv)>(::dlsym(gles, "glVertexAttrib2fv"));
    api.VertexAttrib3f = reinterpret_cast<decltype(api.VertexAttrib3f)>(::dlsym(gles, "glVertexAttrib3f"));
    api.VertexAttrib3fv = reinterpret_cast<decltype(api.VertexAttrib3fv)>(::dlsym(gles, "glVertexAttrib3fv"));
    api.VertexAttrib4f = reinterpret_cast<decltype(api.VertexAttrib4f)>(::dlsym(gles, "glVertexAttrib4f"));
    api.VertexAttrib4fv = reinterpret_cast<decltype(api.VertexAttrib4fv)>(::dlsym(gles, "glVertexAttrib4fv"));
    api.VertexAttribI4i = reinterpret_cast<decltype(api.VertexAttribI4i)>(::dlsym(gles, "glVertexAttribI4i"));
    api.VertexAttribI4iv = reinterpret_cast<decltype(api.VertexAttribI4iv)>(::dlsym(gles, "glVertexAttribI4iv"));
    api.VertexAttribI4ui = reinterpret_cast<decltype(api.VertexAttribI4ui)>(::dlsym(gles, "glVertexAttribI4ui"));
    api.VertexAttribI4uiv = reinterpret_cast<decltype(api.VertexAttribI4uiv)>(::dlsym(gles, "glVertexAttribI4uiv"));
    api.TexParameteriv = reinterpret_cast<decltype(api.TexParameteriv)>(::dlsym(gles, "glTexParameteriv"));
    api.TexParameterIiv = reinterpret_cast<decltype(api.TexParameterIiv)>(::dlsym(gles, "glTexParameterIiv"));
    api.TexParameterIuiv = reinterpret_cast<decltype(api.TexParameterIuiv)>(::dlsym(gles, "glTexParameterIuiv"));
    api.SamplerParameteriv = reinterpret_cast<decltype(api.SamplerParameteriv)>(::dlsym(gles, "glSamplerParameteriv"));
    api.SamplerParameterIiv = reinterpret_cast<decltype(api.SamplerParameterIiv)>(::dlsym(gles, "glSamplerParameterIiv"));
    api.SamplerParameterIuiv = reinterpret_cast<decltype(api.SamplerParameterIuiv)>(::dlsym(gles, "glSamplerParameterIuiv"));
    api.GetVertexAttribiv = reinterpret_cast<decltype(api.GetVertexAttribiv)>(::dlsym(gles, "glGetVertexAttribiv"));
    api.GetVertexAttribPointerv = reinterpret_cast<decltype(api.GetVertexAttribPointerv)>(::dlsym(gles, "glGetVertexAttribPointerv"));
    api.InvalidateSubFramebuffer = reinterpret_cast<decltype(api.InvalidateSubFramebuffer)>(::dlsym(gles, "glInvalidateSubFramebuffer"));
    api.MultiDrawElementsIndirect = reinterpret_cast<decltype(api.MultiDrawElementsIndirect)>(::dlsym(gles, "glMultiDrawElementsIndirect"));
    api.MultiDrawArraysIndirect = reinterpret_cast<decltype(api.MultiDrawArraysIndirect)>(::dlsym(gles, "glMultiDrawArraysIndirect"));
    api.DrawElementsIndirect = reinterpret_cast<decltype(api.DrawElementsIndirect)>(::dlsym(gles, "glDrawElementsIndirect"));
    api.DrawArraysIndirect = reinterpret_cast<decltype(api.DrawArraysIndirect)>(::dlsym(gles, "glDrawArraysIndirect"));
    api.CopyImageSubData = reinterpret_cast<decltype(api.CopyImageSubData)>(::dlsym(gles, "glCopyImageSubData"));
    api.GetActiveUniform = reinterpret_cast<decltype(api.GetActiveUniform)>(::dlsym(gles, "glGetActiveUniform"));
    if (!api.GetActiveUniform) complete = false;
    api.GetUniformfv = reinterpret_cast<decltype(api.GetUniformfv)>(::dlsym(gles, "glGetUniformfv"));
    if (!api.GetUniformfv) complete = false;
    api.GetUniformiv = reinterpret_cast<decltype(api.GetUniformiv)>(::dlsym(gles, "glGetUniformiv"));
    if (!api.GetUniformiv) complete = false;
    api.GetUniformuiv = reinterpret_cast<decltype(api.GetUniformuiv)>(::dlsym(gles, "glGetUniformuiv"));
    if (!api.GetUniformuiv) complete = false;
    api.CullFace = reinterpret_cast<decltype(api.CullFace)>(::dlsym(gles, "glCullFace"));
    if (!api.CullFace) complete = false;
    api.FrontFace = reinterpret_cast<decltype(api.FrontFace)>(::dlsym(gles, "glFrontFace"));
    if (!api.FrontFace) complete = false;
    api.PolygonOffset = reinterpret_cast<decltype(api.PolygonOffset)>(::dlsym(gles, "glPolygonOffset"));
    if (!api.PolygonOffset) complete = false;
    api.BindBuffer = reinterpret_cast<decltype(api.BindBuffer)>(::dlsym(gles, "glBindBuffer"));
    api.BindBufferBase = reinterpret_cast<decltype(api.BindBufferBase)>(::dlsym(gles, "glBindBufferBase"));
    api.BindBufferRange = reinterpret_cast<decltype(api.BindBufferRange)>(::dlsym(gles, "glBindBufferRange"));
    api.GenSamplers = reinterpret_cast<decltype(api.GenSamplers)>(::dlsym(gles, "glGenSamplers"));
    api.SamplerParameteri = reinterpret_cast<decltype(api.SamplerParameteri)>(::dlsym(gles, "glSamplerParameteri"));
    api.DeleteProgram = reinterpret_cast<decltype(api.DeleteProgram)>(::dlsym(gles, "glDeleteProgram"));
    api.Uniform1fv = reinterpret_cast<decltype(api.Uniform1fv)>(::dlsym(gles, "glUniform1fv"));
    if (!api.Uniform1fv) complete = false;
    api.Uniform1f = reinterpret_cast<decltype(api.Uniform1f)>(::dlsym(gles, "glUniform1f"));
    api.Uniform2fv = reinterpret_cast<decltype(api.Uniform2fv)>(::dlsym(gles, "glUniform2fv"));
    if (!api.Uniform2fv) complete = false;
    api.Uniform2f = reinterpret_cast<decltype(api.Uniform2f)>(::dlsym(gles, "glUniform2f"));
    api.Uniform3fv = reinterpret_cast<decltype(api.Uniform3fv)>(::dlsym(gles, "glUniform3fv"));
    if (!api.Uniform3fv) complete = false;
    api.Uniform3f = reinterpret_cast<decltype(api.Uniform3f)>(::dlsym(gles, "glUniform3f"));
    api.Uniform4fv = reinterpret_cast<decltype(api.Uniform4fv)>(::dlsym(gles, "glUniform4fv"));
    if (!api.Uniform4fv) complete = false;
    api.Uniform4f = reinterpret_cast<decltype(api.Uniform4f)>(::dlsym(gles, "glUniform4f"));
    api.Uniform1iv = reinterpret_cast<decltype(api.Uniform1iv)>(::dlsym(gles, "glUniform1iv"));
    if (!api.Uniform1iv) complete = false;
    api.Uniform1i = reinterpret_cast<decltype(api.Uniform1i)>(::dlsym(gles, "glUniform1i"));
    api.Uniform2iv = reinterpret_cast<decltype(api.Uniform2iv)>(::dlsym(gles, "glUniform2iv"));
    if (!api.Uniform2iv) complete = false;
    api.Uniform2i = reinterpret_cast<decltype(api.Uniform2i)>(::dlsym(gles, "glUniform2i"));
    api.Uniform3iv = reinterpret_cast<decltype(api.Uniform3iv)>(::dlsym(gles, "glUniform3iv"));
    if (!api.Uniform3iv) complete = false;
    api.Uniform3i = reinterpret_cast<decltype(api.Uniform3i)>(::dlsym(gles, "glUniform3i"));
    api.Uniform4iv = reinterpret_cast<decltype(api.Uniform4iv)>(::dlsym(gles, "glUniform4iv"));
    if (!api.Uniform4iv) complete = false;
    api.Uniform4i = reinterpret_cast<decltype(api.Uniform4i)>(::dlsym(gles, "glUniform4i"));
    api.Uniform1uiv = reinterpret_cast<decltype(api.Uniform1uiv)>(::dlsym(gles, "glUniform1uiv"));
    if (!api.Uniform1uiv) complete = false;
    api.Uniform1ui = reinterpret_cast<decltype(api.Uniform1ui)>(::dlsym(gles, "glUniform1ui"));
    api.Uniform2uiv = reinterpret_cast<decltype(api.Uniform2uiv)>(::dlsym(gles, "glUniform2uiv"));
    if (!api.Uniform2uiv) complete = false;
    api.Uniform2ui = reinterpret_cast<decltype(api.Uniform2ui)>(::dlsym(gles, "glUniform2ui"));
    api.Uniform3uiv = reinterpret_cast<decltype(api.Uniform3uiv)>(::dlsym(gles, "glUniform3uiv"));
    if (!api.Uniform3uiv) complete = false;
    api.Uniform3ui = reinterpret_cast<decltype(api.Uniform3ui)>(::dlsym(gles, "glUniform3ui"));
    api.Uniform4uiv = reinterpret_cast<decltype(api.Uniform4uiv)>(::dlsym(gles, "glUniform4uiv"));
    if (!api.Uniform4uiv) complete = false;
    api.Uniform4ui = reinterpret_cast<decltype(api.Uniform4ui)>(::dlsym(gles, "glUniform4ui"));
    api.UniformMatrix2fv = reinterpret_cast<decltype(api.UniformMatrix2fv)>(::dlsym(gles, "glUniformMatrix2fv"));
    if (!api.UniformMatrix2fv) complete = false;
    api.UniformMatrix3fv = reinterpret_cast<decltype(api.UniformMatrix3fv)>(::dlsym(gles, "glUniformMatrix3fv"));
    if (!api.UniformMatrix3fv) complete = false;
    api.UniformMatrix4fv = reinterpret_cast<decltype(api.UniformMatrix4fv)>(::dlsym(gles, "glUniformMatrix4fv"));
    if (!api.UniformMatrix4fv) complete = false;
    api.UniformMatrix2x3fv = reinterpret_cast<decltype(api.UniformMatrix2x3fv)>(::dlsym(gles, "glUniformMatrix2x3fv"));
    if (!api.UniformMatrix2x3fv) complete = false;
    api.UniformMatrix3x2fv = reinterpret_cast<decltype(api.UniformMatrix3x2fv)>(::dlsym(gles, "glUniformMatrix3x2fv"));
    if (!api.UniformMatrix3x2fv) complete = false;
    api.UniformMatrix2x4fv = reinterpret_cast<decltype(api.UniformMatrix2x4fv)>(::dlsym(gles, "glUniformMatrix2x4fv"));
    if (!api.UniformMatrix2x4fv) complete = false;
    api.UniformMatrix4x2fv = reinterpret_cast<decltype(api.UniformMatrix4x2fv)>(::dlsym(gles, "glUniformMatrix4x2fv"));
    if (!api.UniformMatrix4x2fv) complete = false;
    api.UniformMatrix3x4fv = reinterpret_cast<decltype(api.UniformMatrix3x4fv)>(::dlsym(gles, "glUniformMatrix3x4fv"));
    if (!api.UniformMatrix3x4fv) complete = false;
    api.UniformMatrix4x3fv = reinterpret_cast<decltype(api.UniformMatrix4x3fv)>(::dlsym(gles, "glUniformMatrix4x3fv"));
    if (!api.UniformMatrix4x3fv) complete = false;
    api.ActiveTexture = reinterpret_cast<decltype(api.ActiveTexture)>(::dlsym(gles, "glActiveTexture"));
    if (!api.ActiveTexture) complete = false;
    api.BindTexture = reinterpret_cast<decltype(api.BindTexture)>(::dlsym(gles, "glBindTexture"));
    if (!api.BindTexture) complete = false;
    api.BindSampler = reinterpret_cast<decltype(api.BindSampler)>(::dlsym(gles, "glBindSampler"));
    if (!api.BindSampler) complete = false;
    api.BindVertexArray = reinterpret_cast<decltype(api.BindVertexArray)>(::dlsym(gles, "glBindVertexArray"));
    if (!api.BindVertexArray) complete = false;
    api.BufferData = reinterpret_cast<decltype(api.BufferData)>(::dlsym(gles, "glBufferData"));
    api.BufferSubData = reinterpret_cast<decltype(api.BufferSubData)>(::dlsym(gles, "glBufferSubData"));
    api.CopyBufferSubData = reinterpret_cast<decltype(api.CopyBufferSubData)>(::dlsym(gles, "glCopyBufferSubData"));
    api.MapBufferRange = reinterpret_cast<decltype(api.MapBufferRange)>(::dlsym(gles, "glMapBufferRange"));
    api.FlushMappedBufferRange = reinterpret_cast<decltype(api.FlushMappedBufferRange)>(::dlsym(gles, "glFlushMappedBufferRange"));
    api.UnmapBuffer = reinterpret_cast<decltype(api.UnmapBuffer)>(::dlsym(gles, "glUnmapBuffer"));
    api.DeleteBuffers = reinterpret_cast<decltype(api.DeleteBuffers)>(::dlsym(gles, "glDeleteBuffers"));
    api.DeleteTextures = reinterpret_cast<decltype(api.DeleteTextures)>(::dlsym(gles, "glDeleteTextures"));
    api.DeleteSamplers = reinterpret_cast<decltype(api.DeleteSamplers)>(::dlsym(gles, "glDeleteSamplers"));
    api.SamplerParameterf = reinterpret_cast<decltype(api.SamplerParameterf)>(::dlsym(gles, "glSamplerParameterf"));
    api.SamplerParameterfv = reinterpret_cast<decltype(api.SamplerParameterfv)>(::dlsym(gles, "glSamplerParameterfv"));
    api.TexParameteri = reinterpret_cast<decltype(api.TexParameteri)>(::dlsym(gles, "glTexParameteri"));
    api.TexParameterf = reinterpret_cast<decltype(api.TexParameterf)>(::dlsym(gles, "glTexParameterf"));
    api.TexParameterfv = reinterpret_cast<decltype(api.TexParameterfv)>(::dlsym(gles, "glTexParameterfv"));
    api.TexImage2D = reinterpret_cast<decltype(api.TexImage2D)>(::dlsym(gles, "glTexImage2D"));
    api.TexSubImage2D = reinterpret_cast<decltype(api.TexSubImage2D)>(::dlsym(gles, "glTexSubImage2D"));
    api.TexStorage2D = reinterpret_cast<decltype(api.TexStorage2D)>(::dlsym(gles, "glTexStorage2D"));
    api.TexImage3D = reinterpret_cast<decltype(api.TexImage3D)>(::dlsym(gles, "glTexImage3D"));
    api.TexSubImage3D = reinterpret_cast<decltype(api.TexSubImage3D)>(::dlsym(gles, "glTexSubImage3D"));
    api.TexStorage3D = reinterpret_cast<decltype(api.TexStorage3D)>(::dlsym(gles, "glTexStorage3D"));
    api.CompressedTexImage2D = reinterpret_cast<decltype(api.CompressedTexImage2D)>(::dlsym(gles, "glCompressedTexImage2D"));
    api.CompressedTexSubImage2D = reinterpret_cast<decltype(api.CompressedTexSubImage2D)>(::dlsym(gles, "glCompressedTexSubImage2D"));
    api.CompressedTexImage3D = reinterpret_cast<decltype(api.CompressedTexImage3D)>(::dlsym(gles, "glCompressedTexImage3D"));
    api.CompressedTexSubImage3D = reinterpret_cast<decltype(api.CompressedTexSubImage3D)>(::dlsym(gles, "glCompressedTexSubImage3D"));
    api.CopyTexSubImage2D = reinterpret_cast<decltype(api.CopyTexSubImage2D)>(::dlsym(gles, "glCopyTexSubImage2D"));
    api.GenerateMipmap = reinterpret_cast<decltype(api.GenerateMipmap)>(::dlsym(gles, "glGenerateMipmap"));
    api.LinkProgram = reinterpret_cast<decltype(api.LinkProgram)>(::dlsym(gles, "glLinkProgram"));
    api.ProgramBinary = reinterpret_cast<decltype(api.ProgramBinary)>(::dlsym(gles, "glProgramBinary"));
    api.DeleteFramebuffers = reinterpret_cast<decltype(api.DeleteFramebuffers)>(::dlsym(gles, "glDeleteFramebuffers"));
    api.FramebufferTexture2D = reinterpret_cast<decltype(api.FramebufferTexture2D)>(::dlsym(gles, "glFramebufferTexture2D"));
    api.FramebufferTextureLayer = reinterpret_cast<decltype(api.FramebufferTextureLayer)>(::dlsym(gles, "glFramebufferTextureLayer"));
    api.FramebufferRenderbuffer = reinterpret_cast<decltype(api.FramebufferRenderbuffer)>(::dlsym(gles, "glFramebufferRenderbuffer"));
    api.RenderbufferStorage = reinterpret_cast<decltype(api.RenderbufferStorage)>(::dlsym(gles, "glRenderbufferStorage"));
    api.RenderbufferStorageMultisample = reinterpret_cast<decltype(api.RenderbufferStorageMultisample)>(::dlsym(gles, "glRenderbufferStorageMultisample"));
    api.DeleteRenderbuffers = reinterpret_cast<decltype(api.DeleteRenderbuffers)>(::dlsym(gles, "glDeleteRenderbuffers"));
    api.ClearBufferfv = reinterpret_cast<decltype(api.ClearBufferfv)>(::dlsym(gles, "glClearBufferfv"));
    api.ClearBufferiv = reinterpret_cast<decltype(api.ClearBufferiv)>(::dlsym(gles, "glClearBufferiv"));
    api.ClearBufferuiv = reinterpret_cast<decltype(api.ClearBufferuiv)>(::dlsym(gles, "glClearBufferuiv"));
    api.ClearBufferfi = reinterpret_cast<decltype(api.ClearBufferfi)>(::dlsym(gles, "glClearBufferfi"));
    api.InvalidateFramebuffer = reinterpret_cast<decltype(api.InvalidateFramebuffer)>(::dlsym(gles, "glInvalidateFramebuffer"));
    api.BlitFramebuffer = reinterpret_cast<decltype(api.BlitFramebuffer)>(::dlsym(gles, "glBlitFramebuffer"));
    api.DispatchCompute = reinterpret_cast<decltype(api.DispatchCompute)>(::dlsym(gles, "glDispatchCompute"));
    api.DispatchComputeIndirect = reinterpret_cast<decltype(api.DispatchComputeIndirect)>(::dlsym(gles, "glDispatchComputeIndirect"));
    api.MemoryBarrier = reinterpret_cast<decltype(api.MemoryBarrier)>(::dlsym(gles, "glMemoryBarrier"));
    api.BindImageTexture = reinterpret_cast<decltype(api.BindImageTexture)>(::dlsym(gles, "glBindImageTexture"));
    api.BeginQuery = reinterpret_cast<decltype(api.BeginQuery)>(::dlsym(gles, "glBeginQuery"));
    api.EndQuery = reinterpret_cast<decltype(api.EndQuery)>(::dlsym(gles, "glEndQuery"));
    api.BeginTransformFeedback = reinterpret_cast<decltype(api.BeginTransformFeedback)>(::dlsym(gles, "glBeginTransformFeedback"));
    api.EndTransformFeedback = reinterpret_cast<decltype(api.EndTransformFeedback)>(::dlsym(gles, "glEndTransformFeedback"));
    api.EnableVertexAttribArray = reinterpret_cast<decltype(api.EnableVertexAttribArray)>(::dlsym(gles, "glEnableVertexAttribArray"));
    api.DisableVertexAttribArray = reinterpret_cast<decltype(api.DisableVertexAttribArray)>(::dlsym(gles, "glDisableVertexAttribArray"));
    api.VertexAttribPointer = reinterpret_cast<decltype(api.VertexAttribPointer)>(::dlsym(gles, "glVertexAttribPointer"));
    api.VertexAttribIPointer = reinterpret_cast<decltype(api.VertexAttribIPointer)>(::dlsym(gles, "glVertexAttribIPointer"));
    api.VertexAttribDivisor = reinterpret_cast<decltype(api.VertexAttribDivisor)>(::dlsym(gles, "glVertexAttribDivisor"));
    api.DeleteVertexArrays = reinterpret_cast<decltype(api.DeleteVertexArrays)>(::dlsym(gles, "glDeleteVertexArrays"));
    api.DepthRangef = reinterpret_cast<decltype(api.DepthRangef)>(::dlsym(gles, "glDepthRangef"));
    if (!api.DepthRangef) complete = false;
    return complete;
}
const Hook* hooks(std::size_t& count) {
    count = std::size(kHooks);
    return kHooks;
}
void* intercept(const char* name, void* address) {
    if (!name || !address) return address;
    const char* canonical = name;
    if (!std::strcmp(name, "glMultiDrawElementsIndirectEXT")) canonical = "glMultiDrawElementsIndirect";
    if (!std::strcmp(name, "glMultiDrawArraysIndirectEXT")) canonical = "glMultiDrawArraysIndirect";
    if (!std::strcmp(name, "glDrawElementsIndirectEXT")) canonical = "glDrawElementsIndirect";
    if (!std::strcmp(name, "glDrawArraysIndirectEXT")) canonical = "glDrawArraysIndirect";
    if (!std::strcmp(name, "glCopyImageSubDataEXT")) canonical = "glCopyImageSubData";
    if (!std::strcmp(name, "glBindVertexArrayOES")) canonical = "glBindVertexArray";
    if (!std::strcmp(name, "glDeleteVertexArraysOES")) canonical = "glDeleteVertexArrays";
    if (!std::strcmp(name, "glVertexAttribDivisorOES")) canonical = "glVertexAttribDivisor";
    if (!std::strcmp(name, "glProgramBinaryOES")) canonical = "glProgramBinary";
    if (!std::strcmp(name, "glDiscardFramebufferEXT")) canonical = "glInvalidateFramebuffer";
    if (!std::strcmp(name, "glTexStorage2DEXT")) canonical = "glTexStorage2D";
    if (!std::strcmp(name, "glTexStorage3DEXT")) canonical = "glTexStorage3D";
    if (!std::strcmp(name, "glFramebufferTextureLayerEXT")) canonical = "glFramebufferTextureLayer";
    if (!std::strcmp(name, "glBlitFramebufferANGLE")) canonical = "glBlitFramebuffer";
    if (!std::strcmp(name, "glRenderbufferStorageMultisampleANGLE")) canonical = "glRenderbufferStorageMultisample";
    if (!std::strcmp(name, "glBeginQueryEXT")) canonical = "glBeginQuery";
    if (!std::strcmp(name, "glEndQueryEXT")) canonical = "glEndQuery";
    if (!std::strcmp(name, "glBeginQueryNV")) canonical = "glBeginQuery";
    if (!std::strcmp(name, "glEndQueryNV")) canonical = "glEndQuery";
    if (!std::strcmp(name, "glTexImage3DOES")) canonical = "glTexImage3D";
    if (!std::strcmp(name, "glTexSubImage3DOES")) canonical = "glTexSubImage3D";
    if (!std::strcmp(name, "glCompressedTexImage3DOES")) canonical = "glCompressedTexImage3D";
    if (!std::strcmp(name, "glCompressedTexSubImage3DOES")) canonical = "glCompressedTexSubImage3D";
    for (const auto& hook : kHooks) {
        if (std::strcmp(canonical, hook.name)) continue;
        void* expected = nullptr;
        hook.original->compare_exchange_strong(expected, address);
        return hook.detour;
    }
    return address;
}
void uniformMatrix4(GLint location, GLsizei count, GLboolean transpose, const GLfloat* values) {
    if (!transpose) remember(Kind::M4, location, count, values);
}
void setDepthClearObserver(void (*observer)()) { depthClearObserver = observer; }
void resetContext() { live() = Live{}; }
void resynchronize() {
    auto& state = live();
    if (api.DeleteVertexArrays) {
        for (const auto& entry : state.vertexCache) api.DeleteVertexArrays(1, &entry.second.name);
        for (const auto vao : state.temporaryVaos) api.DeleteVertexArrays(1, &vao);
    }
    state = Live{};
}

void useProgram(GLuint program) { live().program = program; }
GLuint currentVertexArray() { return vertexArray(); }
void cap(GLenum name, bool value) {
    auto& s = live();
    if (name == GL_CULL_FACE) s.pipeline.cull = value;
    else if (name == kPolygonOffset) s.pipeline.offset = value;
}
bool pending() { return !live().draws.empty(); }
bool shouldSkipDraw() { return live().queryDepth != 0 || live().transformFeedback; }
unsigned programGeneration(GLuint program) {
    const auto& generations = live().generations;
    auto it = generations.find(program);
    return it == generations.end() ? 0 : it->second;
}

bool queue(const Target& target, GLuint program, const DrawCall& draw, const MatrixUpload* matrices, int matrixCount,
           unsigned cascadeMask, int maxDraws, bool twoSided) {
    auto& s = live();
    s.program = program;
    if (!s.healthy) return false;
    if (s.flushing || shouldSkipDraw() || !cascadeMask || draw.count <= 0 || draw.instances <= 0) return false;
    if (matrixCount < 1 || matrixCount > 5) return false;
    if (!primeProgram(program).supported) { flush(); return false; }
    if (!s.draws.empty() && (s.target.framebuffer != target.framebuffer || s.draws.size() >= static_cast<std::size_t>(std::max(1, maxDraws)) ||
                            queuedBytes() > kMemoryLimit / 2)) flush();
    s.target = target;
    auto& p = s.programs[program];
    for (int i = 0; i < matrixCount; ++i) {
        if (p.cameraLocations[i] != matrices[i].location) {
            const auto slot = p.locations.find(matrices[i].location);
            if (slot == p.locations.end() || p.uniforms[slot->second.index].kind != Kind::M4) { flush(); return false; }
            p.cameraLocations[i] = matrices[i].location;
            p.cameraSlots[i] = slot->second;
        }
    }
    if (p.serial != s.serial) {
        p.serial = s.serial;
        s.usedPrograms.push_back(program);
        for (std::size_t index = 0; index < p.uniforms.size(); ++index) {
            const auto& u = p.uniforms[index];
            if (!u.camera) appendUniform(program, {index, 0}, u.kind, u.count, u.values.data());
        }
    }
    for (const auto index : p.uniformBuffers) {
        const auto binding = currentUbo(index);
        const auto recorded = s.recordedUbos.find(index);
        if (recorded == s.recordedUbos.end() || !(recorded->second == binding)) {
            s.recordedUbos[index] = binding;
            s.commands.push_back({CommandKind::Indexed, s.indexedCommands.size()});
            s.indexedCommands.push_back({index, binding});
        }
    }
    for (const auto& u : p.uniforms) {
        if (!u.sampler) continue;
        const int targetIndexValue = targetIndex(u.sampler);
        for (int element = 0; element < u.count; ++element) {
            GLint unitValue = 0;
            std::memcpy(&unitValue, u.values.data() + element, sizeof(unitValue));
            if (unitValue < 0 || unitValue >= 256 || targetIndexValue < 0) continue;
            const auto unit = static_cast<unsigned>(unitValue);
            const auto targetSlot = static_cast<unsigned>(targetIndexValue);
            const TextureUnit binding = textureUnit(unit, targetSlot);
            auto& recorded = s.recorded[unit];
            if (!(recorded.known & (1u << targetSlot)) || recorded.targets[targetSlot] != binding.targets[targetSlot] ||
                !recorded.haveSampler || recorded.sampler != binding.sampler) {
                if (!(recorded.known & (1u << targetSlot))) s.touched.emplace_back(unit, targetSlot);
                recorded.targets[targetSlot] = binding.targets[targetSlot];
                recorded.known |= 1u << targetSlot;
                recorded.sampler = binding.sampler;
                recorded.haveSampler = true;
                s.commands.push_back({CommandKind::Texture, s.bindings.size()});
                s.bindings.push_back({unit, targetSlot, binding.targets[targetSlot], binding.sampler});
            }
        }
    }
    DrawCommand command;
    command.draw = draw;
    command.program = program;
    command.vao = vertexArray();
    command.pipeline = pipeline();
    if (twoSided) command.pipeline.cull = false;
    std::copy_n(matrices, matrixCount, command.matrices.begin());
    command.matrixCount = matrixCount;
    command.mask = cascadeMask;
    std::copy_n(p.cameraSlots.begin(), matrixCount, command.matrixSlots.begin());
    s.cascadeMask |= cascadeMask;
    s.commands.push_back({CommandKind::Draw, s.draws.size()});
    auto& pending = s.pendingVaoDraws[command.vao];
    if (pending.empty()) s.pendingVaoSources.push_back(command.vao);
    pending.push_back(s.draws.size());
    s.draws.push_back(std::move(command));

    return true;
}

void flush() {
    auto& s = live();
    if (s.draws.empty() || s.flushing) return;
    s.flushing = true;

    const bool checking = s.errorChecks < 4;
    if (checking) drainErrors();
    const Saved saved = save();
    auto& finalBindings = s.finalTextures;
    finalBindings.assign(s.textures.begin(), s.textures.end());
    for (const auto& [unit, target] : s.touched) finalBindings[unit] = textureUnit(unit, target);
    GLint savedUniformBuffer = 0;
    if (!s.indexedCommands.empty()) real.GetIntegerv(0x8A28, &savedUniformBuffer);
    bool uboChanged = false;
    s.replayTextures.assign(finalBindings.begin(), finalBindings.end());
    GLenum activeTexture = static_cast<GLenum>(saved.active);
    const auto bindTexture = [&](unsigned unit, unsigned target, GLuint texture, GLuint sampler) {
        auto& current = s.replayTextures[unit];
        if (current.targets[target] != texture) {
            const GLenum selected = GL_TEXTURE0 + unit;
            if (activeTexture != selected) { real.ActiveTexture(selected); activeTexture = selected; }
            real.BindTexture(kTargets[target], texture);
            current.targets[target] = texture;
        }
        if (current.sampler != sampler) { real.BindSampler(unit, sampler); current.sampler = sampler; }
    };
    real.BindFramebuffer(GL_DRAW_FRAMEBUFFER, s.target.framebuffer);
    real.Enable(GL_DEPTH_TEST);
    real.DepthMask(1);
    real.DepthFunc(GL_LESS);
    real.DepthRangef(0.0f, 1.0f);
    real.ColorMask(0, 0, 0, 0);
    real.Disable(GL_BLEND);
    real.Disable(GL_SCISSOR_TEST);
    real.Disable(GL_STENCIL_TEST);
    real.Disable(GL_RASTERIZER_DISCARD);
    for (int cascade = s.target.columns == 2 ? 0 : 1; cascade < 2; ++cascade) {
        if (!(s.cascadeMask & (1u << cascade))) continue;
        const int tile = s.target.columns == 2 ? cascade : 0;
        real.Viewport(tile * s.target.resolution, 0, s.target.resolution, s.target.resolution);
        GLuint currentProgram = ~GLuint{}, currentVao = ~GLuint{};
        Pipeline currentPipeline;
        bool havePipeline = false;
        auto bindProgram = [&](GLuint program) {
            if (currentProgram != program) { real.UseProgram(program); currentProgram = program; }
        };
        for (const auto& command : s.commands) {
            if (command.kind == CommandKind::Uniform) {
                const auto& u = s.uniforms[command.index];
                auto& uniform = s.programs.at(u.program).uniforms[u.slot.index];
                if (updateReplay(uniform, u.slot.element, u.count, s.words.data() + u.offset)) {
                    bindProgram(u.program);
                    upload(uniform.kind, uniform.location + u.slot.element, u.count, s.words.data() + u.offset);
                }
            } else if (command.kind == CommandKind::Texture) {
                const auto& binding = s.bindings[command.index];
                bindTexture(binding.unit, binding.target, binding.texture, binding.sampler);
            } else if (command.kind == CommandKind::Indexed) {
                const auto& indexed = s.indexedCommands[command.index];
                uboChanged |= replayUbo(indexed.index, indexed.binding);
            } else {
                const auto& draw = s.draws[command.index];
                if (!(draw.mask & (1u << cascade))) continue;
                bindProgram(draw.program);
                if (currentVao != draw.vao) { real.BindVertexArray(draw.vao); currentVao = draw.vao; }
                const auto& pipelineValue = draw.pipeline;
                if (!havePipeline || currentPipeline.cull != pipelineValue.cull) enabled(GL_CULL_FACE, pipelineValue.cull);
                if (!havePipeline || currentPipeline.cullFace != pipelineValue.cullFace) api.CullFace(pipelineValue.cullFace);
                if (!havePipeline || currentPipeline.frontFace != pipelineValue.frontFace) api.FrontFace(pipelineValue.frontFace);
                if (!havePipeline || currentPipeline.offset != pipelineValue.offset) enabled(kPolygonOffset, pipelineValue.offset);
                if (!havePipeline || currentPipeline.factor != pipelineValue.factor || currentPipeline.units != pipelineValue.units)
                    api.PolygonOffset(pipelineValue.factor, pipelineValue.units);
                currentPipeline = pipelineValue;
                havePipeline = true;
                auto& program = s.programs.at(draw.program);
                for (int i = 0; i < draw.matrixCount; ++i) {
                    const auto& matrix = draw.matrices[i];
                    const auto slot = draw.matrixSlots[i];
                    auto& uniform = program.uniforms[slot.index];
                    if (updateReplay(uniform, slot.element, 1, matrix.light[cascade].data()))
                        real.UniformMatrix4fv(matrix.location, 1, 0, matrix.light[cascade].data());
                }
                issue(draw.draw);

            }
        }
    }
    for (const GLuint program : s.usedPrograms) {
        bool bound = false;
        for (const auto& u : s.programs.at(program).uniforms) {
            if (u.replaySerial != s.serial || u.replayValues == u.values) continue;
            if (!bound) { real.UseProgram(program); bound = true; }
            upload(u.kind, u.location, u.count, u.values.data());
        }
    }
    if (uboChanged) {
        for (const auto& [index, binding] : s.replayUbos) {
            const auto final = currentUbo(index);
            if (!(binding == final)) bindUbo(index, final);
        }
        api.BindBuffer(0x8A11, static_cast<GLuint>(savedUniformBuffer));
    }
    for (const auto& [unit, target] : s.touched) {
        bindTexture(unit, target, finalBindings[unit].targets[target], finalBindings[unit].sampler);
    }
    restore(saved);
    if (checking) {
        ++s.errorChecks;
        const GLenum error = real.GetError();
        drainErrors();
        if (error != 0) {
            s.healthy = false;

        }
    }

    s.commands.clear();
    s.uniforms.clear();
    s.bindings.clear();
    s.draws.clear();
    s.words.clear();
    s.usedPrograms.clear();
    s.cascadeMask = 0;
    for (const auto source : s.pendingVaoSources) {
        const auto found = s.pendingVaoDraws.find(source);
        if (found != s.pendingVaoDraws.end()) found->second.clear();
    }
    s.pendingVaoSources.clear();
    s.recordedUbos.clear();
    s.replayUbos.clear();
    s.indexedCommands.clear();
    for (const auto vao : s.temporaryVaos) api.DeleteVertexArrays(1, &vao);
    s.temporaryVaos.clear();
    s.touched.clear();
    for (auto& unit : s.recorded) unit = TextureUnit{};
    ++s.serial;
    s.flushing = false;
}

void finishFrame() {
    flush();
    ++live().completedFrames;
}
}
