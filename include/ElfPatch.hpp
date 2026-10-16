#pragma once

#include <atomic>
#include <cstdint>
#include <string_view>

namespace shadow::elf {

std::uintptr_t findModuleBase(const char* soName);

int patchImport(std::uintptr_t base, const char* name, void* detour, std::atomic<void*>& originalOut);

void restoreAll();

struct PatternMatch { std::uintptr_t address{}; unsigned matches{}; };
PatternMatch findUniqueExecutablePattern(const char* module, std::string_view pattern);

}
