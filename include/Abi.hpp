#pragma once

struct ModRegistration {
    void* instance{};
    bool (*load)(void*, void*){};
    bool (*enable)(void*, void*){};
    bool (*disable)(void*, void*){};
    bool (*unload)(void*, void*){};
};
