#include "ThornyEvents.hpp"

namespace thorny {

char const* toString(ThornyEvent event) {
    switch (event) {
        case ThornyEvent::None: return "None";
        case ThornyEvent::FirstLaunch: return "FirstLaunch";
        case ThornyEvent::EnteredMainMenu: return "EnteredMainMenu";
        case ThornyEvent::StartedLevel: return "StartedLevel";
        case ThornyEvent::FailedLevel: return "FailedLevel";
        case ThornyEvent::CompletedLevel: return "CompletedLevel";
        case ThornyEvent::ExitedLevel: return "ExitedLevel";
        case ThornyEvent::OpenedEditor: return "OpenedEditor";
        case ThornyEvent::ExitedEditor: return "ExitedEditor";
        case ThornyEvent::EditedLevel: return "EditedLevel";
        case ThornyEvent::ReturnedToMenu: return "ReturnedToMenu";
        case ThornyEvent::StayedIdle: return "StayedIdle";
        case ThornyEvent::FellAsleep: return "FellAsleep";
        default: return "Unknown";
    }
}

} // namespace thorny
