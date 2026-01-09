#pragma once

#include "terminal/terminal_panel.h"
#include "ui/metrics_panel.h"
#include "ui/claude_code_panel.h"
#include "adapters/profile_store.h"
#include <memory>

namespace agent47 {

class AppShell {
public:
    void init();
    void render();
    void shutdown();
    
    TerminalPanel& terminal_panel() { return *terminal_panel_; }
    MetricsPanel& metrics_panel() { return *metrics_panel_; }
    ClaudeCodePanel& claude_code_panel() { return *claude_code_panel_; }
    ProfileStore& profile_store() { return *profile_store_; }

private:
    bool first_frame_ = true;
    std::unique_ptr<TerminalPanel> terminal_panel_;
    std::unique_ptr<MetricsPanel> metrics_panel_;
    std::unique_ptr<ClaudeCodePanel> claude_code_panel_;
    std::unique_ptr<ProfileStore> profile_store_;
};

}
