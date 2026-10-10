#include <Geode/Geode.hpp>

#include "ThornyManager.hpp"

using namespace geode::prelude;

// Entry point. All the real work lives in ThornyManager; the hooks that feed it
// are in ThornyHooks.cpp.
$on_mod(Loaded) {
    log::info("[Thorny] Mod loaded");
    thorny::ThornyManager::get().initialize();
}
