#pragma once

#include "adapters/opencode_profile_store.h"
#include <imgui.h>
#include <array>
#include <map>

namespace diana {

class OpenCodePanel {
public:
    OpenCodePanel();
    
    void render();
    void render_content();
    
    void set_profile_store(OpenCodeProfileStore* store) { profile_store_ = store; }
    
    const std::string& selected_profile_for_launch() const { return editing_profile_name_; }

private:
    void render_profile_list();
    void render_config_editor();
    void render_basic_settings();
    void render_providers_section();
    void render_agents_section();
    void render_permissions_section();
    void render_tools_section();
    void render_mcp_section();
    void render_tui_section();
    void render_advanced_section();
    
    void select_profile(const std::string& name);
    void create_new_profile();
    void save_current_profile();
    void delete_current_profile();
    void import_from_config();
    
    void render_string_list(const char* label, std::vector<std::string>& list);
    
    OpenCodeProfileStore* profile_store_ = nullptr;
    
    std::string editing_profile_name_;
    std::string editing_profile_new_name_;
    OpenCodeConfig editing_config_;
    std::string editing_description_;
    bool config_modified_ = false;
    
    std::string new_profile_name_;
    std::string new_provider_id_;
    std::string new_agent_id_;
    std::string new_command_;
    std::string new_path_;
    std::string new_tool_id_;
    std::string new_mcp_id_;
    
    std::map<std::string, std::string> new_model_id_buffers_;
    
    std::string status_message_;
    float status_time_ = 0.0f;
    
    bool show_new_profile_popup_ = false;
    bool show_delete_confirm_ = false;
};

}
