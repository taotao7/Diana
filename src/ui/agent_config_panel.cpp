#include "agent_config_panel.h"
#include "ui/agent_config_panel.h"
#include <imgui.h>

namespace diana {

void AgentConfigPanel::render(bool* is_open) {
    if (is_open && !*is_open) {
        return;
    }

    ImGui::SetNextWindowSizeConstraints(ImVec2(400, 300), ImVec2(FLT_MAX, FLT_MAX));

    if (!ImGui::Begin("Agent Config", nullptr)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("AgentConfigTabs")) {
        if (ImGui::BeginTabItem("Claude Code")) {
            active_tab_ = 0;
            if (claude_panel_) {
                claude_panel_->render_content();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Codex")) {
            active_tab_ = 1;
            if (codex_panel_) {
                codex_panel_->render_content();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("OpenCode")) {
            active_tab_ = 2;
            if (opencode_panel_) {
                opencode_panel_->render_content();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Marketplace")) {
            active_tab_ = 3;
            if (marketplace_panel_) {
                marketplace_panel_->render_content();
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

}
