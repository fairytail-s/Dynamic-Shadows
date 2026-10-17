#include "Mod.hpp"

#include "Config.hpp"
#include "GameTime.hpp"
#include "GlHooks.hpp"
#include "Menu.hpp"

namespace shadow {
bool ShadowMod::load() {
    (void)loadConfigFromDisk();
    setConfigReloadedCallback(&notifyConfigReloaded);
    startConfigAutosave();

    return true;
}

bool ShadowMod::enable() {
    menu_ = registerMenu();

    (void)installTimeHook();
    if (!installGlHooks()) {
        removeTimeHook();
        return menu_;
    }
    hooked_ = true;

    return true;
}

bool ShadowMod::disable() {
    if (menu_) {
        unregisterMenu();
        menu_ = false;
    }
    if (hooked_) {
        removeGlHooks();
        hooked_ = false;
    }
    removeTimeHook();

    return true;
}

bool ShadowMod::unload() {
    disable();
    stopConfigAutosave();

    return true;
}
}
