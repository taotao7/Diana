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

    auto is_valid_session = [](const TerminalSession& s) {
        return s.config().app != AppKind::Shell && s.state() != SessionState::Idle;
    };

    TerminalSession* selected = terminal_panel_->find_session(selected_session_id_);
    
    if (selected && !is_valid_session(*selected)) {
        selected = nullptr;
    }

    if (!selected) {
        selected = terminal_panel_->active_session();
        if (!selected || !is_valid_session(*selected)) {
            selected = nullptr;
            for (const auto& session : terminal_panel_->sessions()) {
                if (is_valid_session(*session)) {
                    selected = session.get();
                    break;
                }
            }
        }
        if (selected) {
            selected_session_id_ = selected->id();
        }
    }

    if (!selected) {
        ImGui::TextDisabled("No active agents.");
        return;
    }

    AppKind app = selected->config().app;
    std::string app_name;
    switch (app) {
        case AppKind::ClaudeCode: app_name = "Claude Code"; break;
        case AppKind::OpenCode: app_name = "OpenCode"; break;
        case AppKind::Codex: app_name = "Codex"; break;
        case AppKind::Shell: app_name = "Shell"; break;
    }

    std::string preview = selected->name() + " (" + app_name + ")";

    ImGui::Text("Scope:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    
    if (ImGui::BeginCombo("##Scope", preview.c_str())) {
        for (const auto& session : terminal_panel_->sessions()) {
            if (!is_valid_session(*session)) continue;

            std::string s_app_name;
            switch (session->config().app) {
                case AppKind::ClaudeCode: s_app_name = "Claude Code"; break;
                case AppKind::OpenCode: s_app_name = "OpenCode"; break;
                case AppKind::Codex: s_app_name = "Codex"; break;
                case AppKind::Shell: s_app_name = "Shell"; break;
            }
            
            std::string label = session->name() + " (" + s_app_name + ")";
            bool is_selected = (selected_session_id_ == session->id());
            
            if (ImGui::Selectable(label.c_str(), is_selected)) {
                selected_session_id_ = session->id();
            }
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    std::string project_key = get_project_key(app, selected->config().working_dir);
    ImGui::TextDisabled("Project: %s", truncate_path(project_key, 30).c_str());
    
    if (selected->config().working_dir.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.0f, 1.0f));
        ImGui::TextWrapped("Warning: Using home directory. Select a project directory for accurate per-project metrics.");
        ImGui::PopStyleColor();
    }

    ImGui::Separator();
    render_stats_for_scope(app, selected->id());

    ImGui::Spacing();
    size_t files = 0, entries = 0;
    if (app == AppKind::ClaudeCode) {
        files = claude_collector_->files_processed();
        entries = claude_collector_->entries_parsed();
    } else if (app == AppKind::OpenCode) {
        files = opencode_collector_->files_processed();
        entries = opencode_collector_->entries_parsed();
    }
    
    ImGui::Text("Files monitored: %zu", files);
    ImGui::SameLine();
    ImGui::Text("Entries parsed: %zu", entries);
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
    std::array<float, MetricsStore::kHistoryHours> rate_history{};
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
        
        ImGui::EndTable();
    }
    
    ImGui::Spacing();
    
    auto y_axis_formatter = [](double value, char* buff, int size, void*) -> int {
        if (value >= 1e9) {
            return snprintf(buff, static_cast<size_t>(size), "%.1fB", value / 1e9);
        } else if (value >= 1e6) {
            return snprintf(buff, static_cast<size_t>(size), "%.1fM", value / 1e6);
        } else if (value >= 1e3) {
            return snprintf(buff, static_cast<size_t>(size), "%.1fK", value / 1e3);
        }
        return snprintf(buff, static_cast<size_t>(size), "%.0f", value);
    };
    
    ImPlot::SetNextAxesToFit();
    if (ImPlot::BeginPlot("Token Rate (last 60h)", ImVec2(-1, 200))) {
        ImPlot::SetupAxes("Time (h)", "Tokens");
        ImPlot::SetupAxisFormat(ImAxis_Y1, y_axis_formatter, nullptr);
        ImPlot::PlotBars("tok/h", rate_history.data(), static_cast<int>(MetricsStore::kHistoryHours));
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

    for (char& c : result) {
        if (c == '/' || c == '_') {
            c = '-';
        }
    }
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
