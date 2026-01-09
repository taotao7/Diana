#include "ui/metrics_panel.h"
#include "imgui.h"
#include "implot.h"

namespace agent47 {

MetricsPanel::MetricsPanel()
    : store_(std::make_unique<MetricsStore>())
    , collector_(std::make_unique<ClaudeUsageCollector>())
{
    collector_->set_metrics_store(store_.get());
}

void MetricsPanel::update() {
    collector_->poll();
    rate_history_ = store_->get_rate_history();
}

void MetricsPanel::render() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(250, 300), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("Token Metrics");
    
    update();
    
    auto stats = store_->compute_stats();
    
    ImGui::Text("Total Tokens: %llu", static_cast<unsigned long long>(stats.total_tokens));
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
        ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(stats.total_input));
        ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(stats.total_output));
        ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(stats.total_tokens));
        
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
    
    ImGui::Spacing();
    ImGui::Text("Files monitored: %zu", collector_->files_processed());
    ImGui::SameLine();
    ImGui::Text("Entries parsed: %zu", collector_->entries_parsed());
    
    if (ImGui::Button("Clear Stats")) {
        store_->clear();
    }
    
    ImGui::End();
}

}
