// Rewrites the game's fragment shaders at runtime (glShaderSource hook) so that block / entity pixels carry their block light in the alpha
// channel of the scene: alpha = 1 - block light. The shadow pass reads it to weaken shadows next to torches, glowstone, ...
// No material.bin / resource pack is needed.
#pragma once
#include <string>

namespace shadow {

// vec4 uShadowLight: x = light-source fade strength, y = fog fade strength. Both 0 = leave alpha alone (blended draws, the shadow pass itself).
// With either > 0 the rewritten shaders write the finished shadow multiplier into alpha: (1 - x * smoothstep(start, end, block light)) * (1 - y * fog).
constexpr const char* kLightUniform = "uShadowLight";

enum class LightPatch { None, Chunk, ChunkVertex, Actor };  // ChunkVertex: the block vertex shader that feeds the Chunk fragment shader

// Looks at one shader. If it is the render-chunk (blocks, vertex + fragment) or the actor (entities, fragment) shader and the output can be found, renames the
// game's main() and appends a main() that calls it and then sets alpha. Returns what it patched (None: `source` is unchanged).
LightPatch patchLightShader(std::string& source);

}  // namespace shadow
