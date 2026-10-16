#pragma once

namespace shadow {

class ShadowMod {
public:
    bool load();
    bool enable();
    bool disable();
    bool unload();

private:
    bool hooked_{};
    bool menu_{};
};

}
