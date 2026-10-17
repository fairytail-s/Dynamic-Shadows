#pragma once

namespace shadow::player {

inline constexpr int kCasterScissorX = 13;
inline constexpr int kCasterScissorY = 17;
inline constexpr int kCasterScissorSize = 1;

bool installHooks();
void removeHooks();
void setRendererReady(bool ready);
bool hooksActive();
bool casterExpected();

}
