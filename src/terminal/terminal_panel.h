#pragma once

#include "terminal_session.h"
#include "process/session_controller.h"
#include <vector>
#include <memory>
#include <unordered_map>

struct ImVec2;

namespace diana {

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
    void render_terminal_line(const TerminalCell* cells, int count);
    void render_screen_row(TerminalSession& session, int screen_row, float line_height);
    void render_cursor(TerminalSession& session, float target_x, float target_y, float char_w, float char_h, const ImVec2& content_start, float scroll_y);
    void render_banner();
    void handle_start_stop(TerminalSession& session);
    
    std::vector<std::unique_ptr<TerminalSession>> sessions_;
    std::vector<uint32_t> sessions_to_close_;
    uint32_t active_session_idx_ = 0;
    uint32_t next_session_id_ = 1;
    
    SessionController controller_;
    
    uint32_t renaming_session_id_ = 0;
    char rename_buffer_[128] = {};
    
    uint32_t confirm_start_session_id_ = 0;
};


}
