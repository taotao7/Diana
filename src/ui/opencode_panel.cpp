#include "opencode_panel.h"
#include "theme.h"
#include <cstring>

namespace diana {

OpenCodePanel::OpenCodePanel() {
    new_profile_name_.resize(64);
    new_provider_id_.resize(64);
    new_agent_id_.resize(64);
    new_command_.resize(256);
    new_path_.resize(256);
    new_tool_id_.resize(64);
    new_mcp_id_.resize(64);
}

void OpenCodePanel::render() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(280, 200), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("OpenCode");
    render_content();
    ImGui::End();
}

void OpenCodePanel::render_content() {
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
    
    ImGui::BeginChild("OpenCodeProfileList", ImVec2(list_width, 0), true);
    render_profile_list();
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    ImGui::BeginChild("OpenCodeConfigEditor", ImVec2(0, 0), true);
    render_config_editor();
    ImGui::EndChild();
    
    if (show_new_profile_popup_) {
        ImGui::OpenPopup("New OpenCode Profile");
        show_new_profile_popup_ = false;
    }
    
    if (ImGui::BeginPopupModal("New OpenCode Profile", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Create a new configuration profile");
        ImGui::Separator();
        
        ImGui::SetNextItemWidth(200);
        ImGui::InputTextWithHint("##Name", "Profile name", new_profile_name_.data(), new_profile_name_.size());
        
        ImGui::Spacing();
        
        if (ImGui::Button("Create", ImVec2(100, 0))) {
            if (new_profile_name_[0] != '\0') {
                OpenCodeProfile profile;
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
        ImGui::OpenPopup("Delete OpenCode Profile?");
        show_delete_confirm_ = false;
    }
    
    if (ImGui::BeginPopupModal("Delete OpenCode Profile?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
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

void OpenCodePanel::render_profile_list() {
    if (ImGui::Button("+ New", ImVec2(-1, 0))) {
        show_new_profile_popup_ = true;
    }
    
    ImGui::Separator();
    
    if (ImGui::Button("Import from opencode.json", ImVec2(-1, 0))) {
        import_from_config();
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

void OpenCodePanel::render_config_editor() {
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
    render_providers_section();
    render_agents_section();
    render_tools_section();
    render_permissions_section();
    render_mcp_section();
    render_tui_section();
    render_advanced_section();
}

void OpenCodePanel::render_basic_settings() {
    if (ImGui::CollapsingHeader("Basic Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        
        char model_buf[256];
        std::strncpy(model_buf, editing_config_.model.c_str(), sizeof(model_buf) - 1);
        model_buf[sizeof(model_buf) - 1] = '\0';
        ImGui::Text("Model:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(250);
        if (ImGui::InputTextWithHint("##Model", "anthropic/claude-sonnet-4-20250514", model_buf, sizeof(model_buf))) {
            editing_config_.model = model_buf;
            config_modified_ = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Format: provider/model\nExamples:\n  anthropic/claude-sonnet-4-20250514\n  google/gemini-2.5-pro\n  openai/gpt-4o");
        }
        
        char small_model_buf[256];
        std::strncpy(small_model_buf, editing_config_.small_model.c_str(), sizeof(small_model_buf) - 1);
        small_model_buf[sizeof(small_model_buf) - 1] = '\0';
        ImGui::Text("Small Model:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(250);
        if (ImGui::InputTextWithHint("##SmallModel", "anthropic/claude-haiku-4-5", small_model_buf, sizeof(small_model_buf))) {
            editing_config_.small_model = small_model_buf;
            config_modified_ = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Lightweight model for title generation etc.");
        }
        
        char theme_buf[64];
        std::strncpy(theme_buf, editing_config_.theme.c_str(), sizeof(theme_buf) - 1);
        theme_buf[sizeof(theme_buf) - 1] = '\0';
        ImGui::Text("Theme:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        if (ImGui::InputTextWithHint("##Theme", "opencode", theme_buf, sizeof(theme_buf))) {
            editing_config_.theme = theme_buf;
            config_modified_ = true;
        }
        
        char agent_buf[64];
        std::strncpy(agent_buf, editing_config_.default_agent.c_str(), sizeof(agent_buf) - 1);
        agent_buf[sizeof(agent_buf) - 1] = '\0';
        ImGui::Text("Default Agent:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        if (ImGui::InputTextWithHint("##DefaultAgent", "build", agent_buf, sizeof(agent_buf))) {
            editing_config_.default_agent = agent_buf;
            config_modified_ = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Default agent when none specified (e.g., build, plan)");
        }
        
        render_string_list("Instructions", editing_config_.instructions);
        
        ImGui::Unindent();
    }
}

void OpenCodePanel::render_providers_section() {
    if (ImGui::CollapsingHeader("Providers")) {
        ImGui::Indent();
        
        std::vector<std::string> to_delete;
        for (auto& [id, provider] : editing_config_.providers) {
            ImGui::PushID(id.c_str());
            
            bool open = ImGui::TreeNode(id.c_str());
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
            if (ImGui::SmallButton("X")) {
                to_delete.push_back(id);
            }
            
            if (open) {
                char key_buf[256];
                std::strncpy(key_buf, provider.api_key.c_str(), sizeof(key_buf) - 1);
                key_buf[sizeof(key_buf) - 1] = '\0';
                ImGui::Text("API Key:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(200);
                if (ImGui::InputTextWithHint("##apikey", "{env:PROVIDER_API_KEY}", key_buf, sizeof(key_buf))) {
                    provider.api_key = key_buf;
                    config_modified_ = true;
                }
                
                char url_buf[256];
                std::strncpy(url_buf, provider.base_url.c_str(), sizeof(url_buf) - 1);
                url_buf[sizeof(url_buf) - 1] = '\0';
                ImGui::Text("Base URL:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(200);
                if (ImGui::InputTextWithHint("##baseurl", "https://api.example.com", url_buf, sizeof(url_buf))) {
                    provider.base_url = url_buf;
                    config_modified_ = true;
                }
                
                ImGui::TreePop();
            }
            
            ImGui::PopID();
        }
        
        for (const auto& id : to_delete) {
            editing_config_.providers.erase(id);
            config_modified_ = true;
        }
        
        ImGui::Spacing();
        ImGui::SetNextItemWidth(120);
        ImGui::InputTextWithHint("##NewProvider", "provider id", new_provider_id_.data(), new_provider_id_.size());
        ImGui::SameLine();
        
        bool can_add = new_provider_id_[0] != '\0';
        ImGui::BeginDisabled(!can_add);
        if (ImGui::Button("Add Provider")) {
            OpenCodeProviderConfig pc;
            pc.id = new_provider_id_.data();
            editing_config_.providers[pc.id] = pc;
            config_modified_ = true;
            std::fill(new_provider_id_.begin(), new_provider_id_.end(), '\0');
        }
        ImGui::EndDisabled();
        
        ImGui::TextDisabled("Common: anthropic, openai, google, amazon-bedrock");
        
        render_string_list("Disabled Providers", editing_config_.disabled_providers);
        render_string_list("Enabled Providers", editing_config_.enabled_providers);
        
        ImGui::Unindent();
    }
}

void OpenCodePanel::render_agents_section() {
    if (ImGui::CollapsingHeader("Agents")) {
        ImGui::Indent();
        
        std::vector<std::string> to_delete;
        for (auto& [id, agent] : editing_config_.agents) {
            ImGui::PushID(id.c_str());
            
            bool open = ImGui::TreeNode(id.c_str());
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
            if (ImGui::SmallButton("X")) {
                to_delete.push_back(id);
            }
            
            if (open) {
                char model_buf[256];
                std::strncpy(model_buf, agent.model.c_str(), sizeof(model_buf) - 1);
                model_buf[sizeof(model_buf) - 1] = '\0';
                ImGui::Text("Model:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(200);
                if (ImGui::InputTextWithHint("##model", "provider/model (empty = default)", model_buf, sizeof(model_buf))) {
                    agent.model = model_buf;
                    config_modified_ = true;
                }
                
                char desc_buf[256];
                std::strncpy(desc_buf, agent.description.c_str(), sizeof(desc_buf) - 1);
                desc_buf[sizeof(desc_buf) - 1] = '\0';
                ImGui::Text("Description:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(200);
                if (ImGui::InputText("##desc", desc_buf, sizeof(desc_buf))) {
                    agent.description = desc_buf;
                    config_modified_ = true;
                }
                
                if (ImGui::Checkbox("Disabled", &agent.disabled)) {
                    config_modified_ = true;
                }
                
                char prompt_buf[1024];
                std::strncpy(prompt_buf, agent.prompt.c_str(), sizeof(prompt_buf) - 1);
                prompt_buf[sizeof(prompt_buf) - 1] = '\0';
                ImGui::Text("Prompt:");
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputTextMultiline("##prompt", prompt_buf, sizeof(prompt_buf), ImVec2(0, 60))) {
                    agent.prompt = prompt_buf;
                    config_modified_ = true;
                }
                
                ImGui::TreePop();
            }
            
            ImGui::PopID();
        }
        
        for (const auto& id : to_delete) {
            editing_config_.agents.erase(id);
            config_modified_ = true;
        }
        
        ImGui::Spacing();
        ImGui::SetNextItemWidth(120);
        ImGui::InputTextWithHint("##NewAgent", "agent id", new_agent_id_.data(), new_agent_id_.size());
        ImGui::SameLine();
        
        bool can_add = new_agent_id_[0] != '\0';
        ImGui::BeginDisabled(!can_add);
        if (ImGui::Button("Add Agent")) {
            OpenCodeAgentConfig ac;
            editing_config_.agents[new_agent_id_.data()] = ac;
            config_modified_ = true;
            std::fill(new_agent_id_.begin(), new_agent_id_.end(), '\0');
        }
        ImGui::EndDisabled();
        
        ImGui::TextDisabled("Built-in agents: build, plan");
        
        ImGui::Unindent();
    }
}

void OpenCodePanel::render_tools_section() {
    if (ImGui::CollapsingHeader("Tools")) {
        ImGui::Indent();
        
        ImGui::TextDisabled("Enable/disable tools for the agent");
        
        std::vector<std::string> to_delete;
        for (auto& [id, enabled] : editing_config_.tools) {
            ImGui::PushID(id.c_str());
            
            if (ImGui::Checkbox(id.c_str(), &enabled)) {
                config_modified_ = true;
            }
            
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
            if (ImGui::SmallButton("X")) {
                to_delete.push_back(id);
            }
            
            ImGui::PopID();
        }
        
        for (const auto& id : to_delete) {
            editing_config_.tools.erase(id);
            config_modified_ = true;
        }
        
        ImGui::Spacing();
        ImGui::SetNextItemWidth(120);
        ImGui::InputTextWithHint("##NewTool", "tool name", new_tool_id_.data(), new_tool_id_.size());
        ImGui::SameLine();
        
        bool can_add = new_tool_id_[0] != '\0';
        ImGui::BeginDisabled(!can_add);
        if (ImGui::Button("Add Tool")) {
            editing_config_.tools[new_tool_id_.data()] = true;
            config_modified_ = true;
            std::fill(new_tool_id_.begin(), new_tool_id_.end(), '\0');
        }
        ImGui::EndDisabled();
        
        ImGui::TextDisabled("Common tools: write, bash, edit, read, glob, grep");
        
        ImGui::Unindent();
    }
}

void OpenCodePanel::render_permissions_section() {
    if (ImGui::CollapsingHeader("Permissions")) {
        ImGui::Indent();
        
        ImGui::TextDisabled("Tool permissions (ask/allow)");
        
        std::vector<std::string> to_delete;
        for (auto& [tool, perm] : editing_config_.permission_tools) {
            ImGui::PushID(tool.c_str());
            
            ImGui::Text("%s:", tool.c_str());
            ImGui::SameLine();
            
            const char* options[] = { "ask", "allow" };
            int current = (perm == "allow") ? 1 : 0;
            ImGui::SetNextItemWidth(80);
            if (ImGui::Combo("##perm", &current, options, 2)) {
                perm = options[current];
                config_modified_ = true;
            }
            
            ImGui::SameLine();
            if (ImGui::SmallButton("X")) {
                to_delete.push_back(tool);
            }
            
            ImGui::PopID();
        }
        
        for (const auto& id : to_delete) {
            editing_config_.permission_tools.erase(id);
            config_modified_ = true;
        }
        
        ImGui::Spacing();
        static char new_perm_tool[64] = "";
        ImGui::SetNextItemWidth(120);
        ImGui::InputTextWithHint("##NewPermTool", "tool name", new_perm_tool, sizeof(new_perm_tool));
        ImGui::SameLine();
        
        bool can_add = new_perm_tool[0] != '\0';
        ImGui::BeginDisabled(!can_add);
        if (ImGui::Button("Add Permission")) {
            editing_config_.permission_tools[new_perm_tool] = "ask";
            config_modified_ = true;
            new_perm_tool[0] = '\0';
        }
        ImGui::EndDisabled();
        
        ImGui::TextDisabled("Example: edit=ask, bash=ask");
        
        ImGui::Unindent();
    }
}

void OpenCodePanel::render_mcp_section() {
    if (ImGui::CollapsingHeader("MCP Servers")) {
        ImGui::Indent();
        
        std::vector<std::string> to_delete;
        for (auto& [id, server] : editing_config_.mcp) {
            ImGui::PushID(id.c_str());
            
            bool open = ImGui::TreeNode(id.c_str());
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20);
            if (ImGui::SmallButton("X")) {
                to_delete.push_back(id);
            }
            
            if (open) {
                const char* type_options[] = { "local", "remote" };
                int type_idx = (server.type == "remote") ? 1 : 0;
                ImGui::Text("Type:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100);
                if (ImGui::Combo("##type", &type_idx, type_options, 2)) {
                    server.type = type_options[type_idx];
                    config_modified_ = true;
                }
                
                if (server.type == "remote") {
                    char url_buf[256];
                    std::strncpy(url_buf, server.url.c_str(), sizeof(url_buf) - 1);
                    url_buf[sizeof(url_buf) - 1] = '\0';
                    ImGui::Text("URL:");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(200);
                    if (ImGui::InputText("##url", url_buf, sizeof(url_buf))) {
                        server.url = url_buf;
                        config_modified_ = true;
                    }
                } else {
                    char cmd_buf[256];
                    std::strncpy(cmd_buf, server.command.c_str(), sizeof(cmd_buf) - 1);
                    cmd_buf[sizeof(cmd_buf) - 1] = '\0';
                    ImGui::Text("Command:");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(200);
                    if (ImGui::InputText("##cmd", cmd_buf, sizeof(cmd_buf))) {
                        server.command = cmd_buf;
                        config_modified_ = true;
                    }
                }
                
                if (ImGui::Checkbox("Enabled", &server.enabled)) {
                    config_modified_ = true;
                }
                
                ImGui::TreePop();
            }
            
            ImGui::PopID();
        }
        
        for (const auto& id : to_delete) {
            editing_config_.mcp.erase(id);
            config_modified_ = true;
        }
        
        ImGui::Spacing();
        ImGui::SetNextItemWidth(120);
        ImGui::InputTextWithHint("##NewMcp", "server name", new_mcp_id_.data(), new_mcp_id_.size());
        ImGui::SameLine();
        
        bool can_add = new_mcp_id_[0] != '\0';
        ImGui::BeginDisabled(!can_add);
        if (ImGui::Button("Add MCP Server")) {
            OpenCodeMcpServer server;
            server.type = "local";
            editing_config_.mcp[new_mcp_id_.data()] = server;
            config_modified_ = true;
            std::fill(new_mcp_id_.begin(), new_mcp_id_.end(), '\0');
        }
        ImGui::EndDisabled();
        
        ImGui::Unindent();
    }
}

void OpenCodePanel::render_tui_section() {
    if (ImGui::CollapsingHeader("TUI Settings")) {
        ImGui::Indent();
        
        if (ImGui::SliderInt("Scroll Speed", &editing_config_.tui.scroll_speed, 1, 10)) {
            config_modified_ = true;
        }
        
        if (ImGui::Checkbox("Scroll Acceleration", &editing_config_.tui.scroll_acceleration)) {
            config_modified_ = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("macOS-style scroll acceleration (overrides scroll speed)");
        }
        
        const char* diff_options[] = { "auto", "stacked" };
        int diff_idx = (editing_config_.tui.diff_style == "stacked") ? 1 : 0;
        ImGui::Text("Diff Style:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        if (ImGui::Combo("##diffstyle", &diff_idx, diff_options, 2)) {
            editing_config_.tui.diff_style = diff_options[diff_idx];
            config_modified_ = true;
        }
        
        ImGui::Unindent();
    }
}

void OpenCodePanel::render_advanced_section() {
    if (ImGui::CollapsingHeader("Advanced")) {
        ImGui::Indent();
        
        const char* share_options[] = { "manual", "auto", "disabled" };
        int share_idx = 0;
        if (editing_config_.share == "auto") share_idx = 1;
        else if (editing_config_.share == "disabled") share_idx = 2;
        ImGui::Text("Share Mode:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        if (ImGui::Combo("##share", &share_idx, share_options, 3)) {
            editing_config_.share = share_options[share_idx];
            config_modified_ = true;
        }
        
        const char* update_options[] = { "true", "false", "notify" };
        int update_idx = 0;
        if (editing_config_.autoupdate == "false") update_idx = 1;
        else if (editing_config_.autoupdate == "notify") update_idx = 2;
        ImGui::Text("Auto Update:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        if (ImGui::Combo("##autoupdate", &update_idx, update_options, 3)) {
            editing_config_.autoupdate = update_options[update_idx];
            config_modified_ = true;
        }
        
        if (ImGui::TreeNode("Compaction")) {
            if (ImGui::Checkbox("Auto Compact", &editing_config_.compaction.auto_compact)) {
                config_modified_ = true;
            }
            if (ImGui::Checkbox("Prune Old Outputs", &editing_config_.compaction.prune)) {
                config_modified_ = true;
            }
            ImGui::TreePop();
        }
        
        render_string_list("Watcher Ignore", editing_config_.watcher_ignore);
        
        ImGui::Unindent();
    }
}

void OpenCodePanel::render_string_list(const char* label, std::vector<std::string>& list) {
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
        ImGui::InputTextWithHint("##new", "New entry...", new_command_.data(), new_command_.size());
        ImGui::SameLine();
        
        bool can_add = new_command_[0] != '\0';
        ImGui::BeginDisabled(!can_add);
        if (ImGui::Button("Add")) {
            list.push_back(new_command_.data());
            config_modified_ = true;
            std::fill(new_command_.begin(), new_command_.end(), '\0');
        }
        ImGui::EndDisabled();
        
        ImGui::TreePop();
    }
    
    ImGui::PopID();
}

void OpenCodePanel::select_profile(const std::string& name) {
    auto profile = profile_store_->get_profile(name);
    if (profile) {
        editing_profile_name_ = name;
        editing_profile_new_name_ = name;
        editing_config_ = profile->config;
        editing_description_ = profile->description;
        config_modified_ = false;
    }
}

void OpenCodePanel::create_new_profile() {
    show_new_profile_popup_ = true;
}

void OpenCodePanel::save_current_profile() {
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
    
    OpenCodeProfile profile;
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

void OpenCodePanel::delete_current_profile() {
    show_delete_confirm_ = true;
}

void OpenCodePanel::import_from_config() {
    OpenCodeConfig config = profile_store_->read_current_config();
    
    std::string base_name = "imported";
    std::string name = base_name;
    int counter = 1;
    
    while (profile_store_->get_profile(name).has_value()) {
        name = base_name + "_" + std::to_string(counter++);
    }
    
    OpenCodeProfile profile;
    profile.name = name;
    profile.description = "Imported from opencode.json";
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
