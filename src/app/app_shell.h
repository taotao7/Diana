#pragma once

#include "terminal/terminal_panel.h"
#include "ui/profiles_panel.h"
#include "ui/metrics_panel.h"
#include "adapters/config_manager.h"
#include <memory>

namespace agent47 {

class AppShell {
public:
    void init();
    void render();
    void shutdown();
    
    TerminalPanel& terminal_panel() { return *terminal_panel_; }
    ProfilesPanel& profiles_panel() { return *profiles_panel_; }
    MetricsPanel& metrics_panel() { return *metrics_panel_; }
    ConfigManager& config_manager() { return *config_manager_; }

private:
    bool first_frame_ = true;
    std::unique_ptr<TerminalPanel> terminal_panel_;
    std::unique_ptr<ProfilesPanel> profiles_panel_;
    std::unique_ptr<MetricsPanel> metrics_panel_;
    std::unique_ptr<ConfigManager> config_manager_;
};

}
