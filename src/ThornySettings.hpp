#pragma once

#include <chrono>
#include <string>

namespace thorny {

// Which language Thorny should speak in.
enum class ThornyLanguage {
    Auto,
    English,
    Russian,
};

// How chatty Thorny is allowed to be.
enum class DialogueFrequency {
    Rare,
    Normal,
    Frequent,
};

// Thin cached wrapper around the Geode mod settings, so the rest of the mod
// does not have to know setting keys or call Mod::getSettingValue everywhere.
class ThornySettings {
public:
    static ThornySettings& get();

    // (Re)read every setting from Geode. Safe to call at any time.
    void applySettings();

    bool enabled() const { return m_enabled; }
    bool movementEnabled() const { return m_movement; }
    bool dialogueEnabled() const { return m_dialogue; }
    DialogueFrequency frequency() const { return m_frequency; }
    ThornyLanguage language() const { return m_language; }

    // Resolved language, taking "Auto" into account.
    bool useRussian() const;

    // Multiplier applied to the normal dialogue cooldown. Smaller means
    // Thorny talks more often.
    float frequencyMultiplier() const;

private:
    ThornySettings() = default;

    bool m_enabled = true;
    bool m_movement = true;
    bool m_dialogue = true;
    DialogueFrequency m_frequency = DialogueFrequency::Normal;
    ThornyLanguage m_language = ThornyLanguage::Auto;
};

} // namespace thorny
