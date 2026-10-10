#pragma once

#include <deque>
#include <string>
#include <vector>

#include "ThornyEvents.hpp"

namespace thorny {

// A category groups lines that fit the same situation. The category is picked
// from the current ThornyEvent by the manager, never from a giant if-chain.
enum class DialogueCategory {
    Generic,
    Greeting,
    LevelStarted,
    LevelFailed,
    LevelCompleted,
    EditorEntered,
    EditorExited,
    EditorModified,
    ReturningToMenu,
    Idle,
    Sleeping,

    Count,
};

char const* categoryKey(DialogueCategory category);

// Maps a detected event to the category Thorny should speak about.
DialogueCategory categoryForEvent(ThornyEvent event);

struct DialogueLine {
    std::string text;
    DialogueCategory category;
    bool russian = false;
    float weight = 1.0f;
};

// Loads Thorny's lines from a JSON resource and picks them at random while
// avoiding repeating a line the player just saw.
class ThornyDialogue {
public:
    // Returns false if the file could not be read or parsed. On failure the
    // database stays empty and Thorny simply never talks.
    bool load(std::string const& json);

    bool empty() const { return m_lines.empty(); }

    // Picks a weighted-random line from `category` in the requested language.
    // `useRussian` selects the language; falls back to the other language if
    // the chosen one has no lines for the category.
    // Returns an empty string when there is nothing suitable.
    std::string pick(DialogueCategory category, bool useRussian);
    // Drops the memory of recently used lines (called when the language
    // changes so the reset is not visible as a repeat).
    void forgetHistory();

private:
    std::vector<DialogueLine> m_lines;

    // Recently spoken text, so we do not repeat ourselves.
    std::deque<std::string> m_recent;
    static constexpr std::size_t kRecentMemory = 3;
};

} // namespace thorny
