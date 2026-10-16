#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "pl/Export.hpp"
namespace pl::modmenu {
enum class ConfigType { Toggle, SliderInt, SliderFloat, Radio, Color, Keybind, Text, Button };
struct ConfigEntry {
  std::string key;
  std::string displayName;
  ConfigType type{};
  std::string defaultValue;
  std::string minValue;
  std::string maxValue;
  std::string dependsOn;
};
struct ModuleInfo {
  std::string moduleId;
  std::string displayName;
  std::string description;
  std::string modId;
  bool defaultEnabled{};
  bool hideInHudEditor{};
  std::vector<ConfigEntry> configs;
  std::function<void(std::string_view moduleId, bool enabled)> onToggle;
  std::function<void(std::string_view moduleId, std::string_view key, std::string_view value)> onConfigChanged;

  std::function<void(std::string_view moduleId, std::string_view key, bool isDown)> onKeybind;
};
PL_EXPORT bool registerModule(const ModuleInfo& info);
PL_EXPORT void unregisterModule(std::string_view moduleId);
PL_EXPORT void setModuleEnabled(std::string_view moduleId, bool enabled);
}
