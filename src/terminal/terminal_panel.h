#pragma once

#include "terminal_session.h"
#include "process/session_controller.h"
#include "adapters/session_config_store.h"
#include <vector>
#include <memory>
#include <unordered_map>

struct ImVec2;

namespace diana {

struct CursorAnimation {
    float current_x = 0.0f;
    float current_y = 0.0f;
    float target_x = 0.0f;
    float target_y = 0.0f;
    float velocity_x = 0.0f;
    float velocity_y = 0.0f;
    
    float current_width = 1.0f;
    float target_width = 1.0f;
    
    static constexpr int TRAIL_LENGTH = 8;
    float trail_x[TRAIL_LENGTH] = {};
    float trail_y[TRAIL_LENGTH] = {};
    float trail_alpha[TRAIL_LENGTH] = {};
    int trail_head = 0;
    float trail_timer = 0.0f;
    
    bool initialized = false;
};

class TerminalPanel {
public:
    TerminalPanel();
    
    void render();
    void process_events();
    
    void save_sessions();
    void load_sessions();
    
    TerminalSession* active_session();
    TerminalSession* find_session(uint32_t id);
    const std::vector<std::unique_ptr<TerminalSession>>& sessions() const { return sessions_; }
    
    uint32_t create_session();
    void close_session(uint32_t id);
    
    SessionController& controller() { return controller_; }

private:
    struct Selection {
        bool active = false;
        bool dragging = false;
        int start_row = 0;
        int start_col = 0;
        int end_row = 0;
        int end_col = 0;
    };

    void render_control_bar(TerminalSession& session);
    void render_output_area(TerminalSession& session);
    void render_input_line(TerminalSession& session);
    void render_terminal_line(const TerminalCell* cells, int count, float line_height, int line_idx, const Selection& selection);
    void render_screen_row(TerminalSession& session, int screen_row, float line_height, int line_idx, const Selection& selection);
    void render_cursor(TerminalSession& session, float target_x, float target_y, float char_w, float char_h, int cell_width);
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
    
    std::unordered_map<uint32_t, CursorAnimation> cursor_animations_;
    std::unordered_map<uint32_t, std::string> cpr_buffers_;
    std::unordered_map<uint32_t, Selection> selections_;
    
    SessionConfigStore config_store_;
};

}
