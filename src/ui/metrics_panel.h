#pragma once

#include "metrics/metrics_store.h"
#include "metrics/multi_metrics_store.h"
#include "metrics/claude_usage_collector.h"
#include "metrics/opencode_usage_collector.h"
#include "terminal/terminal_panel.h"
#include <memory>

namespace diana {

class MetricsPanel {
public:
    MetricsPanel();
    ~MetricsPanel() = default;
    
    void set_terminal_panel(TerminalPanel* panel) { terminal_panel_ = panel; }
    
    void render();
    void update();

private:
    void render_active_section();
    void render_scope_selector(AppKind app, uint32_t& selected_tab_id);
    void render_stats_for_scope(AppKind app, uint32_t selected_tab_id);
    
    std::string get_project_key(AppKind app, const std::string& working_dir) const;
    std::string truncate_path(const std::string& path, size_t max_len) const;
    std::string format_tokens(uint64_t tokens) const;
    
    std::unique_ptr<MultiMetricsStore> hub_;
    std::unique_ptr<ClaudeUsageCollector> claude_collector_;
    std::unique_ptr<OpencodeUsageCollector> opencode_collector_;
    
    TerminalPanel* terminal_panel_ = nullptr;
    
    uint32_t selected_claude_tab_id_ = 0;
    uint32_t selected_opencode_tab_id_ = 0;
    
    bool show_clear_confirm_ = false;
};

}
