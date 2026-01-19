#pragma once

#include "claude_code_panel.h"
#include "opencode_panel.h"
#include "codex_panel.h"
#include "marketplace/marketplace_panel.h"


namespace diana {

class AgentConfigPanel {
public:
    void render(bool* is_open);
    
    void set_claude_panel(ClaudeCodePanel* panel) { claude_panel_ = panel; }
    void set_opencode_panel(OpenCodePanel* panel) { opencode_panel_ = panel; }
    void set_codex_panel(CodexPanel* panel) { codex_panel_ = panel; }
    void set_marketplace_panel(MarketplacePanel* panel) { marketplace_panel_ = panel; }

private:
    ClaudeCodePanel* claude_panel_ = nullptr;
    OpenCodePanel* opencode_panel_ = nullptr;
    CodexPanel* codex_panel_ = nullptr;
    MarketplacePanel* marketplace_panel_ = nullptr;
    int active_tab_ = 0;
};


}
