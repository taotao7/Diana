#include "ui/agent_token_panel.h"
#include "imgui.h"
#include "implot.h"
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace agent47 {

AgentTokenPanel::AgentTokenPanel()
    : store_(std::make_unique<AgentTokenStore>())
{
}

void AgentTokenPanel::update() {
    store_->poll();
    
    monthly_data_.clear();
    auto sessions = store_->get_sessions(selected_agent_);
    
    for (const auto& session : sessions) {
        auto time_t_val = std::chrono::system_clock::to_time_t(session.first_seen);
        std::tm* tm = std::localtime(&time_t_val);
        
        std::ostringstream oss;
        oss << std::put_time(tm, "%Y-%m");
        std::string month_key = oss.str();
        
        auto& data = monthly_data_[month_key];
        data.tokens += session.tokens.total();
        data.cost += session.tokens.cost_usd;
        data.session_count++;
    }
}

void AgentTokenPanel::render() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(300, 400), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("Agent Token Stats");
    
    update();
    
    render_agent_selector();
    ImGui::Separator();
    render_summary_stats();
    ImGui::Separator();
    render_token_breakdown();
    ImGui::Separator();
    render_monthly_chart();
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
            monthly_data_.clear();
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
    auto stats = store_->get_stats(selected_agent_);
    
    ImGui::Text("Total Tokens: %llu", static_cast<unsigned long long>(stats.total_tokens.total()));
    ImGui::SameLine(200);
    ImGui::Text("Cost: $%.4f", stats.total_tokens.cost_usd);
    
    ImGui::Text("Sessions: %zu", stats.session_count);
    ImGui::SameLine(200);
    ImGui::Text("Active: %zu", stats.active_sessions);
}

void AgentTokenPanel::render_token_breakdown() {
    auto stats = store_->get_stats(selected_agent_);
    
    if (ImGui::BeginTable("token_breakdown", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Tokens");
        ImGui::TableHeadersRow();
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Input");
        ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(stats.total_tokens.input_tokens));
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Output");
        ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(stats.total_tokens.output_tokens));
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Cache Creation");
        ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(stats.total_tokens.cache_creation_tokens));
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Cache Read");
        ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(stats.total_tokens.cache_read_tokens));
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Total");
        ImGui::TableNextColumn(); ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%llu", 
            static_cast<unsigned long long>(stats.total_tokens.total()));
        
        ImGui::EndTable();
    }
}

void AgentTokenPanel::render_monthly_chart() {
    if (monthly_data_.empty()) {
        ImGui::TextDisabled("No monthly data available");
        return;
    }
    
    std::vector<std::string> months;
    std::vector<double> tokens;
    
    for (const auto& [month, data] : monthly_data_) {
        months.push_back(month);
        tokens.push_back(static_cast<double>(data.tokens) / 1000.0);
    }
    
    if (ImPlot::BeginPlot("Monthly Token Usage (K)", ImVec2(-1, 120))) {
        ImPlot::SetupAxes("Month", "Tokens (K)");
        
        std::vector<const char*> month_labels;
        for (const auto& m : months) {
            month_labels.push_back(m.c_str());
        }
        
        ImPlot::SetupAxisTicks(ImAxis_X1, 0, static_cast<double>(months.size() - 1), 
                               static_cast<int>(months.size()), month_labels.data());
        
        ImPlot::PlotBars("Tokens", tokens.data(), static_cast<int>(tokens.size()));
        ImPlot::EndPlot();
    }
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
    auto sessions = store_->get_sessions(selected_agent_);
    
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
