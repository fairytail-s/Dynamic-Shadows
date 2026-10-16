#include "Config.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <mutex>
#include <sstream>
#include <locale>
#include <iomanip>
#include <string_view>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace shadow {
namespace {
constexpr std::size_t kMaxConfigBytes = 8 * 1024;
constexpr auto kDebounce = std::chrono::milliseconds(250);
constexpr const char* kModDirName = "DynamicShadows";

std::mutex gMutex;
std::condition_variable gCv;
std::thread gThread;
bool gRunning = false;
bool gStop = false;
std::uint64_t gRequested = 0;
std::uint64_t gWritten = 0;

bool makeDir(const std::string& path) {
    if (path.empty()) return false;
    if (::mkdir(path.c_str(), 0700) == 0) return true;
    return errno == EEXIST;
}

std::string selfLibraryPath() {
    Dl_info info{};
    static int anchor = 0;
    if (::dladdr(&anchor, &info) != 0 && info.dli_fname && *info.dli_fname) return info.dli_fname;
    return {};
}

std::string siblingConfigPath() {
    const std::string self = selfLibraryPath();
    const auto slash = self.find_last_of('/');
    return slash == std::string::npos ? std::string{} : self.substr(0, slash + 1) + "config.json";
}

std::string discoverConfigPath() {
    if (const char* overridePath = std::getenv("DYNAMICSHADOWS_CONFIG")) {
        if (*overridePath) return overridePath;
    }
    if (const char* overridePath = std::getenv("SHADOW_CONFIG")) {
        if (*overridePath) return overridePath;
    }
    const std::string self = selfLibraryPath();
    if (self.empty()) return {};
    const auto cachePos = self.find("/cache/");
    if (cachePos != std::string::npos) {
        const std::string files = self.substr(0, cachePos) + "/files";
        const std::string dir = files + "/native_mod_configs";
        const std::string modDir = dir + "/" + kModDirName;
        (void)makeDir(files);
        if (makeDir(dir) && makeDir(modDir)) return modDir + "/config.json";
    }
    return siblingConfigPath();
}

std::string& pathStorage() {
    static std::string path = discoverConfigPath();
    return path;
}

bool readTextFile(const std::string& path, std::string& text) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    text.assign(kMaxConfigBytes, '\0');
    std::size_t total = 0;
    while (total < text.size()) {
        const ssize_t n = ::read(fd, text.data() + total, text.size() - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            ::close(fd);
            return false;
        }
        if (n == 0) break;
        total += static_cast<std::size_t>(n);
    }
    ::close(fd);
    if (total == 0) return false;
    text.resize(total);
    return true;
}

bool valueStart(const std::string& text, const char* key, std::size_t& pos) {
    const std::string needle = std::string("\"") + key + "\"";
    pos = text.find(needle);
    if (pos == std::string::npos) return false;
    pos = text.find(':', pos + needle.size());
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\r' || text[pos] == '\n')) ++pos;
    return pos < text.size();
}

bool readBool(const std::string& text, const char* key, bool& out) {
    std::size_t p{};
    if (!valueStart(text, key, p)) return false;
    if (text.compare(p, 4, "true") == 0) { out = true; return true; }
    if (text.compare(p, 5, "false") == 0) { out = false; return true; }
    return false;
}

bool readFloat(const std::string& text, const char* key, float& out) {
    std::size_t p{};
    if (!valueStart(text, key, p)) return false;
    const char* begin = text.c_str() + p;
    char* end = nullptr;
    errno = 0;
    const float v = std::strtof(begin, &end);
    if (end == begin || errno == ERANGE || !std::isfinite(v)) return false;
    out = v;
    return true;
}

bool writeAll(int fd, const char* data, std::size_t size) {
    while (size) {
        const ssize_t n = ::write(fd, data, size);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;
        data += n;
        size -= static_cast<std::size_t>(n);
    }
    return true;
}

bool applyConfigText(const std::string& text) {
    auto& c = config();
    bool enabled = c.enabled.load();
    if (readBool(text, "enabled", enabled)) c.enabled.store(enabled);
    for (const auto& setting : kBoolSettings) {
        bool value = (c.*setting.value).load();
        if (readBool(text, setting.key, value)) (c.*setting.value).store(value);
    }
    for (const auto& setting : kFloatSettings) {
        float value = (c.*setting.value).load();
        if (readFloat(text, setting.key, value)) (c.*setting.value).store(std::clamp(value, setting.minimum, setting.maximum));
    }
    for (const auto& setting : kIntSettings) {
        float value = static_cast<float>((c.*setting.value).load());
        if (!readFloat(text, setting.key, value)) continue;
        value = std::clamp(value, static_cast<float>(setting.minimum), static_cast<float>(setting.maximum));
        int rounded = static_cast<int>(std::lround(value));
        if (setting.value == &RuntimeConfig::depthBits) rounded = rounded < 20 ? 16 : 24;
        (c.*setting.value).store(rounded);
    }
    float version = 0.0f;
    (void)readFloat(text, "version", version);
    if (version >= 5.0f) return false;
    const auto replaceDefault = [](std::atomic<float>& value, float oldDefault, float newDefault) {
        if (std::fabs(value.load() - oldDefault) < 0.00001f) value.store(newDefault);
    };
    if (version < 4.0f) {
        replaceDefault(c.strength, 0.55f, kDefaultStrength);
        replaceDefault(c.softness, 1.0f, 0.85f);
        replaceDefault(c.depthBias, 1.0f, 0.85f);
        replaceDefault(c.normalBias, 1.0f, 0.65f);
        replaceDefault(c.fadeStart, 0.75f, 0.85f);
        if (std::fabs(c.tintRed.load() - 0.04f) < 0.00001f && std::fabs(c.tintGreen.load() - 0.06f) < 0.00001f && std::fabs(c.tintBlue.load() - 0.14f) < 0.00001f) {
            c.tintRed.store(0.025f);
            c.tintGreen.store(0.035f);
            c.tintBlue.store(0.055f);
        }
    }
    replaceDefault(c.strength, 0.78f, kDefaultStrength);
    return true;
}

bool saveConfigToDisk() {
    const std::string& path = pathStorage();
    if (path.empty()) return false;
    const auto& c = config();
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(6) << "{\n  \"version\": 5,\n  \"enabled\": " << (c.enabled.load() ? "true" : "false");
    for (const auto& setting : kBoolSettings)
        stream << ",\n  \"" << setting.key << "\": " << ((c.*setting.value).load() ? "true" : "false");
    for (const auto& setting : kFloatSettings)
        stream << ",\n  \"" << setting.key << "\": " << (c.*setting.value).load();
    for (const auto& setting : kIntSettings)
        stream << ",\n  \"" << setting.key << "\": " << (c.*setting.value).load();
    stream << "\n}\n";
    const std::string json = stream.str();
    const std::string tmp = path + ".tmp";
    const int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) return false;
    bool ok = writeAll(fd, json.data(), json.size());
    if (::close(fd) != 0) ok = false;
    if (ok && ::rename(tmp.c_str(), path.c_str()) != 0) ok = false;
    if (!ok) (void)::unlink(tmp.c_str());
    return ok;
}

void autosaveWorker() {
    std::unique_lock<std::mutex> lock(gMutex);
    for (;;) {
        gCv.wait(lock, [] { return gStop || gRequested != gWritten; });
        if (gStop) break;
        std::uint64_t seen = gRequested;
        while (!gStop && gCv.wait_for(lock, kDebounce, [&] { return gStop || gRequested != seen; })) seen = gRequested;
        if (gStop) break;
        lock.unlock();
        (void)saveConfigToDisk();
        lock.lock();
        gWritten = seen;
    }
    const bool dirty = gRequested != gWritten;
    lock.unlock();
    if (dirty) (void)saveConfigToDisk();
}
}

RuntimeConfig& config() {
    static RuntimeConfig c;
    return c;
}

void resetConfigDefaults() {
    auto& c = config();
    c.enabled.store(true);
    for (const auto& setting : kBoolSettings) (c.*setting.value).store(setting.initial);
    for (const auto& setting : kFloatSettings) (c.*setting.value).store(setting.initial);
    for (const auto& setting : kIntSettings) (c.*setting.value).store(setting.initial);
}

void applyQualityPreset(int preset) {
    auto& c = config();
    c.batchDraws.store(true);
    c.layoutSnapshots.store(true);
    c.directDepth.store(true);
    c.cullDraws.store(true);
    c.twoSided.store(false);
    c.nearFraction.store(0.30f);
    c.strength.store(kDefaultStrength);
    c.tintRed.store(0.025f);
    c.tintGreen.store(0.035f);
    c.tintBlue.store(0.055f);
    c.softness.store(0.85f);
    c.depthBias.store(0.85f);
    c.normalBias.store(0.65f);
    c.fadeStart.store(0.85f);
    c.maxBatchDraws.store(4096);
    c.shadowRes.store(preset == 0 ? 1024 : preset == 1 ? 1536 : preset == 3 ? 3072 : 2048);
    c.shadowDist.store(preset == 0 ? 56.0f : preset == 1 ? 80.0f : preset == 3 ? 128.0f : 96.0f);
    c.cascades.store(preset != 0);
    c.depthBits.store(preset == 0 ? 16 : 24);
    c.filterQuality.store(preset < 2 ? 1 : 2);
    c.updateInterval.store(preset == 0 ? 2 : 1);
}

const std::string& configFilePath() { return pathStorage(); }

bool loadConfigFromDisk() {
    std::string text;
    const std::string& persistent = pathStorage();
    if (!persistent.empty() && readTextFile(persistent, text)) {
        if (applyConfigText(text)) (void)saveConfigToDisk();
        return true;
    }

    constexpr std::string_view suffix = "/native_mod_configs/DynamicShadows/config.json";
    if (persistent.ends_with(suffix)) {
        const std::string legacy = persistent.substr(0, persistent.size() - suffix.size()) + "/native_mod_configs/Shadow/config.json";
        if (readTextFile(legacy, text)) {
            (void)applyConfigText(text);
            (void)saveConfigToDisk();
            return true;
        }
    }

    const std::string packaged = siblingConfigPath();
    if (!packaged.empty() && packaged != persistent && readTextFile(packaged, text)) {
        (void)applyConfigText(text);
        if (!persistent.empty()) (void)saveConfigToDisk();
        return true;
    }
    return false;
}

void startConfigAutosave() {
    std::lock_guard<std::mutex> lock(gMutex);
    if (gRunning) return;
    gStop = false;
    gRequested = gWritten = 0;
    gThread = std::thread(autosaveWorker);
    gRunning = true;
}

void touchConfig() {
    std::lock_guard<std::mutex> lock(gMutex);
    if (!gRunning) {
        (void)saveConfigToDisk();
        return;
    }
    ++gRequested;
    gCv.notify_one();
}

void stopConfigAutosave() {
    {
        std::lock_guard<std::mutex> lock(gMutex);
        if (!gRunning) return;
        gStop = true;
        gCv.notify_one();
    }
    if (gThread.joinable()) gThread.join();
    std::lock_guard<std::mutex> lock(gMutex);
    gRunning = false;
    gStop = false;
}
}
