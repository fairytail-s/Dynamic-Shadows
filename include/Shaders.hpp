#pragma once

namespace shadow::shaders {

inline constexpr const char* kVertexShader = R"GLSL(#version 300 es
precision highp float;
uniform vec4 uCam;
out vec2 uv;
#ifdef SHADOW_WORLD
uniform mat4 uInvVP;
out highp vec4 worldBase;
#endif
void main()
{
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    uv = p;
#ifdef SHADOW_WORLD
    worldBase = uInvVP * vec4(p * 2.0 - 1.0, 0.0, 1.0);
#endif
    gl_Position = vec4(p * 2.0 - 1.0, uCam.w > 0.5 ? -0.99998 : 0.99998, 1.0);
}
)GLSL";

inline constexpr const char* kFragmentCommon = R"GLSL(#version 300 es
precision highp float;
precision highp int;
uniform highp sampler2DShadow uShadow;
uniform vec4 uCam;
uniform vec4 uSun;
uniform vec4 uMisc;
uniform vec4 uTint;
uniform vec4 uFilter;
uniform vec4 uAtlas;
in vec2 uv;
out vec4 outColor;

float shadowTaps(vec2 base, float ref, int cascadeIndex)
{
    float start = cascadeIndex == 0 ? 0.0 : uAtlas.y;
    vec2 lo = vec2(start, 0.0) + uMisc.xy * 0.5;
    vec2 hi = vec2(start + uAtlas.x, 1.0) - uMisc.xy * 0.5;
#if SHADOW_SMOOTH == 0
    return 1.0 - texture(uShadow, vec3(clamp(base, lo, hi), ref));
#else
    vec2 o = uMisc.xy * (0.5 * uFilter.y);
    float lit = texture(uShadow, vec3(clamp(base + vec2(-o.x, -o.y), lo, hi), ref)) +
                texture(uShadow, vec3(clamp(base + vec2(o.x, -o.y), lo, hi), ref)) +
                texture(uShadow, vec3(clamp(base + vec2(-o.x, o.y), lo, hi), ref)) +
                texture(uShadow, vec3(clamp(base + vec2(o.x, o.y), lo, hi), ref));
    return 1.0 - lit * 0.25;
#endif
}
void result(float shadow)
{
    if (uTint.w > 0.5) outColor = vec4(vec3(1.0 - shadow), 1.0);
    else outColor = vec4(uTint.rgb, shadow * uSun.w);
}
)GLSL";

inline constexpr const char* kShadowMapBody = R"GLSL(
uniform highp sampler2D uDepth;
uniform mat4 uInvVP;
uniform mat4 uLightVP0;
uniform mat4 uLightVP1;
uniform vec4 uSm0;
uniform vec4 uSm1;
uniform vec4 uTuning;
uniform vec4 uDepthRect;
in highp vec4 worldBase;

float cascade(int k, mat4 lightVP, vec4 sm, vec3 P, vec3 normal, vec2 bias, out float border)
{
    vec3 q = P + normal * min(sm.x * bias.x * uFilter.w, 0.6 * uFilter.w);
    vec3 s = (lightVP * vec4(q, 1.0)).xyz * 0.5 + 0.5;
    border = max(abs(s.x * 2.0 - 1.0), abs(s.y * 2.0 - 1.0));
    if (border >= 1.0 || s.z >= 1.0 || s.z <= 0.0) return 0.0;
    float offset = k == 0 ? 0.0 : uAtlas.y;
    return shadowTaps(vec2(s.x * uAtlas.x + offset, s.y), s.z - sm.x * bias.y * uFilter.z / sm.y, k);
}
void main()
{
    float raw = texture(uDepth, uDepthRect.xy + uv * uDepthRect.zw).r;
    bool sky = uCam.w > 0.5 ? raw < 0.00001 : raw > 0.99999;
    float d = uCam.w > 0.5 ? 1.0 - raw : raw;
    vec4 clip = worldBase + uInvVP[2] * (d * 2.0 - 1.0);
    vec3 P = clip.xyz / clip.w;
    vec3 dx = dFdx(P), dy = dFdy(P);
    if (sky) { result(0.0); return; }
    vec3 n = cross(dx, dy);
    n *= inversesqrt(max(dot(n, n), 1e-20));
    vec3 toCam = uCam.xyz - P;
    if (dot(n, toCam) < 0.0) n = -n;
    vec3 an = abs(n);
    float mx = max(an.x, max(an.y, an.z));
    if (mx > 0.8) n = sign(n) * vec3(an.x >= mx ? 1.0 : 0.0, an.y >= mx ? 1.0 : 0.0, an.z >= mx ? 1.0 : 0.0);
    float dist2 = dot(toCam, toCam);
    bool edge = max(dot(dx, dx), dot(dy, dy)) > 0.0064 * dist2;
    float ndl = edge ? 1.0 : max(dot(n, uSun.xyz), 0.0);
    float lit = edge ? 1.0 : smoothstep(0.0, 0.2, ndl);
    if (lit <= 0.0 || dist2 >= uMisc.w * uMisc.w) { result(1.0 - lit); return; }
    float dist = sqrt(dist2);
    float fade = 1.0 - smoothstep(uTuning.x * uMisc.w, uMisc.w, dist);
    float sinT = sqrt(max(1.0 - ndl * ndl, 0.0));
    float tanT = clamp(sinT / max(ndl, 0.05), 0.0, 4.0);
    vec2 bias = vec2(1.5 + sinT, 1.0 + 2.0 * tanT);
    float border = 2.0, nearShadow = 0.0, farShadow = 0.0, nearWeight = 0.0;
#if SHADOW_CASCADES
    {
        nearShadow = cascade(0, uLightVP0, uSm0, P, n, bias, border);
        nearWeight = 1.0 - smoothstep(0.80, 0.95, border);
    }
#endif
    if (nearWeight < 1.0) {
        farShadow = cascade(1, uLightVP1, uSm1, P, n, bias, border);
        farShadow *= 1.0 - smoothstep(0.85, 1.0, border);
    }
    float shadow = mix(farShadow, nearShadow, nearWeight) * fade;
    result(max(1.0 - lit, shadow * lit));
}
)GLSL";

inline constexpr const char* kHandBody = R"GLSL(
uniform mat4 uLightVP;
uniform vec4 uSm;
void main()
{
    vec3 s = (uLightVP * vec4(uCam.xyz + uSun.xyz * 0.05, 1.0)).xyz * 0.5 + 0.5;
    float shadow = 0.0;
    float border = max(abs(s.x * 2.0 - 1.0), abs(s.y * 2.0 - 1.0));
    int cascadeIndex = int(uMisc.z);
    if (border < 1.0 && s.z > 0.0 && s.z < 1.0) {
        float offset = cascadeIndex == 0 ? 0.0 : uAtlas.y;
        shadow = shadowTaps(vec2(s.x * uAtlas.x + offset, s.y), s.z - uSm.x * 1.2 * uFilter.z / uSm.y, cascadeIndex);
    }
    if (uTint.w > 0.5) outColor = vec4(vec3(1.0 - shadow), 1.0);
    else outColor = vec4(uTint.rgb, shadow * uSun.w * 0.85);
}
)GLSL";

}
