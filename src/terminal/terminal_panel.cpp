#include "terminal_panel.h"
#include "core/session_events.h"
#include "ui/theme.h"
#include <imgui.h>
#include <nfd.h>
#include <algorithm>
#include <cstdlib>

namespace agent47 {

namespace {

const char* APP_NAMES[] = { "Claude Code", "Codex", "OpenCode" };
constexpr int APP_COUNT = 3;

uint32_t chars_to_utf8_codepoint(const uint32_t* chars) {
    if (chars[0] == 0) return ' ';
    return chars[0];
}

void utf8_encode(uint32_t codepoint, char* out, int* len) {
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
                    
                    if (ImGui::BeginTabItem(session->name().c_str(), &open, flags)) {
                        active_session_idx_ = static_cast<uint32_t>(i);
                        
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
                    session->request_scroll_to_bottom();
                }
            }
            else if constexpr (std::is_same_v<T, ExitEvent>) {
                if (auto* session = find_session(evt.session_id)) {
                    session->set_state(SessionState::Idle);
                    std::string msg = "\r\n[Process exited with code " + std::to_string(evt.exit_code) + "]\r\n";
                    session->write_to_terminal(msg.data(), msg.size());
                    session->request_scroll_to_bottom();
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
    std::string display_path = workdir.empty() ? "(not set)" : workdir;
    ImGui::InputText("##WorkDir", const_cast<char*>(display_path.c_str()), 
                     display_path.size() + 1, ImGuiInputTextFlags_ReadOnly);
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
        controller_.stop_session(session);
        std::string msg = "\r\n[Restarting...]\r\n";
        session.write_to_terminal(msg.data(), msg.size());
        session.set_state(SessionState::Starting);
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
    if (ImGui::BeginChild("OutputArea", ImVec2(0, -footer_height), true, ImGuiWindowFlags_HorizontalScrollbar)) {
        const auto& terminal = session.terminal();
        const auto& scrollback = terminal.scrollback();
        
        ImVec2 char_size = ImGui::CalcTextSize("W");
        float line_height = char_size.y;
        
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
                    std::vector<TerminalCell> row_cells;
                    row_cells.reserve(static_cast<size_t>(terminal.cols()));
                    
                    for (int col = 0; col < terminal.cols(); ++col) {
                        row_cells.push_back(terminal.get_cell(screen_row, col));
                    }
                    
                    render_terminal_line(row_cells.data(), static_cast<int>(row_cells.size()));
                    
                    auto cursor = terminal.get_cursor();
                    if (cursor.visible && cursor.row == screen_row) {
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
            }
        }
        
        if (session.scroll_to_bottom()) {
            ImGui::SetScrollHereY(1.0f);
            session.set_scroll_to_bottom(false);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
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
        
        uint32_t cp = chars_to_utf8_codepoint(cell.chars);
        char utf8[5] = {0};
        int len;
        utf8_encode(cp, utf8, &len);
        text.append(utf8, static_cast<size_t>(len));
    }
    
    flush_text();
    ImGui::NewLine();
}

void TerminalPanel::render_input_line(TerminalSession& session) {
    bool can_input = session.state() == SessionState::Running;
    
    if (can_input) {
        ImGuiIO& io = ImGui::GetIO();
        
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
                controller_.send_raw_key(session, "\r");
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                controller_.send_raw_key(session, "\x1b");
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
                controller_.send_raw_key(session, "\x7f");
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
                controller_.send_raw_key(session, "\t");
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
                controller_.send_raw_key(session, "\x1b[A");
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
                controller_.send_raw_key(session, "\x1b[B");
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
                controller_.send_raw_key(session, "\x1b[C");
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
                controller_.send_raw_key(session, "\x1b[D");
            }
            else if (io.InputQueueCharacters.Size > 0) {
                for (int i = 0; i < io.InputQueueCharacters.Size; ++i) {
                    ImWchar c = io.InputQueueCharacters[i];
                    if (c > 0) {
                        char utf8[5] = {0};
                        if (c < 0x80) {
                            utf8[0] = static_cast<char>(c);
                        } else if (c < 0x800) {
                            utf8[0] = static_cast<char>(0xC0 | (c >> 6));
                            utf8[1] = static_cast<char>(0x80 | (c & 0x3F));
                        } else {
                            utf8[0] = static_cast<char>(0xE0 | (c >> 12));
                            utf8[1] = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
                            utf8[2] = static_cast<char>(0x80 | (c & 0x3F));
                        }
                        controller_.send_raw_key(session, utf8);
                    }
                }
            }
        }
    }
    
    ImGui::TextDisabled("Press keys to interact with terminal (Enter, Esc, numbers, letters...)");
}

}
