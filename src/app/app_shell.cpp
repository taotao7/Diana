#include "app/app_shell.h"
#include "app/dockspace.h"

#include "imgui.h"

namespace agent47 {

void AppShell::init() {
    config_manager_ = std::make_unique<ConfigManager>();
    terminal_panel_ = std::make_unique<TerminalPanel>();
    profiles_panel_ = std::make_unique<ProfilesPanel>();
    metrics_panel_ = std::make_unique<MetricsPanel>();
    profiles_panel_->set_config_manager(config_manager_.get());
}

void AppShell::render() {
    render_dockspace(first_frame_);
    first_frame_ = false;

    terminal_panel_->render();
    profiles_panel_->render();
    metrics_panel_->render();
}

void AppShell::shutdown() {
}

}
