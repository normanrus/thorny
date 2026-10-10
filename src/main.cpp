#include <Geode/Geode.hpp>

#include "ThornyManager.hpp"

using namespace geode::prelude;

// Entry point. All the real work lives in ThornyManager; the hooks that feed it
// are in ThornyHooks.cpp.
$on_mod(Loaded) {
    log::info("[Thorny] Mod loaded");

    // Existing installs were tested at scale 1.0; bump them once now that the
    // default changed, without overwriting a user's own choice.
    auto mod = Mod::get();
    if (!mod->getSavedValue<bool>("size-migrated-v2", false)) {
        if (mod->getSettingValue<double>("size") == 1.0) {
            mod->setSettingValue<double>("size", 1.5);
        }
        mod->setSavedValue("size-migrated-v2", true);
    }

    thorny::ThornyManager::get().initialize();
}
