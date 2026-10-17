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
#include <jni.h>
#include <mutex>
#include <sstream>
#include <locale>
#include <iomanip>
#include <string_view>
#include <sys/stat.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

namespace shadow {
namespace {
constexpr std::size_t kMaxConfigBytes = 64 * 1024;
constexpr auto kDebounce = std::chrono::milliseconds(250);
constexpr auto kPoll = std::chrono::milliseconds(400);  // how often the config file is checked for edits made outside the game
constexpr const char* kModDirName = "DynamicShadows";

void (*gReloaded)() = nullptr;  // told when the file was changed from outside and the new values are live

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

// mkdir -p. Parents we are not allowed to create (e.g. /storage) are skipped; only the final directory has to exist.
bool makeDirs(const std::string& path) {
    if (path.empty()) return false;
    std::string current;
    for (std::size_t i = 0; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            if (!current.empty()) (void)::mkdir(current.c_str(), 0770);
        }
        if (i < path.size()) current += path[i];
    }
    struct stat st{};
    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
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

// The launcher copies a mod's package into <app>/cache/mods/<mod id>/ and loads the library from there; that copy has no config folder.
// The real mod folder (<mods dir>/<mod id>/config, the one the launcher's config editor lists) is known to the launcher's ModManager, so ask it.
// Returns an empty string when that is not possible (no JVM / thread not attached / class not found); nothing here may throw or crash.
std::string jstr(JNIEnv* env, jobject value) {
    if (!value) return {};
    const char* chars = env->GetStringUTFChars(static_cast<jstring>(value), nullptr);
    if (!chars) return {};
    std::string out(chars);
    env->ReleaseStringUTFChars(static_cast<jstring>(value), chars);
    return out;
}

std::string launcherConfigDir(const std::string& modId) {
    if (modId.empty()) return {};
    using GetVmsFn = jint (*)(JavaVM**, jsize, jsize*);
    const auto getVms = reinterpret_cast<GetVmsFn>(::dlsym(RTLD_DEFAULT, "JNI_GetCreatedJavaVMs"));
    if (!getVms) return {};
    JavaVM* vm = nullptr;
    jsize count = 0;
    if (getVms(&vm, 1, &count) != JNI_OK || count < 1 || !vm) return {};
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK || !env) return {};  // never attach a thread just for this
    if (env->PushLocalFrame(48) != 0) {
        env->ExceptionClear();
        return {};
    }
    std::string result;
    const auto failed = [&] {
        if (!env->ExceptionCheck()) return false;
        env->ExceptionClear();
        return true;
    };
    do {
        jclass managerClass = env->FindClass("org/levimc/launcher/core/mods/ModManager");
        if (!managerClass || failed()) break;
        jmethodID getInstance = env->GetStaticMethodID(managerClass, "getInstance", "()Lorg/levimc/launcher/core/mods/ModManager;");
        if (!getInstance || failed()) break;
        jmethodID getMods = env->GetMethodID(managerClass, "getMods", "()Ljava/util/List;");
        if (!getMods || failed()) break;
        jclass listClass = env->FindClass("java/util/List");
        if (!listClass || failed()) break;
        jmethodID listSize = env->GetMethodID(listClass, "size", "()I");
        if (!listSize || failed()) break;
        jmethodID listGet = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");
        if (!listGet || failed()) break;
        jclass modClass = env->FindClass("org/levimc/launcher/core/mods/Mod");
        if (!modClass || failed()) break;
        jmethodID getId = env->GetMethodID(modClass, "getId", "()Ljava/lang/String;");
        if (!getId || failed()) break;
        jmethodID getConfigDirPath = env->GetMethodID(modClass, "getConfigDirPath", "()Ljava/lang/String;");
        if (!getConfigDirPath || failed()) break;

        jobject manager = env->CallStaticObjectMethod(managerClass, getInstance);
        if (!manager || failed()) break;
        jobject mods = env->CallObjectMethod(manager, getMods);
        if (!mods || failed()) break;
        const jint size = env->CallIntMethod(mods, listSize);
        if (failed()) break;
        for (jint i = 0; i < size; ++i) {
            jobject mod = env->CallObjectMethod(mods, listGet, i);
            if (!mod || failed()) continue;
            jobject id = env->CallObjectMethod(mod, getId);
            const bool match = !failed() && jstr(env, id) == modId;
            if (id) env->DeleteLocalRef(id);
            if (match) {
                jobject dir = env->CallObjectMethod(mod, getConfigDirPath);
                if (!failed()) result = jstr(env, dir);
                if (dir) env->DeleteLocalRef(dir);
                env->DeleteLocalRef(mod);
                break;
            }
            env->DeleteLocalRef(mod);
        }
    } while (false);
    (void)failed();
    env->PopLocalFrame(nullptr);
    return result.empty() || result.front() != '/' ? std::string{} : result;
}

// <app>/files/native_mod_configs/<name>/config.json: where earlier versions kept the config (private to the app, not visible to the launcher).
std::string appPrivateConfigPath(const char* name) {
    const std::string self = selfLibraryPath();
    const auto cachePos = self.find("/cache/");
    if (cachePos == std::string::npos) return {};
    return self.substr(0, cachePos) + "/files/native_mod_configs/" + name + "/config.json";
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

    // Preferred: <mod folder>/config/config.json, the file the launcher's config editor shows.
    const auto slash = self.find_last_of('/');
    if (slash != std::string::npos && slash > 0) {
        const auto parentSlash = self.find_last_of('/', slash - 1);
        const std::string modId = self.substr(parentSlash == std::string::npos ? 0 : parentSlash + 1, slash - (parentSlash == std::string::npos ? 0 : parentSlash + 1));
        const std::string dir = launcherConfigDir(modId);
        if (!dir.empty() && makeDirs(dir)) return dir + "/config.json";
    }

    // The launcher could not be asked: keep working from the app-private folder (and move to the mod folder once it is available).
    const std::string legacy = appPrivateConfigPath(kModDirName);
    if (!legacy.empty()) {
        const auto legacySlash = legacy.find_last_of('/');
        const std::string files = legacy.substr(0, legacy.find("/native_mod_configs/"));
        const std::string dir = files + "/native_mod_configs";
        (void)makeDir(files);
        if (makeDir(dir) && makeDir(legacy.substr(0, legacySlash))) return legacy;
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

// Identity of the config file as last read or written by us. A different stamp means someone else (a text editor, the launcher's config
// editor) changed it.
struct FileStamp {
    std::uint64_t ino = 0;
    std::int64_t size = 0, mtimeNs = 0, ctimeNs = 0;
    bool operator==(const FileStamp& o) const { return ino == o.ino && size == o.size && mtimeNs == o.mtimeNs && ctimeNs == o.ctimeNs; }
};

std::mutex gStampMutex;
FileStamp gKnownStamp;
bool gKnownValid = false;

bool stampOf(const std::string& path, FileStamp& out) {
    struct stat st{};
    if (::stat(path.c_str(), &st) != 0) return false;
    out.ino = static_cast<std::uint64_t>(st.st_ino);
    out.size = static_cast<std::int64_t>(st.st_size);
    out.mtimeNs = static_cast<std::int64_t>(st.st_mtim.tv_sec) * 1000000000LL + st.st_mtim.tv_nsec;
    out.ctimeNs = static_cast<std::int64_t>(st.st_ctim.tv_sec) * 1000000000LL + st.st_ctim.tv_nsec;
    return true;
}

void rememberStamp(const std::string& path) {
    FileStamp now;
    const bool ok = stampOf(path, now);
    std::lock_guard<std::mutex> lock(gStampMutex);
    gKnownValid = ok;
    if (ok) gKnownStamp = now;
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
    const auto slash = path.find_last_of('/');
    if (slash != std::string::npos && slash > 0) (void)makeDirs(path.substr(0, slash));  // the folder may have been deleted meanwhile
    const std::string tmp = path + ".tmp";
    const int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0660);
    if (fd < 0) return false;
    bool ok = writeAll(fd, json.data(), json.size());
    if (::close(fd) != 0) ok = false;
    if (ok && ::rename(tmp.c_str(), path.c_str()) != 0) ok = false;
    if (!ok) (void)::unlink(tmp.c_str());
    if (ok) rememberStamp(path);  // our own write is not an outside edit
    return ok;
}

// Called by the worker every kPoll when nothing is waiting to be saved: if the file changed behind our back (edited in a text editor or in
// the launcher's config editor), load it so the new values apply right away, and tell the menu to show them.
void pollExternalEdits() {
    const std::string& path = pathStorage();
    if (path.empty()) return;
    FileStamp now;
    if (!stampOf(path, now)) return;  // missing: an editor may be replacing it, or the user removed it; the next save recreates it
    {
        std::lock_guard<std::mutex> lock(gStampMutex);
        if (gKnownValid && now == gKnownStamp) return;
    }
    // Only act on a file that stayed the same for two polls, so a half-written file is never read.
    static FileStamp pending;
    static bool havePending = false;
    if (!havePending || !(pending == now)) {
        pending = now;
        havePending = true;
        return;
    }
    havePending = false;
    std::string text;
    if (!readTextFile(path, text)) return;  // empty file (editor truncated it): keep what we have, look again on the next change
    const bool migrated = applyConfigText(text);
    {
        std::lock_guard<std::mutex> lock(gStampMutex);
        gKnownStamp = now;
        gKnownValid = true;
    }
    if (migrated) (void)saveConfigToDisk();  // older layout: write it back in the current one
    if (gReloaded) gReloaded();
}

void autosaveWorker() {
    std::unique_lock<std::mutex> lock(gMutex);
    for (;;) {
        const bool pending = gCv.wait_for(lock, kPoll, [] { return gStop || gRequested != gWritten; });
        if (gStop) break;
        if (!pending) {
            lock.unlock();
            pollExternalEdits();
            lock.lock();
            continue;
        }
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

void setConfigReloadedCallback(void (*callback)()) { gReloaded = callback; }

bool loadConfigFromDisk() {
    std::string text;
    const std::string& persistent = pathStorage();
    if (!persistent.empty() && readTextFile(persistent, text)) {
        if (applyConfigText(text)) (void)saveConfigToDisk();
        rememberStamp(persistent);
        return true;
    }

    // First start with this layout: carry over what earlier versions saved in the app-private folder.
    for (const char* name : {kModDirName, "Shadow"}) {
        const std::string legacy = appPrivateConfigPath(name);
        if (legacy.empty() || legacy == persistent || !readTextFile(legacy, text)) continue;
        (void)applyConfigText(text);
        if (!persistent.empty()) (void)saveConfigToDisk();
        return true;
    }

    const std::string packaged = siblingConfigPath();
    if (!packaged.empty() && packaged != persistent && readTextFile(packaged, text)) {
        (void)applyConfigText(text);
        if (!persistent.empty()) (void)saveConfigToDisk();
        return true;
    }

    // No config anywhere: write the defaults so the file exists (and shows up in the launcher's config editor).
    if (!persistent.empty()) (void)saveConfigToDisk();
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
