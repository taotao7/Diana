#include "ui/metrics_panel.h"
#include "ui/theme.h"
#include "imgui.h"
#include "implot.h"
#include <algorithm>
#include <cstdlib>

namespace diana {

MetricsPanel::MetricsPanel()
    : hub_(std::make_unique<MultiMetricsStore>())
    , collector_(std::make_unique<ClaudeUsageCollector>())
{
    collector_->set_multi_store(hub_.get());
}

void MetricsPanel::update() {
    collector_->poll();
}

void MetricsPanel::render() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(250, 300), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("Token Metrics");
    
    update();
    
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Claude Code - Real-time");
    ImGui::Separator();
    
    auto active_session = get_active_claude_session();
    if (!active_session) {
        ImGui::TextDisabled("No active Claude Code session.");
        ImGui::TextDisabled("Start a Claude Code agent with a project");
        ImGui::TextDisabled("directory to see token metrics.");
        ImGui::End();
        return;
    }
    
    render_scope_selector();
    ImGui::Separator();
    
    render_stats_for_scope();
    
    ImGui::Spacing();
    ImGui::Text("Files monitored: %zu", collector_->files_processed());
    ImGui::SameLine();
    ImGui::Text("Entries parsed: %zu", collector_->entries_parsed());
    
    if (ImGui::Button("Clear Stats")) {
        show_clear_confirm_ = true;
    }
    
    if (show_clear_confirm_) {
        ImGui::OpenPopup("Confirm Clear##MetricsPanel");
    }
    
    if (ImGui::BeginPopupModal("Confirm Clear##MetricsPanel", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Clear all token statistics?");
        ImGui::Text("This action cannot be undone.");
        ImGui::Spacing();
        
        if (ImGui::Button("Yes, Clear", ImVec2(120, 0))) {
            hub_->clear();
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

const TerminalSession* MetricsPanel::get_active_claude_session() const {
    if (!terminal_panel_) return nullptr;
    for (const auto& session : terminal_panel_->sessions()) {
        if (session->config().app == AppKind::ClaudeCode &&
            session->state() == SessionState::Running) {
            return session.get();
        }
    }
    return nullptr;
}

bool MetricsPanel::has_claude_sessions() const {
    if (!terminal_panel_) return false;
    for (const auto& session : terminal_panel_->sessions()) {
        if (session->config().app == AppKind::ClaudeCode) {
            return true;
        }
    }
    return false;
}

void MetricsPanel::render_scope_selector() {
    const auto& sessions = terminal_panel_->sessions();
    std::vector<const TerminalSession*> claude_sessions;
    for (const auto& session : sessions) {
        if (session->config().app == AppKind::ClaudeCode) {
            claude_sessions.push_back(session.get());
        }
    }
    
    if (selected_tab_id_ == 0) {
        selected_tab_id_ = claude_sessions[0]->id();
    }
    
    std::string selected_name;
    for (const auto* session : claude_sessions) {
        if (session->id() == selected_tab_id_) {
            selected_name = session->name();
            break;
        }
    }
    
    if (selected_name.empty()) {
        selected_tab_id_ = claude_sessions[0]->id();
        selected_name = claude_sessions[0]->name();
    }
    
    ImGui::Text("Scope:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    
    if (ImGui::BeginCombo("##Scope", selected_name.c_str())) {
        for (const auto* session : claude_sessions) {
            std::string label = session->name();
            std::string project_key = get_claude_project_key(session->config().working_dir);
            
            if (!hub_->has_source(project_key)) {
                label += " (no data)";
            }
            
            if (ImGui::Selectable(label.c_str(), selected_tab_id_ == session->id())) {
                selected_tab_id_ = session->id();
            }
            
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Project: %s", project_key.c_str());
            }
        }
        ImGui::EndCombo();
    }
    
    for (const auto* session : claude_sessions) {
        if (session->id() == selected_tab_id_) {
            std::string working_dir = session->config().working_dir;
            std::string project_key = get_claude_project_key(working_dir);
            ImGui::TextDisabled("Project: %s", truncate_path(project_key, 30).c_str());
            
            if (working_dir.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
                ImGui::TextWrapped("Warning: Using home directory. Select a project directory for accurate per-project metrics.");
                ImGui::PopStyleColor();
            }
            break;
        }
    }
}

std::string MetricsPanel::format_tokens(uint64_t tokens) const {
    if (tokens >= 1000000000) {
        return std::to_string(tokens / 1000000000) + "." + std::to_string((tokens % 1000000000) / 100000000) + "B";
    } else if (tokens >= 1000000) {
        return std::to_string(tokens / 1000000) + "." + std::to_string((tokens % 1000000) / 100000) + "M";
    } else if (tokens >= 1000) {
        return std::to_string(tokens / 1000) + "." + std::to_string((tokens % 1000) / 100) + "K";
    }
    return std::to_string(tokens);
}

void MetricsPanel::render_stats_for_scope() {
    MetricsStore* store = nullptr;
    
    if (terminal_panel_) {
        for (const auto& session : terminal_panel_->sessions()) {
            if (session->id() == selected_tab_id_) {
                std::string project_key = get_claude_project_key(session->config().working_dir);
                store = hub_->get_source_store(project_key);
                break;
            }
        }
    }
    
    TokenStats stats{};
    if (store) {
        stats = store->compute_stats();
        rate_history_ = store->get_rate_history();
    } else {
        rate_history_.fill(0.0f);
    }
    
    ImGui::Text("Total Tokens: %s", format_tokens(stats.total_tokens).c_str());
    ImGui::SameLine(200);
    ImGui::Text("Cost: $%.4f", stats.total_cost);
    
    ImGui::Separator();
    
    if (ImGui::BeginTable("stats_table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Metric");
        ImGui::TableSetupColumn("Input");
        ImGui::TableSetupColumn("Output");
        ImGui::TableSetupColumn("Total");
        ImGui::TableHeadersRow();
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Tokens");
        ImGui::TableNextColumn(); ImGui::Text("%s", format_tokens(stats.total_input).c_str());
        ImGui::TableNextColumn(); ImGui::Text("%s", format_tokens(stats.total_output).c_str());
        ImGui::TableNextColumn(); ImGui::Text("%s", format_tokens(stats.total_tokens).c_str());
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Tok/min");
        ImGui::TableNextColumn(); ImGui::Text("%.1f", stats.input_per_min);
        ImGui::TableNextColumn(); ImGui::Text("%.1f", stats.output_per_min);
        ImGui::TableNextColumn(); ImGui::Text("%.1f", stats.tokens_per_min);
        
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::Text("Tok/sec");
        ImGui::TableNextColumn(); ImGui::Text("%.1f", stats.input_per_sec);
        ImGui::TableNextColumn(); ImGui::Text("%.1f", stats.output_per_sec);
        ImGui::TableNextColumn(); ImGui::Text("%.1f", stats.tokens_per_sec);
        
        ImGui::EndTable();
    }
    
    ImGui::Spacing();
    
    ImPlot::SetNextAxesToFit();
    if (ImPlot::BeginPlot("Token Rate (last 60s)", ImVec2(-1, 200))) {
        ImPlot::SetupAxes("Time (s)", "Tokens");
        ImPlot::PlotBars("tok/s", rate_history_.data(), 60);
        ImPlot::EndPlot();
    }
}

std::string MetricsPanel::get_claude_project_key(const std::string& working_dir) const {
    if (working_dir.empty()) {
        const char* home = std::getenv("HOME");
        if (home) {
            std::string result = home;
            std::replace(result.begin(), result.end(), '/', '-');
            if (!result.empty() && result[0] == '-') {
                result = result.substr(1);
            }
            return result;
        }
        return "unknown";
    }
    
    std::string result = working_dir;
    std::replace(result.begin(), result.end(), '/', '-');
    if (!result.empty() && result[0] == '-') {
        result = result.substr(1);
    }
    return result;
}

std::string MetricsPanel::truncate_path(const std::string& path, size_t max_len) const {
    if (path.length() <= max_len) return path;
    return "..." + path.substr(path.length() - max_len + 3);
}

}
