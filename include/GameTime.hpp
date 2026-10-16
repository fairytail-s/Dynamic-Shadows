#pragma once

namespace shadow {

bool installTimeHook();
void removeTimeHook();
bool timeHookActive();

int currentGameTime();

}
