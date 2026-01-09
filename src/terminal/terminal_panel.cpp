#include "terminal_panel.h"
#include "vterminal.h"
#include "core/session_events.h"
#include "ui/theme.h"
#include <imgui.h>
#include <nfd.h>
#include <algorithm>
#include <cstdlib>

extern "C" {
#include <vterm_keycodes.h>
}

namespace agent47 {

namespace {

const char* APP_NAMES[] = { "Claude Code", "Codex", "OpenCode" };
constexpr int APP_COUNT = 3;

void utf8_encode(uint32_t codepoint, char* out, int* len) {
    if (codepoint > 0x10FFFF) {
        out[0] = '?';
        *len = 1;
        return;
    }
    if (codepoint < 0x80) {
        out[0] = static_cast<char>(codepoint);
        *len = 1;
    } else if (codepoint < 0x800) {
        out[0] = static_cast<char>(0xC0 | (codepoint >> 6));
        out[1] = static_cast<char>(0x80 | (codepoint & 0x3F));
        *len = 2;
    } else if (codepoint < 0x10000) {
        out[0] = static_cast<char>(0xE0 | (codepoint >> 12));
        out[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
        *len = 3;
    } else {
        out[0] = static_cast<char>(0xF0 | (codepoint >> 18));
        out[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        out[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
        *len = 4;
    }
}

void cell_to_utf8(const uint32_t* chars, std::string& out) {
    if (chars[0] == 0 || chars[0] > 0x10FFFF) {
        out += ' ';
        return;
    }
    for (int i = 0; i < TERMINAL_MAX_CHARS_PER_CELL && chars[i] != 0; ++i) {
        uint32_t cp = chars[i];
        if (cp > 0x10FFFF) {
            break;
        }
        char utf8[5] = {0};
        int len;
        utf8_encode(cp, utf8, &len);
        out.append(utf8, static_cast<size_t>(len));
    }
}

ImVec4 u32_to_imvec4(uint32_t color) {
    return ImVec4(
        ((color >> 0) & 0xFF) / 255.0f,
        ((color >> 8) & 0xFF) / 255.0f,
        ((color >> 16) & 0xFF) / 255.0f,
        ((color >> 24) & 0xFF) / 255.0f
    );
}

}

TerminalPanel::TerminalPanel() {
    create_session();
}

void TerminalPanel::render() {
    process_events();
    
    for (uint32_t id : sessions_to_close_) {
        close_session(id);
    }
    sessions_to_close_.clear();
    
    ImGui::SetNextWindowSizeConstraints(ImVec2(400, 300), ImVec2(FLT_MAX, FLT_MAX));
    if (ImGui::Begin("Terminal")) {
        if (sessions_.empty()) {
            ImGui::TextDisabled("No sessions. Press + to create one.");
            if (ImGui::Button("+##CreateSession")) {
                create_session();
            }
        } else {
            if (ImGui::BeginTabBar("SessionTabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable)) {
                for (size_t i = 0; i < sessions_.size(); ++i) {
                    auto& session = sessions_[i];
                    bool open = true;
                    
                    ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
                    if (session->state() == SessionState::Running) {
                        flags |= ImGuiTabItemFlags_UnsavedDocument;
                    }
                    
                    bool is_renaming = (renaming_session_id_ == session->id());
                    
                    if (is_renaming) {
                        ImGui::SetNextItemWidth(100);
                        if (ImGui::InputText("##RenameTab", rename_buffer_, sizeof(rename_buffer_), 
                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                            if (rename_buffer_[0] != '\0') {
                                session->set_name(rename_buffer_);
                            }
                            renaming_session_id_ = 0;
                        }
                        if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0)) {
                            renaming_session_id_ = 0;
                        }
                        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                            renaming_session_id_ = 0;
                        }
                        ImGui::SameLine();
                    }
                    
                    if (ImGui::BeginTabItem(session->name().c_str(), &open, flags)) {
                        active_session_idx_ = static_cast<uint32_t>(i);
                        
                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                            renaming_session_id_ = session->id();
                            std::strncpy(rename_buffer_, session->name().c_str(), sizeof(rename_buffer_) - 1);
                            rename_buffer_[sizeof(rename_buffer_) - 1] = '\0';
                        }
                        
                        render_control_bar(*session);
                        handle_start_stop(*session);
                        ImGui::Separator();
                        render_output_area(*session);
                        render_input_line(*session);
                        
                        ImGui::EndTabItem();
                    }
                    
                    if (!open) {
                        if (session->state() == SessionState::Running) {
                            controller_.stop_session(*session);
                        }
                        close_session(session->id());
                        break;
                    }
                }
                
                if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) {
                    create_session();
                }
                
                ImGui::EndTabBar();
            }
        }
    }
    ImGui::End();
}

void TerminalPanel::process_events() {
    while (auto event_opt = controller_.event_queue().try_pop()) {
        std::visit([this](auto&& evt) {
            using T = std::decay_t<decltype(evt)>;
            
            if constexpr (std::is_same_v<T, OutputEvent>) {
                if (auto* session = find_session(evt.session_id)) {
                    session->write_to_terminal(evt.data.data(), evt.data.size());
                    if (!session->user_scrolled_up()) {
                        session->request_scroll_to_bottom();
                    }
                }
            }
            else if constexpr (std::is_same_v<T, ExitEvent>) {
                if (auto* session = find_session(evt.session_id)) {
                    std::string msg = "\r\n[Process exited with code " + std::to_string(evt.exit_code) + "]\r\n";
                    session->write_to_terminal(msg.data(), msg.size());
                    session->request_scroll_to_bottom();
                    
                    if (session->pending_restart()) {
                        session->set_pending_restart(false);
                        session->set_state(SessionState::Starting);
                    } else {
                        session->set_state(SessionState::Idle);
                    }
                }
            }
        }, *event_opt);
    }
}

TerminalSession* TerminalPanel::active_session() {
    if (sessions_.empty() || active_session_idx_ >= sessions_.size()) {
        return nullptr;
    }
    return sessions_[active_session_idx_].get();
}

TerminalSession* TerminalPanel::find_session(uint32_t id) {
    for (auto& s : sessions_) {
        if (s->id() == id) return s.get();
    }
    return nullptr;
}

uint32_t TerminalPanel::create_session() {
    uint32_t id = next_session_id_++;
    sessions_.push_back(std::make_unique<TerminalSession>(id));
    return id;
}

void TerminalPanel::close_session(uint32_t id) {
    auto it = std::find_if(sessions_.begin(), sessions_.end(),
        [id](const auto& s) { return s->id() == id; });
    
    if (it != sessions_.end()) {
        sessions_.erase(it);
        
        if (active_session_idx_ >= sessions_.size() && !sessions_.empty()) {
            active_session_idx_ = static_cast<uint32_t>(sessions_.size() - 1);
        }
    }
}

void TerminalPanel::render_control_bar(TerminalSession& session) {
    ImGui::PushItemWidth(120);
    
    int app_idx = static_cast<int>(session.config().app);
    bool can_change = session.state() == SessionState::Idle;
    
    if (!can_change) ImGui::BeginDisabled();
    if (ImGui::Combo("##App", &app_idx, APP_NAMES, APP_COUNT)) {
        session.config().app = static_cast<AppKind>(app_idx);
    }
    if (!can_change) ImGui::EndDisabled();
    
    ImGui::SameLine();
    
    ImGui::PushItemWidth(250);
    const std::string& workdir = session.config().working_dir;
    char workdir_buf[512];
    std::strncpy(workdir_buf, workdir.c_str(), sizeof(workdir_buf) - 1);
    workdir_buf[sizeof(workdir_buf) - 1] = '\0';
    ImGui::InputTextWithHint("##WorkDir", "Select agent execution path (working directory)", 
                             workdir_buf, sizeof(workdir_buf), ImGuiInputTextFlags_ReadOnly);
    ImGui::PopItemWidth();
    
    ImGui::SameLine();
    
    if (!can_change) ImGui::BeginDisabled();
    if (ImGui::Button("Browse...##WorkDir")) {
        nfdchar_t* out_path = nullptr;
        const char* default_path = workdir.empty() ? nullptr : workdir.c_str();
        nfdresult_t result = NFD_PickFolder(&out_path, default_path);
        if (result == NFD_OKAY && out_path) {
            session.config().working_dir = out_path;
            NFD_FreePath(out_path);
        }
    }
    if (!can_change) ImGui::EndDisabled();
    
    ImGui::SameLine();
    
    bool is_running = session.state() == SessionState::Running;
    bool is_starting = session.state() == SessionState::Starting;
    bool is_stopping = session.state() == SessionState::Stopping;
    
    if (is_starting || is_stopping) ImGui::BeginDisabled();
    
    if (is_running) {
        if (ImGui::Button("Stop")) {
            controller_.stop_session(session);
        }
    } else {
        if (ImGui::Button("Start")) {
            controller_.start_session(session);
        }
    }
    
    if (is_starting || is_stopping) ImGui::EndDisabled();
    
    ImGui::SameLine();
    
    if (is_starting || is_stopping || !is_running) ImGui::BeginDisabled();
    if (ImGui::Button("Restart")) {
        session.set_pending_restart(true);
        controller_.stop_session(session);
        std::string msg = "\r\n[Restarting...]\r\n";
        session.write_to_terminal(msg.data(), msg.size());
    }
    if (is_starting || is_stopping || !is_running) ImGui::EndDisabled();
    
    ImGui::SameLine();
    ImGui::TextDisabled("| %s", TerminalSession::state_name(session.state()));
    
    ImGui::PopItemWidth();
}

void TerminalPanel::handle_start_stop(TerminalSession& session) {
    if (session.state() == SessionState::Starting) {
        controller_.start_session(session);
    }
}

void TerminalPanel::render_output_area(TerminalSession& session) {
    float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    
    const auto& theme = get_current_theme();
    ImVec4 term_bg = u32_to_imvec4(theme.terminal_bg);
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, term_bg);
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs;
    if (ImGui::BeginChild("OutputArea", ImVec2(0, -footer_height), true, flags)) {
        ImVec2 char_size = ImGui::CalcTextSize("W");
        ImVec2 content_size = ImGui::GetContentRegionAvail();
        
        int new_cols = std::max(1, static_cast<int>(content_size.x / char_size.x));
        int new_rows = std::max(1, static_cast<int>(content_size.y / char_size.y));
        
        const auto& terminal = session.terminal();
        if (new_cols != terminal.cols() || new_rows != terminal.rows()) {
            controller_.resize_pty(session, new_rows, new_cols);
        }
        
        const auto& scrollback = terminal.scrollback();
        bool has_scrollback = !scrollback.empty();
        
        if (has_scrollback) {
            int total_lines = static_cast<int>(scrollback.size()) + terminal.rows();
            
            ImGuiListClipper clipper;
            clipper.Begin(total_lines);
            
            while (clipper.Step()) {
                for (int line_idx = clipper.DisplayStart; line_idx < clipper.DisplayEnd; ++line_idx) {
                    bool is_scrollback = line_idx < static_cast<int>(scrollback.size());
                    
                    if (is_scrollback) {
                        const auto& line = scrollback[static_cast<size_t>(line_idx)];
                        render_terminal_line(line.data(), static_cast<int>(line.size()));
                    } else {
                        int screen_row = line_idx - static_cast<int>(scrollback.size());
                        render_screen_row(session, screen_row, char_size.y);
                    }
                }
            }
            
            float scroll_y = ImGui::GetScrollY();
            float scroll_max_y = ImGui::GetScrollMaxY();
            bool at_bottom = (scroll_max_y <= 0.0f) || (scroll_y >= scroll_max_y - 1.0f);
            
            if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0.0f) {
                session.set_user_scrolled_up(!at_bottom);
            }
            
            if (at_bottom) {
                session.set_user_scrolled_up(false);
            }
            
            if (session.scroll_to_bottom()) {
                ImGui::SetScrollHereY(1.0f);
                session.set_scroll_to_bottom(false);
            }
        } else {
            for (int row = 0; row < terminal.rows(); ++row) {
                render_screen_row(session, row, char_size.y);
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void TerminalPanel::render_screen_row(TerminalSession& session, int screen_row, float line_height) {
    const auto& terminal = session.terminal();
    
    std::vector<TerminalCell> row_cells;
    row_cells.reserve(static_cast<size_t>(terminal.cols()));
    
    for (int col = 0; col < terminal.cols(); ++col) {
        row_cells.push_back(terminal.get_cell(screen_row, col));
    }
    
    render_terminal_line(row_cells.data(), static_cast<int>(row_cells.size()));
    
    auto cursor = terminal.get_cursor();
    if (cursor.visible && cursor.row == screen_row) {
        ImVec2 char_size = ImGui::CalcTextSize("W");
        ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
        cursor_pos.x = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMin().x + cursor.col * char_size.x;
        cursor_pos.y = ImGui::GetCursorScreenPos().y - line_height;
        
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(
            cursor_pos,
            ImVec2(cursor_pos.x + char_size.x, cursor_pos.y + line_height),
            IM_COL32(200, 200, 200, 128)
        );
    }
}

void TerminalPanel::render_terminal_line(const TerminalCell* cells, int count) {
    if (count <= 0) {
        ImGui::NewLine();
        return;
    }
    
    std::string text;
    text.reserve(static_cast<size_t>(count) * 4);
    uint32_t current_fg = cells[0].fg;
    
    auto flush_text = [&]() {
        if (!text.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, u32_to_imvec4(current_fg));
            ImGui::TextUnformatted(text.c_str(), text.c_str() + text.size());
            ImGui::PopStyleColor();
            ImGui::SameLine(0.0f, 0.0f);
            text.clear();
        }
    };
    
    for (int i = 0; i < count; ++i) {
        const auto& cell = cells[i];
        
        if (cell.width == 0) continue;
        
        if (cell.fg != current_fg) {
            flush_text();
            current_fg = cell.fg;
        }
        
        cell_to_utf8(cell.chars, text);
    }
    
    flush_text();
    ImGui::NewLine();
}

void TerminalPanel::render_input_line(TerminalSession& session) {
    bool can_input = session.state() == SessionState::Running;
    
    if (can_input) {
        ImGuiIO& io = ImGui::GetIO();
        
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
            bool has_ime_input = false;
            for (int i = 0; i < io.InputQueueCharacters.Size; ++i) {
                if (io.InputQueueCharacters[i] >= 0x80) {
                    has_ime_input = true;
                    break;
                }
            }
            
            if (!has_ime_input) {
                if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
                    if (io.KeyShift) {
                        controller_.send_char(session, '\n');
                    } else {
                        controller_.send_key(session, VTERM_KEY_ENTER);
                    }
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    controller_.send_key(session, VTERM_KEY_ESCAPE);
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
                    controller_.send_key(session, VTERM_KEY_BACKSPACE);
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
                    controller_.send_key(session, VTERM_KEY_TAB);
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                    controller_.send_key(session, VTERM_KEY_UP);
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
                    controller_.send_key(session, VTERM_KEY_DOWN);
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
                    controller_.send_key(session, VTERM_KEY_RIGHT);
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
                    controller_.send_key(session, VTERM_KEY_LEFT);
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                    controller_.send_key(session, VTERM_KEY_DEL);
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
                    controller_.send_key(session, VTERM_KEY_HOME);
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_End)) {
                    controller_.send_key(session, VTERM_KEY_END);
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) {
                    controller_.send_key(session, VTERM_KEY_PAGEUP);
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) {
                    controller_.send_key(session, VTERM_KEY_PAGEDOWN);
                }
            }
            
            for (int i = 0; i < io.InputQueueCharacters.Size; ++i) {
                ImWchar c = io.InputQueueCharacters[i];
                if (c > 0 && c != 127) {
                    controller_.send_char(session, static_cast<uint32_t>(c));
                }
            }
        }
    }
    
    ImGui::TextDisabled("Press keys to interact with terminal (Enter, Esc, arrows, letters...)");
}

}
