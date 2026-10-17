#pragma once
namespace shadow {
bool registerMenu();
// The config file was changed from outside the game: show the new values in the launcher menu.
void notifyConfigReloaded();
void unregisterMenu();
}
