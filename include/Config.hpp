#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <string>

namespace shadow {

constexpr float kDefaultStrength = 0.42f, kMinStrength = 0.0f, kMaxStrength = 1.0f;
constexpr float kDefaultAzimuth = 90.0f, kMinAzimuth = 0.0f, kMaxAzimuth = 360.0f;
constexpr float kDefaultElevation = 55.0f, kMinElevation = 10.0f, kMaxElevation = 85.0f;
constexpr float kDefaultTilt = 0.0f, kMinTilt = -60.0f, kMaxTilt = 60.0f;
constexpr bool kDefaultDayCycle = true, kDefaultMoon = true, kDefaultHand = true, kDefaultCascades = true, kDefaultCull = true, kDefaultBeforeBlend = true;
constexpr int kDefaultShadowRes = 1536, kMinShadowRes = 256, kMaxShadowRes = 3072;
constexpr float kDefaultSkyFade = 0.85f;
constexpr float kDefaultShadowDist = 80.0f, kMinShadowDist = 16.0f, kMaxShadowDist = 192.0f;

struct RuntimeConfig {
    std::atomic_bool enabled{true}, reversedDepth{false}, debugView{false};
    std::atomic_bool dayCycle{kDefaultDayCycle}, moon{kDefaultMoon}, handShadow{kDefaultHand}, cascades{kDefaultCascades};
    std::atomic_bool beforeBlend{kDefaultBeforeBlend}, cullDraws{kDefaultCull}, batchDraws{true}, directDepth{true};
    std::atomic_bool translucentCasters{false}, translucentTint{false};
    std::atomic_bool twoSided{false}, layoutSnapshots{true}, syncSun{true};
    std::atomic<float> strength{kDefaultStrength}, azimuth{kDefaultAzimuth}, elevation{kDefaultElevation}, tilt{kDefaultTilt};
    std::atomic<int> shadowRes{kDefaultShadowRes}, depthBits{24}, filterQuality{1}, updateInterval{1}, maxBatchDraws{4096};
    std::atomic<float> shadowDist{kDefaultShadowDist}, nearFraction{0.30f}, softness{0.85f}, depthBias{0.85f}, normalBias{0.65f};
    std::atomic<float> skyFade{kDefaultSkyFade};
    std::atomic<float> lightFade{0.8f}, fogFade{1.0f}, fadeStart{0.85f}, tintRed{0.025f}, tintGreen{0.035f}, tintBlue{0.055f};
};

struct BoolSetting {
    const char *key, *title, *description, *category, *section;
    std::atomic_bool RuntimeConfig::*value;
    bool initial;
};
struct FloatSetting {
    const char *key, *title, *description, *category, *section;
    std::atomic<float> RuntimeConfig::*value;
    float initial, minimum, maximum, step;
};
struct IntSetting {
    const char *key, *title, *description, *category, *section;
    std::atomic<int> RuntimeConfig::*value;
    int initial, minimum, maximum, step;
};

inline constexpr std::array kBoolSettings{
    BoolSetting{"day_cycle", "Follow the day and night", "Move the light with the game time. Turn off to choose a fixed sun position.", "lighting", "light_motion", &RuntimeConfig::dayCycle, true},
    BoolSetting{"sync_visible_sun", "Move the visible sun and moon", "Make the sun and moon you see in the sky follow the direction set here, so they match the shadows. Applies to shaders compiled after the mod loads; restart the game once after changing it.", "lighting", "light_motion", &RuntimeConfig::syncSun, true},
    BoolSetting{"moon_shadows", "Moon shadows", "Allow softer, weaker shadows at night.", "lighting", "light_motion", &RuntimeConfig::moon, true},
    BoolSetting{"hand_shadow", "Shade hand and held item", "Darken your hand when you stand in shadow. Requires a compatible world and hand target.", "appearance", "shadow_style", &RuntimeConfig::handShadow, true},
    BoolSetting{"shadow_cascades", "Sharper nearby shadows", "Use a second map for close objects. Improves nearby detail and costs more to draw.", "quality", "map_detail", &RuntimeConfig::cascades, true},
    BoolSetting{"cull_shadow_draws", "Skip distant shadow geometry", "Avoid drawing objects outside the shadow maps. Keep this on for better FPS.", "performance", "render_work", &RuntimeConfig::cullDraws, true},
    BoolSetting{"batch_shadow_draws", "Group shadow draws", "Draw compatible objects together. Keep on unless shadows appear incorrect.", "performance", "render_work", &RuntimeConfig::batchDraws, true},
    BoolSetting{"cache_shadow_layouts", "Reuse geometry layouts", "Keep shadow draws grouped when the game changes geometry. Turn off only for compatibility.", "performance", "render_work", &RuntimeConfig::layoutSnapshots, true},
    BoolSetting{"direct_depth_sampling", "Avoid extra depth copies", "Use the existing scene depth when supported. Other targets use a safe copy automatically.", "performance", "render_work", &RuntimeConfig::directDepth, true},
    BoolSetting{"shadow_before_blend", "Shadows behind water and glass", "Apply shadows before transparent objects when the game draw order allows it. Turned off automatically while translucent casters are enabled, since that needs the shadow map finished after transparent objects draw.", "advanced", "compatibility", &RuntimeConfig::beforeBlend, true},
    BoolSetting{"translucent_casters", "Water and glass cast shadows", "Let transparent objects (water, glass, leaves drawn with blending) block light in the shadow map like solid blocks, instead of only opaque geometry. Delays the shadow pass until after transparent objects draw, so it also costs a bit more and disables the option above automatically.", "appearance", "shadow_style", &RuntimeConfig::translucentCasters, false},
    BoolSetting{"translucent_tint", "Tint shadows through glass and water", "Color the shadow by the actual color of transparent objects it passes through, like light through stained glass, instead of only fully blocking or fully passing. Also delays the shadow pass until after transparent objects draw, and costs more: it renders transparent objects a second time into their own small map. Works best on its own, without 'Water and glass cast shadows' turned on too, since that makes those objects fully block light instead and the tint becomes hard to notice.", "appearance", "shadow_style", &RuntimeConfig::translucentTint, false},
    BoolSetting{"two_sided_casters", "Thin objects cast on both sides", "Can help thin surfaces. Draws extra faces and may reduce FPS.", "advanced", "compatibility", &RuntimeConfig::twoSided, false},
    BoolSetting{"reversed_depth", "Reverse scene depth", "Compatibility setting. Change only if shadows do not line up with the scene.", "advanced", "compatibility", &RuntimeConfig::reversedDepth, false},
    BoolSetting{"debug_view", "Preview the shadow mask", "Show only lit and shadowed areas while adjusting the look.", "advanced", "surface_stability", &RuntimeConfig::debugView, false}
};

inline constexpr std::array kFloatSettings{
    FloatSetting{"strength", "Shadow darkness", "Higher values make shadows darker. This does not change rendering cost.", "appearance", "shadow_style", &RuntimeConfig::strength, 0.42f, 0.0f, 1.0f, 0.02f},
    FloatSetting{"shadow_distance", "Shadow distance", "How far shadows reach in blocks. Shorter coverage improves detail and can improve FPS.", "quality", "map_detail", &RuntimeConfig::shadowDist, 80.0f, 16.0f, 192.0f, 8.0f},
    FloatSetting{"near_fraction", "Nearby detail coverage", "Portion of the full distance covered by the sharper nearby map.", "quality", "map_detail", &RuntimeConfig::nearFraction, 0.30f, 0.15f, 0.60f, 0.05f},
    FloatSetting{"shadow_softness", "Soft edge width", "Width of soft edges with Smooth filtering. Crisp and Fast soft use their fixed kernels.", "appearance", "shadow_edges", &RuntimeConfig::softness, 0.85f, 0.25f, 3.0f, 0.05f},
    FloatSetting{"depth_bias", "Reduce surface speckles", "Raise if surfaces show shadow speckles. Lower if shadows start too far from objects.", "advanced", "surface_stability", &RuntimeConfig::depthBias, 0.85f, 0.10f, 3.0f, 0.05f},
    FloatSetting{"normal_bias", "Shadow contact offset", "Lower values keep shadows closer to the object. Raise if edges show unwanted streaks.", "advanced", "surface_stability", &RuntimeConfig::normalBias, 0.65f, 0.0f, 3.0f, 0.05f},
    FloatSetting{"fade_start", "Start distance fading", "Keep shadows at full strength until this portion of their maximum distance.", "appearance", "shadow_edges", &RuntimeConfig::fadeStart, 0.85f, 0.30f, 0.95f, 0.05f},
    FloatSetting{"light_fade", "Fade near light sources", "Shadows get weaker close to torches, glowstone and other light sources. 0 turns it off.", "appearance", "shadow_style", &RuntimeConfig::lightFade, 0.8f, 0.0f, 1.0f, 0.05f},
    FloatSetting{"cave_fade", "Fade shadows underground", "Shadows get weaker where the terrain has little sky light, such as caves and deep overhangs, so hidden terrain above cannot leave uneven patches. 0 turns it off.", "appearance", "shadow_style", &RuntimeConfig::skyFade, kDefaultSkyFade, 0.0f, 1.0f, 0.05f},
    FloatSetting{"fog_fade", "Fade in fog", "Shadows fade out where the game's fog covers the world, so distant fogged terrain stays flat. 0 turns it off.", "appearance", "shadow_style", &RuntimeConfig::fogFade, 1.0f, 0.0f, 1.0f, 0.05f},
    FloatSetting{"azimuth", "Sun compass direction", "Fixed sun direction, or sunrise direction when following the day cycle.", "lighting", "light_position", &RuntimeConfig::azimuth, 90.0f, 0.0f, 360.0f, 5.0f},
    FloatSetting{"elevation", "Fixed sun height", "Light angle above the horizon when the day cycle is turned off.", "lighting", "light_position", &RuntimeConfig::elevation, 55.0f, 10.0f, 85.0f, 1.0f},
    FloatSetting{"sun_tilt", "Sun path tilt", "Tilt the daily light path toward the north or south.", "lighting", "light_position", &RuntimeConfig::tilt, 0.0f, -60.0f, 60.0f, 1.0f},
    FloatSetting{"tint_red", "Red tint", "Amount of red in the shadow color. Low values keep shadows neutral and dark.", "appearance", "shadow_color", &RuntimeConfig::tintRed, 0.025f, 0.0f, 1.0f, 0.005f},
    FloatSetting{"tint_green", "Green tint", "Amount of green in the shadow color.", "appearance", "shadow_color", &RuntimeConfig::tintGreen, 0.035f, 0.0f, 1.0f, 0.005f},
    FloatSetting{"tint_blue", "Blue tint", "Amount of blue in the shadow color. Increase for a cooler look.", "appearance", "shadow_color", &RuntimeConfig::tintBlue, 0.055f, 0.0f, 1.0f, 0.005f}
};

inline constexpr std::array kIntSettings{
    IntSetting{"shadow_res", "Shadow detail", "Map resolution. Higher values sharpen edges and use more GPU memory.", "quality", "map_detail", &RuntimeConfig::shadowRes, 1536, 256, 3072, 256},
    IntSetting{"filter_quality", "Edge filtering", "Crisp is fastest. Fast soft smooths edges cheaply. Smooth uses a wider four-sample filter.", "appearance", "shadow_edges", &RuntimeConfig::filterQuality, 1, 0, 2, 1},
    IntSetting{"shadow_depth_bits", "Depth precision", "16-bit saves bandwidth. 24-bit improves depth precision and surface stability.", "performance", "map_updates", &RuntimeConfig::depthBits, 24, 16, 24, 8},
    IntSetting{"shadow_update_interval", "Shadow refresh", "Every frame follows motion immediately. Longer intervals can save work while still and delay moving shadows.", "performance", "map_updates", &RuntimeConfig::updateInterval, 1, 1, 8, 1},
    IntSetting{"max_batch_draws", "Shadow draw group limit", "Maximum objects in one group. Larger groups can reduce target changes. Resource writes still end a group.", "advanced", "compatibility", &RuntimeConfig::maxBatchDraws, 4096, 32, 8192, 32}
};

RuntimeConfig& config();
void resetConfigDefaults();
void applyQualityPreset(int preset);
void touchConfig();

void setConfigReloadedCallback(void (*callback)());
bool loadConfigFromDisk();
void startConfigAutosave();
void stopConfigAutosave();
const std::string& configFilePath();

}
