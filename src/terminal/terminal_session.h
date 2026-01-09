#pragma once

#include "vterminal.h"
#include "core/types.h"
#include <string>
#include <cstdint>
#include <memory>

namespace agent47 {

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
    
    VTerminal& terminal() { return *terminal_; }
    const VTerminal& terminal() const { return *terminal_; }
    
    std::string& input_buffer() { return input_buffer_; }
    const std::string& input_buffer() const { return input_buffer_; }
    
    bool scroll_to_bottom() const { return scroll_to_bottom_; }
    void set_scroll_to_bottom(bool v) { scroll_to_bottom_ = v; }
    void request_scroll_to_bottom() { scroll_to_bottom_ = true; }
    
    bool user_scrolled_up() const { return user_scrolled_up_; }
    void set_user_scrolled_up(bool v) { user_scrolled_up_ = v; }
    
    int scroll_offset() const { return scroll_offset_; }
    void set_scroll_offset(int offset) { scroll_offset_ = offset; }
    
    bool pending_restart() const { return pending_restart_; }
    void set_pending_restart(bool v) { pending_restart_ = v; }
    
    void resize_terminal(int rows, int cols);
    void write_to_terminal(const char* data, size_t len);
    
    static const char* app_kind_name(AppKind kind);
    static const char* state_name(SessionState state);

private:
    uint32_t id_;
    std::string name_;
    SessionState state_ = SessionState::Idle;
    SessionConfig config_;
    std::unique_ptr<VTerminal> terminal_;
    std::string input_buffer_;
    bool scroll_to_bottom_ = true;
    bool user_scrolled_up_ = false;
    bool pending_restart_ = false;
    int scroll_offset_ = 0;
};

}
