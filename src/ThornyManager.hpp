#pragma once

#include <Geode/Geode.hpp>

#include <string>

#include "Thorny.hpp"
#include "ThornyDialogue.hpp"
#include "ThornyEvents.hpp"

namespace thorny {

// Owns the Thorny instance, the dialogue database and the last-known context
// describing "where the player just came from". All hooks funnel into this
// class so the game classes themselves stay untouched apart from the hook.
class ThornyManager {
public:
    static ThornyManager& get();

    // Called once from $on_mod(Loaded): reads settings, loads dialogue, wires
    // up the setting listeners.
    void initialize();

    // ---- scene / gameplay notifications (called from hooks) ----
    void onMainMenuEntered(cocos2d::CCLayer* menu);

    void onLevelStarted(GJGameLevel* level);
    void onPlayerDied();
    void onLevelCompleted(GJGameLevel* level, bool practice);
    void onLevelQuit();

    void onEditorOpened(LevelEditorLayer* editor);
    void onEditorModified(LevelEditorLayer* editor);
    // The editor's "playtest" button: leaving a playtest returns to the editor,
    // not the menu, so it must not be mistaken for an editor exit.
    void onEditorPlaytest();
    void onEditorPlaytestEnded();

    // Records the level the player is looking at, so we know the name/author
    // even before the level is actually played.
    void rememberLevelInfo(GJGameLevel* level);

    // ---- queries used by the Thorny node ----
    std::string dialogueForEvent(ThornyEvent event, ThornyContext const* context = nullptr);
    ThornyContext const& context() const { return m_context; }
    bool isEnabled() const;

    // Called by setting listeners.
    void onSettingChanged();
    void onLanguageChanged();

private:
    ThornyManager() = default;

    void spawnThorny(cocos2d::CCLayer* menu);
    void removeThorny();
    void finalizeEditorExit();
    ThornyEvent resolveMenuEvent(bool firstLaunch);
    DialogueCategory resolveCategory(ThornyEvent event, ThornyContext const* context) const;
    void clearTransientContext();

    ThornyDialogue m_dialogue;
    geode::WeakRef<Thorny> m_thorny;
    geode::WeakRef<cocos2d::CCLayer> m_menu;

    // Editor bookkeeping. We never write into game memory; we only latch
    // whether the editor was open, whether it was modified, and whether a
    // playtest is currently running.
    bool m_editorTracked = false;
    // True while the editor's playtest is running; its end returns to the
    // editor rather than the menu.
    bool m_editorPlaytesting = false;

    ThornyContext m_context;

    // Dialogue pacing.
    double m_lastDialogueTime = 0.0;
    double m_lastHighPriorityTime = 0.0;
    ThornyEvent m_lastSpokenEvent = ThornyEvent::None;
    double m_lastSpokenEventTime = 0.0;

    bool m_ready = false;
    bool m_initialized = false;
};

} // namespace thorny
