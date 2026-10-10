#include "ThornyDialogue.hpp"

#include <Geode/Geode.hpp>

#include <random>

namespace thorny {

namespace {

std::mt19937& rng() {
    static std::mt19937 engine{ std::random_device{}() };
    return engine;
}

float randomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng());
}

bool isRecent(std::deque<std::string> const& recent, std::string const& text) {
    for (auto const& entry : recent) {
        if (entry == text) {
            return true;
        }
    }
    return false;
}

} // namespace

char const* categoryKey(DialogueCategory category) {
    switch (category) {
        case DialogueCategory::Generic: return "generic";
        case DialogueCategory::Greeting: return "greeting";
        case DialogueCategory::LevelStarted: return "level_started";
        case DialogueCategory::LevelFailed: return "level_failed";
        case DialogueCategory::LevelCompleted: return "level_completed";
        case DialogueCategory::EditorEntered: return "editor_entered";
        case DialogueCategory::EditorExited: return "editor_exited";
        case DialogueCategory::EditorModified: return "editor_modified";
        case DialogueCategory::ReturningToMenu: return "returning_to_menu";
        case DialogueCategory::Idle: return "idle";
        case DialogueCategory::Sleeping: return "sleeping";
        default: return "generic";
    }
}

DialogueCategory categoryForEvent(ThornyEvent event) {
    switch (event) {
        case ThornyEvent::FirstLaunch: return DialogueCategory::Greeting;
        case ThornyEvent::StartedLevel: return DialogueCategory::LevelStarted;
        case ThornyEvent::FailedLevel: return DialogueCategory::LevelFailed;
        case ThornyEvent::CompletedLevel: return DialogueCategory::LevelCompleted;
        case ThornyEvent::OpenedEditor: return DialogueCategory::EditorEntered;
        case ThornyEvent::ExitedEditor: return DialogueCategory::EditorExited;
        case ThornyEvent::EditedLevel: return DialogueCategory::EditorModified;
        case ThornyEvent::ReturnedToMenu: return DialogueCategory::ReturningToMenu;
        case ThornyEvent::StayedIdle: return DialogueCategory::Idle;
        case ThornyEvent::FellAsleep: return DialogueCategory::Sleeping;
        default: return DialogueCategory::Generic;
    }
}

bool ThornyDialogue::load(std::string const& json) {
    m_lines.clear();

    auto parsed = matjson::parse(json);
    if (!parsed) {
        geode::log::error("[Thorny] Failed to parse dialogue JSON: {}", parsed.unwrapErr().message);
        return false;
    }
    auto root = parsed.unwrap();

    auto const& languages = root["languages"];
    if (!languages.isObject()) {
        geode::log::error("[Thorny] dialogue JSON has no 'languages' object");
        return false;
    }

    for (auto const& [langCode, categories] : languages) {
        bool russian = langCode == "ru";
        if (!categories.isObject()) {
            continue;
        }
        for (auto const& [categoryName, entries] : categories) {
            if (!entries.isArray()) {
                continue;
            }
            DialogueCategory category = DialogueCategory::Count;
            for (int i = 0; i < static_cast<int>(DialogueCategory::Count); ++i) {
                auto candidate = static_cast<DialogueCategory>(i);
                if (categoryName == categoryKey(candidate)) {
                    category = candidate;
                    break;
                }
            }
            if (category == DialogueCategory::Count) {
                geode::log::warn("[Thorny] Unknown dialogue category '{}'", categoryName);
                continue;
            }
            for (auto const& entry : entries) {
                if (!entry.isString()) {
                    continue;
                }
                m_lines.push_back(DialogueLine{
                    entry.asString().unwrapOr(""), category, russian, 1.0f
                });
            }
        }
    }

    geode::log::info("[Thorny] Loaded {} dialogue lines", m_lines.size());
    return !m_lines.empty();
}

std::string ThornyDialogue::pick(DialogueCategory category, bool useRussian) {
    if (m_lines.empty()) {
        return {};
    }

    // Try the requested language first, then fall back to the other one.
    std::vector<DialogueLine const*> candidates;
    float totalWeight = 0.0f;

    for (bool wantRussian : { useRussian, !useRussian }) {
        candidates.clear();
        totalWeight = 0.0f;
        for (auto const& line : m_lines) {
            if (line.category != category || line.russian != wantRussian) {
                continue;
            }
            if (isRecent(m_recent, line.text)) {
                continue;
            }
            candidates.push_back(&line);
            totalWeight += line.weight;
        }
        if (!candidates.empty()) {
            break;
        }
    }

    // Everything was recently said; allow repeats from the requested language.
    if (candidates.empty()) {
        for (auto const& line : m_lines) {
            if (line.category == category && line.russian == useRussian) {
                candidates.push_back(&line);
                totalWeight += line.weight;
            }
        }
    }
    if (candidates.empty() || totalWeight <= 0.0f) {
        return {};
    }

    float roll = randomFloat(0.0f, totalWeight);
    DialogueLine const* chosen = candidates.back();
    for (auto const* candidate : candidates) {
        roll -= candidate->weight;
        if (roll <= 0.0f) {
            chosen = candidate;
            break;
        }
    }

    m_recent.push_back(chosen->text);
    while (m_recent.size() > kRecentMemory) {
        m_recent.pop_front();
    }
    return chosen->text;
}

void ThornyDialogue::forgetHistory() {
    m_recent.clear();
}

} // namespace thorny
