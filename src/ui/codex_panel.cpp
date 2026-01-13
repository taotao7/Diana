#include "codex_panel.h"
#include "theme.h"
#include <cstring>

namespace diana {

CodexPanel::CodexPanel() {
    new_profile_name_.resize(64);
    new_provider_id_.resize(64);
    new_mcp_id_.resize(64);
}

void CodexPanel::render() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(280, 200), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("Codex");
    render_content();
    ImGui::End();
}

void CodexPanel::render_content() {
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
    
    ImGui::BeginChild("CodexProfileList", ImVec2(list_width, 0), true);
    render_profile_list();
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    ImGui::BeginChild("CodexConfigEditor", ImVec2(0, 0), true);
    render_config_editor();
    ImGui::EndChild();
    
    if (show_new_profile_popup_) {
        ImGui::OpenPopup("New Codex Profile");
        show_new_profile_popup_ = false;
    }
    
    if (ImGui::BeginPopupModal("New Codex Profile", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create a new configuration profile");
        ImGui::Separator();
        
        ImGui::SetNextItemWidth(200);
        ImGui::InputTextWithHint("##Name", "Profile name", new_profile_name_.data(), new_profile_name_.size());
        
        ImGui::Spacing();
        
        if (ImGui::Button("Create", ImVec2(100, 0))) {
            if (new_profile_name_[0] != '\0') {
                CodexProfile profile;
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
        ImGui::OpenPopup("Delete Codex Profile?");
        show_delete_confirm_ = false;
    }
    
    if (ImGui::BeginPopupModal("Delete Codex Profile?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Delete profile \"%s\"?", editing_profile_name_.c_str());
        ImGui::Text("This cannot be undone.");
        ImGui::Separator();
        
        if (ImGui::Button("Delete", ImVec2(100, 0))) {
            profile_store_->delete_profile(editing_profile_name_);
            editing_profile_name_.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void CodexPanel::render_profile_list() {
    if (ImGui::Button("+ New", ImVec2(-1, 0))) {
        show_new_profile_popup_ = true;
    }
    
    ImGui::Separator();
    
    if (ImGui::Button("Import from config.toml", ImVec2(-1, 0))) {
        import_from_config();
    }
    
    ImGui::Separator();
    ImGui::TextDisabled("Profiles");
    
    const auto& profiles = profile_store_->profiles();
    const std::string& active = profile_store_->active_profile();
    
    if (profiles.empty()) {
        ImGui::TextDisabled("(none)");
        return;
    }
    
    for (const auto& p : profiles) {
        ImGui::PushID(p.name.c_str());
        
        if (p.name == active) {
            if (ImGui::RadioButton("##active", true)) {
                if (config_modified_ && !editing_profile_name_.empty()) {
                    save_current_profile();
                }
                if (profile_store_->set_active_profile(p.name)) {
                    status_message_ = "Activated: " + p.name;
                    status_time_ = 2.0f;
                }
            }
        } else {
            if (ImGui::RadioButton("##active", false)) {
                if (config_modified_ && !editing_profile_name_.empty()) {
                    save_current_profile();
                }
                if (profile_store_->set_active_profile(p.name)) {
                    status_message_ = "Activated: " + p.name;
                    status_time_ = 2.0f;
                }
            }
        }
        
        ImGui::SameLine();
        bool is_selected = (editing_profile_name_ == p.name);
        if (ImGui::Selectable(p.name.c_str(), is_selected)) {
            if (config_modified_ && !editing_profile_name_.empty()) {
                save_current_profile();
            }
            select_profile(p.name);
        }
        
        ImGui::PopID();
    }
}

void CodexPanel::render_config_editor() {
    if (editing_profile_name_.empty()) {
        ImGui::TextDisabled("Select or create a profile to edit");
        return;
    }
    
    bool is_active = (profile_store_->active_profile() == editing_profile_name_);
    
    char name_buf[256];
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

    if (ImGui::CollapsingHeader("Basic Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        render_basic_settings();
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Model Providers")) {
        ImGui::Indent();
        render_model_providers_section();
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Features")) {
        ImGui::Indent();
        render_features_section();
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("MCP Servers")) {
        ImGui::Indent();
        render_mcp_section();
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Tools")) {
        ImGui::Indent();
        render_tools_section();
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("TUI Settings")) {
        ImGui::Indent();
        render_tui_section();
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Sandbox")) {
        ImGui::Indent();
        render_sandbox_section();
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Advanced")) {
        ImGui::Indent();
        render_advanced_section();
        ImGui::Unindent();
    }
}

void CodexPanel::render_basic_settings() {
    char buf[512];
    
    ImGui::Text("Core Settings");
    ImGui::Separator();
    
    std::strncpy(buf, editing_config_.model.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    ImGui::Text("Model:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(250);
    if (ImGui::InputText("##Model", buf, sizeof(buf))) {
        editing_config_.model = buf;
        config_modified_ = true;
    }
    
    std::strncpy(buf, editing_config_.model_provider.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    ImGui::Text("Model Provider:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(250);
    if (ImGui::InputText("##ModelProvider", buf, sizeof(buf))) {
        editing_config_.model_provider = buf;
        config_modified_ = true;
    }
    
    std::strncpy(buf, editing_config_.approval_policy.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    ImGui::Text("Approval Policy:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("##ApprovalPolicy", buf, sizeof(buf))) {
        editing_config_.approval_policy = buf;
        config_modified_ = true;
    }
    ImGui::TextDisabled("untrusted | on-failure | on-request | never");
    
    std::strncpy(buf, editing_config_.sandbox_mode.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    ImGui::Text("Sandbox Mode:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("##SandboxMode", buf, sizeof(buf))) {
        editing_config_.sandbox_mode = buf;
        config_modified_ = true;
    }
    ImGui::TextDisabled("read-only | workspace-write | danger-full-access");
    
    ImGui::Spacing();
    ImGui::Text("Optional Settings");
    ImGui::Separator();
    
    std::strncpy(buf, editing_config_.developer_instructions.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    ImGui::Text("Developer Instructions:");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::InputTextMultiline("##DevInstructions", buf, sizeof(buf), ImVec2(-1, 60))) {
        editing_config_.developer_instructions = buf;
        config_modified_ = true;
    }
    
    std::strncpy(buf, editing_config_.file_opener.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    ImGui::Text("File Opener:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("##FileOpener", buf, sizeof(buf))) {
        editing_config_.file_opener = buf;
        config_modified_ = true;
    }
    ImGui::TextDisabled("vscode | vscode-insiders | windsurf | cursor | none");
    
    if (ImGui::Checkbox("Check for Updates on Startup", &editing_config_.check_for_update_on_startup)) {
        config_modified_ = true;
    }
    
    if (ImGui::Checkbox("Hide Agent Reasoning", &editing_config_.hide_agent_reasoning)) {
        config_modified_ = true;
    }
}

void CodexPanel::render_model_providers_section() {
    ImGui::SetNextItemWidth(150);
    ImGui::InputTextWithHint("##NewProvider", "New provider ID", new_provider_id_.data(), new_provider_id_.size());
    ImGui::SameLine();
    if (ImGui::Button("Add Provider")) {
        if (new_provider_id_[0] != '\0') {
            std::string id = new_provider_id_.data();
            if (editing_config_.model_providers.find(id) == editing_config_.model_providers.end()) {
                editing_config_.model_providers[id] = CodexModelProvider{};
                config_modified_ = true;
                status_message_ = "Added provider: " + id;
                status_time_ = 2.0f;
            }
            std::fill(new_provider_id_.begin(), new_provider_id_.end(), '\0');
        }
    }
    
    ImGui::Spacing();
    
    std::vector<std::string> to_remove;
    for (auto& [id, provider] : editing_config_.model_providers) {
        ImGui::PushID(id.c_str());
        
        if (ImGui::TreeNode(id.c_str())) {
            char buf[512];
            
            std::strncpy(buf, provider.name.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            ImGui::Text("Name:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(200);
            if (ImGui::InputText("##Name", buf, sizeof(buf))) {
                provider.name = buf;
                config_modified_ = true;
            }
            
            std::strncpy(buf, provider.base_url.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            ImGui::Text("Base URL:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(250);
            if (ImGui::InputText("##BaseURL", buf, sizeof(buf))) {
                provider.base_url = buf;
                config_modified_ = true;
            }
            
            std::strncpy(buf, provider.env_key.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            ImGui::Text("Env Key:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(200);
            if (ImGui::InputText("##EnvKey", buf, sizeof(buf))) {
                provider.env_key = buf;
                config_modified_ = true;
            }
            
            std::strncpy(buf, provider.wire_api.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            ImGui::Text("Wire API:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150);
            if (ImGui::InputText("##WireAPI", buf, sizeof(buf))) {
                provider.wire_api = buf;
                config_modified_ = true;
            }
            ImGui::TextDisabled("chat | responses");
            
            if (ImGui::Checkbox("Requires OpenAI Auth", &provider.requires_openai_auth)) {
                config_modified_ = true;
            }
            
            if (ImGui::Button("Remove Provider")) {
                to_remove.push_back(id);
            }
            
            ImGui::TreePop();
        }
        
        ImGui::PopID();
    }
    
    for (const auto& id : to_remove) {
        editing_config_.model_providers.erase(id);
        config_modified_ = true;
    }
}

void CodexPanel::render_features_section() {
    if (ImGui::Checkbox("Apply Patch Freeform", &editing_config_.features.apply_patch_freeform)) {
        config_modified_ = true;
    }
    
    if (ImGui::Checkbox("Exec Policy", &editing_config_.features.exec_policy)) {
        config_modified_ = true;
    }
    
    if (ImGui::Checkbox("Remote Compaction", &editing_config_.features.remote_compaction)) {
        config_modified_ = true;
    }
    
    if (ImGui::Checkbox("Remote Models", &editing_config_.features.remote_models)) {
        config_modified_ = true;
    }
    
    if (ImGui::Checkbox("Shell Snapshot", &editing_config_.features.shell_snapshot)) {
        config_modified_ = true;
    }
    
    if (ImGui::Checkbox("Shell Tool", &editing_config_.features.shell_tool)) {
        config_modified_ = true;
    }
    
    if (ImGui::Checkbox("TUI2", &editing_config_.features.tui2)) {
        config_modified_ = true;
    }
    
    if (ImGui::Checkbox("Unified Exec", &editing_config_.features.unified_exec)) {
        config_modified_ = true;
    }
    
    if (ImGui::Checkbox("Web Search Request", &editing_config_.features.web_search_request)) {
        config_modified_ = true;
    }
}

void CodexPanel::render_mcp_section() {
    ImGui::SetNextItemWidth(150);
    ImGui::InputTextWithHint("##NewMcp", "New server ID", new_mcp_id_.data(), new_mcp_id_.size());
    ImGui::SameLine();
    if (ImGui::Button("Add Server")) {
        if (new_mcp_id_[0] != '\0') {
            std::string id = new_mcp_id_.data();
            if (editing_config_.mcp_servers.find(id) == editing_config_.mcp_servers.end()) {
                editing_config_.mcp_servers[id] = CodexMcpServer{};
                config_modified_ = true;
                status_message_ = "Added MCP server: " + id;
                status_time_ = 2.0f;
            }
            std::fill(new_mcp_id_.begin(), new_mcp_id_.end(), '\0');
        }
    }
    
    ImGui::Spacing();
    
    std::vector<std::string> to_remove;
    for (auto& [id, server] : editing_config_.mcp_servers) {
        ImGui::PushID(id.c_str());
        
        if (ImGui::TreeNode(id.c_str())) {
            char buf[512];
            
            std::strncpy(buf, server.command.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            ImGui::Text("Command:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(250);
            if (ImGui::InputText("##Command", buf, sizeof(buf))) {
                server.command = buf;
                config_modified_ = true;
            }
            
            std::strncpy(buf, server.url.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            ImGui::Text("URL:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(250);
            if (ImGui::InputText("##URL", buf, sizeof(buf))) {
                server.url = buf;
                config_modified_ = true;
            }
            
            std::strncpy(buf, server.cwd.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            ImGui::Text("Working Directory:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(200);
            if (ImGui::InputText("##CWD", buf, sizeof(buf))) {
                server.cwd = buf;
                config_modified_ = true;
            }
            
            if (ImGui::Checkbox("Enabled", &server.enabled)) {
                config_modified_ = true;
            }
            
            if (ImGui::Button("Remove Server")) {
                to_remove.push_back(id);
            }
            
            ImGui::TreePop();
        }
        
        ImGui::PopID();
    }
    
    for (const auto& id : to_remove) {
        editing_config_.mcp_servers.erase(id);
        config_modified_ = true;
    }
}

void CodexPanel::render_tools_section() {
    if (ImGui::Checkbox("Web Search", &editing_config_.tools.web_search)) {
        config_modified_ = true;
    }
}

void CodexPanel::render_tui_section() {
    if (ImGui::Checkbox("Animations", &editing_config_.tui.animations)) {
        config_modified_ = true;
    }
    
    if (ImGui::Checkbox("Show Tooltips", &editing_config_.tui.show_tooltips)) {
        config_modified_ = true;
    }
    
    if (ImGui::Checkbox("Scroll Invert", &editing_config_.tui.scroll_invert)) {
        config_modified_ = true;
    }
    
    char buf[128];
    std::strncpy(buf, editing_config_.tui.scroll_mode.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    ImGui::Text("Scroll Mode:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputText("##ScrollMode", buf, sizeof(buf))) {
        editing_config_.tui.scroll_mode = buf;
        config_modified_ = true;
    }
    ImGui::TextDisabled("auto | wheel | trackpad");
}

void CodexPanel::render_sandbox_section() {
    if (ImGui::Checkbox("Exclude /tmp", &editing_config_.sandbox_workspace_write.exclude_slash_tmp)) {
        config_modified_ = true;
    }
    
    if (ImGui::Checkbox("Exclude $TMPDIR", &editing_config_.sandbox_workspace_write.exclude_tmpdir_env_var)) {
        config_modified_ = true;
    }
    
    if (ImGui::Checkbox("Network Access", &editing_config_.sandbox_workspace_write.network_access)) {
        config_modified_ = true;
    }
    
    ImGui::Spacing();
    ImGui::Text("Writable Roots");
    render_string_list("##WritableRoots", editing_config_.sandbox_workspace_write.writable_roots);
}

void CodexPanel::render_advanced_section() {
    char buf[512];
    
    std::strncpy(buf, editing_config_.profile.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    ImGui::Text("Default Profile:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("##DefaultProfile", buf, sizeof(buf))) {
        editing_config_.profile = buf;
        config_modified_ = true;
    }
    
    std::strncpy(buf, editing_config_.review_model.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    ImGui::Text("Review Model:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("##ReviewModel", buf, sizeof(buf))) {
        editing_config_.review_model = buf;
        config_modified_ = true;
    }
    
    std::strncpy(buf, editing_config_.oss_provider.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    ImGui::Text("OSS Provider:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    if (ImGui::InputText("##OSSProvider", buf, sizeof(buf))) {
        editing_config_.oss_provider = buf;
        config_modified_ = true;
    }
    ImGui::TextDisabled("lmstudio | ollama");
    
    ImGui::Spacing();
    ImGui::Text("History");
    ImGui::Separator();
    
    std::strncpy(buf, editing_config_.history.persistence.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    ImGui::Text("Persistence:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputText("##Persistence", buf, sizeof(buf))) {
        editing_config_.history.persistence = buf;
        config_modified_ = true;
    }
    ImGui::TextDisabled("save-all | none");
    
    ImGui::Spacing();
    ImGui::Text("Feedback");
    ImGui::Separator();
    
    if (ImGui::Checkbox("Feedback Enabled", &editing_config_.feedback.enabled)) {
        config_modified_ = true;
    }
}

void CodexPanel::render_string_list(const char* label, std::vector<std::string>& list) {
    ImGui::PushID(label);
    
    static char new_item[256] = "";
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##NewItem", "Add new item", new_item, sizeof(new_item));
    ImGui::SameLine();
    if (ImGui::Button("Add")) {
        if (new_item[0] != '\0') {
            list.push_back(new_item);
            new_item[0] = '\0';
            config_modified_ = true;
        }
    }
    
    ImGui::Spacing();
    
    int to_remove = -1;
    for (size_t i = 0; i < list.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        
        char buf[256];
        std::strncpy(buf, list[i].c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        
        ImGui::SetNextItemWidth(200);
        if (ImGui::InputText("##Item", buf, sizeof(buf))) {
            list[i] = buf;
            config_modified_ = true;
        }
        
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            to_remove = static_cast<int>(i);
        }
        ImGui::PopID();
    }
    
    if (to_remove >= 0) {
        list.erase(list.begin() + to_remove);
        config_modified_ = true;
    }
    
    ImGui::PopID();
}

void CodexPanel::select_profile(const std::string& name) {
    auto profile = profile_store_->get_profile(name);
    if (profile) {
        editing_profile_name_ = name;
        editing_profile_new_name_ = name;
        editing_config_ = profile->config;
        editing_description_ = profile->description;
        config_modified_ = false;
    }
}

void CodexPanel::create_new_profile() {
    show_new_profile_popup_ = true;
}

void CodexPanel::save_current_profile() {
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
    
    CodexProfile profile;
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

void CodexPanel::delete_current_profile() {
    show_delete_confirm_ = true;
}

void CodexPanel::import_from_config() {
    CodexConfig config = profile_store_->read_current_config();
    
    std::string name = "Imported";
    int counter = 1;
    
    while (profile_store_->get_profile(name).has_value()) {
        name = "Imported " + std::to_string(counter++);
    }
    
    CodexProfile profile;
    profile.name = name;
    profile.description = "Imported from config.toml";
    profile.config = config;
    
    if (profile_store_->add_profile(profile)) {
        select_profile(name);
        status_message_ = "Imported: " + name;
        status_time_ = 2.0f;
    }
}

}
