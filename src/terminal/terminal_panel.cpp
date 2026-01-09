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
                    const auto& theme = get_current_theme();
                    uint32_t color = evt.is_stderr ? theme.error : theme.terminal_fg;
                    session->buffer().append_text(evt.data, color);
                    session->request_scroll_to_bottom();
                }
            }
            else if constexpr (std::is_same_v<T, ExitEvent>) {
                if (auto* session = find_session(evt.session_id)) {
                    session->set_state(SessionState::Idle);
                    std::string msg = "Process exited with code " + std::to_string(evt.exit_code);
                    session->buffer().append_line(msg, get_current_theme().warning);
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
        session.buffer().add_restart_marker("Restarting...");
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
    ImVec4 term_bg;
    term_bg.x = ((theme.terminal_bg >> 0) & 0xFF) / 255.0f;
    term_bg.y = ((theme.terminal_bg >> 8) & 0xFF) / 255.0f;
    term_bg.z = ((theme.terminal_bg >> 16) & 0xFF) / 255.0f;
    term_bg.w = 1.0f;
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, term_bg);
    if (ImGui::BeginChild("OutputArea", ImVec2(0, -footer_height), true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
        
        const auto& segments = session.buffer().segments();
        for (const auto& segment : segments) {
            for (const auto& line : segment.lines) {
                for (size_t i = 0; i < line.spans.size(); ++i) {
                    const auto& span = line.spans[i];
                    
                    ImVec4 color;
                    color.x = ((span.color >> 0) & 0xFF) / 255.0f;
                    color.y = ((span.color >> 8) & 0xFF) / 255.0f;
                    color.z = ((span.color >> 16) & 0xFF) / 255.0f;
                    color.w = ((span.color >> 24) & 0xFF) / 255.0f;
                    
                    if (i > 0) {
                        ImGui::SameLine(0.0f, 0.0f);
                    }
                    
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextUnformatted(span.text.c_str());
                    ImGui::PopStyleColor();
                }
            }
        }
        
        ImGui::PopStyleVar();
        
        if (session.scroll_to_bottom()) {
            ImGui::SetScrollHereY(1.0f);
            session.set_scroll_to_bottom(false);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
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
