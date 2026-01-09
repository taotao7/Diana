#pragma once

#include "metrics/agent_token_store.h"
#include <imgui.h>
#include <memory>
#include <array>
#include <map>

namespace agent47 {

struct MonthlyTokenData {
    uint64_t tokens = 0;
    double cost = 0.0;
    size_t session_count = 0;
};

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
    void render_monthly_chart();
    
    std::string truncate_session_id(const std::string& id, size_t max_len);
    
    std::unique_ptr<AgentTokenStore> store_;
    
    AgentType selected_agent_ = AgentType::ClaudeCode;
    int selected_agent_idx_ = 0;
    
    std::map<std::string, MonthlyTokenData> monthly_data_;
    bool show_clear_confirm_ = false;
};

}
