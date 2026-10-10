#pragma once

#include <string>

// Everything Thorny needs to know about "where the player just came from".
// The manager owns one global instance of this and updates it from hooks and
// from the scene tracking in ThornyManager.
namespace thorny {

// What Thorny should react to. Not every scene transition produces one of
// these - only the transitions we can detect reliably do.
enum class ThornyEvent {
    None,
    FirstLaunch,
    EnteredMainMenu,
    StartedLevel,
    FailedLevel,
    CompletedLevel,
    ExitedLevel,
    OpenedEditor,
    ExitedEditor,
    EditedLevel,
    ReturnedToMenu,
    StayedIdle,
    FellAsleep,
};

char const* toString(ThornyEvent event);

// A snapshot of the last meaningful thing that happened.
struct ThornyContext {
    ThornyEvent lastEvent = ThornyEvent::None;
    ThornyEvent previousEvent = ThornyEvent::None;

    // Last level the player interacted with.
    std::string lastLevelName;
    int lastLevelID = 0;
    std::string lastLevelAuthor;
    bool lastLevelWasCompleted = false;

    // Attempt bookkeeping for the level that was just played.
    bool levelJustFailed = false;
    bool levelJustCompleted = false;
    bool levelWasQuit = false;
    bool levelWasInPractice = false;

    // Editor bookkeeping.
    bool cameFromEditor = false;
    bool editorWasModified = false;

    // Seconds since the player last did something meaningful.
    double secondsSinceActivity = 0.0;
    double lastEventTime = 0.0;

    bool hasPlayedLevel() const { return lastLevelID != 0 || !lastLevelName.empty(); }

    // Records a new "current" event, keeping the previous one for context.
    // Events that should be re-recorded (completed / failed / editor return)
    // are allowed to repeat; purely informational ones are not.
    void pushEvent(ThornyEvent event) {
        if (event == ThornyEvent::None) {
            return;
        }
        if (event == lastEvent && !repeatsWhenIdentical(event)) {
            return;
        }
        previousEvent = lastEvent;
        lastEvent = event;
    }

    static bool repeatsWhenIdentical(ThornyEvent event) {
        switch (event) {
            case ThornyEvent::CompletedLevel:
            case ThornyEvent::FailedLevel:
            case ThornyEvent::ExitedEditor:
            case ThornyEvent::EditedLevel:
                return true;
            default:
                return false;
        }
    }
};

} // namespace thorny
