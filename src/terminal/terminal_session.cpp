#include "terminal_session.h"

namespace diana {

namespace {
constexpr int DEFAULT_ROWS = 24;
constexpr int DEFAULT_COLS = 80;
}

TerminalSession::TerminalSession(uint32_t id)
    : id_(id)
    , name_("Session " + std::to_string(id))
    , terminal_(std::make_unique<VTerminal>(DEFAULT_ROWS, DEFAULT_COLS))
{
}

void TerminalSession::resize_terminal(int rows, int cols) {
    terminal_->resize(rows, cols);
}

void TerminalSession::write_to_terminal(const char* data, size_t len) {
    terminal_->write(data, len);
}

const char* TerminalSession::app_kind_name(AppKind kind) {
    switch (kind) {
        case AppKind::ClaudeCode: return "Claude Code";
        case AppKind::Codex:      return "Codex";
        case AppKind::OpenCode:   return "OpenCode";
        case AppKind::Shell:      return "Shell";
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
