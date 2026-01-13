#include "claude_code_panel.h"
#include "theme.h"
#include <cstring>

namespace diana {

ClaudeCodePanel::ClaudeCodePanel() {
    new_profile_name_.resize(64);
    new_env_key_.resize(128);
    new_env_value_.resize(512);
    new_permission_rule_.resize(256);
}

void ClaudeCodePanel::render() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(280, 200), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("Claude Code");
    render_content();
    ImGui::End();
}

void ClaudeCodePanel::render_content() {
    if (!profile_store_) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "ProfileStore not initialized");
        return;
    }
    
    if (status_time_ > 0) {
        ImVec4 color = status_message_.find("Error") != std::string::npos
            ? ImVec4(1, 0.4f, 0.4f, 1) : ImVec4(0.4f, 1, 0.4f, 1);
        ImGui::TextColored(color, "%s", status_message_.c_str());
        status_time_ -= ImGui::GetIO().DeltaTime;
    }
    
    float list_width = 180.0f;
    
    ImGui::BeginChild("ClaudeProfileList", ImVec2(list_width, 0), true);
    render_profile_list();
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    ImGui::BeginChild("ClaudeConfigEditor", ImVec2(0, 0), true);
    render_config_editor();
    ImGui::EndChild();
    
    if (show_new_profile_popup_) {
        ImGui::OpenPopup("New Claude Profile");
        show_new_profile_popup_ = false;
    }
    
    if (ImGui::BeginPopupModal("New Claude Profile", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create a new configuration profile");
        ImGui::Separator();
        
        ImGui::SetNextItemWidth(200);
        ImGui::InputTextWithHint("##Name", "Profile name", new_profile_name_.data(), new_profile_name_.size());
        
        ImGui::Spacing();
        
        if (ImGui::Button("Create", ImVec2(100, 0))) {
            if (new_profile_name_[0] != '\0') {
                ClaudeCodeProfile profile;
                profile.name = new_profile_name_.data();
                profile.description = "";
                
                if (profile_store_->add_profile(profile)) {
                    select_profile(profile.name);
                    status_message_ = "Created: " + profile.name;
                    status_time_ = 2.0f;
                } else {
                    status_message_ = "Error: Name already exists";
                    status_time_ = 3.0f;
                }
                std::fill(new_profile_name_.begin(), new_profile_name_.end(), '\0');
            }
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            std::fill(new_profile_name_.begin(), new_profile_name_.end(), '\0');
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
    
    if (show_delete_confirm_) {
        ImGui::OpenPopup("Delete Claude Profile?");
        show_delete_confirm_ = false;
    }
    
    if (ImGui::BeginPopupModal("Delete Claude Profile?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete profile \"%s\"?", editing_profile_name_.c_str());
        ImGui::Text("This cannot be undone.");
        ImGui::Separator();
        
        if (ImGui::Button("Delete", ImVec2(100, 0))) {
            profile_store_->delete_profile(editing_profile_name_);
            editing_profile_name_.clear();
            config_modified_ = false;
            status_message_ = "Profile deleted";
            status_time_ = 2.0f;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void ClaudeCodePanel::render_profile_list() {
    if (ImGui::Button("+ New", ImVec2(-1, 0))) {
        show_new_profile_popup_ = true;
    }
    
    ImGui::Separator();
    
    if (ImGui::Button("Import from settings.json", ImVec2(-1, 0))) {
        import_from_settings();
    }
    
    ImGui::Separator();
    ImGui::TextDisabled("Profiles");
    
    const auto& profiles = profile_store_->profiles();
    const std::string& active = profile_store_->active_profile();
    
    if (profiles.empty()) {
        ImGui::TextDisabled("(none)");
    }
    
    for (const auto& p : profiles) {
        ImGui::PushID(p.name.c_str());
        
        bool is_active = (active == p.name);
        if (ImGui::RadioButton("##active", is_active)) {
            if (!is_active) {
                if (config_modified_ && !editing_profile_name_.empty()) {
                    save_current_profile();
                }
                if (profile_store_->set_active_profile(p.name)) {
                    status_message_ = "Activated: " + p.name;
                    status_time_ = 2.0f;
                }
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Set as active config");
        }
        
        ImGui::SameLine();
        
        bool is_selected = (editing_profile_name_ == p.name);
        if (ImGui::Selectable(p.name.c_str(), is_selected)) {
            if (config_modified_ && !editing_profile_name_.empty()) {
                save_current_profile();
            }
            select_profile(p.name);
        }
        
        if (ImGui::IsItemHovered() && !p.description.empty()) {
            ImGui::SetTooltip("%s", p.description.c_str());
        }
        
        ImGui::PopID();
    }
}

void ClaudeCodePanel::render_config_editor() {
    if (editing_profile_name_.empty()) {
        ImGui::TextDisabled("Select or create a profile to edit");
        return;
    }
    
    bool is_active = (profile_store_->active_profile() == editing_profile_name_);
    
    char name_buf[128];
    std::strncpy(name_buf, editing_profile_new_name_.c_str(), sizeof(name_buf) - 1);
    name_buf[sizeof(name_buf) - 1] = '\0';
    ImGui::Text("Name:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("##ProfileName", name_buf, sizeof(name_buf))) {
        editing_profile_new_name_ = name_buf;
        config_modified_ = true;
    }
    
    if (is_active) {
        ImGui::SameLine();
        const auto& theme = get_current_theme();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme.success), "(active)");
    }
    
    if (config_modified_) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "*");
    }
    
    if (ImGui::Button("Save")) {
        save_current_profile();
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        show_delete_confirm_ = true;
    }
    
    ImGui::Separator();
    
    char desc_buf[256];
    std::strncpy(desc_buf, editing_description_.c_str(), sizeof(desc_buf) - 1);
    desc_buf[sizeof(desc_buf) - 1] = '\0';
    ImGui::Text("Description:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextWithHint("##Desc", "Optional description", desc_buf, sizeof(desc_buf))) {
        editing_description_ = desc_buf;
        config_modified_ = true;
    }
    
    ImGui::Separator();
    
    render_basic_settings();
    render_env_section();
    render_permissions_section();
    render_sandbox_section();
    render_attribution_section();
    render_advanced_section();
}

void ClaudeCodePanel::render_basic_settings() {
    if (ImGui::CollapsingHeader("Basic Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        
        if (ImGui::Checkbox("Extended Thinking", &editing_config_.always_thinking_enabled)) {
            config_modified_ = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enable extended thinking for more thorough reasoning");
        }
        
        const char* output_styles[] = { "", "Default", "Explanatory", "Learning" };
        int style_idx = 0;
        for (int i = 0; i < 4; ++i) {
            if (editing_config_.output_style == output_styles[i]) {
                style_idx = i;
                break;
            }
        }
        ImGui::Text("Output Style:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        if (ImGui::Combo("##OutputStyle", &style_idx, output_styles, 4)) {
            editing_config_.output_style = output_styles[style_idx];
            config_modified_ = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Default: Standard responses\nExplanatory: More detailed explanations\nLearning: Educational focus");
        }
        
        char lang_buf[64];
        std::strncpy(lang_buf, editing_config_.language.c_str(), sizeof(lang_buf) - 1);
        lang_buf[sizeof(lang_buf) - 1] = '\0';
        ImGui::Text("Language:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        if (ImGui::InputTextWithHint("##Language", "english", lang_buf, sizeof(lang_buf))) {
            editing_config_.language = lang_buf;
            config_modified_ = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Response language (e.g., english, japanese, spanish)");
        }
        
        ImGui::Unindent();
    }
}

void ClaudeCodePanel::render_env_section() {
    if (ImGui::CollapsingHeader("Environment Variables")) {
        ImGui::Indent();
        
        std::vector<std::string> to_delete;
        for (auto& [key, val] : editing_config_.env) {
            ImGui::PushID(key.c_str());
            
            ImGui::Text("%s", key.c_str());
            ImGui::SameLine();
            ImGui::SetNextItemWidth(200);
            
            char val_buf[512];
            std::strncpy(val_buf, val.c_str(), sizeof(val_buf) - 1);
            val_buf[sizeof(val_buf) - 1] = '\0';
            
            bool is_secret = key.find("KEY") != std::string::npos || 
                           key.find("TOKEN") != std::string::npos ||
                           key.find("SECRET") != std::string::npos;
            
            ImGuiInputTextFlags flags = is_secret ? ImGuiInputTextFlags_Password : ImGuiInputTextFlags_None;
            if (ImGui::InputText("##val", val_buf, sizeof(val_buf), flags)) {
                val = val_buf;
                config_modified_ = true;
            }
            
            ImGui::SameLine();
            if (ImGui::Button("X")) {
                to_delete.push_back(key);
            }
            
            ImGui::PopID();
        }
        
        for (const auto& k : to_delete) {
            editing_config_.env.erase(k);
            config_modified_ = true;
        }
        
        ImGui::Spacing();
        ImGui::SetNextItemWidth(120);
        ImGui::InputTextWithHint("##NewKey", "Name", new_env_key_.data(), new_env_key_.size());
        ImGui::SameLine();
        ImGui::SetNextItemWidth(180);
        ImGui::InputTextWithHint("##NewVal", "Value", new_env_value_.data(), new_env_value_.size());
        ImGui::SameLine();
        
        bool can_add = new_env_key_[0] != '\0';
        ImGui::BeginDisabled(!can_add);
        if (ImGui::Button("Add")) {
            editing_config_.env[new_env_key_.data()] = new_env_value_.data();
            config_modified_ = true;
            std::fill(new_env_key_.begin(), new_env_key_.end(), '\0');
            std::fill(new_env_value_.begin(), new_env_value_.end(), '\0');
        }
        ImGui::EndDisabled();
        
        ImGui::Unindent();
    }
}

void ClaudeCodePanel::render_permissions_section() {
    if (ImGui::CollapsingHeader("Permissions")) {
        ImGui::Indent();
        
        const char* modes[] = { "", "default", "acceptEdits", "plan", "execute", "bypassPermissions" };
        int mode_idx = 0;
        for (int i = 0; i < 6; ++i) {
            if (editing_config_.permissions.default_mode == modes[i]) {
                mode_idx = i;
                break;
            }
        }
        ImGui::Text("Default Mode:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        if (ImGui::Combo("##DefaultMode", &mode_idx, modes, 6)) {
            editing_config_.permissions.default_mode = modes[mode_idx];
            config_modified_ = true;
        }
        
        if (ImGui::Checkbox("Disable Bypass Permissions Mode", &editing_config_.permissions.disable_bypass_permissions_mode)) {
            config_modified_ = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Prevent users from enabling bypass permissions mode");
        }
        
        render_string_list("Allow Rules", editing_config_.permissions.allow);
        render_string_list("Deny Rules", editing_config_.permissions.deny);
        render_string_list("Ask Rules", editing_config_.permissions.ask);
        render_string_list("Additional Directories", editing_config_.permissions.additional_directories);
        
        ImGui::Unindent();
    }
}

void ClaudeCodePanel::render_sandbox_section() {
    if (ImGui::CollapsingHeader("Sandbox")) {
        ImGui::Indent();
        
        if (ImGui::Checkbox("Enable Sandbox", &editing_config_.sandbox.enabled)) {
            config_modified_ = true;
        }
        
        if (ImGui::Checkbox("Auto-allow Bash if Sandboxed", &editing_config_.sandbox.auto_allow_bash_if_sandboxed)) {
            config_modified_ = true;
        }
        
        if (ImGui::Checkbox("Allow Unsandboxed Commands", &editing_config_.sandbox.allow_unsandboxed_commands)) {
            config_modified_ = true;
        }
        
        render_string_list("Excluded Commands", editing_config_.sandbox.excluded_commands);
        
        if (ImGui::TreeNode("Network Settings")) {
            if (ImGui::Checkbox("Allow Local Binding", &editing_config_.sandbox.network_allow_local_binding)) {
                config_modified_ = true;
            }
            render_string_list("Allow Unix Sockets", editing_config_.sandbox.network_allow_unix_sockets);
            ImGui::TreePop();
        }
        
        ImGui::Unindent();
    }
}

void ClaudeCodePanel::render_attribution_section() {
    if (ImGui::CollapsingHeader("Attribution")) {
        ImGui::Indent();
        
        char commit_buf[512];
        std::strncpy(commit_buf, editing_config_.attribution.commit.c_str(), sizeof(commit_buf) - 1);
        commit_buf[sizeof(commit_buf) - 1] = '\0';
        ImGui::Text("Commit:");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputTextMultiline("##CommitAttr", commit_buf, sizeof(commit_buf), ImVec2(0, 50))) {
            editing_config_.attribution.commit = commit_buf;
            config_modified_ = true;
        }
        
        char pr_buf[256];
        std::strncpy(pr_buf, editing_config_.attribution.pr.c_str(), sizeof(pr_buf) - 1);
        pr_buf[sizeof(pr_buf) - 1] = '\0';
        ImGui::Text("Pull Request:");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##PRAttr", pr_buf, sizeof(pr_buf))) {
            editing_config_.attribution.pr = pr_buf;
            config_modified_ = true;
        }
        
        ImGui::Unindent();
    }
}

void ClaudeCodePanel::render_advanced_section() {
    if (ImGui::CollapsingHeader("Advanced")) {
        ImGui::Indent();
        
        if (ImGui::Checkbox("Disable All Hooks", &editing_config_.disable_all_hooks)) {
            config_modified_ = true;
        }
        
        if (ImGui::Checkbox("Respect Gitignore", &editing_config_.respect_gitignore)) {
            config_modified_ = true;
        }
        
        char helper_buf[256];
        std::strncpy(helper_buf, editing_config_.api_key_helper.c_str(), sizeof(helper_buf) - 1);
        helper_buf[sizeof(helper_buf) - 1] = '\0';
        ImGui::Text("API Key Helper:");
        ImGui::SetNextItemWidth(200);
        if (ImGui::InputText("##ApiKeyHelper", helper_buf, sizeof(helper_buf))) {
            editing_config_.api_key_helper = helper_buf;
            config_modified_ = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Custom script/command to provide API key");
        }
        
        const char* login_methods[] = { "", "claudeai", "console" };
        int login_idx = 0;
        for (int i = 0; i < 3; ++i) {
            if (editing_config_.force_login_method == login_methods[i]) {
                login_idx = i;
                break;
            }
        }
        ImGui::Text("Force Login:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        if (ImGui::Combo("##ForceLogin", &login_idx, login_methods, 3)) {
            editing_config_.force_login_method = login_methods[login_idx];
            config_modified_ = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("claudeai: Claude Pro login\nconsole: Anthropic Console login");
        }
        
        char org_buf[128];
        std::strncpy(org_buf, editing_config_.force_login_org_uuid.c_str(), sizeof(org_buf) - 1);
        org_buf[sizeof(org_buf) - 1] = '\0';
        ImGui::Text("Org UUID:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        if (ImGui::InputTextWithHint("##OrgUUID", "Organization UUID", org_buf, sizeof(org_buf))) {
            editing_config_.force_login_org_uuid = org_buf;
            config_modified_ = true;
        }
        
        ImGui::Unindent();
    }
}

void ClaudeCodePanel::render_string_list(const char* label, std::vector<std::string>& list) {
    ImGui::PushID(label);
    
    if (ImGui::TreeNode(label)) {
        std::vector<size_t> to_delete;
        
        for (size_t i = 0; i < list.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            
            char buf[256];
            std::strncpy(buf, list[i].c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            
            ImGui::SetNextItemWidth(200);
            if (ImGui::InputText("##item", buf, sizeof(buf))) {
                list[i] = buf;
                config_modified_ = true;
            }
            
            ImGui::SameLine();
            if (ImGui::Button("X")) {
                to_delete.push_back(i);
            }
            
            ImGui::PopID();
        }
        
        for (auto it = to_delete.rbegin(); it != to_delete.rend(); ++it) {
            list.erase(list.begin() + static_cast<ptrdiff_t>(*it));
            config_modified_ = true;
        }
        
        ImGui::SetNextItemWidth(200);
        ImGui::InputTextWithHint("##new", "New entry...", new_permission_rule_.data(), new_permission_rule_.size());
        ImGui::SameLine();
        
        bool can_add = new_permission_rule_[0] != '\0';
        ImGui::BeginDisabled(!can_add);
        if (ImGui::Button("Add")) {
            list.push_back(new_permission_rule_.data());
            config_modified_ = true;
            std::fill(new_permission_rule_.begin(), new_permission_rule_.end(), '\0');
        }
        ImGui::EndDisabled();
        
        ImGui::TreePop();
    }
    
    ImGui::PopID();
}

void ClaudeCodePanel::select_profile(const std::string& name) {
    auto profile = profile_store_->get_profile(name);
    if (profile) {
        editing_profile_name_ = name;
        editing_profile_new_name_ = name;
        editing_config_ = profile->config;
        editing_description_ = profile->description;
        config_modified_ = false;
    }
}

void ClaudeCodePanel::create_new_profile() {
    show_new_profile_popup_ = true;
}

void ClaudeCodePanel::save_current_profile() {
    if (editing_profile_name_.empty()) return;
    
    bool name_changed = (editing_profile_new_name_ != editing_profile_name_);
    
    if (name_changed) {
        if (editing_profile_new_name_.empty()) {
            status_message_ = "Error: Name cannot be empty";
            status_time_ = 3.0f;
            return;
        }
        
        if (!profile_store_->rename_profile(editing_profile_name_, editing_profile_new_name_)) {
            status_message_ = "Error: Name already exists";
            status_time_ = 3.0f;
            return;
        }
        editing_profile_name_ = editing_profile_new_name_;
    }
    
    ClaudeCodeProfile profile;
    profile.name = editing_profile_name_;
    profile.description = editing_description_;
    profile.config = editing_config_;
    
    if (profile_store_->update_profile(editing_profile_name_, profile)) {
        config_modified_ = false;
        status_message_ = "Saved: " + editing_profile_name_;
        status_time_ = 2.0f;
        
        if (profile_store_->active_profile() == editing_profile_name_) {
            profile_store_->write_current_config(editing_config_);
        }
    } else {
        status_message_ = "Error: Failed to save";
        status_time_ = 3.0f;
    }
}

void ClaudeCodePanel::delete_current_profile() {
    show_delete_confirm_ = true;
}

void ClaudeCodePanel::import_from_settings() {
    ClaudeCodeConfig config = profile_store_->read_current_config();
    
    std::string base_name = "imported";
    std::string name = base_name;
    int counter = 1;
    
    while (profile_store_->get_profile(name).has_value()) {
        name = base_name + "_" + std::to_string(counter++);
    }
    
    ClaudeCodeProfile profile;
    profile.name = name;
    profile.description = "Imported from settings.json";
    profile.config = config;
    
    if (profile_store_->add_profile(profile)) {
        select_profile(name);
        status_message_ = "Imported as: " + name;
        status_time_ = 2.0f;
    } else {
        status_message_ = "Error: Failed to import";
        status_time_ = 3.0f;
    }
}

}
