#include "ShaderPatch.hpp"

#include <cstddef>
#include <regex>
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

bool patchSunMoonVertex(std::string& src, std::size_t mainName) {
    for (std::string_view t : {"a_position", "a_texcoord0", "a_color0", "v_worldPos", "u_viewProj", "u_model"})
        if (findToken(src, t) == std::string::npos) return false;
    for (std::string_view t : {"a_texcoord1", "a_normal", "a_tangent", "a_bitangent", "a_indices", "a_weight", "a_texcoord2"})
        if (findToken(src, t) != std::string::npos) return false;
    if (findToken(src, kSunUniform) != std::string::npos) return false;

    static const std::regex statement(R"(gl_Position\s*=\s*u_viewProj\s*\*\s*vec4\(\s*(\w+)\.xyz\s*,\s*1\.0\s*\)\s*;)");
    std::smatch m;
    if (!std::regex_search(src, m, statement)) return false;
    const std::size_t begin = static_cast<std::size_t>(m.position(0));
    const std::size_t nameBegin = static_cast<std::size_t>(m.position(1));

    const std::size_t nameEnd = nameBegin + static_cast<std::size_t>(m.length(1)) + 4;
    if (nameBegin < begin || nameEnd > src.size()) return false;
    const std::string expr = src.substr(nameBegin, nameEnd - nameBegin);
    src.replace(nameBegin, nameEnd - nameBegin, "shadowSunMove(" + expr + ")");

    const std::size_t voidPos = src.rfind("void", mainName);
    if (voidPos == std::string::npos) return false;
    src.insert(voidPos, std::string("uniform highp mat3 ") + kSunUniform + ";\nvec3 shadowSunMove(vec3 p) { return p + " + kSunUniform + " * p; }\n");
    return true;
}

}

constexpr const char* kChunkLightVarying = "vShadowLightUV";
constexpr const char* kLightFadeStart = "0.35";
constexpr const char* kLightFadeEnd = "0.9";
constexpr const char* kChunkLightExpr = "fract(a_texcoord1.y*vec2(256.0, 4096.0))";

LightPatch patchLightShader(std::string& src) {
    const std::size_t mainName = findMainName(src);
    if (mainName == std::string::npos) return LightPatch::None;
    const bool es3 = src.find("#version 3") != std::string::npos;

    if (findToken(src, "gl_Position") != std::string::npos) {
        if (patchSunMoonVertex(src, mainName)) return LightPatch::SunMoon;
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

    LightPatch kind = LightPatch::None;
    std::string blockLight;
    std::string decl;

    const bool packChunk = findToken(src, "s_LightMapTexture") == std::string::npos && findToken(src, "s_MatTexture") != std::string::npos &&
                           findToken(src, "v_fog") != std::string::npos && findToken(src, "v_color0") != std::string::npos &&
                           findToken(src, "v_extra") != std::string::npos && findToken(src, "v_refl") != std::string::npos;
    if (findToken(src, "s_LightMapTexture") != std::string::npos || packChunk) {
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

}
