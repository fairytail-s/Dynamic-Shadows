#pragma once

#include "GlApi.hpp"
#include "ShadowMath.hpp"

#include <array>
#include <atomic>
#include <cstdint>

namespace shadow::batch {

using namespace glapi;

enum class DrawKind { Elements, Arrays, ElementsInstanced, ArraysInstanced };

struct DrawCall {
    DrawKind kind{};
    GLenum mode{};
    GLsizei count{};
    GLenum type{};
    const void* indices{};
    GLint first{};
    GLsizei instances{1};
};

struct MatrixUpload {
    GLint location{-1};
    std::array<math::Mat4, 2> light{};
};

struct Target {
    GLuint framebuffer{};
    int resolution{};
    int columns{2};
};

struct Hook {
    const char* name;
    void* detour;
    std::atomic<void*>* original;
};

bool loadApi(void* gles);
const Hook* hooks(std::size_t& count);
void* intercept(const char* name, void* address);
void resetContext();
void resynchronize();
void setDepthClearObserver(void (*observer)());
void useProgram(GLuint program);
void cap(GLenum name, bool enabled);
void uniformMatrix4(GLint location, GLsizei count, GLboolean transpose, const GLfloat* values);
bool queue(const Target& target, GLuint program, const DrawCall& draw, const MatrixUpload* matrices, int matrixCount,
           unsigned cascadeMask, int maxDraws, bool twoSided);
void flush();
void finishFrame();
bool pending();
bool shouldSkipDraw();
unsigned programGeneration(GLuint program);
GLuint currentVertexArray();

}
