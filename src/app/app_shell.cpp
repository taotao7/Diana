#include "app/app_shell.h"
#include "app/dockspace.h"
#include "ui/agent_token_panel.h"

#include "imgui.h"

namespace diana {

void AppShell::init() {
    profile_store_ = std::make_unique<ClaudeProfileStore>();
    profile_store_->load();
    profile_store_->detect_active_profile();
    
    opencode_profile_store_ = std::make_unique<OpenCodeProfileStore>();
    opencode_profile_store_->load();
    opencode_profile_store_->detect_active_profile();
    
    codex_profile_store_ = std::make_unique<CodexProfileStore>();
    codex_profile_store_->load();
    codex_profile_store_->detect_active_profile();
    
    terminal_panel_ = std::make_unique<TerminalPanel>();
    metrics_panel_ = std::make_unique<MetricsPanel>();
    claude_code_panel_ = std::make_unique<ClaudeCodePanel>();
    opencode_panel_ = std::make_unique<OpenCodePanel>();
    codex_panel_ = std::make_unique<CodexPanel>();
    agent_config_panel_ = std::make_unique<AgentConfigPanel>();
    agent_token_panel_ = std::make_unique<AgentTokenPanel>();
    
    claude_code_panel_->set_profile_store(profile_store_.get());
    opencode_panel_->set_profile_store(opencode_profile_store_.get());
    codex_panel_->set_profile_store(codex_profile_store_.get());
    agent_config_panel_->set_claude_panel(claude_code_panel_.get());
    agent_config_panel_->set_opencode_panel(opencode_panel_.get());
    agent_config_panel_->set_codex_panel(codex_panel_.get());
    metrics_panel_->set_terminal_panel(terminal_panel_.get());
}

void AppShell::render() {
    render_dockspace(first_frame_);
    first_frame_ = false;

    terminal_panel_->render();
    metrics_panel_->render();
    agent_config_panel_->render();
    agent_token_panel_->render();
}

void AppShell::shutdown() {
    terminal_panel_->save_sessions();
}

}
