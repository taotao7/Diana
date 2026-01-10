#pragma once

#include "claude_code_panel.h"
#include "opencode_panel.h"

namespace diana {

class AgentConfigPanel {
public:
    void render();
    
    void set_claude_panel(ClaudeCodePanel* panel) { claude_panel_ = panel; }
    void set_opencode_panel(OpenCodePanel* panel) { opencode_panel_ = panel; }

private:
    ClaudeCodePanel* claude_panel_ = nullptr;
    OpenCodePanel* opencode_panel_ = nullptr;
    int active_tab_ = 0;
};

}
