#pragma once

#include "metrics/agent_token_store.h"
#include <imgui.h>
#include <memory>
#include <array>

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
    void render_rate_chart();
    
    std::unique_ptr<AgentTokenStore> store_;
    
    AgentType selected_agent_ = AgentType::ClaudeCode;
    int selected_agent_idx_ = 0;
    
    std::array<float, 60> rate_history_{};
    uint64_t last_total_tokens_ = 0;
    std::chrono::steady_clock::time_point last_update_{};
};

}
