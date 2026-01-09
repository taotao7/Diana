#pragma once

#include "metrics/agent_token_store.h"
#include <imgui.h>
#include <memory>
#include <array>
#include <map>
#include <vector>

namespace agent47 {

class AgentTokenPanel {
public:
    AgentTokenPanel();
    
    void render();
    void update();

private:
    void render_agent_selector();
    void render_summary_stats();
    void render_token_breakdown();
    void render_session_list();
    void render_heatmap();
    
    std::string truncate_session_id(const std::string& id, size_t max_len);
    ImU32 get_heatmap_color(uint64_t tokens, uint64_t max_tokens);
    
    std::unique_ptr<AgentTokenStore> store_;
    
    AgentType selected_agent_ = AgentType::ClaudeCode;
    int selected_agent_idx_ = 0;
    
    std::vector<DailyTokenData> daily_data_;
    bool show_clear_confirm_ = false;
};

}
