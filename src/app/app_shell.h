#pragma once

#include "terminal/terminal_panel.h"
#include "ui/metrics_panel.h"
#include "ui/claude_code_panel.h"
#include "ui/opencode_panel.h"
#include "ui/codex_panel.h"
#include "ui/agent_config_panel.h"
#include "ui/agent_token_panel.h"
#include "adapters/claude_profile_store.h"
#include "adapters/opencode_profile_store.h"
#include "adapters/codex_profile_store.h"
#include <memory>

namespace diana {

class AppShell {
public:
    void init();
    void render();
    void shutdown();
    
    TerminalPanel& terminal_panel() { return *terminal_panel_; }
    MetricsPanel& metrics_panel() { return *metrics_panel_; }
    ClaudeCodePanel& claude_code_panel() { return *claude_code_panel_; }
    OpenCodePanel& opencode_panel() { return *opencode_panel_; }
    CodexPanel& codex_panel() { return *codex_panel_; }
    AgentConfigPanel& agent_config_panel() { return *agent_config_panel_; }
    AgentTokenPanel& agent_token_panel() { return *agent_token_panel_; }
    ClaudeProfileStore& profile_store() { return *profile_store_; }
    OpenCodeProfileStore& opencode_profile_store() { return *opencode_profile_store_; }
    CodexProfileStore& codex_profile_store() { return *codex_profile_store_; }

private:
    bool first_frame_ = true;
    std::unique_ptr<TerminalPanel> terminal_panel_;
    std::unique_ptr<MetricsPanel> metrics_panel_;
    std::unique_ptr<ClaudeCodePanel> claude_code_panel_;
    std::unique_ptr<OpenCodePanel> opencode_panel_;
    std::unique_ptr<CodexPanel> codex_panel_;
    std::unique_ptr<AgentConfigPanel> agent_config_panel_;
    std::unique_ptr<AgentTokenPanel> agent_token_panel_;
    std::unique_ptr<ClaudeProfileStore> profile_store_;
    std::unique_ptr<OpenCodeProfileStore> opencode_profile_store_;
    std::unique_ptr<CodexProfileStore> codex_profile_store_;
};

}
