#include "ThornyManager.hpp"

#include "ThornySettings.hpp"

#include <Geode/loader/Mod.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <Geode/utils/file.hpp>

#include <chrono>

using namespace geode::prelude;

namespace thorny {

namespace {

// Normal cooldown between two low-priority lines.
constexpr double kDialogueCooldown = 15.0;
// Extra cooldown for an event type that we have already talked about.
constexpr double kSameEventCooldown = 30.0;
// Completed level / editor return / first launch / returning to menu bypass
// the normal cooldown, but still respect a small floor so they never spam.
constexpr double kHighPriorityCooldown = 3.0;

bool isHighPriority(ThornyEvent event) {
    switch (event) {
        case ThornyEvent::CompletedLevel:
        case ThornyEvent::ExitedEditor:
        case ThornyEvent::EditedLevel:
        case ThornyEvent::FirstLaunch:
        case ThornyEvent::StartedLevel:
        case ThornyEvent::FailedLevel:
        case ThornyEvent::ReturnedToMenu:
            return true;
        default:
            return false;
    }
}

double now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

std::string levelName(GJGameLevel* level) {
    if (!level) {
        return {};
    }
    auto name = level->m_levelName;
    return std::string(name.c_str(), name.size());
}

std::string levelAuthor(GJGameLevel* level) {
    if (!level) {
        return {};
    }
    auto author = level->m_creatorName;
    return std::string(author.c_str(), author.size());
}

} // namespace

ThornyManager& ThornyManager::get() {
    static ThornyManager instance;
    return instance;
}

void ThornyManager::initialize() {
    if (m_initialized) {
        return;
    }
    m_initialized = true;

    ThornySettings::get().applySettings();

    // Load the dialogue database from the mod resources.
    auto dialoguePath = Mod::get()->getResourcesDir() / "thorny_dialogue.json";
    auto contents = file::readString(dialoguePath);
    if (contents) {
        m_dialogue.load(contents.unwrap());
    } else {
        log::error("[Thorny] Could not read {}", dialoguePath);
    }

    // React to settings changes live.
    listenForSettingChanges<bool>("enabled", [](bool) {
        ThornyManager::get().onSettingChanged();
    });
    listenForSettingChanges<bool>("movement", [](bool) {
        ThornyManager::get().onSettingChanged();
    });
    listenForSettingChanges<bool>("dialogue", [](bool) {
        ThornyManager::get().onSettingChanged();
    });
    listenForSettingChanges<double>("size", [](double) {
        ThornyManager::get().onSettingChanged();
    });
    listenForSettingChanges<std::string>("dialogue-frequency", [](std::string) {
        ThornyManager::get().onSettingChanged();
    });
    listenForSettingChanges<std::string>("language", [](std::string) {
        ThornyManager::get().onSettingChanged();
        ThornyManager::get().onLanguageChanged();
    });

    m_ready = true;
}

bool ThornyManager::isEnabled() const {
    return m_ready && ThornySettings::get().enabled();
}

// --------------------------------------------------------------------------- scenes

void ThornyManager::onMainMenuEntered(cocos2d::CCLayer* menu) {
    if (!isEnabled()) {
        return;
    }
    if (!menu) {
        return;
    }

    log::info("[Thorny] Main menu entered");

    // If the player was in the editor and came back, this is where we notice
    // it: the editor is gone and no level was started in between.
    finalizeEditorExit();

    // Determine what the player just did, then react.
    bool firstLaunch = !Mod::get()->getSavedValue<bool>("first-launch-done", false);
    auto event = resolveMenuEvent(firstLaunch);

    if (firstLaunch) {
        Mod::get()->setSavedValue("first-launch-done", true);
        m_context.pushEvent(ThornyEvent::FirstLaunch);
    }

    if (event != ThornyEvent::None) {
        m_context.pushEvent(event);
    }

    // Spawn directly onto the layer we were just handed by the hook, and react
    // on the node it returns. During MenuLayer::init the layer is not yet the
    // running scene, so currentThorny() would not find him.
    if (auto* thorny = spawnThorny(menu)) {
        thorny->onEvent(m_context.lastEvent, m_context);
    }

    clearTransientContext();
}

void ThornyManager::finalizeEditorExit() {
    if (!m_editorTracked) {
        return;
    }
    m_editorTracked = false;

    // We only get here from the main menu, and every gameplay event that fires
    // while the editor is open is gated out in onLevelStarted/onPlayerDied/
    // onLevelCompleted/onLevelQuit. So this really is "came back from the
    // editor" - whether or not the player ran a playtest first.
    m_context.cameFromEditor = true;
    m_context.pushEvent(ThornyEvent::ExitedEditor);
    log::info("[Thorny] Editor exited (modified: {}, playtested: {})",
              m_context.editorWasModified, m_editorPlaytesting);
    m_editorPlaytesting = false;
}

Thorny* ThornyManager::spawnThorny(cocos2d::CCNode* menu) {
    removeThorny();

    if (!menu) {
        return nullptr;
    }

    // Skip if a previous Thorny is somehow still present on this layer.
    if (auto* existing = geode::cast::typeinfo_cast<Thorny*>(
            menu->getChildByIDRecursive("thorny-spike"_spr))) {
        return existing;
    }

    auto thorny = Thorny::create();
    if (!thorny) {
        log::error("[Thorny] Failed to create Thorny");
        return nullptr;
    }

    menu->addChild(thorny, 1000);
    log::debug("[Thorny] Spawned Thorny");
    return thorny;
}

void ThornyManager::removeThorny() {
    if (auto* thorny = currentThorny()) {
        thorny->removeFromParentAndCleanup(true);
    }
}

Thorny* ThornyManager::currentThorny() const {
    // Resolve Thorny through the live scene every time instead of holding a
    // pointer. `getChildByIDRecursive` does a setLookup, so this is cheap and it
    // can never return a node that belongs to a destroyed scene.
    auto* menu = Thorny::findMainMenu();
    if (!menu) {
        return nullptr;
    }
    return geode::cast::typeinfo_cast<Thorny*>(menu->getChildByIDRecursive("thorny-spike"_spr));
}

ThornyEvent ThornyManager::resolveMenuEvent(bool firstLaunch) {
    // Priority: level completion > level failure > editor return > plain return.
    if (m_context.levelJustCompleted) {
        return ThornyEvent::CompletedLevel;
    }
    if (m_context.levelJustFailed) {
        return ThornyEvent::FailedLevel;
    }
    if (m_context.cameFromEditor) {
        return m_context.editorWasModified ? ThornyEvent::EditedLevel : ThornyEvent::ExitedEditor;
    }
    if (m_context.levelWasQuit) {
        return ThornyEvent::ExitedLevel;
    }
    if (firstLaunch) {
        return ThornyEvent::FirstLaunch;
    }
    if (m_context.previousEvent == ThornyEvent::StartedLevel ||
        m_context.previousEvent == ThornyEvent::CompletedLevel ||
        m_context.previousEvent == ThornyEvent::FailedLevel ||
        m_context.previousEvent == ThornyEvent::ExitedLevel ||
        m_context.previousEvent == ThornyEvent::ExitedEditor ||
        m_context.previousEvent == ThornyEvent::EditedLevel) {
        return ThornyEvent::ReturnedToMenu;
    }
    return ThornyEvent::EnteredMainMenu;
}

void ThornyManager::clearTransientContext() {
    m_context.levelJustCompleted = false;
    m_context.levelJustFailed = false;
    m_context.levelWasQuit = false;
    m_context.cameFromEditor = false;
    m_context.editorWasModified = false;
}

// --------------------------------------------------------------------------- gameplay

void ThornyManager::rememberLevelInfo(GJGameLevel* level) {
    if (!level) {
        return;
    }
    m_context.lastLevelName = levelName(level);
    m_context.lastLevelID = level->m_levelID;
    m_context.lastLevelAuthor = levelAuthor(level);
    m_context.lastLevelWasCompleted = level->m_normalPercent >= 100;

    log::debug("[Thorny] Remembered level '{}' (id {})", m_context.lastLevelName, m_context.lastLevelID);
}

void ThornyManager::onLevelStarted(GJGameLevel* level) {
    // A level started while the editor is open is the editor's playtest. It
    // belongs to the editor session (leaving it returns to the editor), so it
    // is never reported as a normal level start.
    if (m_editorTracked) {
        m_editorPlaytesting = true;
        log::debug("[Thorny] Editor playtest started (level init)");
        return;
    }

    rememberLevelInfo(level);

    // Fresh attempt: nothing has been failed or completed yet.
    m_context.levelJustFailed = false;
    m_context.levelJustCompleted = false;
    m_context.levelWasQuit = false;
    m_context.levelWasInPractice = false;

    m_context.pushEvent(ThornyEvent::StartedLevel);
    log::info("[Thorny] Level started: {}", m_context.lastLevelName.empty() ? "(unknown)" : m_context.lastLevelName);
}

void ThornyManager::onPlayerDied() {
    // Deaths inside the editor's playtest are not real level failures.
    if (m_editorTracked) {
        return;
    }
    // Practice mode deaths are not real failures.
    if (PlayLayer::get() && PlayLayer::get()->m_isPracticeMode) {
        return;
    }
    m_context.levelJustFailed = true;
    m_context.levelJustCompleted = false;
    m_context.pushEvent(ThornyEvent::FailedLevel);
    log::info("[Thorny] Level failed");
}

void ThornyManager::onLevelCompleted(GJGameLevel* level, bool practice) {
    // Completions inside the editor's playtest are not real level completions.
    if (m_editorTracked) {
        return;
    }
    if (level) {
        rememberLevelInfo(level);
    }
    m_context.lastLevelWasCompleted = true;
    m_context.levelJustCompleted = true;
    m_context.levelJustFailed = false;
    m_context.levelWasInPractice = practice;
    m_context.pushEvent(ThornyEvent::CompletedLevel);
    log::info("[Thorny] Level completed{}", practice ? " (practice)" : "");
}

void ThornyManager::onLevelQuit() {
    // Leaving a playtest returns to the editor, not the menu.
    if (m_editorTracked) {
        return;
    }
    // Only meaningful if the player did not already fail or complete the level.
    if (m_context.levelJustCompleted || m_context.levelJustFailed) {
        return;
    }
    m_context.levelWasQuit = true;
    m_context.pushEvent(ThornyEvent::ExitedLevel);
    log::info("[Thorny] Level quit");
}

// --------------------------------------------------------------------------- editor

void ThornyManager::onEditorOpened(LevelEditorLayer* editor) {
    if (!editor) {
        return;
    }
    m_editorTracked = true;
    m_context.cameFromEditor = false;
    m_context.editorWasModified = false;
    m_context.pushEvent(ThornyEvent::OpenedEditor);
    log::info("[Thorny] Editor opened");
}

void ThornyManager::onEditorModified(LevelEditorLayer* editor) {
    if (editor && editor->m_level && editor->m_level->m_hasBeenModified) {
        m_context.editorWasModified = true;
    }
}

void ThornyManager::onEditorPlaytest() {
    // Test-play keeps the editor session alive; it is not a level to be
    // reported, and leaving it returns to the editor, not the menu.
    m_editorPlaytesting = true;
    log::debug("[Thorny] Editor playtest started");
}

void ThornyManager::onEditorPlaytestEnded() {
    m_editorPlaytesting = false;
    log::debug("[Thorny] Editor playtest ended");
}

// --------------------------------------------------------------------------- dialogue

DialogueCategory ThornyManager::resolveCategory(ThornyEvent event, ThornyContext const* context) const {
    // Greeting only on the very first line ever.
    if (event == ThornyEvent::FirstLaunch) {
        return DialogueCategory::Greeting;
    }
    // Exited editor with modifications is a more specific line.
    if (event == ThornyEvent::EditedLevel) {
        return DialogueCategory::EditorModified;
    }
    if (context && event == ThornyEvent::ExitedEditor && context->editorWasModified) {
        return DialogueCategory::EditorModified;
    }
    if (context && (event == ThornyEvent::EnteredMainMenu || event == ThornyEvent::None)) {
        // Nothing exciting happened; use the generic pool.
        return DialogueCategory::Generic;
    }
    return categoryForEvent(event);
}

std::string ThornyManager::dialogueForEvent(ThornyEvent event, ThornyContext const* context) {
    if (!isEnabled() || !ThornySettings::get().dialogueEnabled() || m_dialogue.empty()) {
        return {};
    }

    double timestamp = now();
    bool highPriority = isHighPriority(event);

    // High-priority events are throttled only briefly; everything else waits
    // for the normal cooldown.
    if (highPriority) {
        if (timestamp - m_lastHighPriorityTime < kHighPriorityCooldown) {
            return {};
        }
    } else if (timestamp - m_lastDialogueTime < kDialogueCooldown) {
        return {};
    }

    // Do not repeat the same category twice in a row.
    if (event == m_lastSpokenEvent && timestamp - m_lastSpokenEventTime < kSameEventCooldown) {
        return {};
    }

    auto category = resolveCategory(event, context);
    auto line = m_dialogue.pick(category, ThornySettings::get().useRussian());
    if (line.empty() && category != DialogueCategory::Generic) {
        // Fall back to generic so Thorny still says something.
        line = m_dialogue.pick(DialogueCategory::Generic, ThornySettings::get().useRussian());
    }
    if (line.empty()) {
        return {};
    }

    m_lastDialogueTime = timestamp;
    if (highPriority) {
        m_lastHighPriorityTime = timestamp;
    }
    m_lastSpokenEvent = event;
    m_lastSpokenEventTime = timestamp;

    log::debug("[Thorny] Says ({}): {}", categoryKey(category), line);
    return line;
}

// --------------------------------------------------------------------------- settings

void ThornyManager::onSettingChanged() {
    ThornySettings::get().applySettings();

    // Setting listeners fire from inside Geode's setting-application code (and,
    // on Android, possibly while the game is mid-frame). Starting/stopping the
    // movement actions from there re-enters the cocos2d action manager and can
    // crash, so all node work - including removal - is deferred to the next
    // main-thread tick.
    geode::queueInMainThread([]() {
        auto& manager = ThornyManager::get();

        if (!ThornySettings::get().enabled()) {
            manager.removeThorny();
            return;
        }

        // Re-resolve the live menu. We must never trust a stored menu pointer:
        // after a scene change a freed MenuLayer's memory can be reused, so a
        // stale pointer would still pass a WeakRef lock and then crash when we
        // add a child to whatever now lives there.
        auto* menu = Thorny::findMainMenu();
        if (!menu) {
            return;
        }

        if (!menu->getChildByIDRecursive("thorny-spike"_spr)) {
            if (auto* thorny = manager.spawnThorny(menu)) {
                thorny->onEvent(ThornyEvent::EnteredMainMenu, manager.m_context);
            }
        } else if (auto* thorny = manager.currentThorny()) {
            thorny->applySettings();
        }
    });
}

void ThornyManager::onLanguageChanged() {
    // Forget recent lines so the language switch is not held back by history.
    m_dialogue.forgetHistory();
}

} // namespace thorny
