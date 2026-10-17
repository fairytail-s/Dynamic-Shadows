#pragma once
#include <string>

namespace shadow {

constexpr const char* kLightUniform = "uShadowLight";

constexpr const char* kSunUniform = "uShadowSunDelta";

constexpr const char* kSunTexture = "s_SunMoonTexture";

enum class LightPatch { None, Chunk, ChunkVertex, Actor, SunMoon };

LightPatch patchLightShader(std::string& source);

}
