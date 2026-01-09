#pragma once

#include "terminal_session.h"
#include "process/session_controller.h"
#include <vector>
#include <memory>

namespace agent47 {

class TerminalPanel {
public:
    TerminalPanel();
    
    void render();
    void process_events();
    
    TerminalSession* active_session();
    TerminalSession* find_session(uint32_t id);
    const std::vector<std::unique_ptr<TerminalSession>>& sessions() const { return sessions_; }
    
    uint32_t create_session();
    void close_session(uint32_t id);
    
    SessionController& controller() { return controller_; }

private:
    void render_control_bar(TerminalSession& session);
    void render_output_area(TerminalSession& session);
    void render_input_line(TerminalSession& session);
    void handle_start_stop(TerminalSession& session);
    
    std::vector<std::unique_ptr<TerminalSession>> sessions_;
    uint32_t active_session_idx_ = 0;
    uint32_t next_session_id_ = 1;
    
    SessionController controller_;
};

}
