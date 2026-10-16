#include <pl/ModMenu.hpp>
#include <pl/memory/Hook.hpp>
#include <pl/memory/Signature.hpp>

namespace pl::memory {
int hook(FuncPtr, FuncPtr, FuncPtr*, HookPriority) { return -1; }
bool unhook(FuncPtr, FuncPtr) { return false; }
uintptr_t resolveSignature(std::string_view, std::string_view) { return 0; }
}

namespace pl::modmenu {
bool registerModule(const ModuleInfo&) { return false; }
void unregisterModule(std::string_view) {}
void setModuleEnabled(std::string_view, bool) {}
}
