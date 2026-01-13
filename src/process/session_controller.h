#pragma once

#include "terminal/terminal_session.h"
#include "process/process_runner.h"
#include "core/event_queue.h"
#include "core/session_events.h"
#include <memory>
#include <unordered_map>

namespace diana {

class SessionController {
public:
    SessionController();
    ~SessionController();
    
    void start_session(TerminalSession& session);
    void stop_session(TerminalSession& session);
    void send_input(TerminalSession& session, const std::string& input);
    void send_raw_key(TerminalSession& session, const std::string& key);
    void send_key(TerminalSession& session, int vterm_key);
    void send_char(TerminalSession& session, uint32_t codepoint);
    void send_paste(TerminalSession& session, const std::string& text);
    void resize_pty(TerminalSession& session, int rows, int cols);
    
    void process_events();
    
    EventQueue<SessionEvent>& event_queue() { return event_queue_; }

private:
    ProcessConfig build_config(const TerminalSession& session);
    
    std::unordered_map<uint32_t, std::unique_ptr<ProcessRunner>> runners_;
    EventQueue<SessionEvent> event_queue_;
};

}
