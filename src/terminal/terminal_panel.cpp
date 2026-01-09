#include "terminal_panel.h"
#include "core/session_events.h"
#include <imgui.h>
#include <nfd.h>
#include <algorithm>
#include <cstdlib>

namespace agent47 {

namespace {

constexpr uint32_t COLOR_OUTPUT_STDOUT = 0xFFDCDCDC;
constexpr uint32_t COLOR_OUTPUT_STDERR = 0xFF6464FF;
constexpr uint32_t COLOR_INPUT_ECHO = 0xFF88FF88;
constexpr uint32_t COLOR_SYSTEM_MSG = 0xFFFFFF88;

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
                    uint32_t color = evt.is_stderr ? COLOR_OUTPUT_STDERR : COLOR_OUTPUT_STDOUT;
                    session->buffer().append_text(evt.data, color);
                    session->request_scroll_to_bottom();
                }
            }
            else if constexpr (std::is_same_v<T, ExitEvent>) {
                if (auto* session = find_session(evt.session_id)) {
                    session->set_state(SessionState::Idle);
                    std::string msg = "Process exited with code " + std::to_string(evt.exit_code);
                    session->buffer().append_line(msg, COLOR_SYSTEM_MSG);
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
    
    ImGui::PushItemWidth(150);
    char provider_buf[64];
    strncpy(provider_buf, session.config().provider.c_str(), sizeof(provider_buf) - 1);
    provider_buf[sizeof(provider_buf) - 1] = '\0';
    
    if (!can_change) ImGui::BeginDisabled();
    if (ImGui::InputTextWithHint("##Provider", "Provider", provider_buf, sizeof(provider_buf))) {
        session.config().provider = provider_buf;
    }
    if (!can_change) ImGui::EndDisabled();
    ImGui::PopItemWidth();
    
    ImGui::SameLine();
    
    ImGui::PushItemWidth(200);
    char model_buf[128];
    strncpy(model_buf, session.config().model.c_str(), sizeof(model_buf) - 1);
    model_buf[sizeof(model_buf) - 1] = '\0';
    
    if (!can_change) ImGui::BeginDisabled();
    if (ImGui::InputTextWithHint("##Model", "Model", model_buf, sizeof(model_buf))) {
        session.config().model = model_buf;
    }
    if (!can_change) ImGui::EndDisabled();
    ImGui::PopItemWidth();
    
    ImGui::SameLine();
    
    ImGui::PushItemWidth(200);
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
    
    if (ImGui::BeginChild("OutputArea", ImVec2(0, -footer_height), true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
        
        const auto& segments = session.buffer().segments();
        for (const auto& segment : segments) {
            for (const auto& line : segment.lines) {
                ImVec4 color;
                color.x = ((line.color >> 0) & 0xFF) / 255.0f;
                color.y = ((line.color >> 8) & 0xFF) / 255.0f;
                color.z = ((line.color >> 16) & 0xFF) / 255.0f;
                color.w = ((line.color >> 24) & 0xFF) / 255.0f;
                
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::TextUnformatted(line.text.c_str());
                ImGui::PopStyleColor();
            }
        }
        
        ImGui::PopStyleVar();
        
        if (session.scroll_to_bottom()) {
            ImGui::SetScrollHereY(1.0f);
            session.set_scroll_to_bottom(false);
        }
    }
    ImGui::EndChild();
}

void TerminalPanel::render_input_line(TerminalSession& session) {
    ImGui::PushItemWidth(-1);
    
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
    
    bool can_input = session.state() == SessionState::Running;
    
    static char input_buf[4096] = "";
    
    if (!can_input) ImGui::BeginDisabled();
    bool send = ImGui::InputText("##Input", input_buf, sizeof(input_buf), flags);
    if (!can_input) ImGui::EndDisabled();
    
    if (send && input_buf[0] != '\0' && can_input) {
        session.buffer().append_line(std::string("> ") + input_buf, COLOR_INPUT_ECHO);
        controller_.send_input(session, input_buf);
        session.request_scroll_to_bottom();
        input_buf[0] = '\0';
    }
    
    ImGui::PopItemWidth();
}

}
