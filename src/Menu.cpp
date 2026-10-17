#include "Menu.hpp"

#include "Config.hpp"

#include <pl/ModMenu.hpp>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <atomic>
#include <cstdlib>
#include <dlfcn.h>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace shadow {
namespace {
constexpr std::string_view kModule = "dynamicshadows";
constexpr std::string_view kOwnerModId = "DynamicShadows";

using SetSchemaFn = bool (*)(std::string_view, std::string_view);
SetSchemaFn gSetSchema{};
std::atomic_bool gRegistered{false};

std::string fstr(float v) {
    char buf[48]{};
    std::snprintf(buf, sizeof(buf), "%.2f", double(v));
    return buf;
}
std::string bstr(bool v) { return v ? "true" : "false"; }

float parseFloat(std::string_view v, float fallback, float lo, float hi) {
    const std::string s(v);
    char* end = nullptr;
    const float x = std::strtof(s.c_str(), &end);
    if (!(end && end != s.c_str()) || !std::isfinite(x)) return fallback;
    return std::clamp(x, lo, hi);
}
bool parseBool(std::string_view v, bool fallback) {
    if (v == "true" || v == "1" || v == "on" || v == "yes") return true;
    if (v == "false" || v == "0" || v == "off" || v == "no") return false;
    return fallback;
}

std::string jsonQuote(std::string_view value) {
    std::string out = "\"";
    for (const char c : value) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + '"';
}

float displayScale(const FloatSetting& setting) {
    return setting.value == &RuntimeConfig::strength || setting.value == &RuntimeConfig::lightFade || setting.value == &RuntimeConfig::fogFade || setting.value == &RuntimeConfig::nearFraction || setting.value == &RuntimeConfig::fadeStart ||
           setting.value == &RuntimeConfig::tintRed || setting.value == &RuntimeConfig::tintGreen || setting.value == &RuntimeConfig::tintBlue ? 100.0f : 1.0f;
}

std::string conditions(std::string_view key) {
    if (key == "elevation") return R"(,"enabled_when":[{"key":"day_cycle","op":"falsy","value":""}])";
    if (key == "sun_tilt") return R"(,"enabled_when":[{"key":"day_cycle","op":"truthy","value":""}])";
    if (key == "moon_shadows") return R"(,"enabled_when":[{"key":"day_cycle","op":"truthy","value":""}])";
    if (key == "near_fraction") return R"(,"enabled_when":[{"key":"shadow_cascades","op":"truthy","value":""}])";
    if (key == "shadow_softness") return R"(,"enabled_when":[{"key":"filter_quality","op":"equals","value":"2"}])";
    if (key == "cache_shadow_layouts" || key == "max_batch_draws") return R"(,"enabled_when":[{"key":"batch_shadow_draws","op":"truthy","value":""}])";
    return {};
}

struct MenuNode { std::string_view section; std::string json; };

MenuNode node(std::string_view key, std::string_view title, std::string_view description, std::string_view category,
                 std::string_view section, std::string_view type, std::string_view def = {}, std::string_view current = {},
                 std::string_view extra = {}) {
    return {section, "{\"id\":" + jsonQuote(key) + ",\"key\":" + jsonQuote(key) + ",\"category\":" + jsonQuote(category) + ",\"title\":" + jsonQuote(title) +
           ",\"description\":" + jsonQuote(description) + ",\"section\":" + jsonQuote(section) + ",\"type\":" + jsonQuote(type) +
           ",\"default_value\":" + jsonQuote(def) + ",\"current_value\":" + jsonQuote(current) + ",\"action_value\":\"true\"" +
           std::string(extra) + conditions(key) + '}'};
}

MenuNode integerNode(const IntSetting& setting, int current) {
    std::string options;
    auto option = [&](int value, const std::string& label) {
        if (!options.empty()) options += ',';
        options += "{\"value\":" + jsonQuote(std::to_string(value)) + ",\"label\":" + jsonQuote(label) + '}';
    };
    if (setting.value == &RuntimeConfig::filterQuality) {
        option(0, "Crisp"); option(1, "Fast soft"); option(2, "Smooth");
    } else if (setting.value == &RuntimeConfig::depthBits) {
        option(16, "16-bit: lower bandwidth"); option(24, "24-bit: higher precision");
    } else if (setting.value == &RuntimeConfig::shadowRes) {
        for (const int value : {256, 512, 768, 1024, 1280, 1536, 1792, 2048, 2304, 2560, 2816, 3072}) option(value, std::to_string(value));
    } else if (setting.value == &RuntimeConfig::updateInterval) {
        option(1, "Every frame");
        for (int value = 2; value <= 8; ++value) option(value, "Up to " + std::to_string(value) + " frames while still");
    }
    const std::string extra = options.empty() ?
        ",\"min_value\":" + jsonQuote(std::to_string(setting.minimum)) + ",\"max_value\":" + jsonQuote(std::to_string(setting.maximum)) +
        ",\"step\":" + jsonQuote(std::to_string(setting.step)) : ",\"style\":\"dropdown\",\"options\":[" + options + ']';
    return node(setting.key, setting.title, setting.description, setting.category, setting.section, options.empty() ? "slider_int" : "choice",
                std::to_string(setting.initial), std::to_string(current), extra);
}

std::string schemaJson() {
    const auto& c = config();
    std::string s = R"({"version":2,"default_category":"presets","categories":[{"id":"presets","title":"Presets","description":"Choose a starting point"},{"id":"appearance","title":"Appearance","description":"Darkness, edges, and color"},{"id":"quality","title":"Quality","description":"Shadow detail and distance"},{"id":"lighting","title":"Sun and moon","description":"Light position and day cycle"},{"id":"performance","title":"Performance","description":"Rendering work and refresh"},{"id":"advanced","title":"Advanced","description":"Surface stability and compatibility"}],"nodes":[)";
    struct Section { const char* id; const char* title; const char* category; };
    constexpr Section sections[]{
        {"choose_preset", "Choose a preset", "presets"}, {"reset_settings", "Reset", "presets"},
        {"shadow_style", "Shadow look", "appearance"}, {"shadow_edges", "Edges and fading", "appearance"}, {"shadow_color", "Shadow color", "appearance"},
        {"map_detail", "Detail and coverage", "quality"}, {"light_motion", "Day and night", "lighting"}, {"light_position", "Light direction", "lighting"},
        {"render_work", "Reduce rendering work", "performance"}, {"map_updates", "Map updates", "performance"},
        {"surface_stability", "Surface stability", "advanced"}, {"compatibility", "Compatibility", "advanced"}
    };
    std::vector<MenuNode> nodes;
    nodes.reserve(kBoolSettings.size() + kFloatSettings.size() + kIntSettings.size() + 5);
    const auto append = [&](MenuNode value) { nodes.push_back(std::move(value)); };
    append(node("preset_performance", "Performance", "1024 detail, 56 blocks, fast soft edges. Dynamic shadows with the lowest preset cost.", "presets", "choose_preset", "button"));
    append(node("preset_balanced", "Balanced: recommended", "1536 detail, 80 blocks, sharper nearby shadows, fast soft edges. The default setup.", "presets", "choose_preset", "button"));
    append(node("preset_quality", "High", "2048 detail, 96 blocks, sharper nearby shadows and smooth edges.", "presets", "choose_preset", "button"));
    append(node("preset_ultra", "Ultra", "3072 detail and 128 blocks with smooth edges. Highest GPU and memory cost.", "presets", "choose_preset", "button"));
    append(node("reset_defaults", "Reset all settings", "Restore the Balanced setup, lighting defaults, and 42% shadow darkness.", "presets", "reset_settings", "button"));
    for (const auto& setting : kBoolSettings)
        append(node(setting.key, setting.title, setting.description, setting.category, setting.section, "toggle", bstr(setting.initial), bstr((c.*setting.value).load())));
    for (const auto& setting : kFloatSettings) {
        const float scale = displayScale(setting);
        const std::string unit = scale == 100.0f ? "%" : setting.value == &RuntimeConfig::shadowDist ? "blocks" :
            setting.value == &RuntimeConfig::azimuth || setting.value == &RuntimeConfig::elevation || setting.value == &RuntimeConfig::tilt ? "degrees" : "";
        const std::string extra = ",\"min_value\":" + jsonQuote(fstr(setting.minimum * scale)) + ",\"max_value\":" + jsonQuote(fstr(setting.maximum * scale)) +
                                  ",\"step\":" + jsonQuote(fstr(setting.step * scale)) + ",\"unit\":" + jsonQuote(unit);
        append(node(setting.key, setting.title, setting.description, setting.category, setting.section, "slider_float", fstr(setting.initial * scale),
                    fstr((c.*setting.value).load() * scale), extra));
    }
    for (const auto& setting : kIntSettings) append(integerNode(setting, (c.*setting.value).load()));
    bool first = true;
    const auto emit = [&](const std::string& json) { if (!first) s += ','; first = false; s += json; };
    for (const auto& section : sections) {
        emit(node(section.id, section.title, "", section.category, "", "section").json);
        for (const auto& entry : nodes) if (entry.section == section.id) emit(entry.json);
    }
    return s + "]}";
}

constexpr auto kSliderSettle = std::chrono::milliseconds(400);
constexpr auto kQuickDelay = std::chrono::milliseconds(30);
constexpr auto kIdleSettle = std::chrono::milliseconds(1200);

std::mutex gPushMutex;
std::string gLastPushed;

void refreshSchema() {
    if (!gSetSchema) return;
    const std::string json = schemaJson();
    std::lock_guard<std::mutex> push(gPushMutex);
    if (json == gLastPushed) return;
    if (gSetSchema(kModule, json)) gLastPushed = json;
}

std::mutex gRefreshMutex;
std::condition_variable gRefreshCv;
std::thread gRefreshThread;
bool gRefreshRunning = false;
bool gRefreshStop = false;
std::uint64_t gRefreshRequested = 0;
std::uint64_t gRefreshDone = 0;
std::chrono::milliseconds gRefreshDelay = kQuickDelay;

void refreshWorker() {
    std::unique_lock<std::mutex> lock(gRefreshMutex);
    for (;;) {
        gRefreshCv.wait(lock, [] { return gRefreshStop || gRefreshRequested != gRefreshDone; });
        if (gRefreshStop) break;
        std::uint64_t seen = gRefreshRequested;
        while (!gRefreshStop && gRefreshCv.wait_for(lock, gRefreshDelay, [&] { return gRefreshStop || gRefreshRequested != seen; })) {
            seen = gRefreshRequested;
        }
        if (gRefreshStop) break;
        gRefreshDelay = kQuickDelay;
        lock.unlock();
        refreshSchema();
        lock.lock();
        gRefreshDone = seen;
    }
}

void startRefreshWorker() {
    std::lock_guard<std::mutex> lock(gRefreshMutex);
    if (gRefreshRunning) return;
    gRefreshStop = false;
    gRefreshRequested = gRefreshDone = 0;
    gRefreshDelay = kQuickDelay;
    gLastPushed.clear();
    gRefreshThread = std::thread(refreshWorker);
    gRefreshRunning = true;
}

void stopRefreshWorker() {
    {
        std::lock_guard<std::mutex> lock(gRefreshMutex);
        if (!gRefreshRunning) return;
        gRefreshStop = true;
        gRefreshCv.notify_one();
    }
    if (gRefreshThread.joinable()) gRefreshThread.join();
    std::lock_guard<std::mutex> lock(gRefreshMutex);
    gRefreshRunning = false;
    gRefreshStop = false;
}

enum class Refresh { Quick, Slider, Idle };

void scheduleSchemaRefresh(Refresh kind) {
    std::lock_guard<std::mutex> lock(gRefreshMutex);
    if (!gRefreshRunning) return;
    const auto delay = kind == Refresh::Slider ? kSliderSettle : kind == Refresh::Idle ? kIdleSettle : kQuickDelay;
    if (delay > gRefreshDelay || gRefreshRequested == gRefreshDone) gRefreshDelay = delay;
    ++gRefreshRequested;
    gRefreshCv.notify_one();
}

constexpr bool kPushSchemaInCallback = true;

void pushChangedValues() {
    {

        std::lock_guard<std::mutex> push(gPushMutex);
        gLastPushed.clear();
    }
    if (kPushSchemaInCallback) refreshSchema();
    else scheduleSchemaRefresh(Refresh::Quick);
}

void handleToggle(bool enabled) {
    config().enabled.store(enabled);
    touchConfig();
    scheduleSchemaRefresh(Refresh::Idle);
}

void setBool(std::atomic_bool& target, std::string_view value, bool def) {
    target.store(parseBool(value, def));
    touchConfig();
    scheduleSchemaRefresh(Refresh::Idle);
}

void handleConfigChanged(std::string_view key, std::string_view value) {
    auto& c = config();
    for (const auto& setting : kBoolSettings) {
        if (key != setting.key) continue;
        setBool(c.*setting.value, value, setting.initial);
        return;
    }
    for (const auto& setting : kFloatSettings) {
        if (key != setting.key) continue;
        const float scale = displayScale(setting);
        (c.*setting.value).store(parseFloat(value, setting.initial * scale, setting.minimum * scale, setting.maximum * scale) / scale);
        touchConfig();
        scheduleSchemaRefresh(Refresh::Idle);
        return;
    }
    for (const auto& setting : kIntSettings) {
        if (key != setting.key) continue;
        const float parsed = parseFloat(value, static_cast<float>(setting.initial), static_cast<float>(setting.minimum), static_cast<float>(setting.maximum));
        const int requested = std::clamp(static_cast<int>(std::lround(parsed)), setting.minimum, setting.maximum);
        int result = requested;
        if (setting.value == &RuntimeConfig::depthBits) result = result < 20 ? 16 : 24;
        (c.*setting.value).store(result);
        touchConfig();
        if (result != requested) pushChangedValues();
        else scheduleSchemaRefresh(Refresh::Idle);
        return;
    }
    if (key == "preset_performance" || key == "preset_balanced" || key == "preset_quality" || key == "preset_ultra") {
        applyQualityPreset(key == "preset_performance" ? 0 : key == "preset_balanced" ? 1 : key == "preset_ultra" ? 3 : 2);
        touchConfig();
        pushChangedValues();
    }
    else if (key == "reset_defaults") {
        resetConfigDefaults();
        touchConfig();
        pushChangedValues();
    }
}
}

void notifyConfigReloaded() {
    if (!gRegistered.load()) return;
    {
        std::lock_guard<std::mutex> push(gPushMutex);
        gLastPushed.clear();
    }
    scheduleSchemaRefresh(Refresh::Quick);
    pl::modmenu::setModuleEnabled(kModule, config().enabled.load());
}

bool registerMenu() {
    gSetSchema = reinterpret_cast<SetSchemaFn>(dlsym(
        RTLD_DEFAULT, "_ZN2pl7modmenu19setConfigSchemaJsonENSt6__ndk117basic_string_viewIcNS1_11char_traitsIcEEEES5_"));

    pl::modmenu::ModuleInfo m{};
    m.moduleId = std::string(kModule);
    m.displayName = "Dynamic Shadows";
    m.description = "Dynamic sun and moon shadows";
    m.modId = std::string(kOwnerModId);
    m.defaultEnabled = config().enabled.load();
    m.hideInHudEditor = true;

    for (const auto& setting : kBoolSettings)
        m.configs.push_back({setting.key, setting.title, pl::modmenu::ConfigType::Toggle, bstr((config().*setting.value).load()), "", "", ""});
    for (const auto& setting : kFloatSettings)
        m.configs.push_back({setting.key, setting.title, pl::modmenu::ConfigType::SliderFloat, fstr((config().*setting.value).load() * displayScale(setting)), fstr(setting.minimum * displayScale(setting)), fstr(setting.maximum * displayScale(setting)), ""});
    for (const auto& setting : kIntSettings)
        m.configs.push_back({setting.key, setting.title, pl::modmenu::ConfigType::SliderInt, std::to_string((config().*setting.value).load()), std::to_string(setting.minimum), std::to_string(setting.maximum), ""});
    for (const auto& action : std::array<std::pair<const char*, const char*>, 5>{{
            {"preset_performance", "Performance preset"}, {"preset_balanced", "Balanced preset"},
            {"preset_quality", "High preset"}, {"preset_ultra", "Ultra preset"},
            {"reset_defaults", "Reset all settings"}}})
        m.configs.push_back({action.first, action.second, pl::modmenu::ConfigType::Button, "", "", "", ""});
    m.onToggle = [](std::string_view, bool enabled) { handleToggle(enabled); };
    m.onConfigChanged = [](std::string_view, std::string_view key, std::string_view value) {
        handleConfigChanged(key, value);
    };
    m.onKeybind = {};

    startRefreshWorker();
    const bool ok = pl::modmenu::registerModule(m);
    if (ok) {
        gRegistered.store(true);
        refreshSchema();
    }
    return ok;
}

void unregisterMenu() {
    gRegistered.store(false);
    stopRefreshWorker();
    pl::modmenu::unregisterModule(kModule);
    gSetSchema = nullptr;
}
}
