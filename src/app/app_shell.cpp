#include "app/app_shell.h"
#include "app/dockspace.h"

#include "imgui.h"

namespace agent47 {

void AppShell::init() {
    profile_store_ = std::make_unique<ProfileStore>();
    profile_store_->load();
    
    terminal_panel_ = std::make_unique<TerminalPanel>();
    metrics_panel_ = std::make_unique<MetricsPanel>();
    claude_code_panel_ = std::make_unique<ClaudeCodePanel>();
    
    claude_code_panel_->set_profile_store(profile_store_.get());
}

void AppShell::render() {
    render_dockspace(first_frame_);
    first_frame_ = false;

    terminal_panel_->render();
    metrics_panel_->render();
    claude_code_panel_->render();
}

void AppShell::shutdown() {
}

}
