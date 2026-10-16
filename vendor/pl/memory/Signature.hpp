#pragma once
#include <cstdint>
#include <string_view>
#include "pl/Export.hpp"
namespace pl::memory {
PL_EXPORT uintptr_t resolveSignature(std::string_view signature, std::string_view moduleName);
}
