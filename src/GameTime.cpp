#include "GameTime.hpp"

#include "ElfPatch.hpp"

#include <pl/memory/Hook.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <dlfcn.h>
#include <mutex>
#include <thread>

namespace shadow {
namespace {
constexpr const char* kGetTimeSignature =
    "? ? ? D1 ? ? ? A9 ? ? ? F9 ? ? ? A9 ? ? ? 91 55 D0 3B D5 ? ? ? F9 ? ? ? F8 ? ? ? F9 ? ? ? F9 00 01 3F D6 ? ? ? F9";
constexpr const char* kGame = "libminecraftpe.so";

using GetTimeFn = int (*)(void*);
std::atomic<void*> gOriginal{nullptr};
std::atomic<int> gTime{-1};
std::atomic_bool gActive{false};
void* gTarget = nullptr;

int getTimeDetour(void* self) {
    auto original = reinterpret_cast<GetTimeFn>(gOriginal.load(std::memory_order_acquire));
    const int value = original ? original(self) : 0;
    if (original) {
        if (value >= 0 && gTime.load(std::memory_order_relaxed) != value) gTime.store(value, std::memory_order_relaxed);
    }
    return value;
}

std::mutex gMutex;
std::condition_variable gCv;
std::thread gRetry;
bool gStopRetry = false;

bool gameLoaded() {
    void* handle = ::dlopen(kGame, RTLD_NOW | RTLD_NOLOAD);
    if (!handle) return false;
    ::dlclose(handle);
    return true;
}

enum class Attempt { Installed, NotLoaded, NotFound, HookFailed };

Attempt tryInstall() {
    if (gActive.load()) return Attempt::Installed;
    if (!gameLoaded()) return Attempt::NotLoaded;
    const auto match = elf::findUniqueExecutablePattern(kGame, kGetTimeSignature);
    if (match.matches != 1 || !match.address) {
        return Attempt::NotFound;
    }
    const std::uintptr_t address = match.address;
    void* original = nullptr;
    if (pl::memory::hook(reinterpret_cast<void*>(address), reinterpret_cast<void*>(&getTimeDetour), &original) != 0 || !original) {
        return Attempt::HookFailed;
    }
    gOriginal.store(original, std::memory_order_release);
    gTarget = reinterpret_cast<void*>(address);
    gActive.store(true);
    return Attempt::Installed;
}
}

bool installTimeHook() {
    const Attempt first = tryInstall();
    if (first == Attempt::NotLoaded) {
        std::lock_guard<std::mutex> lock(gMutex);
        if (!gRetry.joinable()) {
            gStopRetry = false;
            gRetry = std::thread([] {
                std::unique_lock<std::mutex> lock(gMutex);
                for (int i = 0; i < 60 && !gStopRetry; ++i) {
                    if (gCv.wait_for(lock, std::chrono::seconds(2), [] { return gStopRetry; })) break;
                    lock.unlock();
                    const Attempt a = tryInstall();
                    lock.lock();
                    if (a == Attempt::NotLoaded) continue;
                    break;
                }
            });
        }
    }
    return first == Attempt::Installed;
}

void removeTimeHook() {
    {
        std::lock_guard<std::mutex> lock(gMutex);
        gStopRetry = true;
        gCv.notify_all();
    }
    if (gRetry.joinable()) gRetry.join();
    if (!gActive.exchange(false)) return;
    if (gTarget) (void)pl::memory::unhook(gTarget, reinterpret_cast<void*>(&getTimeDetour));
    gTarget = nullptr;
    gTime.store(-1);
}

bool timeHookActive() { return gActive.load(); }

int currentGameTime() { return gTime.load(std::memory_order_relaxed); }
}
