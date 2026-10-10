#include <Geode/Geode.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include "ThornyManager.hpp"

using namespace geode::prelude;

// All gameplay / editor / menu hooks live here. Every hook only records what
// happened; the actual reaction happens when Thorny is (re)created in the main
// menu, so we never touch a layer that is being torn down.

// ---------------------------------------------------------------------------
// Main menu: spawn Thorny on enter.
//
// We deliberately do NOT hook MenuLayer::onExit: MenuLayer does not override
// onExit, so it has no address in the bindings and the override would never be
// called. Instead Thorny (our own class) cleans itself up in Thorny::onExit, and
// the manager never stores a menu pointer at all - it finds the live scene when
// it needs one.
// ---------------------------------------------------------------------------
class $modify(ThornyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) {
            return false;
        }
        thorny::ThornyManager::get().onMainMenuEntered(this);
        return true;
    }
};

// ---------------------------------------------------------------------------
// Level info: remember the level the player is looking at, so Thorny knows the
// name / author even if the level is started without any other hook firing.
// ---------------------------------------------------------------------------
class $modify(ThornyLevelInfoLayer, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) {
            return false;
        }
        thorny::ThornyManager::get().rememberLevelInfo(level);
        return true;
    }
};

// ---------------------------------------------------------------------------
// Play layer: detect level start, death and completion.
// ---------------------------------------------------------------------------
class $modify(ThornyPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }
        thorny::ThornyManager::get().onLevelStarted(level);
        return true;
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        PlayLayer::destroyPlayer(player, object);

        // The base call is what actually marks the player dead; fake / harmless
        // collisions never reach here with m_isDead set.
        if (player && player->m_isDead) {
            thorny::ThornyManager::get().onPlayerDied();
        }
    }

    void levelComplete() {
        PlayLayer::levelComplete();
        thorny::ThornyManager::get().onLevelCompleted(this->m_level, this->m_isPracticeMode);
    }

    void onQuit() {
        // Leaving the level without dying or finishing.
        thorny::ThornyManager::get().onLevelQuit();
        PlayLayer::onQuit();
    }
};

// ---------------------------------------------------------------------------
// Editor: detect open and modification.
//
// The exit itself is not hooked on LevelEditorLayer::onExit: that is an
// inherited CCLayer virtual and hooking it can interfere with Geode's own
// handling. Instead the manager notices the exit when the main menu reappears.
// ---------------------------------------------------------------------------
class $modify(ThornyEditorLayer, LevelEditorLayer) {
    bool init(GJGameLevel* level, bool noUI) {
        if (!LevelEditorLayer::init(level, noUI)) {
            return false;
        }
        thorny::ThornyManager::get().onEditorOpened(this);
        return true;
    }

    // Called whenever the editor settings change, which only happens once the
    // player has actually edited something.
    void levelSettingsUpdated() {
        LevelEditorLayer::levelSettingsUpdated();
        thorny::ThornyManager::get().onEditorModified(this);
    }

    // The playtest button. Leaving a playtest returns to the editor, so these
    // keep the editor session marked as "in the editor" rather than letting
    // gameplay hooks fire as if it were a normal level.
    void onPlaytest() {
        LevelEditorLayer::onPlaytest();
        thorny::ThornyManager::get().onEditorPlaytest();
    }

    void onStopPlaytest() {
        LevelEditorLayer::onStopPlaytest();
        thorny::ThornyManager::get().onEditorPlaytestEnded();
    }
};
