#include "app/app_shell.h"
#include "app/dockspace.h"
#include "ui/agent_token_panel.h"

#include "imgui.h"

namespace diana {

void AppShell::init() {
    profile_store_ = std::make_unique<ClaudeProfileStore>();
    profile_store_->load();
    profile_store_->detect_active_profile();
    
    terminal_panel_ = std::make_unique<TerminalPanel>();
    metrics_panel_ = std::make_unique<MetricsPanel>();
    claude_code_panel_ = std::make_unique<ClaudeCodePanel>();
    agent_token_panel_ = std::make_unique<AgentTokenPanel>();
    
    claude_code_panel_->set_profile_store(profile_store_.get());
    metrics_panel_->set_terminal_panel(terminal_panel_.get());
}

void AppShell::render() {
    render_dockspace(first_frame_);
    first_frame_ = false;

    terminal_panel_->render();
    metrics_panel_->render();
    claude_code_panel_->render();
    agent_token_panel_->render();
}

void AppShell::shutdown() {
}

}
