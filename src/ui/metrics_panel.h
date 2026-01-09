#pragma once

#include "metrics/metrics_store.h"
#include "metrics/multi_metrics_store.h"
#include "metrics/claude_usage_collector.h"
#include "terminal/terminal_panel.h"
#include <memory>
#include <unordered_map>

namespace diana {

class MetricsPanel {
public:
    MetricsPanel();
    
    void set_terminal_panel(TerminalPanel* panel) { terminal_panel_ = panel; }
    
    void render();
    void update();

private:
    void render_scope_selector();
    void render_stats_for_scope();
    bool has_claude_sessions() const;
    const TerminalSession* get_active_claude_session() const;
    
    std::string get_claude_project_key(const std::string& working_dir) const;
    std::string truncate_path(const std::string& path, size_t max_len) const;
    std::string format_tokens(uint64_t tokens) const;
    
    std::unique_ptr<MultiMetricsStore> hub_;
    std::unique_ptr<ClaudeUsageCollector> collector_;
    
    TerminalPanel* terminal_panel_ = nullptr;
    
    uint32_t selected_tab_id_ = 0;
    
    std::array<float, 60> rate_history_{};
    bool show_clear_confirm_ = false;
};

}
