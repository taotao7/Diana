#include "agent_config_panel.h"
#include <imgui.h>

namespace diana {

void AgentConfigPanel::render() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(400, 300), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("Agent Config");
    
    if (ImGui::BeginTabBar("AgentConfigTabs")) {
        if (ImGui::BeginTabItem("Claude Code")) {
            active_tab_ = 0;
            if (claude_panel_) {
                claude_panel_->render_content();
            }
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("OpenCode")) {
            active_tab_ = 1;
            if (opencode_panel_) {
                opencode_panel_->render_content();
            }
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }
    
    ImGui::End();
}

}
