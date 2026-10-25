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
#if defined(SHADOW_WORLD) && defined(FB_FETCH)
layout(location = 0) inout highp vec4 outColor;
uniform vec4 uLight;
#else
out vec4 outColor;
#endif
#ifdef SHADOW_TINT
// Set by the shadow-map body before calling result(), from the standalone translucent tint
// target. gTintAlpha 0 means "no translucent surface was between the light and this point".
vec3 gTintColor = vec3(1.0);
float gTintAlpha = 0.0;
#endif

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
#if defined(SHADOW_WORLD) && defined(FB_FETCH)

    if (uLight.x > 0.0) shadow *= outColor.a;
#endif
    if (uTint.w > 0.5) { outColor = vec4(vec3(1.0 - shadow), 1.0); return; }
#ifdef SHADOW_TINT
    // Where a translucent surface sits between the light and this point, let it color the overlay
    // instead of (or alongside) the usual flat shadow tint, and let it darken the point a little
    // even where the opaque shadow test alone found nothing blocking — glass still dims and colors
    // light that passes through it, independent of whether anything opaque is also in the way.
    float a = max(shadow * uSun.w, gTintAlpha * uSun.w * 0.85);
    if (a < 0.004) discard;
    vec3 col = mix(uTint.rgb, uTint.rgb * gTintColor, gTintAlpha);
    outColor = vec4(col, a);
#else
    if (shadow * uSun.w < 0.004) discard;
    outColor = vec4(uTint.rgb, shadow * uSun.w);
#endif
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
#ifdef SHADOW_TINT
// Plain (non shadow-compare) depth + color from the standalone translucent tint target. Always
// built with the far cascade's transform (uLightVP1 / uSm1), regardless of whether the near
// cascade is in use for the hard occluder shadow — see shadowReplay's nearCascade restriction.
uniform highp sampler2D uTintDepth;
uniform highp sampler2D uTintColor;
#endif

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

    bool sky = uCam.w > 0.5 ? raw < 0.0001 : raw > 0.9999;
    float d = uCam.w > 0.5 ? 1.0 - raw : raw;
    vec4 clip = worldBase + uInvVP[2] * (d * 2.0 - 1.0);
    vec3 P = clip.xyz / clip.w;
    vec3 dx = dFdx(P), dy = dFdy(P);
    if (sky) {
        if (uTint.w > 0.5) { outColor = vec4(0.0, 0.35, 1.0, 1.0); return; }
        result(0.0);
        return;
    }
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
    if (lit <= 0.0 || dist2 >= uMisc.w * uMisc.w) {

        float farFade = 1.0 - smoothstep(uMisc.w * 2.0, uMisc.w * 3.0, sqrt(dist2));
        result((1.0 - lit) * farFade);
        return;
    }
    float dist = sqrt(dist2);
    float fade = 1.0 - smoothstep(uTuning.x * uMisc.w, uMisc.w, dist);
    float sinT = sqrt(max(1.0 - ndl * ndl, 0.0));
    float tanT = clamp(sinT / max(ndl, 0.05), 0.0, 4.0);
    vec2 bias = vec2(1.5 + sinT, 1.0 + 2.0 * tanT);
#ifdef SHADOW_TINT
    {
        // Same light-space projection as cascade(1, ...) below, duplicated here (rather than
        // threading an out-parameter through cascade()) since the hard shadow test and the tint
        // test use different source textures and don't otherwise share this value.
        vec3 q = P + n * min(uSm1.x * bias.x * uFilter.w, 0.6 * uFilter.w);
        vec3 s = (uLightVP1 * vec4(q, 1.0)).xyz * 0.5 + 0.5;
        float tb = max(abs(s.x * 2.0 - 1.0), abs(s.y * 2.0 - 1.0));
        if (tb < 1.0 && s.z > 0.0 && s.z < 1.0) {
            float td = texture(uTintDepth, s.xy).r;
            float ref = s.z - uSm1.x * bias.y * uFilter.z / uSm1.y;
            if (td < ref) {
                vec4 tc = texture(uTintColor, s.xy);
                gTintAlpha = tc.a * (1.0 - smoothstep(0.85, 1.0, tb));
                gTintColor = tc.rgb;
            }
        }
    }
#endif
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
