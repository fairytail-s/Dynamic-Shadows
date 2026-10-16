#include "Abi.hpp"
#include "Mod.hpp"

namespace {
shadow::ShadowMod gMod;

template <bool (shadow::ShadowMod::*Fn)()>
bool thunk(void*, void*) {
    try {
        return (gMod.*Fn)();
    } catch (...) {
        return false;
    }
}
}

extern "C" __attribute__((visibility("default"))) ModRegistration* PLGetModRegistration() {
    static ModRegistration reg{&gMod, &thunk<&shadow::ShadowMod::load>, &thunk<&shadow::ShadowMod::enable>,
                               &thunk<&shadow::ShadowMod::disable>, &thunk<&shadow::ShadowMod::unload>};
    return &reg;
}
