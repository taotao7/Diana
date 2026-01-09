#pragma once

#include "adapters/profile_store.h"
#include <imgui.h>
#include <array>

namespace agent47 {

class ClaudeCodePanel {
public:
    ClaudeCodePanel();
    
    void render();
    
    void set_profile_store(ProfileStore* store) { profile_store_ = store; }
    
    const std::string& selected_profile_for_launch() const { return editing_profile_name_; }

private:
    void render_profile_list();
    void render_config_editor();
    void render_basic_settings();
    void render_env_section();
    void render_permissions_section();
    void render_sandbox_section();
    void render_attribution_section();
    void render_advanced_section();
    
    void select_profile(const std::string& name);
    void create_new_profile();
    void save_current_profile();
    void delete_current_profile();
    void import_from_settings();
    
    void render_string_list(const char* label, std::vector<std::string>& list);
    
    ProfileStore* profile_store_ = nullptr;
    
    std::string editing_profile_name_;
    std::string editing_profile_new_name_;
    ClaudeCodeConfig editing_config_;
    std::string editing_description_;
    bool config_modified_ = false;
    
    std::string new_profile_name_;
    std::string new_env_key_;
    std::string new_env_value_;
    std::string new_permission_rule_;
    
    std::string status_message_;
    float status_time_ = 0.0f;
    
    bool show_new_profile_popup_ = false;
    bool show_delete_confirm_ = false;
};

}
