#include "ShaderPatch.hpp"

#include <cstddef>
#include <string_view>

namespace shadow {
namespace {

bool isIdentChar(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

std::size_t findToken(const std::string& s, std::string_view token, std::size_t from = 0) {
    std::size_t p = from;
    while ((p = s.find(token, p)) != std::string::npos) {
        const bool left = p == 0 || !isIdentChar(s[p - 1]);
        const std::size_t end = p + token.size();
        const bool right = end >= s.size() || !isIdentChar(s[end]);
        if (left && right) return p;
        p = end;
    }
    return std::string::npos;
}

// Position of the `main` token in "void main(" (npos if there is no such function).
std::size_t findMainName(const std::string& s) {
    std::size_t p = 0;
    while ((p = s.find("void", p)) != std::string::npos) {
        std::size_t q = p + 4;
        while (q < s.size() && (s[q] == ' ' || s[q] == '\t' || s[q] == '\r' || s[q] == '\n')) ++q;
        if (s.compare(q, 4, "main") == 0 && (q + 4 >= s.size() || !isIdentChar(s[q + 4]))) {
            std::size_t r = q + 4;
            while (r < s.size() && (s[r] == ' ' || s[r] == '\t' || s[r] == '\r' || s[r] == '\n')) ++r;
            if (r < s.size() && s[r] == '(') return q;
        }
        p += 4;
    }
    return std::string::npos;
}

// The colour output expression: gl_FragColor (GLSL ES 1.00), or the first `out vec4 name;` / `out vec4 name[N];` of GLSL ES 3.00
// (an array output is written as name[0]). Function parameters (`out` after a '(') are not outputs.
std::string findOutput(const std::string& s) {
    if (findToken(s, "gl_FragColor") != std::string::npos) return "gl_FragColor";
    if (findToken(s, "bgfx_FragData") != std::string::npos && s.find("bgfx_FragData[") != std::string::npos) return "bgfx_FragData[0]";
    std::size_t p = 0;
    while ((p = findToken(s, "out", p)) != std::string::npos) {
        const std::size_t lineStart = s.rfind('\n', p) == std::string::npos ? 0 : s.rfind('\n', p) + 1;
        const std::size_t end = s.find(';', p);
        if (end == std::string::npos) break;
        const std::string before = s.substr(lineStart, p - lineStart);
        const std::string decl = s.substr(p + 3, end - p - 3);
        const std::size_t v = decl.find("vec4");
        if ((before.find('(') == std::string::npos || (before.find("layout") != std::string::npos && before.find(')') == before.rfind(')') && before.find(')') != std::string::npos)) && decl.find('(') == std::string::npos && decl.find('{') == std::string::npos && v != std::string::npos) {
            std::size_t n = v + 4;
            while (n < decl.size() && (decl[n] == ' ' || decl[n] == '\t')) ++n;
            std::size_t e = n;
            while (e < decl.size() && isIdentChar(decl[e])) ++e;
            if (e > n) return decl.substr(n, e - n) + (decl.find('[', e) != std::string::npos ? "[0]" : "");
        }
        p += 3;
    }
    return {};
}

}  // namespace

// Block light of a render-chunk vertex, computed exactly like the game's vertex shader does (x = block light, y = sky light). It is
// recomputed from the raw vertex attribute so it keeps working when a modified material renames or drops v_lightmapUV.
constexpr const char* kChunkLightVarying = "vShadowLightUV";
constexpr const char* kLightFadeStart = "0.35";  // block light where the fade starts / is complete
constexpr const char* kLightFadeEnd = "0.9";
constexpr const char* kChunkLightExpr = "fract(a_texcoord1.y*vec2(256.0, 4096.0))";

LightPatch patchLightShader(std::string& src) {
    const std::size_t mainName = findMainName(src);
    if (mainName == std::string::npos) return LightPatch::None;
    const bool es3 = src.find("#version 3") != std::string::npos;

    // Vertex stage of the blocks: pass the block light on in our own varying.
    if (findToken(src, "gl_Position") != std::string::npos) {
        if (findToken(src, "a_texcoord1") == std::string::npos || findToken(src, kChunkLightVarying) != std::string::npos) return LightPatch::None;
        src.replace(mainName, 4, "shadowOrigVertexMain");
        src += es3 ? "\nout vec2 " : "\nvarying vec2 ";
        src += kChunkLightVarying;
        src += ";\nvoid main()\n{\n    shadowOrigVertexMain();\n    ";
        src += kChunkLightVarying;
        src += " = ";
        src += kChunkLightExpr;
        src += ";\n}\n";
        return LightPatch::ChunkVertex;
    }

    if (src.find(kLightUniform) != std::string::npos) return LightPatch::None;

    // Blocks: the lightmap sampler marks the render-chunk fragment shader; the light comes from our varying (see above).
    // Entities: TileLightIntensity.r / .g hold the block / sky light the same way the lightmap UV does (per draw, not per vertex).
    LightPatch kind = LightPatch::None;
    std::string blockLight;
    std::string decl;
    if (findToken(src, "s_LightMapTexture") != std::string::npos) {
        kind = LightPatch::Chunk;
        blockLight = std::string("clamp(") + kChunkLightVarying + ".x, 0.0, 1.0)";
        decl = std::string(es3 ? "\nin mediump vec2 " : "\nvarying mediump vec2 ") + kChunkLightVarying + ";";
    } else if (findToken(src, "v_light") != std::string::npos && findToken(src, "v_fog") != std::string::npos &&
               findToken(src, "OverlayColor") != std::string::npos && findToken(src, "MatColor") != std::string::npos) {
        kind = LightPatch::Actor;
        blockLight = "clamp(TileLightIntensity.r, 0.0, 1.0)";
        if (findToken(src, "TileLightIntensity") == std::string::npos) decl = "\nuniform vec4 TileLightIntensity;";
    } else {
        return LightPatch::None;
    }
    const std::string out = findOutput(src);
    if (out.empty()) return LightPatch::None;
    // Fog amount: v_fog.a is the weight the game mixes its fog colour in with (blocks and entities both have it).
    const std::string fog = findToken(src, "v_fog") != std::string::npos ? "clamp(v_fog.a, 0.0, 1.0)" : "0.0";
    const std::string alpha = std::string("(1.0 - ") + kLightUniform + ".x * smoothstep(" + kLightFadeStart + ", " + kLightFadeEnd + ", " + blockLight + ")) * (1.0 - " +
                              kLightUniform + ".y * " + fog + ")";

    src.replace(mainName, 4, "shadowOrigMain");
    src += decl;
    src += "\nuniform vec4 ";
    src += kLightUniform;
    src += ";\nvoid main()\n{\n    shadowOrigMain();\n    if (";
    src += kLightUniform;
    src += ".x + ";
    src += kLightUniform;
    src += ".y > 0.0) ";
    src += out + ".a = " + alpha + ";\n}\n";
    return kind;
}

}  // namespace shadow
