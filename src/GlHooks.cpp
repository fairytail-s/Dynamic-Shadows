#include "GlHooks.hpp"

#include "Config.hpp"
#include "DrawBatch.hpp"
#include "ElfPatch.hpp"
#include "GameTime.hpp"
#include "GlApi.hpp"
#include "ShaderPatch.hpp"
#include "Shaders.hpp"
#include "ShadowMath.hpp"
#include "Sun.hpp"

#include <pl/memory/Hook.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace shadow {
using namespace glapi;
using namespace math;
using namespace shaders;

namespace {
void drainErrors() { for (unsigned i = 0; i < 16 && real.GetError() != 0; ++i) {} }

enum Slot { kDrawElements, kDrawArrays, kDrawElementsInstanced, kDrawArraysInstanced, kClear, kUniformMatrix4fv, kEnable, kDisable,
            kDepthMask, kDepthFunc, kBindFramebuffer, kUseProgram, kShaderSource, kColorMask, kSlotCount };
std::atomic<void*> gOrig[kSlotCount];
std::atomic<void*> gSwapOrig{nullptr};
std::atomic<void*> gEglOrig{nullptr};
std::atomic_bool gActive{false};
bool gBatchAvailable = false;
std::atomic<unsigned> gSunPatched{0};
std::atomic<unsigned> gLightPatched{0};

std::atomic_bool gSwapViaGot{false};
std::atomic_bool gSwapFallbackTried{false};
void* (*gGetCurrentContext)() = nullptr;

std::atomic_bool gSwapSeen{false};

template <class F>
F resolve(Slot slot, F fallback) {
    void* p = gOrig[slot].load(std::memory_order_acquire);
    return p ? reinterpret_cast<F>(p) : fallback;
}

struct Attach {
    GLuint object = 0;
    bool texture = false;
};

template<std::size_t N> struct ShaderVariant { GLuint program{}; std::array<GLint, N> locations{}; };

struct Own {
    std::array<ShaderVariant<16>, 8> worldShaders;
    std::array<ShaderVariant<9>, 2> handShaders;
    int worldVariant{-1}, handVariant{-1};
    GLuint vao = 0, blitFbo = 0, depthTex = 0;
    GLenum depthFormat = 0;
    int width = 0, height = 0;
    int assumeIdx = 0;
    bool failed = false;

    GLuint smProgram = 0, smTex = 0, smFbo = 0;
    int smRes = 0, smRequested = 0, smColumns = 2, smBits = 24, smFilter = -1;
    GLuint depthSampler = 0;
    bool directFailed = false;
    bool smFailed = false;
    unsigned smGen = 0;
    GLint smDepth = -1, smShadow = -1, smInvVP = -1, smLightVP0 = -1, smLightVP1 = -1, smCam = -1, smSun = -1, smSm0 = -1, smSm1 = -1, smMisc = -1, smTint = -1, smFilterUniform = -1, smAtlas = -1, smTuning = -1, smDepthRect = -1, smLight = -1;
    bool smFetch = false;
    bool fetchFailed = false;

    GLuint handProgram = 0;
    bool handFailed = false;
    GLint hShadow = -1, hLightVP = -1, hCam = -1, hSun = -1, hSm = -1, hMisc = -1, hTint = -1, hFilter = -1, hAtlas = -1;
};

struct SceneInfo {
    bool valid = false;
    GLuint fbo = 0;
    int vx = 0, vy = 0, w = 0, h = 0;
};

struct ProgInfo {
    unsigned generation = 0;
    GLint view = -1, proj = -1, viewProj = -1, modelView = -1, modelViewProj = -1;
    GLint light = -1;
    float lightVal[2] = {-1.0f, -1.0f};
    GLint sunDelta = -1, sunTex = -1;
    float sunVal[9] = {};
    bool sunKnown = false;
    Mat4 vView{}, vProj{}, vVP{}, vMV{}, vMVP{};
    bool hView = false, hProj = false, hVP = false, hMV = false, hMVP = false;
};

struct ShadowFrame {
    bool attempted = false, ok = false, haveView = false, reused = false;
    bool batchDraws = true, cullDraws = true, twoSided = false;
    int maxBatchDraws = 2048;
    float cameraForward[3]{};
    GLint fbo = -1;
    GLint viewport[4]{};
    Mat4 invVP{}, cameraVP{}, anchorMVP{};
    std::uint64_t anchorKey = 0;
    bool anchorValid = false;

    Mat4 lightView[2]{}, lightProj[2]{}, lightVP[2]{}, T[2]{}, Tv[2]{};
    float texel[2]{}, range[2]{}, radius[2]{};
    int cascades = 2;
    float center[3]{}, sun[3]{};
    float strength = 1.0f;
    unsigned draws = 0;
};

struct State {
    std::array<float, 16> view{}, proj{};
    bool haveView = false, haveProj = false;
    std::unordered_map<GLuint, ProgInfo> progs;
    ShadowFrame sf, cachedShadow;
    bool cachedShadowValid = false;
    unsigned cachedShadowFrame = 0, cachedMapGeneration = 0;
    std::array<std::uint32_t, 12> mapKey{}, cachedMapKey{};
    unsigned smChecks = 0;
    void* ctx = nullptr;
    unsigned retries = 0, failedFrames = 0;
    unsigned frame = 0;
    unsigned frameDraws = 0, depthDraws = 0;
    unsigned long long totalDraws = 0;

    struct Tracked {
        GLint fbo = -1, program = -1, depthFunc = -1;
        signed char depthTest = -1, blend = -1, scissor = -1, depthMask = -1;
        GLboolean mask[4]{1, 1, 1, 1};
        bool maskKnown = false;
        bool bad = false;
    } tr;
    GLuint cachedProg = 0;
    ProgInfo* cachedInfo = nullptr;
    bool shadowedThisFrame = false;
    bool alphaForced = false;

    unsigned opaqueSeen = 0, prevOpaqueSeen = 0;
    unsigned opaqueAtTrigger = 0;
    bool earlyThisFrame = false;
    bool prevEarly = false;
    bool opaqueFrozen = false;
    bool earlyDisabled = false;
    unsigned earlyBad = 0;
    bool handPhase = false, handDone = false;
    unsigned handDraws = 0;
    SceneInfo scene;
    unsigned skips = 0;
    Own own;
};
State& state() {
    static thread_local State s;
    return s;
}

bool trackedFlag(State& st, signed char& slot, GLenum cap) {
    if (!st.tr.bad && slot >= 0) return slot != 0;
    const bool v = real.IsEnabled(cap) != 0;
    if (!st.tr.bad) slot = v ? 1 : 0;
    return v;
}
bool trackedDepthTest(State& st) { return trackedFlag(st, st.tr.depthTest, GL_DEPTH_TEST); }
bool trackedBlend(State& st) { return trackedFlag(st, st.tr.blend, GL_BLEND); }
bool trackedScissor(State& st) { return trackedFlag(st, st.tr.scissor, GL_SCISSOR_TEST); }

bool trackedDepthMask(State& st) {
    if (!st.tr.bad && st.tr.depthMask >= 0) return st.tr.depthMask != 0;
    GLboolean v = 0;
    real.GetBooleanv(GL_DEPTH_WRITEMASK, &v);
    if (!st.tr.bad) st.tr.depthMask = v ? 1 : 0;
    return v != 0;
}

GLint trackedInt(State& st, GLint& slot, GLenum pname) {
    if (!st.tr.bad && slot >= 0) return slot;
    GLint v = 0;
    real.GetIntegerv(pname, &v);
    if (!st.tr.bad) slot = v;
    return v;
}
GLint trackedFbo(State& st) { return trackedInt(st, st.tr.fbo, GL_DRAW_FRAMEBUFFER_BINDING); }
GLint trackedProgram(State& st) { return trackedInt(st, st.tr.program, GL_CURRENT_PROGRAM); }
GLint trackedDepthFunc(State& st) { return trackedInt(st, st.tr.depthFunc, GL_DEPTH_FUNC); }

void verifyTracking(State& st) {
    auto& t = st.tr;
    GLint v = 0;
    GLboolean b = 0;
    const char* wrong = nullptr;
    if (t.fbo >= 0 && (real.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &v), v != t.fbo)) wrong = "framebuffer";
    else if (t.program >= 0 && (real.GetIntegerv(GL_CURRENT_PROGRAM, &v), v != t.program)) wrong = "program";
    else if (t.depthFunc >= 0 && (real.GetIntegerv(GL_DEPTH_FUNC, &v), v != t.depthFunc)) wrong = "depth function";
    else if (t.depthTest >= 0 && (real.IsEnabled(GL_DEPTH_TEST) != 0) != (t.depthTest != 0)) wrong = "depth test";
    else if (t.blend >= 0 && (real.IsEnabled(GL_BLEND) != 0) != (t.blend != 0)) wrong = "blend";
    else if (t.scissor >= 0 && (real.IsEnabled(GL_SCISSOR_TEST) != 0) != (t.scissor != 0)) wrong = "scissor";
    else if (t.depthMask >= 0 && (real.GetBooleanv(GL_DEPTH_WRITEMASK, &b), (b != 0) != (t.depthMask != 0))) wrong = "depth mask";
    if (wrong) {
        batch::resynchronize();
        st.sf.ok = false;
        st.sf.attempted = true;
        st.cachedShadowValid = false;
        t = State::Tracked{};

    }
}

ProgInfo& progInfo(State& st, GLuint program) {
    const unsigned generation = batch::programGeneration(program);
    if (st.cachedInfo && program == st.cachedProg && st.cachedInfo->generation == generation) return *st.cachedInfo;
    auto it = st.progs.find(program);
    if (it != st.progs.end() && it->second.generation != generation) {
        st.cachedInfo = nullptr;
        st.progs.erase(it);
        it = st.progs.end();
    }
    if (it != st.progs.end()) {
        st.cachedProg = program;
        st.cachedInfo = &it->second;
        return it->second;
    }
    ProgInfo pi;
    pi.generation = generation;
    pi.view = real.GetUniformLocation(program, "u_view");
    pi.proj = real.GetUniformLocation(program, "u_proj");
    pi.viewProj = real.GetUniformLocation(program, "u_viewProj");
    pi.modelView = real.GetUniformLocation(program, "u_modelView");
    pi.modelViewProj = real.GetUniformLocation(program, "u_modelViewProj");
    pi.light = real.GetUniformLocation(program, kLightUniform);
    if (gSunPatched.load(std::memory_order_relaxed) > 0) {
        pi.sunDelta = real.GetUniformLocation(program, kSunUniform);
        if (pi.sunDelta >= 0) pi.sunTex = real.GetUniformLocation(program, kSunTexture);
    }
    ProgInfo& stored = st.progs.emplace(program, pi).first->second;
    st.cachedProg = program;
    st.cachedInfo = &stored;
    return stored;
}

constexpr int kDepthUnit = 15, kShadowUnit = 13;

bool compile(GLuint shader, const char* source) {
    real.ShaderSource(shader, 1, &source, nullptr);
    real.CompileShader(shader);
    GLint ok = 0;
    real.GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    return ok != 0;
}

bool queryAttachment(const Attach& a, int& w, int& h, GLenum& format, int& samples) {
    GLint iw = 0, ih = 0, fmt = 0, ms = 0;
    if (a.texture) {
        real.ActiveTexture(GL_TEXTURE0 + kDepthUnit);
        real.BindTexture(GL_TEXTURE_2D, a.object);
        real.GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &iw);
        real.GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &ih);
        real.GetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &fmt);
    } else {
        real.BindRenderbuffer(GL_RENDERBUFFER, a.object);
        real.GetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &iw);
        real.GetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &ih);
        real.GetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &fmt);
        real.GetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &ms);
    }
    samples = ms;
    w = iw;
    h = ih;
    format = static_cast<GLenum>(fmt);
    return iw > 0 && ih > 0 && fmt != 0;
}

bool depthFormatOk(GLenum f) { return f == 0x81A5 || f == 0x81A6 || f == 0x8CAC || f == GL_DEPTH24_STENCIL8 || f == GL_DEPTH32F_STENCIL8; }

bool currentAttachment(GLenum attachment, Attach& out) {
    GLint type = 0, name = 0, level = 0;
    real.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment, 0x8CD0, &type);
    if (type != 0x1702 && type != static_cast<GLint>(GL_RENDERBUFFER)) return false;
    real.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment, 0x8CD1, &name);
    if (type == 0x1702) {
        real.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment, 0x8CD2, &level);
        if (level != 0) return false;
    }
    out = {static_cast<GLuint>(name), type == 0x1702};
    return name != 0;
}

bool isDepthStencil(GLenum f) { return f == GL_DEPTH24_STENCIL8 || f == GL_DEPTH32F_STENCIL8; }

bool ensureTextures(Own& own, int w, int h, GLenum depthFormat) {
    if (own.depthTex && own.width == w && own.height == h && own.depthFormat == depthFormat) return true;
    if (own.depthTex) real.DeleteTextures(1, &own.depthTex);
    own.depthTex = 0;

    real.GenTextures(1, &own.depthTex);
    real.ActiveTexture(GL_TEXTURE0 + kDepthUnit);
    real.BindTexture(GL_TEXTURE_2D, own.depthTex);
    real.TexStorage2D(GL_TEXTURE_2D, 1, depthFormat, w, h);
    real.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    real.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    real.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    real.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (!own.blitFbo) real.GenFramebuffers(1, &own.blitFbo);
    real.BindFramebuffer(GL_DRAW_FRAMEBUFFER, own.blitFbo);
    const GLenum none = 0;
    real.DrawBuffers(1, &none);
    real.FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    real.FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, 0x8D20, GL_TEXTURE_2D, 0, 0);
    real.FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, isDepthStencil(depthFormat) ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT,
                              GL_TEXTURE_2D, own.depthTex, 0);
    const GLenum status = real.CheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        return false;
    }
    own.width = w;
    own.height = h;
    own.depthFormat = depthFormat;

    return true;
}

struct Saved {
    GLint drawFbo, readFbo, program, vao, activeTexture, renderbuffer;
    GLint texDepth, texShadow, samplerDepth, samplerShadow, depthFunc;
    GLfloat depthRange[2];
    GLint viewport[4];
    GLint blendSrcRgb, blendDstRgb, blendSrcAlpha, blendDstAlpha, blendEqRgb, blendEqAlpha;
    GLboolean scissor, depthTest, blend, cull, stencil, dither, discard, depthMask, colorMask[4];
};

void save(Saved& s) {
    real.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &s.drawFbo);
    real.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &s.readFbo);
    real.GetIntegerv(GL_CURRENT_PROGRAM, &s.program);
    real.GetIntegerv(GL_VERTEX_ARRAY_BINDING, &s.vao);
    real.GetIntegerv(GL_RENDERBUFFER_BINDING, &s.renderbuffer);
    real.GetIntegerv(GL_ACTIVE_TEXTURE, &s.activeTexture);
    real.ActiveTexture(GL_TEXTURE0 + kShadowUnit);
    real.GetIntegerv(GL_TEXTURE_BINDING_2D, &s.texShadow);
    real.GetIntegerv(GL_SAMPLER_BINDING, &s.samplerShadow);
    real.ActiveTexture(GL_TEXTURE0 + kDepthUnit);
    real.GetIntegerv(GL_TEXTURE_BINDING_2D, &s.texDepth);
    real.GetIntegerv(GL_SAMPLER_BINDING, &s.samplerDepth);
    real.GetIntegerv(GL_VIEWPORT, s.viewport);
    real.GetIntegerv(GL_DEPTH_FUNC, &s.depthFunc);
    real.GetFloatv(GL_DEPTH_RANGE, s.depthRange);
    real.GetIntegerv(GL_BLEND_SRC_RGB, &s.blendSrcRgb);
    real.GetIntegerv(GL_BLEND_DST_RGB, &s.blendDstRgb);
    real.GetIntegerv(GL_BLEND_SRC_ALPHA, &s.blendSrcAlpha);
    real.GetIntegerv(GL_BLEND_DST_ALPHA, &s.blendDstAlpha);
    real.GetIntegerv(GL_BLEND_EQUATION_RGB, &s.blendEqRgb);
    real.GetIntegerv(GL_BLEND_EQUATION_ALPHA, &s.blendEqAlpha);
    s.scissor = real.IsEnabled(GL_SCISSOR_TEST);
    s.depthTest = real.IsEnabled(GL_DEPTH_TEST);
    s.blend = real.IsEnabled(GL_BLEND);
    s.cull = real.IsEnabled(GL_CULL_FACE);
    s.stencil = real.IsEnabled(GL_STENCIL_TEST);
    s.dither = real.IsEnabled(GL_DITHER);
    s.discard = real.IsEnabled(GL_RASTERIZER_DISCARD);
    real.GetBooleanv(GL_DEPTH_WRITEMASK, &s.depthMask);
    real.GetBooleanv(GL_COLOR_WRITEMASK, s.colorMask);
}

void setEnabled(GLenum cap, GLboolean on) {
    if (on) real.Enable(cap);
    else real.Disable(cap);
}

void restore(const Saved& s) {
    real.UseProgram(static_cast<GLuint>(s.program));
    real.BindVertexArray(static_cast<GLuint>(s.vao));
    real.ActiveTexture(GL_TEXTURE0 + kShadowUnit);
    real.BindTexture(GL_TEXTURE_2D, static_cast<GLuint>(s.texShadow));
    real.BindSampler(kShadowUnit, static_cast<GLuint>(s.samplerShadow));
    real.ActiveTexture(GL_TEXTURE0 + kDepthUnit);
    real.BindTexture(GL_TEXTURE_2D, static_cast<GLuint>(s.texDepth));
    real.BindSampler(kDepthUnit, static_cast<GLuint>(s.samplerDepth));
    real.ActiveTexture(static_cast<GLenum>(s.activeTexture));
    real.BindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(s.renderbuffer));
    real.BindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(s.readFbo));
    real.BindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(s.drawFbo));
    real.Viewport(s.viewport[0], s.viewport[1], s.viewport[2], s.viewport[3]);
    setEnabled(GL_SCISSOR_TEST, s.scissor);
    setEnabled(GL_DEPTH_TEST, s.depthTest);
    real.DepthFunc(static_cast<GLenum>(s.depthFunc));
    real.DepthRangef(s.depthRange[0], s.depthRange[1]);
    real.BlendEquationSeparate(static_cast<GLenum>(s.blendEqRgb), static_cast<GLenum>(s.blendEqAlpha));
    real.BlendFuncSeparate(static_cast<GLenum>(s.blendSrcRgb), static_cast<GLenum>(s.blendDstRgb), static_cast<GLenum>(s.blendSrcAlpha),
                           static_cast<GLenum>(s.blendDstAlpha));
    setEnabled(GL_BLEND, s.blend);
    setEnabled(GL_CULL_FACE, s.cull);
    setEnabled(GL_STENCIL_TEST, s.stencil);
    setEnabled(GL_DITHER, s.dither);
    setEnabled(GL_RASTERIZER_DISCARD, s.discard);
    real.DepthMask(s.depthMask);
    real.ColorMask(s.colorMask[0], s.colorMask[1], s.colorMask[2], s.colorMask[3]);
}

GLuint buildProgram(const char* fragmentBody, bool world, bool smooth, bool cascades, bool fetch = false) {
    const std::string defines = std::string("#define SHADOW_SMOOTH ") + (smooth ? "1\n" : "0\n") +
        (world ? std::string("#define SHADOW_WORLD 1\n#define SHADOW_CASCADES ") + (cascades ? "1\n" : "0\n") : "");
    const auto specialize = [&](const char* source) {
        std::string result(source);
        result.insert(result.find('\n') + 1, defines);
        return result;
    };
    const std::string vertex = specialize(kVertexShader);
    std::string fragment = specialize(kFragmentCommon) + fragmentBody;
    if (fetch) {
        fragment.insert(fragment.find('\n') + 1, "#extension GL_EXT_shader_framebuffer_fetch : require\n#define FB_FETCH 1\n");
    }
    const GLuint vs = real.CreateShader(GL_VERTEX_SHADER);
    const GLuint fs = real.CreateShader(GL_FRAGMENT_SHADER);
    GLuint program = 0;
    if (compile(vs, vertex.c_str()) && compile(fs, fragment.c_str())) {
        program = real.CreateProgram();
        real.AttachShader(program, vs);
        real.AttachShader(program, fs);
        real.LinkProgram(program);
        GLint ok = 0;
        real.GetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            real.DeleteProgram(program);
            program = 0;
        }
    }
    real.DeleteShader(vs);
    real.DeleteShader(fs);
    return program;
}

bool hasFramebufferFetch() {
    static const bool has = [] {
        const char* ext = real.GetString ? reinterpret_cast<const char*>(real.GetString(GL_EXTENSIONS)) : nullptr;
        const bool ok = ext && std::strstr(ext, "GL_EXT_shader_framebuffer_fetch") != nullptr;
        return ok;
    }();
    return has;
}

bool ensureShadowProgram(Own& own, bool cascades, bool fetch) {
    const bool smooth = own.smFilter == 2;
    const int index = (fetch ? 4 : 0) + (cascades ? 2 : 0) + int(smooth);
    if (own.smProgram && own.worldVariant == index) { own.smFetch = fetch; return true; }
    auto& variant = own.worldShaders[index];
    if (!variant.program) {
        variant.program = buildProgram(kShadowMapBody, true, smooth, cascades, fetch);
        if (!variant.program && fetch) {
            own.fetchFailed = true;
            return ensureShadowProgram(own, cascades, false);
        }
        if (!variant.program) { own.smFailed = true; return false; }
        constexpr const char* names[]{"uDepth", "uShadow", "uInvVP", "uLightVP0", "uLightVP1", "uCam", "uSun", "uSm0", "uSm1", "uMisc", "uTint", "uFilter", "uAtlas", "uTuning", "uDepthRect", "uLight"};
        for (std::size_t i = 0; i < variant.locations.size(); ++i) variant.locations[i] = real.GetUniformLocation(variant.program, names[i]);
    }
    const std::array<GLint*, 16> locations{&own.smDepth, &own.smShadow, &own.smInvVP, &own.smLightVP0, &own.smLightVP1, &own.smCam, &own.smSun,
        &own.smSm0, &own.smSm1, &own.smMisc, &own.smTint, &own.smFilterUniform, &own.smAtlas, &own.smTuning, &own.smDepthRect, &own.smLight};
    for (std::size_t i = 0; i < locations.size(); ++i) *locations[i] = variant.locations[i];
    own.smProgram = variant.program;
    own.worldVariant = index;
    own.smFetch = fetch;
    if (!own.vao) real.GenVertexArrays(1, &own.vao);
    return true;
}

bool ensureHandProgram(Own& own) {
    const int index = own.smFilter == 2 ? 1 : 0;
    if (own.handProgram && own.handVariant == index) return true;
    if (own.handFailed) return false;
    auto& variant = own.handShaders[index];
    if (!variant.program) {
        variant.program = buildProgram(kHandBody, false, index != 0, false);
        if (!variant.program) { own.handFailed = true; return false; }
        constexpr const char* names[]{"uShadow", "uLightVP", "uCam", "uSun", "uSm", "uMisc", "uTint", "uFilter", "uAtlas"};
        for (std::size_t i = 0; i < variant.locations.size(); ++i) variant.locations[i] = real.GetUniformLocation(variant.program, names[i]);
    }
    const std::array<GLint*, 9> locations{&own.hShadow, &own.hLightVP, &own.hCam, &own.hSun, &own.hSm, &own.hMisc, &own.hTint, &own.hFilter, &own.hAtlas};
    for (std::size_t i = 0; i < locations.size(); ++i) *locations[i] = variant.locations[i];
    own.handProgram = variant.program;
    own.handVariant = index;
    if (!own.vao) real.GenVertexArrays(1, &own.vao);
    return true;
}

void beginTintPass(const SceneInfo& sc, bool reversed, bool maskByDepth) {
    real.BindFramebuffer(GL_DRAW_FRAMEBUFFER, sc.fbo);
    real.Viewport(sc.vx, sc.vy, sc.w, sc.h);
    real.Disable(GL_SCISSOR_TEST);
    if (maskByDepth) real.Enable(GL_DEPTH_TEST);
    else real.Disable(GL_DEPTH_TEST);
    real.DepthFunc(reversed ? GL_LESS : GL_GREATER);
    real.DepthMask(0);
    real.DepthRangef(0.0f, 1.0f);
    real.Enable(GL_BLEND);
    real.BlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    real.BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
    real.Disable(GL_CULL_FACE);
    real.Disable(GL_STENCIL_TEST);
    real.Disable(GL_RASTERIZER_DISCARD);
    real.ColorMask(1, 1, 1, 1);
}

bool ensureShadowMap(Own& own, int requestedRes, int columns, int bits, int filter) {
    if (own.smTex && own.smRequested == requestedRes && own.smColumns == columns && own.smBits == bits && own.smFilter == filter) return true;
    Saved saved{};
    save(saved);
    real.ActiveTexture(GL_TEXTURE0 + kShadowUnit);
    GLint maxSize = 0;
    real.GetIntegerv(0x0D33, &maxSize);
    const int res = std::min(requestedRes, maxSize / columns);
    if (res < 256) { restore(saved); return false; }
    const bool rebuild = !own.smTex || own.smRes != res || own.smColumns != columns || own.smBits != bits;
    if (rebuild) {
        batch::flush();
        if (own.smTex) real.DeleteTextures(1, &own.smTex);
        own.smTex = 0;
        real.GenTextures(1, &own.smTex);
        real.BindTexture(GL_TEXTURE_2D, own.smTex);
        real.TexStorage2D(GL_TEXTURE_2D, 1, bits == 16 ? 0x81A5 : GL_DEPTH_COMPONENT24, res * columns, res);
        real.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, static_cast<GLint>(GL_COMPARE_REF_TO_TEXTURE));
        real.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, static_cast<GLint>(GL_LEQUAL));
        real.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        real.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (!own.smFbo) real.GenFramebuffers(1, &own.smFbo);
        real.BindFramebuffer(GL_DRAW_FRAMEBUFFER, own.smFbo);
        const GLenum none = 0;
        real.DrawBuffers(1, &none);
        real.FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, own.smTex, 0);
        ++own.smGen;
    } else {
        real.BindTexture(GL_TEXTURE_2D, own.smTex);
        real.BindFramebuffer(GL_DRAW_FRAMEBUFFER, own.smFbo);
    }
    real.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter == 0 ? GL_NEAREST : GL_LINEAR);
    real.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter == 0 ? GL_NEAREST : GL_LINEAR);
    const GLenum status = real.CheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    restore(saved);
    if (status != GL_FRAMEBUFFER_COMPLETE || real.GetError() != 0) {
        return false;
    }
    own.smRes = res;
    own.smRequested = requestedRes;
    own.smColumns = columns;
    own.smBits = bits;
    own.smFilter = filter;

    return true;
}

bool shadowModeActive(const State& st) {
    const auto& c = config();
    return gActive.load(std::memory_order_relaxed) && c.enabled.load(std::memory_order_relaxed) &&
           (c.strength.load(std::memory_order_relaxed) > 0.001f || c.debugView.load(std::memory_order_relaxed)) && !st.own.failed && !st.own.smFailed;
}

void shadowFail(State& st) {
    st.own.smFailed = true;
}

bool beginShadowFrame(State& st, const ProgInfo& pi, GLuint fbo) {
    ShadowFrame& sf = st.sf;
    const auto& c = config();

    Mat4 camVP{}, camView{};
    bool haveVP = false;
    sf.haveView = false;
    if (pi.hView && plausibleView(pi.vView)) {
        camView = pi.vView;
        sf.haveView = true;
    } else if (st.haveView && plausibleView(st.view)) {
        camView = st.view;
        sf.haveView = true;
    }
    if (pi.hVP && plausibleVP(pi.vVP)) {
        camVP = pi.vVP;
        haveVP = true;
    } else if (pi.hProj && plausibleProj(pi.vProj) && sf.haveView) {
        camVP = mul(pi.vProj, camView);
        haveVP = true;
    } else if (st.haveProj && plausibleProj(st.proj) && sf.haveView) {
        camVP = mul(st.proj, camView);
        haveVP = true;
    }
    if (!haveVP || !invert(camVP, sf.invVP)) return false;
    sf.cameraVP = camVP;
    float center[3];
    if (!transformPoint(sf.invVP, 0.0f, 0.0f, -1.0f, center)) return false;

    SunParams sp;
    sp.dayCycle = c.dayCycle.load();
    sp.timeOfDay = currentGameTime();
    sp.azimuth = c.azimuth.load();
    sp.elevation = c.elevation.load();
    sp.tilt = c.tilt.load();
    sp.moon = c.moon.load();
    const SunResult sun = computeSun(sp);
    if (sun.strength < 0.01f) return false;
    sf.strength = sun.strength;

    const int requestedRes = c.shadowRes.load();
    const bool cascades = c.cascades.load();
    if (!ensureShadowMap(st.own, requestedRes, cascades ? 2 : 1, c.depthBits.load(), c.filterQuality.load())) {
        shadowFail(st);
        return false;
    }
    Own& own = st.own;

    const int res = own.smRes;
    const float dist = c.shadowDist.load();
    st.mapKey = {static_cast<std::uint32_t>(requestedRes), static_cast<std::uint32_t>(cascades), static_cast<std::uint32_t>(c.depthBits.load()),
                 std::bit_cast<std::uint32_t>(dist), std::bit_cast<std::uint32_t>(c.nearFraction.load()), std::bit_cast<std::uint32_t>(sp.azimuth),
                 std::bit_cast<std::uint32_t>(sp.elevation), std::bit_cast<std::uint32_t>(sp.tilt), static_cast<std::uint32_t>(sp.dayCycle),
                 static_cast<std::uint32_t>(sp.moon), static_cast<std::uint32_t>(c.cullDraws.load()), static_cast<std::uint32_t>(c.twoSided.load())};
    if (sf.haveView) {
        sf.cameraForward[0] = camView[2]; sf.cameraForward[1] = camView[6]; sf.cameraForward[2] = camView[10];
    }
    real.GetIntegerv(GL_VIEWPORT, sf.viewport);
    const int interval = c.updateInterval.load();
    if (interval > 1 && st.cachedShadowValid && st.cachedMapGeneration == own.smGen && st.mapKey == st.cachedMapKey &&
        st.cachedShadow.fbo == static_cast<GLint>(fbo) && st.frame - st.cachedShadowFrame < static_cast<unsigned>(interval) && sf.haveView && sf.anchorValid &&
        st.cachedShadow.anchorValid && sf.anchorKey == st.cachedShadow.anchorKey && sf.anchorMVP == st.cachedShadow.anchorMVP &&
        sf.cameraVP == st.cachedShadow.cameraVP) {
        const auto& cached = st.cachedShadow;
        float delta = 0.0f, facing = 0.0f;
        for (int i = 0; i < 3; ++i) {
            delta += (center[i] - cached.center[i]) * (center[i] - cached.center[i]);
            facing += sf.cameraForward[i] * cached.cameraForward[i];
        }
        if (delta < 4.0f && facing > 0.985f) {
            const auto currentInverse = sf.invVP;
            GLint viewport[4];
            std::copy_n(sf.viewport, 4, viewport);
            sf = cached;
            sf.reused = true;
            sf.invVP = currentInverse;
            sf.strength = sun.strength;
            std::copy_n(center, 3, sf.center);
            std::copy_n(viewport, 4, sf.viewport);
            return true;
        }
    }
    sf.batchDraws = c.batchDraws.load(std::memory_order_relaxed) && gBatchAvailable;
    sf.cullDraws = c.cullDraws.load(std::memory_order_relaxed);
    sf.twoSided = c.twoSided.load(std::memory_order_relaxed);
    sf.maxBatchDraws = c.maxBatchDraws.load(std::memory_order_relaxed);
    sf.cascades = cascades ? 2 : 1;
    for (int i = 0; i < 3; ++i) sf.sun[i] = sun.dir[i];
    sf.center[0] = center[0]; sf.center[1] = center[1]; sf.center[2] = center[2];
    Mat4 invView{};
    if (!(sf.haveView && invert(camView, invView))) sf.haveView = false;

    const float radius[2] = {std::max(12.0f, dist * c.nearFraction.load()), dist * 1.05f};
    for (int k = cascades ? 0 : 1; k < 2; ++k) {
        Light light;
        buildLight(center, sf.sun, radius[k], res, light);
        sf.lightView[k] = light.view;
        sf.lightProj[k] = light.proj;
        sf.lightVP[k] = light.vp;
        sf.texel[k] = light.texel;
        sf.range[k] = light.range;
        sf.radius[k] = radius[k];
        sf.T[k] = mul(light.vp, sf.invVP);
        if (sf.haveView) sf.Tv[k] = mul(light.view, invView);
    }

    real.GetIntegerv(GL_VIEWPORT, sf.viewport);
    GLfloat clearDepth = 1.0f;
    real.GetFloatv(GL_DEPTH_CLEAR_VALUE, &clearDepth);
    const GLboolean scissor = real.IsEnabled(GL_SCISSOR_TEST);
    real.BindFramebuffer(GL_DRAW_FRAMEBUFFER, own.smFbo);
    real.Viewport(0, 0, own.smRes * own.smColumns, own.smRes);
    if (scissor) real.Disable(GL_SCISSOR_TEST);
    real.ClearDepthf(1.0f);
    real.Clear(GL_DEPTH_BUFFER_BIT);
    real.ClearDepthf(clearDepth);
    if (scissor) real.Enable(GL_SCISSOR_TEST);
    real.Viewport(sf.viewport[0], sf.viewport[1], sf.viewport[2], sf.viewport[3]);
    real.BindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    return true;
}

enum class DrawKind { Elements, Arrays, ElementsInstanced, ArraysInstanced };
struct DrawCall {
    DrawKind kind;
    GLenum mode;
    GLsizei count;
    GLenum type;
    const void* indices;
    GLint first;
    GLsizei instances;
};

struct DrawCtx {
    bool replay = false;
    GLint fbo = 0, program = 0;
};

void issue(const DrawCall& d) {
    switch (d.kind) {
        case DrawKind::Elements: real.DrawElements(d.mode, d.count, d.type, d.indices); break;
        case DrawKind::Arrays: real.DrawArrays(d.mode, d.first, d.count); break;
        case DrawKind::ElementsInstanced: real.DrawElementsInstanced(d.mode, d.count, d.type, d.indices, d.instances); break;
        case DrawKind::ArraysInstanced: real.DrawArraysInstanced(d.mode, d.first, d.count, d.instances); break;
    }
}

void shadowReplay(State& st, const DrawCtx& ctx, const DrawCall& dc) {
    ShadowFrame& sf = st.sf;
    Own& own = st.own;
    ProgInfo& pi = progInfo(st, static_cast<GLuint>(ctx.program));

    const bool canPosition = pi.modelViewProj >= 0 || pi.viewProj >= 0 || (pi.view >= 0 && pi.proj >= 0) || (pi.modelView >= 0 && pi.proj >= 0);
    if (!canPosition) {
        return;
    }

    if ((pi.view >= 0 && !pi.hView) || (pi.proj >= 0 && !pi.hProj) || (pi.viewProj >= 0 && !pi.hVP) || (pi.modelView >= 0 && !pi.hMV) ||
        (pi.modelViewProj >= 0 && !pi.hMVP)) {
        return;
    }

    if (!sf.attempted) {
        sf.anchorValid = pi.hMVP;
        sf.anchorMVP = pi.vMVP;
        sf.anchorKey = (static_cast<std::uint64_t>(static_cast<GLuint>(ctx.program)) << 32) ^ batch::currentVertexArray() ^
                       (static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(dc.indices)) * 0x9E3779B185EBCA87ull) ^
                       (static_cast<std::uint64_t>(static_cast<unsigned>(dc.count)) << 16) ^ static_cast<unsigned>(dc.first);
        sf.attempted = true;
        sf.ok = beginShadowFrame(st, pi, static_cast<GLuint>(ctx.fbo));
        if (!sf.ok) return;
    }
    if (!sf.ok || sf.reused || batch::shouldSkipDraw()) return;
    if (pi.modelView >= 0 && !sf.haveView) {
        return;
    }

    struct Swap {
        GLint location;
        const Mat4* camera;
        Mat4 light;
    };

    const bool cull = sf.cullDraws;
    const bool instanced = dc.kind == DrawKind::ElementsInstanced || dc.kind == DrawKind::ArraysInstanced;
    bool cullable = cull && !instanced && pi.modelViewProj >= 0;
    if (cullable) {
        const Mat4& m = pi.vMVP;
        float w[4];
        for (int r = 0; r < 4; ++r) w[r] = sf.invVP[r] * m[12] + sf.invVP[4 + r] * m[13] + sf.invVP[8 + r] * m[14] + sf.invVP[12 + r] * m[15];
        if (std::fabs(w[3]) < 1e-6f || (std::fabs(w[0]) + std::fabs(w[1]) + std::fabs(w[2])) < 1e-3f * std::fabs(w[3])) cullable = false;
    }
    constexpr float kChunkMargin = 40.0f;
    Swap swaps[2][5];
    bool want[2] = {false, false};
    int n = 0;
    for (int k = sf.cascades == 2 ? 0 : 1; k < 2; ++k) {
        int m = 0;
        if (pi.viewProj >= 0) swaps[k][m++] = {pi.viewProj, &pi.vVP, sf.lightVP[k]};
        if (pi.modelViewProj >= 0) {
            if (cullable) {
                float oc[4];
                mulColumn3(sf.T[k], pi.vMVP, oc);
                const float limit = 1.0f + kChunkMargin / sf.radius[k];
                if (std::fabs(oc[0]) > limit * std::fabs(oc[3]) || std::fabs(oc[1]) > limit * std::fabs(oc[3])) {
                    continue;
                }
            }
            swaps[k][m++] = {pi.modelViewProj, &pi.vMVP, mul(sf.T[k], pi.vMVP)};
        }
        if (pi.view >= 0) swaps[k][m++] = {pi.view, &pi.vView, sf.lightView[k]};
        if (pi.proj >= 0) swaps[k][m++] = {pi.proj, &pi.vProj, sf.lightProj[k]};
        if (pi.modelView >= 0) swaps[k][m++] = {pi.modelView, &pi.vMV, mul(sf.Tv[k], pi.vMV)};
        n = m;
        want[k] = true;
    }
    if (!want[0] && !want[1]) return;

    if (sf.batchDraws) {
        std::array<batch::MatrixUpload, 5> uploads{};
        const int first = want[0] ? 0 : 1;
        unsigned mask = 0;
        for (int i = 0; i < n; ++i) uploads[i].location = swaps[first][i].location;
        for (int k = 0; k < 2; ++k) {
            if (!want[k]) continue;
            mask |= 1u << k;
            for (int i = 0; i < n; ++i) uploads[i].light[k] = swaps[k][i].light;
        }
        const batch::DrawCall draw{static_cast<batch::DrawKind>(dc.kind), dc.mode, dc.count, dc.type, dc.indices, dc.first, dc.instances};
        if (batch::queue({own.smFbo, own.smRes, own.smColumns}, static_cast<GLuint>(ctx.program), draw, uploads.data(), n, mask, sf.maxBatchDraws, sf.twoSided)) {
            ++sf.draws;
            return;
        }
    }
    batch::flush();

    const bool checking = st.smChecks < 8;
    if (checking) {
        ++st.smChecks;
        drainErrors();
    }

    const GLint depthFunc = trackedDepthFunc(st);
    const GLboolean scissor = trackedScissor(st);
    GLboolean colorMask[4]{};
    real.GetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    const GLboolean stencil = real.IsEnabled(GL_STENCIL_TEST), cullFace = real.IsEnabled(GL_CULL_FACE);
    GLfloat depthRange[2]{};
    real.GetFloatv(GL_DEPTH_RANGE, depthRange);
    real.ColorMask(0, 0, 0, 0);
    real.Disable(GL_STENCIL_TEST);
    if (sf.twoSided) real.Disable(GL_CULL_FACE);
    real.DepthRangef(0.0f, 1.0f);
    real.BindFramebuffer(GL_DRAW_FRAMEBUFFER, own.smFbo);
    if (scissor) real.Disable(GL_SCISSOR_TEST);

    const bool fixDepthFunc = depthFunc != static_cast<GLint>(GL_LESS) && depthFunc != static_cast<GLint>(GL_LEQUAL);
    if (fixDepthFunc) real.DepthFunc(GL_LESS);
    int lastSet = -1;
    for (int k = 0; k < 2; ++k) {
        if (!want[k]) continue;
        real.Viewport((own.smColumns == 2 ? k : 0) * own.smRes, 0, own.smRes, own.smRes);
        for (int i = 0; i < n; ++i) real.UniformMatrix4fv(swaps[k][i].location, 1, 0, swaps[k][i].light.data());
        issue(dc);

        lastSet = k;
    }
    for (int i = 0; i < n; ++i) real.UniformMatrix4fv(swaps[lastSet][i].location, 1, 0, swaps[lastSet][i].camera->data());
    real.ColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    setEnabled(GL_STENCIL_TEST, stencil);
    setEnabled(GL_CULL_FACE, cullFace);
    real.DepthRangef(depthRange[0], depthRange[1]);
    if (fixDepthFunc) real.DepthFunc(static_cast<GLenum>(depthFunc));
    if (scissor) real.Enable(GL_SCISSOR_TEST);
    real.Viewport(sf.viewport[0], sf.viewport[1], sf.viewport[2], sf.viewport[3]);
    real.BindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(ctx.fbo));
    ++sf.draws;

    if (checking) {
        const GLenum error = real.GetError();
        drainErrors();
        if (error != 0) shadowFail(st);
    }
}

void noteSkip(State& st) {
    if (++st.skips >= 5 && !st.own.failed) {
        st.own.failed = true;

    }
}

void runShadowPassImpl(State& st, GLuint sceneFbo) {
    Own& own = st.own;
    if (own.failed) return;
    const auto& c = config();
    const ShadowFrame& sf = st.sf;

    if (!(sf.ok && sf.draws > 0 && sf.fbo == static_cast<GLint>(sceneFbo) && own.smTex && !own.smFailed)) {
        return;
    }

    drainErrors();

    Saved saved{};
    save(saved);
    real.BindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneFbo);

    int w = 0, h = 0, depthSamples = 0;
    GLenum depthFormat = 0;
    Attach depthAttach;
    int vx = 0, vy = 0;
    bool assumedDepth = false;
    static const GLenum kGuesses[4] = {GL_DEPTH24_STENCIL8, 0x81A6  , 0x8CAC  , 0x81A5  };
    if (sceneFbo == 0) {
        GLint depthBits = 0, stencilBits = 0, samples = 0;

        real.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_DEPTH, GL_ATTACHMENT_DEPTH_SIZE, &depthBits);
        real.GetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_STENCIL, GL_ATTACHMENT_STENCIL_SIZE, &stencilBits);
        real.GetIntegerv(GL_SAMPLES, &samples);
        if (real.GetError() != 0) {
            noteSkip(st);
            restore(saved);
            return;
        }
        if (depthBits == 0) {
            GLint db = 0, sb = 0;
            real.GetIntegerv(0x0D56  , &db);
            real.GetIntegerv(0x0D57  , &sb);
            drainErrors();
            if (db > 0) {
                depthBits = db;
                stencilBits = sb;
            } else {
                depthBits = 24;
                stencilBits = 8;
                assumedDepth = true;
            }
        }

        vx = sf.viewport[0];
        vy = sf.viewport[1];
        w = sf.viewport[2];
        h = sf.viewport[3];
        depthSamples = samples;
        depthFormat = depthBits == 24 ? (stencilBits == 8 ? GL_DEPTH24_STENCIL8 : 0x81A6) : (depthBits == 16 && stencilBits == 0 ? 0x81A5 : depthBits == 32 ? (stencilBits == 8 ? GL_DEPTH32F_STENCIL8 : 0x8CAC) : 0);
        if (assumedDepth) depthFormat = kGuesses[own.assumeIdx % 4];
    } else {
        const bool attached = currentAttachment(GL_DEPTH_ATTACHMENT, depthAttach);
        if (!attached) {
            noteSkip(st);
            restore(saved);
            return;
        }
        const bool queried = queryAttachment(depthAttach, w, h, depthFormat, depthSamples);
        if (!queried || real.GetError() != 0) {
            noteSkip(st);
            restore(saved);
            return;
        }
    }

    if (depthSamples != 0 || !depthFormatOk(depthFormat) || w < 64 || h < 64) {
        noteSkip(st);
        restore(saved);
        return;
    }
    const bool wantFetch = (c.lightFade.load() > 0.0f || c.fogFade.load() > 0.0f) && gLightPatched.load(std::memory_order_relaxed) > 0 && !own.fetchFailed && hasFramebufferFetch();
    if (!ensureShadowProgram(own, sf.cascades == 2, wantFetch)) {
        restore(saved);
        return;
    }
    const int attachmentWidth = w, attachmentHeight = h;
    if (sceneFbo != 0) {
        vx = sf.viewport[0]; vy = sf.viewport[1];
        w = sf.viewport[2]; h = sf.viewport[3];
        if (vx < 0 || vy < 0 || w < 64 || h < 64 || vx + w > attachmentWidth || vy + h > attachmentHeight) {
            noteSkip(st);
            restore(saved);
            return;
        }
    }
    st.scene = {true, sceneFbo, vx, vy, w, h};
    bool directDepth = c.directDepth.load() && !own.directFailed && sceneFbo != 0 && depthAttach.texture;
    Attach stencilAttach;
    bool sharedStencil = false;
    if (directDepth) {
        sharedStencil = currentAttachment(0x8D20, stencilAttach) && stencilAttach.texture && stencilAttach.object == depthAttach.object;
        real.FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
        if (sharedStencil) real.FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, 0x8D20, GL_TEXTURE_2D, 0, 0);
        const GLenum status = real.CheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        const GLenum error = real.GetError();
        if (status != GL_FRAMEBUFFER_COMPLETE || error != 0) {
            real.FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthAttach.object, 0);
            if (sharedStencil) real.FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, 0x8D20, GL_TEXTURE_2D, depthAttach.object, 0);
            drainErrors();
            own.directFailed = true;
            directDepth = false;

        }
    }
    GLuint sampledDepth = depthAttach.object;
    if (!directDepth) {
        if (!ensureTextures(own, w, h, depthFormat)) {
            own.failed = true;

            restore(saved);
            return;
        }
        sampledDepth = own.depthTex;
        real.Disable(GL_SCISSOR_TEST);
        real.BindFramebuffer(GL_READ_FRAMEBUFFER, sceneFbo);
        real.BindFramebuffer(GL_DRAW_FRAMEBUFFER, own.blitFbo);
        real.BlitFramebuffer(vx, vy, vx + w, vy + h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    }
    if (!own.depthSampler) {
        real.GenSamplers(1, &own.depthSampler);
        real.SamplerParameteri(own.depthSampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        real.SamplerParameteri(own.depthSampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        real.SamplerParameteri(own.depthSampler, GL_TEXTURE_COMPARE_MODE, 0);
        real.SamplerParameteri(own.depthSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        real.SamplerParameteri(own.depthSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    beginTintPass(st.scene, c.reversedDepth.load(), false);

    real.UseProgram(own.smProgram);
    real.ActiveTexture(GL_TEXTURE0 + kDepthUnit);
    real.BindTexture(GL_TEXTURE_2D, sampledDepth);
    real.BindSampler(kDepthUnit, own.depthSampler);
    real.Uniform1i(own.smDepth, kDepthUnit);
    real.ActiveTexture(GL_TEXTURE0 + kShadowUnit);
    real.BindTexture(GL_TEXTURE_2D, own.smTex);
    real.BindSampler(kShadowUnit, 0);
    real.Uniform1i(own.smShadow, kShadowUnit);
    real.UniformMatrix4fv(own.smInvVP, 1, 0, sf.invVP.data());
    const int k0 = sf.cascades == 2 ? 0 : 1;
    if (own.smLightVP0 >= 0) real.UniformMatrix4fv(own.smLightVP0, 1, 0, sf.lightVP[k0].data());
    real.UniformMatrix4fv(own.smLightVP1, 1, 0, sf.lightVP[1].data());
    real.Uniform4f(own.smCam, sf.center[0], sf.center[1], sf.center[2], c.reversedDepth.load() ? 1.0f : 0.0f);
    real.Uniform4f(own.smSun, sf.sun[0], sf.sun[1], sf.sun[2], c.strength.load() * sf.strength);
    if (own.smSm0 >= 0) real.Uniform4f(own.smSm0, sf.texel[k0], sf.range[k0], sf.radius[k0], 0.0f);
    real.Uniform4f(own.smSm1, sf.texel[1], sf.range[1], sf.radius[1], 0.0f);
    real.Uniform4f(own.smMisc, 1.0f / static_cast<float>(own.smRes * own.smColumns), 1.0f / static_cast<float>(own.smRes), sf.cascades == 2 ? 1.0f : 0.0f, c.shadowDist.load());
    real.Uniform4f(own.smTint, c.tintRed.load(), c.tintGreen.load(), c.tintBlue.load(), c.debugView.load() ? 1.0f : 0.0f);
    real.Uniform4f(own.smFilterUniform, static_cast<float>(c.filterQuality.load()), c.softness.load(), c.depthBias.load(), c.normalBias.load());
    real.Uniform4f(own.smAtlas, 1.0f / static_cast<float>(own.smColumns), own.smColumns == 2 ? 0.5f : 0.0f, 0.0f, 0.0f);
    real.Uniform4f(own.smTuning, c.fadeStart.load(), 0.0f, 0.0f, 0.0f);
    if (own.smLight >= 0) real.Uniform4f(own.smLight, (c.lightFade.load() > 0.0f || c.fogFade.load() > 0.0f) ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
    real.Uniform4f(own.smDepthRect, directDepth ? float(vx) / float(attachmentWidth) : 0.0f, directDepth ? float(vy) / float(attachmentHeight) : 0.0f,
                  directDepth ? float(w) / float(attachmentWidth) : 1.0f, directDepth ? float(h) / float(attachmentHeight) : 1.0f);
    real.BindVertexArray(own.vao);
    real.DrawArrays(GL_TRIANGLES, 0, 3);

    if (directDepth) {
        real.FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthAttach.object, 0);
        if (sharedStencil) real.FramebufferTexture2D(GL_DRAW_FRAMEBUFFER, 0x8D20, GL_TEXTURE_2D, depthAttach.object, 0);
    }
    restore(saved);

    const GLenum error = real.GetError();
    drainErrors();
    if (error != 0 && directDepth) {
        own.directFailed = true;

        return;
    }
    if (error != 0 && assumedDepth && own.assumeIdx < 3) {
        ++own.assumeIdx;
        return;
    }
    if (error != 0 && own.smFetch) {
        own.fetchFailed = true;
        return;
    }
    if (error != 0) {
        own.failed = true;

        return;
    }

    st.skips = 0;
}

void resetSceneAlpha(GLuint sceneFbo) {
    Saved saved{};
    save(saved);
    GLfloat clearColor[4]{};
    real.GetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
    real.BindFramebuffer(GL_DRAW_FRAMEBUFFER, sceneFbo);
    real.Disable(GL_SCISSOR_TEST);
    real.ColorMask(0, 0, 0, 1);
    real.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    real.Clear(GL_COLOR_BUFFER_BIT);
    real.ClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    restore(saved);
    drainErrors();
}

void runShadowPass(State& st, GLuint sceneFbo) {
    runShadowPassImpl(st, sceneFbo);
    if (gLightPatched.load(std::memory_order_relaxed) > 0) resetSceneAlpha(sceneFbo);
}

void runHandPass(State& st) {
    Own& own = st.own;
    const auto& c = config();
    const ShadowFrame& sf = st.sf;
    const SceneInfo& sc = st.scene;
    if (batch::shouldSkipDraw() || !c.handShadow.load() || own.failed || own.handFailed || !sc.valid || !sf.ok || sf.draws == 0 || !own.smTex || own.smFailed) return;
    if (!ensureHandProgram(own)) return;

    drainErrors();
    Saved saved{};
    save(saved);

    beginTintPass(sc, c.reversedDepth.load(), true);
    real.UseProgram(own.handProgram);
    real.ActiveTexture(GL_TEXTURE0 + kShadowUnit);
    real.BindTexture(GL_TEXTURE_2D, own.smTex);
    real.BindSampler(kShadowUnit, 0);
    real.Uniform1i(own.hShadow, kShadowUnit);
    const int kh = sf.cascades == 2 ? 0 : 1;
    real.UniformMatrix4fv(own.hLightVP, 1, 0, sf.lightVP[kh].data());
    real.Uniform4f(own.hCam, sf.center[0], sf.center[1], sf.center[2], c.reversedDepth.load() ? 1.0f : 0.0f);
    real.Uniform4f(own.hSun, sf.sun[0], sf.sun[1], sf.sun[2], c.strength.load() * sf.strength);
    real.Uniform4f(own.hSm, sf.texel[kh], sf.range[kh], 0.0f, 0.0f);
    real.Uniform4f(own.hMisc, 1.0f / static_cast<float>(own.smRes * own.smColumns), 1.0f / static_cast<float>(own.smRes), static_cast<float>(kh), 0.0f);
    real.Uniform4f(own.hTint, c.tintRed.load(), c.tintGreen.load(), c.tintBlue.load(), c.debugView.load() ? 1.0f : 0.0f);
    real.Uniform4f(own.hFilter, static_cast<float>(c.filterQuality.load()), c.softness.load(), c.depthBias.load(), c.normalBias.load());
    real.Uniform4f(own.hAtlas, 1.0f / static_cast<float>(own.smColumns), own.smColumns == 2 ? 0.5f : 0.0f, 0.0f, 0.0f);
    real.BindVertexArray(own.vao);
    real.DrawArrays(GL_TRIANGLES, 0, 3);

    restore(saved);

    const GLenum error = real.GetError();
    drainErrors();
    if (error != 0) {
        own.handFailed = true;

        return;
    }
}

void onContextChanged(State& st, void* ctx) {
    st.ctx = ctx;
    batch::resetContext();
    st.cachedShadowValid = false;
    st.own = Own{};
    st.progs.clear();
    st.cachedInfo = nullptr;
    st.view = {};
    st.proj = {};
    st.haveView = st.haveProj = false;
    st.sf = ShadowFrame{};
    st.scene = SceneInfo{};
    st.shadowedThisFrame = false;
    st.opaqueSeen = st.prevOpaqueSeen = st.opaqueAtTrigger = st.earlyBad = 0;
    st.earlyThisFrame = st.prevEarly = st.opaqueFrozen = st.earlyDisabled = false;
    st.handPhase = st.handDone = false;
    st.handDraws = 0;
    st.depthDraws = 0;
    st.cachedProg = 0;
    st.cachedInfo = nullptr;
    {
        const bool bad = st.tr.bad;
        st.tr = State::Tracked{};
        st.tr.bad = bad;
    }
    st.smChecks = st.skips = 0;
    st.retries = st.failedFrames = 0;
}

bool hasContext(State& st) {
    void* ctx = gGetCurrentContext ? gGetCurrentContext() : nullptr;
    if (!ctx) return false;
    if (ctx != st.ctx) onContextChanged(st, ctx);
    return true;
}

bool canShow(const State& st) {
    if (!gActive.load(std::memory_order_relaxed) || st.shadowedThisFrame || st.own.failed || batch::shouldSkipDraw()) return false;
    const auto& c = config();
    return c.enabled.load() && st.depthDraws > 0;
}

void triggerShadowPass(State& st, GLuint fbo) {
    batch::flush();
    st.shadowedThisFrame = true;

    GLuint target = fbo;
    const ShadowFrame& sf = st.sf;
    if (sf.ok && sf.draws > 0 && sf.fbo >= 0 && static_cast<GLuint>(sf.fbo) != fbo) target = static_cast<GLuint>(sf.fbo);

    runShadowPass(st, target);
}

void endFrame(State& st);
void watchdogSwapHook(unsigned long long totalDraws);

constexpr unsigned kStuckFrameDraws = 200000;
void watchdogStuckFrame(State& st) {
    if (st.frameDraws < kStuckFrameDraws) return;

    endFrame(st);
}

void updateLightMode(State& st) {
    const GLint program = trackedProgram(st);
    if (program <= 0) return;
    ProgInfo& pi = progInfo(st, static_cast<GLuint>(program));
    if (pi.light < 0) return;
    const auto& c = config();
    const float lightStrength = c.lightFade.load(), fogStrength = c.fogFade.load();
    const bool on = c.enabled.load() && (lightStrength > 0.0f || fogStrength > 0.0f) && !st.own.failed && !st.own.fetchFailed && !st.shadowedThisFrame && !trackedBlend(st);
    const float a = on ? lightStrength : 0.0f, b = on ? fogStrength : 0.0f;
    if (pi.lightVal[0] != a || pi.lightVal[1] != b) {
        real.Uniform4f(pi.light, a, b, 0.0f, 0.0f);
        pi.lightVal[0] = a;
        pi.lightVal[1] = b;
    }
    if (!on) return;
    if (!st.tr.maskKnown) {
        real.GetBooleanv(GL_COLOR_WRITEMASK, st.tr.mask);
        st.tr.maskKnown = true;
    }
    if (!st.tr.mask[3]) {
        real.ColorMask(st.tr.mask[0], st.tr.mask[1], st.tr.mask[2], 1);
        st.alphaForced = true;
    }
}

void updateSunMoon(State& st) {
    const GLint program = trackedProgram(st);
    if (program <= 0) return;
    ProgInfo& pi = progInfo(st, static_cast<GLuint>(program));
    if (pi.sunDelta < 0) return;
    float delta[9] = {};
    const auto& c = config();
    if (pi.sunTex >= 0 && c.enabled.load() && c.syncSun.load()) {
        SunParams sp;
        sp.dayCycle = c.dayCycle.load();
        sp.timeOfDay = currentGameTime();
        sp.azimuth = c.azimuth.load();
        sp.elevation = c.elevation.load();
        sp.tilt = c.tilt.load();
        (void)computeSunDelta(sp, delta);
    }
    if (pi.sunKnown && std::memcmp(delta, pi.sunVal, sizeof delta) == 0) return;
    real.UniformMatrix3fv(pi.sunDelta, 1, 0, delta);
    std::memcpy(pi.sunVal, delta, sizeof delta);
    pi.sunKnown = true;
}
void endForcedAlpha(State& st) {
    real.ColorMask(st.tr.mask[0], st.tr.mask[1], st.tr.mask[2], st.tr.mask[3]);
    st.alphaForced = false;
}

DrawCtx onDraw() {
    DrawCtx ctx;
    State& st = state();
    if (!hasContext(st)) return ctx;
    if (!config().enabled.load(std::memory_order_relaxed)) st.cachedShadowValid = false;
    if ((st.totalDraws & 1023) == 0 && !st.tr.bad) verifyTracking(st);
    if ((st.totalDraws & 4095) == 0) watchdogSwapHook(st.totalDraws);
    watchdogStuckFrame(st);
    ++st.totalDraws;
    ++st.frameDraws;
    if (gSunPatched.load(std::memory_order_relaxed) > 0) updateSunMoon(st);
    if (gLightPatched.load(std::memory_order_relaxed) > 0) updateLightMode(st);
    const bool depthTest = trackedDepthTest(st);
    GLint fbo = -1, program = 0;
    auto drawFbo = [&] {
        if (fbo < 0) fbo = trackedFbo(st);
        return fbo;
    };
    if (st.handPhase && !st.handDone && st.scene.valid && static_cast<GLuint>(drawFbo()) == st.scene.fbo) {
        if (depthTest) {
            ++st.handDraws;
        } else if (st.handDraws > 0) {
            st.handDone = true;
            st.handPhase = false;
            runHandPass(st);
        }
    }
    if (!depthTest) return ctx;
    ++st.depthDraws;

    if (st.opaqueFrozen || (st.shadowedThisFrame && !st.earlyThisFrame) || !shadowModeActive(st)) return ctx;
    program = trackedProgram(st);
    if (program == 0) return ctx;
    const bool depthMask = trackedDepthMask(st);
    const bool blend = trackedBlend(st);
    if (depthMask && !blend) {
        if (st.sf.fbo < 0) st.sf.fbo = drawFbo();
        if (drawFbo() == st.sf.fbo) {
            ++st.opaqueSeen;
            if (!st.shadowedThisFrame) {
                ctx.replay = true;
                ctx.fbo = fbo;
                ctx.program = program;
            }
        }
    } else if (blend && config().beforeBlend.load() && !st.earlyDisabled && !st.shadowedThisFrame && st.sf.ok && st.sf.draws > 0 &&
               st.prevOpaqueSeen >= 30 && st.opaqueSeen * 100 >= st.prevOpaqueSeen * (st.prevEarly ? 75u : 90u) && drawFbo() == st.sf.fbo &&
               canShow(st)) {
        st.earlyThisFrame = true;
        st.opaqueAtTrigger = st.opaqueSeen;
        triggerShadowPass(st, static_cast<GLuint>(fbo));
    }
    return ctx;
}

void uniformMatrix4fvDetour(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
    State& st = state();
    if (count == 1 && value && location >= 0 && hasContext(st)) {
        const GLint program = trackedProgram(st);
        batch::useProgram(static_cast<GLuint>(program));
        ProgInfo& pi = progInfo(st, static_cast<GLuint>(program));
        if (location == pi.view || location == pi.proj || location == pi.viewProj || location == pi.modelView || location == pi.modelViewProj) {
            Mat4 m;
            for (std::size_t i = 0; i < 16; ++i) m[i] = transpose ? value[(i % 4) * 4 + i / 4] : value[i];
            if (location == pi.view) {
                pi.vView = m;
                pi.hView = true;
                st.view = m;
                st.haveView = true;

            } else if (location == pi.proj) {
                pi.vProj = m;
                pi.hProj = true;
                st.proj = m;
                st.haveProj = true;

            } else if (location == pi.viewProj) {
                pi.vVP = m;
                pi.hVP = true;
            } else if (location == pi.modelView) {
                pi.vMV = m;
                pi.hMV = true;
            } else {
                pi.vMVP = m;
                pi.hMVP = true;
            }
        }
    }
    batch::uniformMatrix4(location, count, transpose, value);
    resolve(kUniformMatrix4fv, real.UniformMatrix4fv)(location, count, transpose, value);
}

void setCap(GLenum cap, signed char v) {
    batch::cap(cap, v != 0);
    auto& t = state().tr;
    if (cap == GL_DEPTH_TEST) t.depthTest = v;
    else if (cap == GL_BLEND) t.blend = v;
    else if (cap == GL_SCISSOR_TEST) t.scissor = v;
}
void enableDetour(GLenum cap) {
    setCap(cap, 1);
    resolve(kEnable, real.Enable)(cap);
}
void disableDetour(GLenum cap) {
    setCap(cap, 0);
    resolve(kDisable, real.Disable)(cap);
}
void depthMaskDetour(GLboolean flag) {
    state().tr.depthMask = flag ? 1 : 0;
    resolve(kDepthMask, real.DepthMask)(flag);
}
void depthFuncDetour(GLenum func) {
    state().tr.depthFunc = static_cast<GLint>(func);
    resolve(kDepthFunc, real.DepthFunc)(func);
}
void bindFramebufferDetour(GLenum target, GLuint framebuffer) {
    if ((target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) && state().tr.fbo != static_cast<GLint>(framebuffer)) batch::flush();
    if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) state().tr.fbo = static_cast<GLint>(framebuffer);
    resolve(kBindFramebuffer, real.BindFramebuffer)(target, framebuffer);
}
void useProgramDetour(GLuint program) {
    batch::useProgram(program);
    state().tr.program = static_cast<GLint>(program);
    resolve(kUseProgram, real.UseProgram)(program);
}

void colorMaskDetour(GLboolean r, GLboolean gr, GLboolean b, GLboolean a) {
    State& st = state();
    st.tr.mask[0] = r; st.tr.mask[1] = gr; st.tr.mask[2] = b; st.tr.mask[3] = a;
    st.tr.maskKnown = true;
    resolve(kColorMask, real.ColorMask)(r, gr, b, a);
}

void shaderSourceDetour(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length) {
    auto original = resolve(kShaderSource, real.ShaderSource);
    if (count > 0 && string && gActive.load(std::memory_order_relaxed)) {
        std::string source;
        for (GLsizei i = 0; i < count; ++i) {
            if (!string[i]) continue;
            if (length && length[i] >= 0) source.append(string[i], static_cast<std::size_t>(length[i]));
            else source.append(string[i]);
        }
        const LightPatch kind = patchLightShader(source);
        if (kind == LightPatch::SunMoon) gSunPatched.fetch_add(1, std::memory_order_relaxed);
        if (kind == LightPatch::ChunkVertex || kind == LightPatch::SunMoon) {
            const char* p = source.c_str();
            original(shader, 1, &p, nullptr);
            return;
        }
        if (kind != LightPatch::None) {
            gLightPatched.fetch_add(1, std::memory_order_relaxed);
            const char* p = source.c_str();
            original(shader, 1, &p, nullptr);
            return;
        }
    }
    original(shader, count, string, length);
}

void beforeDepthClear() {
    State& st = state();
    if (!hasContext(st)) return;
    const GLint fbo = trackedFbo(st);
    if (canShow(st)) triggerShadowPass(st, static_cast<GLuint>(fbo));
    if (st.shadowedThisFrame) st.opaqueFrozen = true;
    if (st.shadowedThisFrame && !st.handPhase && !st.handDone && st.sf.ok && st.sf.draws > 0) {
        st.handPhase = true;
        st.handDraws = 0;
    }
}
void clearDetour(GLbitfield mask) {
    if ((mask & GL_DEPTH_BUFFER_BIT) != 0) beforeDepthClear();
    resolve(kClear, real.Clear)(mask);
}

void drawElementsDetour(GLenum mode, GLsizei count, GLenum type, const void* indices) {
    const DrawCtx ctx = onDraw();
    resolve(kDrawElements, real.DrawElements)(mode, count, type, indices);
    if (state().alphaForced) endForcedAlpha(state());
    if (ctx.replay) shadowReplay(state(), ctx, {DrawKind::Elements, mode, count, type, indices, 0, 1});
}
void drawArraysDetour(GLenum mode, GLint first, GLsizei count) {
    const DrawCtx ctx = onDraw();
    resolve(kDrawArrays, real.DrawArrays)(mode, first, count);
    if (state().alphaForced) endForcedAlpha(state());
    if (ctx.replay) shadowReplay(state(), ctx, {DrawKind::Arrays, mode, count, 0, nullptr, first, 1});
}
void drawElementsInstancedDetour(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instances) {
    const DrawCtx ctx = onDraw();
    resolve(kDrawElementsInstanced, real.DrawElementsInstanced)(mode, count, type, indices, instances);
    if (state().alphaForced) endForcedAlpha(state());
    if (ctx.replay) shadowReplay(state(), ctx, {DrawKind::ElementsInstanced, mode, count, type, indices, 0, instances});
}
void drawArraysInstancedDetour(GLenum mode, GLint first, GLsizei count, GLsizei instances) {
    const DrawCtx ctx = onDraw();
    resolve(kDrawArraysInstanced, real.DrawArraysInstanced)(mode, first, count, instances);
    if (state().alphaForced) endForcedAlpha(state());
    if (ctx.replay) shadowReplay(state(), ctx, {DrawKind::ArraysInstanced, mode, count, 0, nullptr, first, instances});
}

void endFrame(State& st) {
    batch::finishFrame();
    if (config().updateInterval.load(std::memory_order_relaxed) > 1 && st.sf.ok && st.sf.draws > 0 && !st.sf.reused && !st.own.failed && !st.own.smFailed) {
        st.cachedShadow = st.sf;
        st.cachedShadowFrame = st.frame;
        st.cachedMapGeneration = st.own.smGen;
        st.cachedMapKey = st.mapKey;
        st.cachedShadowValid = true;
    } else if (config().updateInterval.load(std::memory_order_relaxed) <= 1) st.cachedShadowValid = false;
    if (st.own.failed || st.own.smFailed || st.own.handFailed) {
        if (++st.failedFrames >= 300 && st.retries < 20) {
            ++st.retries;
            st.failedFrames = 0;
            st.skips = 0;
            st.own.failed = st.own.smFailed = st.own.handFailed = false;

        }
    } else {
        st.failedFrames = 0;
    }
    if (st.earlyThisFrame) {
        const unsigned late = st.opaqueSeen - st.opaqueAtTrigger;

        if (late * 20 > st.opaqueSeen) {
            if (++st.earlyBad >= 5) {
                st.earlyDisabled = true;

            }
        } else {
            st.earlyBad = 0;
        }
    }
    st.prevOpaqueSeen = st.opaqueSeen;
    st.prevEarly = st.earlyThisFrame;
    st.opaqueSeen = st.opaqueAtTrigger = 0;
    st.earlyThisFrame = st.opaqueFrozen = false;
    st.frameDraws = 0;
    st.depthDraws = 0;
    st.shadowedThisFrame = false;
    st.handPhase = st.handDone = false;
    st.handDraws = 0;
    st.scene.valid = false;
    st.sf = ShadowFrame{};
    ++st.frame;
}

using SwapFn = unsigned (*)(void*, void*);

unsigned eglSwapBuffersDetour(void* display, void* surface) {
    if (!gSwapSeen.load(std::memory_order_relaxed)) gSwapSeen.store(true, std::memory_order_relaxed);
    State& st = state();
    hasContext(st);
    endFrame(st);
    auto original = reinterpret_cast<SwapFn>(gSwapOrig.load(std::memory_order_acquire));
    return original ? original(display, surface) : 0;
}

void* gEglInlineTarget = nullptr;
void* gSwapInlineTarget = nullptr;

struct HookName {
    const char* name;
    void* detour;
};
const HookName kHooks[kSlotCount] = {
    {"glDrawElements", reinterpret_cast<void*>(&drawElementsDetour)},
    {"glDrawArrays", reinterpret_cast<void*>(&drawArraysDetour)},
    {"glDrawElementsInstanced", reinterpret_cast<void*>(&drawElementsInstancedDetour)},
    {"glDrawArraysInstanced", reinterpret_cast<void*>(&drawArraysInstancedDetour)},
    {"glClear", reinterpret_cast<void*>(&clearDetour)},
    {"glUniformMatrix4fv", reinterpret_cast<void*>(&uniformMatrix4fvDetour)},
    {"glEnable", reinterpret_cast<void*>(&enableDetour)},
    {"glDisable", reinterpret_cast<void*>(&disableDetour)},
    {"glDepthMask", reinterpret_cast<void*>(&depthMaskDetour)},
    {"glDepthFunc", reinterpret_cast<void*>(&depthFuncDetour)},
    {"glBindFramebuffer", reinterpret_cast<void*>(&bindFramebufferDetour)},
    {"glUseProgram", reinterpret_cast<void*>(&useProgramDetour)},
    {"glShaderSource", reinterpret_cast<void*>(&shaderSourceDetour)},
    {"glColorMask", reinterpret_cast<void*>(&colorMaskDetour)},
};

using EglGetProcAddressFn = void* (*)(const char*);

void* eglGetProcAddressDetour(const char* name) {
    auto original = reinterpret_cast<EglGetProcAddressFn>(gEglOrig.load(std::memory_order_acquire));
    if (!original) return nullptr;
    void* address = original(name);
    if (!address || !name) return address;
    const char* canonical = name;
    if (!std::strcmp(name, "glDrawElementsInstancedOES")) canonical = "glDrawElementsInstanced";
    if (!std::strcmp(name, "glDrawArraysInstancedOES")) canonical = "glDrawArraysInstanced";
    for (int i = 0; i < kSlotCount; ++i) {
        if (std::strcmp(canonical, kHooks[i].name) != 0) continue;
        void* expected = nullptr;
        gOrig[i].compare_exchange_strong(expected, address);
        return kHooks[i].detour;
    }
    if (!std::strcmp(name, "eglSwapBuffers")) {
        void* expected = nullptr;
        gSwapOrig.compare_exchange_strong(expected, address);
        return reinterpret_cast<void*>(&eglSwapBuffersDetour);
    }
    return gBatchAvailable ? batch::intercept(name, address) : address;
}
}

namespace {
bool installSwapInlineHook() {
    void* swap = ::dlsym(RTLD_DEFAULT, "eglSwapBuffers");
    if (!swap) return false;
    void* original = nullptr;
    if (pl::memory::hook(swap, reinterpret_cast<void*>(&eglSwapBuffersDetour), &original) != 0 || !original) return false;
    gSwapOrig.store(original);
    gSwapInlineTarget = swap;
    return true;
}

void watchdogSwapHook(unsigned long long totalDraws) {
    if (!gSwapViaGot.load(std::memory_order_relaxed)) return;
    if (gSwapSeen.load(std::memory_order_relaxed)) return;
    if (totalDraws < 20000) return;
    if (gSwapFallbackTried.exchange(true, std::memory_order_relaxed)) return;

    (void)installSwapInlineHook();
}
}

bool installGlHooks() {
    if (gActive.load()) return true;

    void* gles = ::dlopen("libGLESv2.so", RTLD_NOW | RTLD_LOCAL);
    if (!gles) gles = ::dlopen("libGLESv3.so", RTLD_NOW | RTLD_LOCAL);
    if (!gles) {
        return false;
    }
    bool complete = true;
    auto load = [&](auto& fn, const char* name) {
        fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(::dlsym(gles, name));
        if (!fn) {
            complete = false;
        }
    };
#define LOAD(n) load(real.n, "gl" #n)
    LOAD(BindFramebuffer); LOAD(FramebufferTexture2D); LOAD(DrawElements); LOAD(DrawArrays);
    LOAD(DrawElementsInstanced); LOAD(DrawArraysInstanced);
    LOAD(GenTextures); LOAD(DeleteTextures); LOAD(BindTexture); LOAD(TexParameteri); LOAD(TexStorage2D); LOAD(ActiveTexture);
    LOAD(GenFramebuffers); LOAD(BlitFramebuffer); LOAD(CheckFramebufferStatus); LOAD(GenVertexArrays); LOAD(BindVertexArray);
    LOAD(CreateShader); LOAD(ShaderSource); LOAD(CompileShader); LOAD(GetShaderiv); LOAD(CreateProgram);
    LOAD(AttachShader); LOAD(LinkProgram); LOAD(GetProgramiv); LOAD(DeleteProgram); LOAD(DeleteShader); LOAD(GetUniformLocation);
    LOAD(UseProgram); LOAD(Uniform1i); LOAD(Uniform4f); LOAD(GetIntegerv); LOAD(GetBooleanv); LOAD(IsEnabled); LOAD(Enable);
    LOAD(Disable); LOAD(Clear); LOAD(UniformMatrix4fv); LOAD(UniformMatrix3fv); LOAD(BlendFuncSeparate); LOAD(BlendEquationSeparate); LOAD(DrawBuffers); LOAD(DepthMask); LOAD(ColorMask); LOAD(Viewport); LOAD(GetError); LOAD(BindSampler);
    LOAD(GenSamplers); LOAD(SamplerParameteri); LOAD(GetTexLevelParameteriv); LOAD(GetRenderbufferParameteriv); LOAD(BindRenderbuffer); LOAD(GetFramebufferAttachmentParameteriv);
    LOAD(ClearDepthf); LOAD(Uniform1f); LOAD(ClearColor); LOAD(GetString); LOAD(GetFloatv); LOAD(DepthFunc); LOAD(DepthRangef);
#undef LOAD
    if (!complete) return false;
    gBatchAvailable = batch::loadApi(gles);
    batch::setDepthClearObserver(&beforeDepthClear);

    gGetCurrentContext = reinterpret_cast<void* (*)()>(::dlsym(RTLD_DEFAULT, "eglGetCurrentContext"));

    auto rollback = [] {
        elf::restoreAll();
        if (gEglInlineTarget) {
            (void)pl::memory::unhook(gEglInlineTarget, reinterpret_cast<void*>(&eglGetProcAddressDetour));
            gEglInlineTarget = nullptr;
        }
        if (gSwapInlineTarget) {
            (void)pl::memory::unhook(gSwapInlineTarget, reinterpret_cast<void*>(&eglSwapBuffersDetour));
            gSwapInlineTarget = nullptr;
        }
    };

    int gotSlots = 0;
    bool eglViaGot = false, swapViaGot = false;
    const std::uintptr_t base = elf::findModuleBase("libminecraftpe.so");
    if (base) {
        for (int i = 0; i < kSlotCount; ++i) {
            const int n = elf::patchImport(base, kHooks[i].name, kHooks[i].detour, gOrig[i]);
            gotSlots += n;

        }
        if (gBatchAvailable) {
            std::size_t count = 0;
            const auto* extra = batch::hooks(count);
            for (std::size_t i = 0; i < count; ++i) gotSlots += elf::patchImport(base, extra[i].name, extra[i].detour, *extra[i].original);
        }
        swapViaGot = elf::patchImport(base, "eglSwapBuffers", reinterpret_cast<void*>(&eglSwapBuffersDetour), gSwapOrig) > 0;
        eglViaGot = elf::patchImport(base, "eglGetProcAddress", reinterpret_cast<void*>(&eglGetProcAddressDetour), gEglOrig) > 0;

    } else {
    }

    bool eglInline = false;
    if (!eglViaGot) {
        void* egl = ::dlsym(RTLD_DEFAULT, "eglGetProcAddress");
        if (egl) {
            void* original = nullptr;
            if (pl::memory::hook(egl, reinterpret_cast<void*>(&eglGetProcAddressDetour), &original) == 0 && original) {
                gEglOrig.store(original);
                gEglInlineTarget = egl;
                eglInline = true;
            }

        }
    }
    bool swapInline = false;
    if (!swapViaGot) {
        swapInline = installSwapInlineHook();

    }

    if (!swapViaGot && !swapInline) {
        rollback();
        return false;
    }
    if (gotSlots == 0 && !eglViaGot && !eglInline) {
        rollback();
        return false;
    }
    gSwapViaGot.store(swapViaGot);
    gActive.store(true);

    return true;
}

void removeGlHooks() {
    if (!gActive.exchange(false)) return;
    elf::restoreAll();
    if (gEglInlineTarget) {
        (void)pl::memory::unhook(gEglInlineTarget, reinterpret_cast<void*>(&eglGetProcAddressDetour));
        gEglInlineTarget = nullptr;
    }
    if (gSwapInlineTarget) {
        (void)pl::memory::unhook(gSwapInlineTarget, reinterpret_cast<void*>(&eglSwapBuffersDetour));
        gSwapInlineTarget = nullptr;
    }
}

bool glHooksActive() { return gActive.load(); }
}
