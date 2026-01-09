#include "ui/agent_token_panel.h"
#include "imgui.h"
#include "implot.h"

namespace agent47 {

AgentTokenPanel::AgentTokenPanel()
    : store_(std::make_unique<AgentTokenStore>())
{
}

void AgentTokenPanel::update() {
    store_->poll();
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_update_);
    
    if (elapsed.count() >= 1) {
        auto stats = store_->get_stats(selected_agent_);
        uint64_t current_total = stats.total_tokens.total();
        
        float rate = 0.0f;
        if (last_total_tokens_ > 0 && current_total >= last_total_tokens_) {
            rate = static_cast<float>(current_total - last_total_tokens_) / elapsed.count();
        }
        
        std::rotate(rate_history_.begin(), rate_history_.begin() + 1, rate_history_.end());
        rate_history_.back() = rate;
        
        last_total_tokens_ = current_total;
        last_update_ = now;
    }
}

void AgentTokenPanel::render() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(280, 350), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("Agent Token Stats");
    
    update();
    
    render_agent_selector();
    ImGui::Separator();
    render_summary_stats();
    ImGui::Separator();
    render_token_breakdown();
    ImGui::Separator();
    render_rate_chart();
    ImGui::Separator();
    render_session_list();
    
    ImGui::Spacing();
    ImGui::Text("Files tracked: %zu", store_->files_processed());
    ImGui::SameLine();
    ImGui::Text("Sessions: %zu", store_->sessions_tracked());
    
    if (ImGui::Button("Clear Stats")) {
        store_->clear();
        std::fill(rate_history_.begin(), rate_history_.end(), 0.0f);
        last_total_tokens_ = 0;
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
        std::fill(rate_history_.begin(), rate_history_.end(), 0.0f);
        last_total_tokens_ = 0;
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

void AgentTokenPanel::render_rate_chart() {
    ImPlot::SetNextAxesToFit();
    if (ImPlot::BeginPlot("Token Rate (last 60s)", ImVec2(-1, 150))) {
        ImPlot::SetupAxes("Time (s)", "Tokens/s");
        ImPlot::PlotBars("tok/s", rate_history_.data(), 60);
        ImPlot::EndPlot();
    }
}

void AgentTokenPanel::render_session_list() {
    auto sessions = store_->get_sessions(selected_agent_);
    
    if (sessions.empty()) {
        ImGui::TextDisabled("No sessions found");
        return;
    }
    
    ImGui::Text("Recent Sessions (%zu):", sessions.size());
    
    ImGui::BeginChild("SessionList", ImVec2(0, 150), true);
    
    size_t display_count = std::min(sessions.size(), static_cast<size_t>(20));
    for (size_t i = 0; i < display_count; ++i) {
        const auto& session = sessions[i];
        
        ImGui::PushID(static_cast<int>(i));
        
        bool is_subagent = session.is_subagent;
        if (is_subagent) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 1.0f, 1.0f), "  [sub] %s", session.session_id.c_str());
        } else {
            ImGui::Text("%s", session.session_id.c_str());
        }
        
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100);
        ImGui::Text("%llu tok", static_cast<unsigned long long>(session.tokens.total()));
        
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("Session: %s", session.session_id.c_str());
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
        
        ImGui::PopID();
    }
    
    if (sessions.size() > display_count) {
        ImGui::TextDisabled("... and %zu more", sessions.size() - display_count);
    }
    
    ImGui::EndChild();
}

}
