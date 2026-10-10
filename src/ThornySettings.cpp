#include "ThornySettings.hpp"

#include <Geode/Geode.hpp>

namespace thorny {

namespace {

DialogueFrequency parseFrequency(std::string const& value) {
    if (value == "Rare") {
        return DialogueFrequency::Rare;
    }
    if (value == "Frequent") {
        return DialogueFrequency::Frequent;
    }
    return DialogueFrequency::Normal;
}

ThornyLanguage parseLanguage(std::string const& value) {
    if (value == "English") {
        return ThornyLanguage::English;
    }
    if (value == "Russian") {
        return ThornyLanguage::Russian;
    }
    return ThornyLanguage::Auto;
}

} // namespace

ThornySettings& ThornySettings::get() {
    static ThornySettings instance;
    return instance;
}

void ThornySettings::applySettings() {
    auto mod = geode::Mod::get();
    if (!mod) {
        return;
    }

    m_enabled = mod->getSettingValue<bool>("enabled");
    m_movement = mod->getSettingValue<bool>("movement");
    m_dialogue = mod->getSettingValue<bool>("dialogue");
    m_frequency = parseFrequency(mod->getSettingValue<std::string>("dialogue-frequency"));
    m_language = parseLanguage(mod->getSettingValue<std::string>("language"));
    m_size = mod->getSettingValue<double>("size");
    if (m_size < 0.25f) {
        m_size = 0.25f;
    } else if (m_size > 2.0f) {
        m_size = 2.0f;
    }
}

bool ThornySettings::useRussian() const {
    switch (m_language) {
        case ThornyLanguage::Russian: return true;
        case ThornyLanguage::English: return false;
        case ThornyLanguage::Auto:
        default: {
            // Auto: follow the platform language. GD's own localisation is not
            // exposed through the bindings, so we use the cocos2d application's
            // current language, which matches the device locale.
            if (auto* app = cocos2d::CCApplication::get()) {
                return app->getCurrentLanguage() == cocos2d::kLanguageRussian;
            }
            return false;
        }
    }
}

float ThornySettings::frequencyMultiplier() const {
    switch (m_frequency) {
        case DialogueFrequency::Rare: return 2.0f;
        case DialogueFrequency::Frequent: return 0.5f;
        case DialogueFrequency::Normal:
        default: return 1.0f;
    }
}

float ThornySettings::sizeScale() const {
    return m_size;
}

} // namespace thorny
