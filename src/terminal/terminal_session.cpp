#include "terminal_session.h"

namespace agent47 {

TerminalSession::TerminalSession(uint32_t id)
    : id_(id)
    , name_("Session " + std::to_string(id))
{
}

const char* TerminalSession::app_kind_name(AppKind kind) {
    switch (kind) {
        case AppKind::ClaudeCode: return "Claude Code";
        case AppKind::Codex:      return "Codex";
        case AppKind::OpenCode:   return "OpenCode";
    }
    return "Unknown";
}

const char* TerminalSession::state_name(SessionState state) {
    switch (state) {
        case SessionState::Idle:     return "Idle";
        case SessionState::Starting: return "Starting";
        case SessionState::Running:  return "Running";
        case SessionState::Stopping: return "Stopping";
    }
    return "Unknown";
}

}
