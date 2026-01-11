#include "terminal_panel.h"
#include "vterminal.h"
#include "core/session_events.h"
#include "ui/theme.h"
#include <imgui.h>
#include <nfd.h>
#include <algorithm>
#include <cstdlib>
#include <cmath>

extern "C" {
#include <vterm_keycodes.h>
}

extern "C" bool diana_is_ctrl_pressed();

namespace diana {

namespace {

const char* APP_NAMES[] = { "Claude Code", "Codex", "OpenCode", "Shell" };
constexpr int APP_COUNT = 4;

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
        
        if (confirm_start_session_id_ != 0) {
            ImGui::OpenPopup("Confirm Start##HomeDir");
        }
        
        if (ImGui::BeginPopupModal("Confirm Start##HomeDir", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Warning: No project directory selected!");
            ImGui::Spacing();
            ImGui::TextWrapped("The agent will run in your home directory. This is not recommended because:");
            ImGui::BulletText("Token metrics will be mixed with other projects");
            ImGui::BulletText("Multiple agents may conflict on the same directory");
            ImGui::Spacing();
            ImGui::TextWrapped("Consider selecting a specific project directory first.");
            ImGui::Spacing();
            
            if (ImGui::Button("Select Directory", ImVec2(140, 0))) {
                if (auto* session = find_session(confirm_start_session_id_)) {
                    nfdchar_t* out_path = nullptr;
                    nfdresult_t result = NFD_PickFolder(&out_path, nullptr);
                    if (result == NFD_OKAY && out_path) {
                        session->config().working_dir = out_path;
                        NFD_FreePath(out_path);
                        controller_.start_session(*session);
                        confirm_start_session_id_ = 0;
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Start Anyway", ImVec2(120, 0))) {
                if (auto* session = find_session(confirm_start_session_id_)) {
                    controller_.start_session(*session);
                }
                confirm_start_session_id_ = 0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0))) {
                confirm_start_session_id_ = 0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
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
            if (session.config().working_dir.empty()) {
                confirm_start_session_id_ = session.id();
            } else {
                controller_.start_session(session);
            }
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

void TerminalPanel::render_banner() {
    const auto& theme = get_current_theme();
    ImVec4 accent = u32_to_imvec4(theme.accent);
    ImVec4 dim = u32_to_imvec4(theme.foreground_dim);
    
    const char* banner_lines[] = {
        "DDDDDDD    IIII    AAA    NN    NN    AAA   ",
        "DD    DD    II    AA AA   NNN   NN   AA AA  ",
        "DD    DD    II   AA   AA  NNNN  NN  AA   AA ",
        "DD    DD    II  AAAAAAAAA NN NN NN AAAAAAAAA",
        "DD    DD    II  AA     AA NN  NNNN AA     AA",
        "DDDDDDD    IIII AA     AA NN   NNN AA     AA"
    };
    const char* tagline = "Your Agent's Best Handler";
    const char* hint = "Select an agent and click Start to begin.";
    
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float line_height = ImGui::GetTextLineHeightWithSpacing();
    float total_height = line_height * 10;
    
    float banner_width = ImGui::CalcTextSize(banner_lines[0]).x;
    float tagline_width = ImGui::CalcTextSize(tagline).x;
    float hint_width = ImGui::CalcTextSize(hint).x;
    
    float top_padding = (avail.y - total_height) * 0.5f;
    if (top_padding > 0.0f) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + top_padding);
    }
    
    float banner_x = (avail.x - banner_width) * 0.5f;
    float tagline_x = (avail.x - tagline_width) * 0.5f;
    float hint_x = (avail.x - hint_width) * 0.5f;
    
    ImGui::PushStyleColor(ImGuiCol_Text, accent);
    for (const char* line : banner_lines) {
        ImGui::SetCursorPosX(banner_x);
        ImGui::TextUnformatted(line);
    }
    ImGui::PopStyleColor();
    
    ImGui::NewLine();
    ImGui::PushStyleColor(ImGuiCol_Text, dim);
    ImGui::SetCursorPosX(tagline_x);
    ImGui::TextUnformatted(tagline);
    ImGui::NewLine();
    ImGui::SetCursorPosX(hint_x);
    ImGui::TextUnformatted(hint);
    ImGui::PopStyleColor();
}

void TerminalPanel::render_output_area(TerminalSession& session) {
    const auto& theme = get_current_theme();
    ImVec4 term_bg = u32_to_imvec4(theme.terminal_bg);
    
    ImGui::PushStyleColor(ImGuiCol_ChildBg, term_bg);
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoNavInputs;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    bool child_open = ImGui::BeginChild("OutputArea", ImVec2(0, 0), true, flags);
    ImGui::PopStyleVar();
    if (child_open) {
        ImVec2 content_size = ImGui::GetContentRegionAvail();
        ImVec2 char_size = ImGui::CalcTextSize("W");
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
        float line_height = ImGui::GetTextLineHeight();
        
        int new_cols = std::max(1, static_cast<int>(content_size.x / char_size.x));
        int new_rows = std::max(1, static_cast<int>(std::ceil(content_size.y / line_height)));
        
        const auto& terminal = session.terminal();
        if (new_cols != terminal.cols() || new_rows != terminal.rows()) {
            controller_.resize_pty(session, new_rows, new_cols);
        }
        
        ImGui::PopStyleVar();
        
        if (session.state() == SessionState::Idle && session.terminal().scrollback().empty()) {
            render_banner();
            ImGui::Dummy(ImVec2(1.0f, ImGui::GetContentRegionAvail().y));
        } else {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
                
                ImVec2 content_start = ImGui::GetCursorScreenPos();
                float scroll_y = ImGui::GetScrollY();
                
                if (session.config().app == AppKind::Shell) {
                    auto cursor = terminal.get_cursor();
                    const auto& sb = terminal.scrollback();
                    
                    float target_x = content_start.x + cursor.col * char_size.x;
                    float target_y;
                    
                    if (!sb.empty()) {
                        int cursor_line_idx = static_cast<int>(sb.size()) + cursor.row;
                        target_y = content_start.y + cursor_line_idx * line_height - scroll_y;
                        
                        ImVec2 clip_min = ImGui::GetWindowPos();
                        ImVec2 clip_max = ImVec2(clip_min.x + ImGui::GetWindowSize().x, clip_min.y + ImGui::GetWindowSize().y);
                        if (target_y >= clip_min.y - line_height && target_y <= clip_max.y) {
                            render_cursor(session, target_x, target_y, char_size.x, line_height);
                        }
                    } else {
                        target_y = content_start.y + cursor.row * line_height;
                        render_cursor(session, target_x, target_y, char_size.x, line_height);
                    }
                }
                
                const auto& scrollback = terminal.scrollback();
                bool has_scrollback = !scrollback.empty();
                
                if (has_scrollback) {
                    int total_lines = static_cast<int>(scrollback.size()) + terminal.rows();
                    
                    ImGuiListClipper clipper;
                    clipper.Begin(total_lines, line_height);
                    
                    while (clipper.Step()) {
                        for (int line_idx = clipper.DisplayStart; line_idx < clipper.DisplayEnd; ++line_idx) {
                            bool is_scrollback = line_idx < static_cast<int>(scrollback.size());
                            
                            if (is_scrollback) {
                                const auto& line = scrollback[static_cast<size_t>(line_idx)];
                                render_terminal_line(line.data(), static_cast<int>(line.size()));
                            } else {
                                int screen_row = line_idx - static_cast<int>(scrollback.size());
                                render_screen_row(session, screen_row, line_height);
                            }
                        }
                    }
                    
                    float scroll_max_y = ImGui::GetScrollMaxY();
                    bool at_bottom = (scroll_max_y <= 0.0f) || (scroll_y >= scroll_max_y - 1.0f);
                    
                    if (ImGui::IsWindowHovered() && ImGui::GetIO().MouseWheel != 0.0f) {
                        session.set_user_scrolled_up(!at_bottom);
                    }
                    
                    if (at_bottom) {
                        session.set_user_scrolled_up(false);
                    }
                    
                    if (session.scroll_to_bottom() || (!session.user_scrolled_up() && session.state() == SessionState::Running)) {
                        ImGui::SetScrollHereY(1.0f);
                        session.set_scroll_to_bottom(false);
                    }
                } else {
                    for (int row = 0; row < terminal.rows(); ++row) {
                        render_screen_row(session, row, line_height);
                    }
                }
                
                float remaining = ImGui::GetContentRegionAvail().y;
                if (remaining > 0.0f) {
                    ImGui::Dummy(ImVec2(1.0f, remaining));
                }
                
                ImGui::PopStyleVar();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void TerminalPanel::render_cursor(TerminalSession& session, float target_x, float target_y, float char_w, float char_h) {
    auto& anim = cursor_animations_[session.id()];
    float dt = ImGui::GetIO().DeltaTime;
    const auto& theme = get_current_theme();
    
    if (!anim.initialized) {
        anim.current_x = target_x;
        anim.current_y = target_y;
        anim.target_x = target_x;
        anim.target_y = target_y;
        for (int i = 0; i < CursorAnimation::TRAIL_LENGTH; ++i) {
            anim.trail_x[i] = target_x;
            anim.trail_y[i] = target_y;
            anim.trail_alpha[i] = 0.0f;
        }
        anim.initialized = true;
    }
    
    anim.target_x = target_x;
    anim.target_y = target_y;
    
    constexpr float SPRING_STIFFNESS = 35.0f;
    constexpr float DAMPING = 8.0f;
    
    float dx = anim.target_x - anim.current_x;
    float dy = anim.target_y - anim.current_y;
    
    float ax = dx * SPRING_STIFFNESS - anim.velocity_x * DAMPING;
    float ay = dy * SPRING_STIFFNESS - anim.velocity_y * DAMPING;
    
    anim.velocity_x += ax * dt;
    anim.velocity_y += ay * dt;
    anim.current_x += anim.velocity_x * dt;
    anim.current_y += anim.velocity_y * dt;
    
    float speed = std::sqrt(anim.velocity_x * anim.velocity_x + anim.velocity_y * anim.velocity_y);
    
    constexpr float TRAIL_INTERVAL = 0.012f;
    anim.trail_timer += dt;
    if (anim.trail_timer >= TRAIL_INTERVAL && speed > 5.0f) {
        anim.trail_timer = 0.0f;
        anim.trail_head = (anim.trail_head + 1) % CursorAnimation::TRAIL_LENGTH;
        anim.trail_x[anim.trail_head] = anim.current_x;
        anim.trail_y[anim.trail_head] = anim.current_y;
        anim.trail_alpha[anim.trail_head] = std::min(1.0f, speed / 800.0f);
    }
    
    for (int i = 0; i < CursorAnimation::TRAIL_LENGTH; ++i) {
        anim.trail_alpha[i] *= (1.0f - dt * 8.0f);
    }
    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    uint8_t cursor_r = (theme.terminal_cursor >> 0) & 0xFF;
    uint8_t cursor_g = (theme.terminal_cursor >> 8) & 0xFF;
    uint8_t cursor_b = (theme.terminal_cursor >> 16) & 0xFF;
    
    for (int i = 0; i < CursorAnimation::TRAIL_LENGTH; ++i) {
        int idx = (anim.trail_head - i + CursorAnimation::TRAIL_LENGTH) % CursorAnimation::TRAIL_LENGTH;
        float alpha = anim.trail_alpha[idx];
        if (alpha < 0.01f) continue;
        
        float size_factor = 1.0f - (static_cast<float>(i) / CursorAnimation::TRAIL_LENGTH) * 0.6f;
        float trail_w = char_w * size_factor;
        float trail_h = char_h * size_factor;
        float offset_x = (char_w - trail_w) * 0.5f;
        float offset_y = (char_h - trail_h) * 0.5f;
        
        ImU32 trail_color = IM_COL32(cursor_r, cursor_g, cursor_b, static_cast<int>(alpha * 100));
        draw_list->AddRectFilled(
            ImVec2(anim.trail_x[idx] + offset_x, anim.trail_y[idx] + offset_y),
            ImVec2(anim.trail_x[idx] + offset_x + trail_w, anim.trail_y[idx] + offset_y + trail_h),
            trail_color,
            2.0f
        );
    }
    
    float blink = (std::sin(ImGui::GetTime() * 4.0f) + 1.0f) * 0.5f;
    int base_alpha = 200 + static_cast<int>(blink * 55);
    
    ImU32 cursor_color = IM_COL32(cursor_r, cursor_g, cursor_b, base_alpha);
    ImU32 glow_color = IM_COL32(cursor_r, cursor_g, cursor_b, static_cast<int>(base_alpha * 0.3f));
    
    draw_list->AddRectFilled(
        ImVec2(anim.current_x - 1, anim.current_y - 1),
        ImVec2(anim.current_x + char_w + 1, anim.current_y + char_h + 1),
        glow_color,
        3.0f
    );
    
    draw_list->AddRectFilled(
        ImVec2(anim.current_x, anim.current_y),
        ImVec2(anim.current_x + char_w, anim.current_y + char_h),
        cursor_color,
        2.0f
    );
}

void TerminalPanel::render_screen_row(TerminalSession& session, int screen_row, float line_height) {
    const auto& terminal = session.terminal();
    
    std::vector<TerminalCell> row_cells;
    row_cells.reserve(static_cast<size_t>(terminal.cols()));
    
    for (int col = 0; col < terminal.cols(); ++col) {
        row_cells.push_back(terminal.get_cell(screen_row, col));
    }
    
    render_terminal_line(row_cells.data(), static_cast<int>(row_cells.size()));
}

static bool is_dark_color(uint32_t abgr) {
    uint8_t r = abgr & 0xFF;
    uint8_t g = (abgr >> 8) & 0xFF;
    uint8_t b = (abgr >> 16) & 0xFF;
    int brightness = (r * 299 + g * 587 + b * 114) / 1000;
    return brightness < 40;
}

void TerminalPanel::render_terminal_line(const TerminalCell* cells, int count) {
    if (count <= 0) {
        ImGui::NewLine();
        return;
    }
    
    const auto& theme = get_current_theme();
    uint32_t default_bg = theme.terminal_bg;
    
    ImVec2 start_pos = ImGui::GetCursorScreenPos();
    ImVec2 char_size = ImGui::CalcTextSize("W");
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    float x_offset = 0.0f;
    for (int i = 0; i < count; ++i) {
        const auto& cell = cells[i];
        if (cell.width == 0) continue;
        
        bool has_custom_bg = (cell.bg & 0xFF000000) != 0 && 
                             cell.bg != default_bg &&
                             !is_dark_color(cell.bg);
        if (has_custom_bg) {
            float cell_width = char_size.x * cell.width;
            ImVec2 bg_min(start_pos.x + x_offset, start_pos.y);
            ImVec2 bg_max(bg_min.x + cell_width, bg_min.y + char_size.y);
            draw_list->AddRectFilled(bg_min, bg_max, cell.bg);
        }
        x_offset += char_size.x * cell.width;
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
            
            bool ctrl_pressed = diana_is_ctrl_pressed();
            
            if (ctrl_pressed && !has_ime_input) {
                if (ImGui::IsKeyPressed(ImGuiKey_C)) {
                    controller_.send_raw_key(session, "\x03");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_D)) {
                    controller_.send_raw_key(session, "\x04");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Z)) {
                    controller_.send_raw_key(session, "\x1a");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_L)) {
                    controller_.send_raw_key(session, "\x0c");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_U)) {
                    controller_.send_raw_key(session, "\x15");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_W)) {
                    controller_.send_raw_key(session, "\x17");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_A)) {
                    controller_.send_raw_key(session, "\x01");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_E)) {
                    controller_.send_raw_key(session, "\x05");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_K)) {
                    controller_.send_raw_key(session, "\x0b");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_R)) {
                    controller_.send_raw_key(session, "\x12");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_P)) {
                    controller_.send_raw_key(session, "\x10");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_N)) {
                    controller_.send_raw_key(session, "\x0e");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_B)) {
                    controller_.send_raw_key(session, "\x02");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_F)) {
                    controller_.send_raw_key(session, "\x06");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_T)) {
                    controller_.send_raw_key(session, "\x14");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Y)) {
                    controller_.send_raw_key(session, "\x19");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_H)) {
                    controller_.send_raw_key(session, "\x08");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_J)) {
                    controller_.send_raw_key(session, "\x0a");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_O)) {
                    controller_.send_raw_key(session, "\x0f");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_G)) {
                    controller_.send_raw_key(session, "\x07");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Backslash)) {
                    controller_.send_raw_key(session, "\x1c");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) {
                    controller_.send_raw_key(session, "\x1b");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) {
                    controller_.send_raw_key(session, "\x1d");
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Minus)) {
                    controller_.send_raw_key(session, "\x1f");
                }
            }
            else if (!has_ime_input) {
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
                else if (ImGui::IsKeyPressed(ImGuiKey_Tab) && io.KeyShift) {
                    controller_.send_raw_key(session, "\x1b[Z");
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
            
            if (!io.KeyCtrl) {
                for (int i = 0; i < io.InputQueueCharacters.Size; ++i) {
                    ImWchar c = io.InputQueueCharacters[i];
                    if (c > 0 && c != 127) {
                        controller_.send_char(session, static_cast<uint32_t>(c));
                    }
                }
            }
        }
    }
    
}

}
