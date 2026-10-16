#pragma once
#include <utility>
#include "pl/Export.hpp"
namespace pl::memory {
using FuncPtr = void*;
enum class HookPriority : int { Highest=0, High=100, Normal=200, Low=300, Lowest=400 };
PL_EXPORT int hook(FuncPtr target, FuncPtr detour, FuncPtr* originalFunc, HookPriority priority=HookPriority::Normal);
PL_EXPORT bool unhook(FuncPtr target, FuncPtr detour);
}
