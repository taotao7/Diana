#pragma once

#include "terminal_buffer.h"
#include <string>
#include <cstdint>

namespace agent47 {

enum class AppKind {
    ClaudeCode,
    Codex,
    OpenCode
};

enum class SessionState {
    Idle,
    Starting,
    Running,
    Stopping
};

struct SessionConfig {
    AppKind app = AppKind::ClaudeCode;
    std::string provider;
    std::string model;
    std::string working_dir;
};

class TerminalSession {
public:
    explicit TerminalSession(uint32_t id);
    
    uint32_t id() const { return id_; }
    const std::string& name() const { return name_; }
    void set_name(const std::string& name) { name_ = name; }
    
    SessionState state() const { return state_; }
    void set_state(SessionState state) { state_ = state; }
    
    const SessionConfig& config() const { return config_; }
    SessionConfig& config() { return config_; }
    
    TerminalBuffer& buffer() { return buffer_; }
    const TerminalBuffer& buffer() const { return buffer_; }
    
    std::string& input_buffer() { return input_buffer_; }
    const std::string& input_buffer() const { return input_buffer_; }
    
    bool scroll_to_bottom() const { return scroll_to_bottom_; }
    void set_scroll_to_bottom(bool v) { scroll_to_bottom_ = v; }
    void request_scroll_to_bottom() { scroll_to_bottom_ = true; }
    
    static const char* app_kind_name(AppKind kind);
    static const char* state_name(SessionState state);

private:
    uint32_t id_;
    std::string name_;
    SessionState state_ = SessionState::Idle;
    SessionConfig config_;
    TerminalBuffer buffer_;
    std::string input_buffer_;
    bool scroll_to_bottom_ = true;
};

}
