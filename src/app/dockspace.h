#pragma once

namespace diana {

struct DockspacePanels {
    bool* show_terminal = nullptr;
    bool* show_agent_config = nullptr;
    bool* show_token_metrics = nullptr;
    bool* show_agent_token_stats = nullptr;
};

void render_dockspace(bool first_frame, const DockspacePanels& panels);

}
