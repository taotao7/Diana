#include "metrics_panel.h"
#include "imgui.h"
#include "implot.h"
#include <algorithm>
#include <array>
#include <cstdlib>

namespace diana {

MetricsPanel::MetricsPanel()
    : hub_(std::make_unique<MultiMetricsStore>())
    , claude_collector_(std::make_unique<ClaudeUsageCollector>())
    , opencode_collector_(std::make_unique<OpencodeUsageCollector>())
{
    claude_collector_->set_multi_store(hub_.get());
    opencode_collector_->set_multi_store(hub_.get());
}

void MetricsPanel::update() {
    claude_collector_->poll();
    opencode_collector_->poll();
}

void MetricsPanel::render() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(250, 300), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("Token Metrics");
    
    update();

    render_active_section();

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

void MetricsPanel::render_active_section() {
    if (!terminal_panel_) {
        ImGui::TextDisabled("No terminal sessions available.");
        return;
    }

    TerminalSession* active = terminal_panel_->active_session();
    if (!active) {
        ImGui::TextDisabled("Select a session tab to view token metrics.");
        return;
    }

    AppKind app = active->config().app;
    uint32_t& selected_tab_id = (app == AppKind::ClaudeCode) ? selected_claude_tab_id_ : selected_opencode_tab_id_;

    const auto& sessions = terminal_panel_->sessions();
    std::vector<const TerminalSession*> filtered;
    for (const auto& session : sessions) {
        if (session->config().app == app && session->state() == SessionState::Running) {
            filtered.push_back(session.get());
        }
    }

    if (filtered.empty()) {
        ImGui::TextDisabled("No running %s session.", app == AppKind::ClaudeCode ? "Claude Code" : "OpenCode");
        ImGui::TextDisabled("Start the selected agent to see token metrics.");
        return;
    }

    if (active->state() == SessionState::Running && active->config().app == app) {
        selected_tab_id = active->id();
    } else if (selected_tab_id == 0) {
        selected_tab_id = filtered[0]->id();
    }

    const char* title = app == AppKind::ClaudeCode ? "Claude Code - Real-time" : "OpenCode - Real-time";
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", title);
    ImGui::Separator();

    render_scope_selector(app, selected_tab_id);
    ImGui::Separator();

    render_stats_for_scope(app, selected_tab_id);

    ImGui::Spacing();
    size_t files = app == AppKind::ClaudeCode ? claude_collector_->files_processed() : opencode_collector_->files_processed();
    size_t entries = app == AppKind::ClaudeCode ? claude_collector_->entries_parsed() : opencode_collector_->entries_parsed();
    ImGui::Text("Files monitored: %zu", files);
    ImGui::SameLine();
    ImGui::Text("Entries parsed: %zu", entries);
}

void MetricsPanel::render_scope_selector(AppKind app, uint32_t& selected_tab_id) {
    const auto& sessions = terminal_panel_->sessions();
    std::vector<const TerminalSession*> filtered;
    for (const auto& session : sessions) {
        if (session->config().app == app) {
            filtered.push_back(session.get());
        }
    }

    if (filtered.empty()) return;

    if (selected_tab_id == 0) {
        selected_tab_id = filtered[0]->id();
    }

    const TerminalSession* selected = nullptr;
    for (const auto* session : filtered) {
        if (session->id() == selected_tab_id) {
            selected = session;
            break;
        }
    }
    if (!selected) {
        selected = filtered[0];
        selected_tab_id = selected->id();
    }

    std::string selected_name = selected->name();

    ImGui::Text("Scope:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    
    if (ImGui::BeginCombo("##Scope", selected_name.c_str())) {
        for (const auto* session : filtered) {
            std::string label = session->name();
            std::string project_key = get_project_key(app, session->config().working_dir);
            
            if (!hub_->has_source(project_key)) {
                label += " (no data)";
            }
            
            if (ImGui::Selectable(label.c_str(), selected_tab_id == session->id())) {
                selected_tab_id = session->id();
            }
            
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Project: %s", project_key.c_str());
            }
        }
        ImGui::EndCombo();
    }

    for (const auto* session : filtered) {
        if (session->id() == selected_tab_id) {
            std::string working_dir = session->config().working_dir;
            std::string project_key = get_project_key(app, working_dir);
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

void MetricsPanel::render_stats_for_scope(AppKind app, uint32_t selected_tab_id) {
    MetricsStore* store = nullptr;
    
    if (terminal_panel_) {
        for (const auto& session : terminal_panel_->sessions()) {
            if (session->id() == selected_tab_id && session->config().app == app) {
                std::string project_key = get_project_key(app, session->config().working_dir);
                store = hub_->get_source_store(project_key);
                break;
            }
        }
    }
    
    TokenStats stats{};
    std::array<float, 60> rate_history{};
    if (store) {
        stats = store->compute_stats();
        rate_history = store->get_rate_history();
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
        ImPlot::PlotBars("tok/s", rate_history.data(), 60);
        ImPlot::EndPlot();
    }
}

std::string MetricsPanel::get_project_key(AppKind app, const std::string& working_dir) const {
    std::string result;
    if (working_dir.empty()) {
        const char* home = std::getenv("HOME");
        if (home) {
            result = home;
        } else {
            result = "unknown";
        }
    } else {
        result = working_dir;
    }

    std::replace(result.begin(), result.end(), '/', '-');
    if (!result.empty() && result[0] == '-') {
        result = result.substr(1);
    }

    if (app == AppKind::OpenCode) {
        result = "opencode:" + result;
    }
    return result;
}

std::string MetricsPanel::truncate_path(const std::string& path, size_t max_len) const {
    if (path.length() <= max_len) return path;
    return "..." + path.substr(path.length() - max_len + 3);
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

}
