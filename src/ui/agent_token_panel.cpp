#include "ui/agent_token_panel.h"
#include "imgui.h"
#include <algorithm>
#include <map>

namespace diana {

namespace {

std::string format_tokens(uint64_t tokens) {
    if (tokens >= 1000000000) {
        return std::to_string(tokens / 1000000000) + "." + std::to_string((tokens % 1000000000) / 100000000) + "B";
    } else if (tokens >= 1000000) {
        return std::to_string(tokens / 1000000) + "." + std::to_string((tokens % 1000000) / 100000) + "M";
    } else if (tokens >= 1000) {
        return std::to_string(tokens / 1000) + "." + std::to_string((tokens % 1000) / 100) + "K";
    }
    return std::to_string(tokens);
}

}

AgentTokenPanel::AgentTokenPanel()
    : store_(std::make_unique<AgentTokenStore>())
{
}

void AgentTokenPanel::update() {
    store_->poll();
    
    auto now = std::chrono::steady_clock::now();
    bool agent_changed = selected_agent_ != last_selected_agent_;
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_update_).count();
    
    if (agent_changed) {
        daily_data_.clear();
        cached_stats_ = AgentTypeStats{};
        cached_sessions_.clear();
    }
    
    if (agent_changed || elapsed >= 1) {
        daily_data_ = store_->get_daily_data(selected_agent_);
        cached_stats_ = store_->get_stats(selected_agent_);
        cached_sessions_ = store_->get_sessions(selected_agent_);
        last_update_ = now;
        last_selected_agent_ = selected_agent_;
    }
}

void AgentTokenPanel::render() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(400, 500), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("Agent Token Stats");
    
    update();
    
    render_agent_selector();
    ImGui::Separator();
    render_summary_stats();
    ImGui::Separator();
    render_token_breakdown();
    ImGui::Separator();
    render_heatmap();
    ImGui::Separator();
    render_session_list();
    
    ImGui::Spacing();
    ImGui::Text("Files: %zu", store_->files_processed());
    ImGui::SameLine();
    ImGui::Text("Sessions: %zu", store_->sessions_tracked());
    
    if (ImGui::Button("Clear Stats")) {
        show_clear_confirm_ = true;
    }
    
    if (show_clear_confirm_) {
        ImGui::OpenPopup("Confirm Clear##AgentTokenPanel");
    }
    
    if (ImGui::BeginPopupModal("Confirm Clear##AgentTokenPanel", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Clear all agent token statistics?");
        ImGui::Text("This action cannot be undone.");
        ImGui::Spacing();
        
        if (ImGui::Button("Yes, Clear", ImVec2(120, 0))) {
            store_->clear();
            daily_data_.clear();
            show_clear_confirm_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            show_clear_confirm_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    
    ImGui::End();
}

void AgentTokenPanel::render_agent_selector() {
    const char* agent_names[] = { "Claude Code", "Codex", "OpenCode" };
    
    ImGui::Text("Select Agent:");
    ImGui::SameLine();
    
    ImGui::SetNextItemWidth(150);
    if (ImGui::Combo("##AgentSelector", &selected_agent_idx_, agent_names, IM_ARRAYSIZE(agent_names))) {
        switch (selected_agent_idx_) {
            case 0: selected_agent_ = AgentType::ClaudeCode; break;
            case 1: selected_agent_ = AgentType::Codex; break;
            case 2: selected_agent_ = AgentType::OpenCode; break;
        }
    }
}

void AgentTokenPanel::render_summary_stats() {
    const auto& stats = cached_stats_;
    
    ImGui::Text("Total Tokens: %s", format_tokens(stats.total_tokens.total()).c_str());
    ImGui::SameLine(200);
    ImGui::Text("Cost: $%.4f", stats.total_tokens.cost_usd);
    
    ImGui::Text("Sessions: %zu", stats.session_count);
    ImGui::SameLine(200);
    ImGui::Text("Active: %zu", stats.active_sessions);
}

void AgentTokenPanel::render_token_breakdown() {
    const auto& stats = cached_stats_;
    
    if (ImGui::BeginTable("token_breakdown", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Tokens");
        ImGui::TableHeadersRow();
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Input");
        ImGui::TableNextColumn(); ImGui::Text("%s", format_tokens(stats.total_tokens.input_tokens).c_str());
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Output");
        ImGui::TableNextColumn(); ImGui::Text("%s", format_tokens(stats.total_tokens.output_tokens).c_str());
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Cache Creation");
        ImGui::TableNextColumn(); ImGui::Text("%s", format_tokens(stats.total_tokens.cache_creation_tokens).c_str());
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Cache Read");
        ImGui::TableNextColumn(); ImGui::Text("%s", format_tokens(stats.total_tokens.cache_read_tokens).c_str());
        
        ImGui::TableNextRow();
        const auto& theme = get_current_theme();
        float accent_r = (theme.accent & 0xFF) / 255.0f;
        float accent_g = ((theme.accent >> 8) & 0xFF) / 255.0f;
        float accent_b = ((theme.accent >> 16) & 0xFF) / 255.0f;
        ImVec4 accent_color(accent_r, accent_g, accent_b, 1.0f);
        
        ImGui::TableNextColumn(); ImGui::TextColored(accent_color, "Total");
        ImGui::TableNextColumn(); ImGui::TextColored(accent_color, "%s", 
            format_tokens(stats.total_tokens.total()).c_str());
        
        ImGui::EndTable();
    }
}

ImU32 AgentTokenPanel::get_heatmap_color(uint64_t tokens, uint64_t max_tokens) {
    const auto& theme = get_current_theme();
    
    if (tokens == 0 || max_tokens == 0) {
        return theme.background_dark;
    }
    
    float ratio = static_cast<float>(tokens) / static_cast<float>(max_tokens);
    
    uint8_t accent_r = theme.accent & 0xFF;
    uint8_t accent_g = (theme.accent >> 8) & 0xFF;
    uint8_t accent_b = (theme.accent >> 16) & 0xFF;
    
    uint8_t bg_r = theme.background_dark & 0xFF;
    uint8_t bg_g = (theme.background_dark >> 8) & 0xFF;
    uint8_t bg_b = (theme.background_dark >> 16) & 0xFF;
    
    float level;
    if (ratio < 0.25f) {
        level = 0.25f;
    } else if (ratio < 0.5f) {
        level = 0.5f;
    } else if (ratio < 0.75f) {
        level = 0.75f;
    } else {
        level = 1.0f;
    }
    
    uint8_t r = static_cast<uint8_t>(bg_r + (accent_r - bg_r) * level);
    uint8_t g = static_cast<uint8_t>(bg_g + (accent_g - bg_g) * level);
    uint8_t b = static_cast<uint8_t>(bg_b + (accent_b - bg_b) * level);
    
    return IM_COL32(r, g, b, 255);
}

void AgentTokenPanel::render_heatmap() {
    if (daily_data_.empty()) {
        ImGui::TextDisabled("No activity data");
        return;
    }
    
    uint64_t max_tokens = 0;
    for (const auto& d : daily_data_) {
        max_tokens = std::max(max_tokens, d.tokens);
    }
    
    const float cell_size = 10.0f;
    const float cell_gap = 2.0f;
    const float label_width = 30.0f;
    const float month_label_height = 16.0f;
    
    ImGui::Text("Token Activity (last 365 days)");
    
    float available_width = ImGui::GetContentRegionAvail().x;
    int weeks_to_show = static_cast<int>((available_width - label_width) / (cell_size + cell_gap));
    weeks_to_show = std::min(weeks_to_show, 53);
    
    float total_height = month_label_height + 7 * (cell_size + cell_gap);
    
    ImGui::BeginChild("HeatmapRegion", ImVec2(-1, total_height + 30), false);
    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    
    const char* weekday_labels[] = { "", "Mon", "", "Wed", "", "Fri", "" };
    for (int row = 0; row < 7; ++row) {
        float y = canvas_pos.y + month_label_height + row * (cell_size + cell_gap);
        ImGui::SetCursorScreenPos(ImVec2(canvas_pos.x, y));
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", weekday_labels[row]);
    }
    
    int start_idx = static_cast<int>(daily_data_.size()) - weeks_to_show * 7;
    if (start_idx < 0) start_idx = 0;
    
    int first_weekday = 0;
    if (start_idx < static_cast<int>(daily_data_.size())) {
        first_weekday = daily_data_[start_idx].weekday;
    }
    
    std::map<std::string, int> month_starts;
    
    int col = 0;
    int row = first_weekday;
    
    for (size_t i = start_idx; i < daily_data_.size(); ++i) {
        const auto& data = daily_data_[i];
        
        if (data.day == 1 || (i == static_cast<size_t>(start_idx) && data.day <= 7)) {
            const char* month_names[] = { "", "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
            std::string month_label = month_names[data.month];
            month_starts[std::to_string(col)] = data.month;
        }
        
        float x = canvas_pos.x + label_width + col * (cell_size + cell_gap);
        float y = canvas_pos.y + month_label_height + row * (cell_size + cell_gap);
        
        ImU32 color = get_heatmap_color(data.tokens, max_tokens);
        draw_list->AddRectFilled(ImVec2(x, y), ImVec2(x + cell_size, y + cell_size), color, 2.0f);
        
        ImVec2 mouse_pos = ImGui::GetMousePos();
        if (mouse_pos.x >= x && mouse_pos.x <= x + cell_size &&
            mouse_pos.y >= y && mouse_pos.y <= y + cell_size) {
            ImGui::BeginTooltip();
            ImGui::Text("%04d-%02d-%02d", data.year, data.month, data.day);
            if (data.tokens > 0) {
                if (data.tokens >= 1000000) {
                    ImGui::Text("Tokens: %.2fM", static_cast<double>(data.tokens) / 1000000.0);
                } else if (data.tokens >= 1000) {
                    ImGui::Text("Tokens: %.1fK", static_cast<double>(data.tokens) / 1000.0);
                } else {
                    ImGui::Text("Tokens: %llu", static_cast<unsigned long long>(data.tokens));
                }
                ImGui::Text("Sessions: %zu", data.session_count);
            } else {
                ImGui::TextDisabled("No activity");
            }
            ImGui::EndTooltip();
        }
        
        row++;
        if (row >= 7) {
            row = 0;
            col++;
        }
    }
    
    const char* month_names[] = { "", "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    int last_drawn_month = 0;
    col = 0;
    row = first_weekday;
    
    const auto& theme = get_current_theme();
    ImU32 label_color = theme.foreground_dim;
    
    for (size_t i = start_idx; i < daily_data_.size(); ++i) {
        const auto& data = daily_data_[i];
        
        if (data.day <= 7 && data.month != last_drawn_month) {
            float x = canvas_pos.x + label_width + col * (cell_size + cell_gap);
            draw_list->AddText(ImVec2(x, canvas_pos.y), label_color, month_names[data.month]);
            last_drawn_month = data.month;
        }
        
        row++;
        if (row >= 7) {
            row = 0;
            col++;
        }
    }
    
    float legend_y = canvas_pos.y + month_label_height + 7 * (cell_size + cell_gap) + 8;
    float legend_x = canvas_pos.x + label_width;
    
    float text_height = ImGui::GetTextLineHeight();
    float square_y_offset = (text_height - cell_size) * 0.5f;
    
    draw_list->AddText(ImVec2(legend_x, legend_y), label_color, "Less");
    legend_x += ImGui::CalcTextSize("Less").x + 4.0f;
    
    uint8_t accent_r = theme.accent & 0xFF;
    uint8_t accent_g = (theme.accent >> 8) & 0xFF;
    uint8_t accent_b = (theme.accent >> 16) & 0xFF;
    uint8_t bg_r = theme.background_dark & 0xFF;
    uint8_t bg_g = (theme.background_dark >> 8) & 0xFF;
    uint8_t bg_b = (theme.background_dark >> 16) & 0xFF;
    
    float levels[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
    
    for (int i = 0; i < 5; ++i) {
        uint8_t r = static_cast<uint8_t>(bg_r + (accent_r - bg_r) * levels[i]);
        uint8_t g = static_cast<uint8_t>(bg_g + (accent_g - bg_g) * levels[i]);
        uint8_t b = static_cast<uint8_t>(bg_b + (accent_b - bg_b) * levels[i]);
        
        draw_list->AddRectFilled(
            ImVec2(legend_x + i * (cell_size + 2), legend_y + square_y_offset),
            ImVec2(legend_x + i * (cell_size + 2) + cell_size, legend_y + square_y_offset + cell_size),
            IM_COL32(r, g, b, 255), 2.0f
        );
    }
    
    legend_x += 5 * (cell_size + 2) + 2.0f;
    draw_list->AddText(ImVec2(legend_x, legend_y), label_color, "More");
    
    ImGui::EndChild();
}

std::string AgentTokenPanel::truncate_session_id(const std::string& id, size_t max_len) {
    if (id.length() <= max_len) {
        return id;
    }
    
    if (max_len <= 3) {
        return id.substr(0, max_len);
    }
    
    size_t prefix_len = (max_len - 2) / 2;
    size_t suffix_len = max_len - 2 - prefix_len;
    
    return id.substr(0, prefix_len) + ".." + id.substr(id.length() - suffix_len);
}

void AgentTokenPanel::render_session_list() {
    const auto& sessions = cached_sessions_;
    
    if (sessions.empty()) {
        ImGui::TextDisabled("No sessions found");
        return;
    }
    
    ImGui::Text("Recent Sessions (%zu):", sessions.size());
    
    if (ImGui::BeginTable("session_table", 3, 
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
        ImVec2(0, 150))) {
        
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Session", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Msgs", ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableSetupColumn("Tokens", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();
        
        size_t display_count = std::min(sessions.size(), static_cast<size_t>(50));
        for (size_t i = 0; i < display_count; ++i) {
            const auto& session = sessions[i];
            
            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(i));
            
            ImGui::TableNextColumn();
            std::string display_id = truncate_session_id(session.session_id, 20);
            if (session.is_subagent) {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 1.0f, 1.0f), "[sub] %s", display_id.c_str());
            } else {
                ImGui::Text("%s", display_id.c_str());
            }
            
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Session: %s", session.session_id.c_str());
                ImGui::Separator();
                ImGui::Text("Messages: %zu", session.message_count);
                ImGui::Text("Input: %llu", static_cast<unsigned long long>(session.tokens.input_tokens));
                ImGui::Text("Output: %llu", static_cast<unsigned long long>(session.tokens.output_tokens));
                ImGui::Text("Cache: %llu", static_cast<unsigned long long>(
                    session.tokens.cache_creation_tokens + session.tokens.cache_read_tokens));
                if (session.tokens.cost_usd > 0) {
                    ImGui::Text("Cost: $%.4f", session.tokens.cost_usd);
                }
                ImGui::EndTooltip();
            }
            
            ImGui::TableNextColumn();
            ImGui::Text("%zu", session.message_count);
            
            ImGui::TableNextColumn();
            uint64_t total = session.tokens.total();
            if (total >= 1000000) {
                ImGui::Text("%.1fM", static_cast<double>(total) / 1000000.0);
            } else if (total >= 1000) {
                ImGui::Text("%.1fK", static_cast<double>(total) / 1000.0);
            } else {
                ImGui::Text("%llu", static_cast<unsigned long long>(total));
            }
            
            ImGui::PopID();
        }
        
        ImGui::EndTable();
    }
    
    if (sessions.size() > 50) {
        ImGui::TextDisabled("... and %zu more", sessions.size() - 50);
    }
}

}
