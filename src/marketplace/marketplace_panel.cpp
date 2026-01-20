#include "marketplace_panel.h"
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <toml++/toml.hpp>
#include <array>
#include <cstdio>
#include <thread>
#include <mutex>

namespace diana {

static void RenderSplitter(const char* id, float* width_ptr, float min_width) {
    ImGui::InvisibleButton(id, ImVec2(4.0f, -1.0f));
    if (ImGui::IsItemActive()) {
        *width_ptr -= ImGui::GetIO().MouseDelta.x;
        if (*width_ptr < min_width) *width_ptr = min_width;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
}

MarketplacePanel::MarketplacePanel() {
    search_buffer_.resize(256, '\0');
    api_key_buffer_.resize(256, '\0');
    
    auto settings = settings_store_.load();
    if (!settings.smithery_api_key.empty()) {
        std::strncpy(api_key_buffer_.data(), settings.smithery_api_key.c_str(), api_key_buffer_.size() - 1);
        client_.set_api_key(settings.smithery_api_key);
    }
}

void MarketplacePanel::render() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(600, 400), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("MCP Marketplace");
    render_content();
    ImGui::End();
}

void MarketplacePanel::render_content() {
    client_.poll();
    
    if (!initial_fetch_done_ && !api_key_buffer_.empty() && api_key_buffer_[0] != '\0') {
        initial_fetch_done_ = true;
        search_query_.clear();
        do_search(1);
        do_skill_search(1);
    }
    
    std::string status_message;
    bool status_is_error = false;
    float status_time = 0.0f;
    {
        std::lock_guard<std::mutex> lock(install_mutex_);
        if (status_time_ > 0) {
            status_time_ -= ImGui::GetIO().DeltaTime;
        }
        status_message = status_message_;
        status_is_error = status_is_error_;
        status_time = status_time_;
    }
    
    if (status_time > 0) {
        ImVec4 color = status_is_error ? ImVec4(1, 0.4f, 0.4f, 1) : ImVec4(0.4f, 1, 0.4f, 1);
        ImGui::TextColored(color, "%s", status_message.c_str());
    }
    
    if (ImGui::BeginTabBar("MarketplaceTabs")) {
        if (ImGui::BeginTabItem("MCP")) {
            marketplace_tab_ = 0;
            render_search_bar();
            
            float avail_width = ImGui::GetContentRegionAvail().x;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float min_list_width = 200.0f;
            float min_detail_width = 200.0f;
            float splitter_width = 4.0f;
            
            float max_detail_width = avail_width - min_list_width - splitter_width - (spacing * 2.0f);
            if (max_detail_width < min_detail_width) max_detail_width = min_detail_width;
            
            if (mcp_detail_width_ < min_detail_width) mcp_detail_width_ = min_detail_width;
            if (mcp_detail_width_ > max_detail_width) mcp_detail_width_ = max_detail_width;
            
            float list_width = avail_width - mcp_detail_width_ - splitter_width - (spacing * 2.0f);
            
            ImGui::BeginChild("ServerList", ImVec2(list_width, 0), true);
            render_server_list();
            ImGui::EndChild();
            
            ImGui::SameLine();
            RenderSplitter("##mcp_splitter", &mcp_detail_width_, min_detail_width);
            ImGui::SameLine();
            
            ImGui::BeginChild("ServerDetail", ImVec2(mcp_detail_width_, 0), true);
            render_server_detail();
            ImGui::EndChild();
            
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Skills")) {
            marketplace_tab_ = 1;
            render_search_bar();
            
            float avail_width = ImGui::GetContentRegionAvail().x;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float min_list_width = 200.0f;
            float min_detail_width = 200.0f;
            float splitter_width = 4.0f;
            
            float max_detail_width = avail_width - min_list_width - splitter_width - (spacing * 2.0f);
            if (max_detail_width < min_detail_width) max_detail_width = min_detail_width;
            
            if (skill_detail_width_ < min_detail_width) skill_detail_width_ = min_detail_width;
            if (skill_detail_width_ > max_detail_width) skill_detail_width_ = max_detail_width;
            
            float list_width = avail_width - skill_detail_width_ - splitter_width - (spacing * 2.0f);
            
            ImGui::BeginChild("SkillList", ImVec2(list_width, 0), true);
            render_skill_list();
            ImGui::EndChild();
            
            ImGui::SameLine();
            RenderSplitter("##skill_splitter", &skill_detail_width_, min_detail_width);
            ImGui::SameLine();
            
            ImGui::BeginChild("SkillDetail", ImVec2(skill_detail_width_, 0), true);
            render_skill_detail();
            ImGui::EndChild();
            
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }
    
    render_install_popup();
}

void MarketplacePanel::render_search_bar() {
    const bool is_mcp = marketplace_tab_ == 0;
    ImGui::Text(is_mcp ? "Search MCP Servers:" : "Search Skills:");
    ImGui::SameLine();
    
    ImGui::SetNextItemWidth(300);
    bool enter_pressed = ImGui::InputTextWithHint(
        "##Search", 
        is_mcp ? "e.g. github, notion, exa..." : "e.g. summarizer, reviewer...", 
        search_buffer_.data(), 
        search_buffer_.size(),
        ImGuiInputTextFlags_EnterReturnsTrue
    );
    
    ImGui::SameLine();
    
    bool search_clicked = ImGui::Button("Search");
    
    if (enter_pressed || search_clicked) {
        search_query_ = search_buffer_.data();
        if (is_mcp) {
            do_search(1);
        } else {
            do_skill_search(1);
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Browse All")) {
        search_query_.clear();
        std::fill(search_buffer_.begin(), search_buffer_.end(), '\0');
        if (is_mcp) {
            do_search(1);
        } else {
            do_skill_search(1);
        }
    }
    
    
    if (list_state_ == FetchState::Loading || detail_state_ == FetchState::Loading) {
        ImGui::SameLine();
        ImGui::TextDisabled("Loading...");
    }

    ImGui::Spacing();
    ImGui::Text("Smithery Key:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300);
    if (ImGui::InputText("##SmitheryKey", api_key_buffer_.data(), api_key_buffer_.size(), ImGuiInputTextFlags_Password)) {
        client_.set_api_key(api_key_buffer_.data());
        MarketplaceSettings settings;
        settings.smithery_api_key = api_key_buffer_.data();
        settings_store_.save(settings);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Get API Key")) {
#ifdef __APPLE__
        system("open \"https://smithery.ai/account/api-keys\"");
#endif
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Open https://smithery.ai/account/api-keys");
    }
}

void MarketplacePanel::render_server_list() {
    if (list_state_ == FetchState::Error) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Error:");
        ImGui::TextWrapped("%s", list_error_.c_str());
        return;
    }
    
    if (list_state_ == FetchState::Idle && search_result_.servers.empty()) {
        ImGui::TextDisabled("Click 'Browse All' or search to find MCP servers");
        return;
    }
    
    if (list_state_ == FetchState::Loading && search_result_.servers.empty()) {
        ImGui::TextDisabled("Loading...");
        return;
    }
    
    for (const auto& server : search_result_.servers) {
        ImGui::PushID(server.id.c_str());
        
        bool is_selected = (selected_server_.id == server.id);
        
        std::string label = server.display_name;
        if (server.verified) {
            label += " [Verified]";
        }
        
        if (ImGui::Selectable(label.c_str(), is_selected)) {
            selected_server_ = server;
            detail_state_ = FetchState::Success;
        }
        
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(server.description.c_str());
            ImGui::Text("Uses: %d", server.use_count);
            ImGui::EndTooltip();
        }
        
        ImGui::PopID();
    }
    
    ImGui::Separator();
    
    auto& pagination = search_result_.pagination;
    if (pagination.total_pages > 1) {
        ImGui::Text("Page %d / %d (%d total)", 
            pagination.current_page, pagination.total_pages, pagination.total_count);
        
        if (pagination.current_page > 1) {
            if (ImGui::Button("< Prev")) {
                do_search(pagination.current_page - 1);
            }
            ImGui::SameLine();
        }
        
        if (pagination.current_page < pagination.total_pages) {
            if (ImGui::Button("Next >")) {
                do_search(pagination.current_page + 1);
            }
        }
    } else if (pagination.total_count > 0) {
        ImGui::Text("%d servers found", pagination.total_count);
    }
}

void MarketplacePanel::render_server_detail() {
    if (selected_server_.id.empty()) {
        ImGui::TextDisabled("Select a server to view details");
        return;
    }
    
    ImGui::TextWrapped("%s", selected_server_.display_name.c_str());
    
    if (selected_server_.verified) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1), "[Verified]");
    }
    
    ImGui::Separator();
    
    ImGui::TextWrapped("%s", selected_server_.description.c_str());
    
    ImGui::Spacing();
    ImGui::Text("Uses: %d", selected_server_.use_count);
    ImGui::Text("ID: %s", selected_server_.qualified_name.c_str());
    
    if (selected_server_.is_deployed) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1), "Remote: Ready to use");
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    if (ImGui::Button("Install", ImVec2(-1, 30))) {
        show_install_popup_ = true;
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    std::string cli_log;
    {
        std::lock_guard<std::mutex> lock(install_mutex_);
        cli_log = cli_log_;
    }
    ImGui::Text("Smithery CLI Output:");
    ImGui::BeginChild("SmitheryCliLog", ImVec2(0, 80), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextWrapped("%s", cli_log.empty() ? "(no output)" : cli_log.c_str());
    ImGui::EndChild();
    
    if (!selected_server_.homepage.empty()) {
        if (ImGui::Button("View on Smithery", ImVec2(-1, 0))) {
#ifdef __APPLE__
            std::string cmd = "open \"" + selected_server_.homepage + "\"";
            system(cmd.c_str());
#endif
        }
    }
}

void MarketplacePanel::render_install_popup() {
    if (show_install_popup_) {
        ImGui::OpenPopup("Install Item");
        show_install_popup_ = false;
    }
    
    if (ImGui::BeginPopupModal("Install Item", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const bool is_mcp = marketplace_tab_ == 0;
        const char* title = is_mcp ? selected_server_.display_name.c_str()
                                   : (selected_skill_index_ >= 0 && selected_skill_index_ < static_cast<int>(skill_result_.skills.size())
                                       ? skill_result_.skills[selected_skill_index_].display_name.c_str() : "");
        ImGui::Text("Install \"%s\"", title);
        ImGui::Separator();
        
        ImGui::Text("Install to:");
        ImGui::RadioButton("Claude Code (global)", &install_target_, 0);
        ImGui::RadioButton("Codex (global)", &install_target_, 1);
        
        ImGui::Spacing();
        
        if (ImGui::Button("Install", ImVec2(120, 0))) {
            if (is_mcp) {
                install_server_cli(selected_server_, install_target_ == 0);
            } else if (selected_skill_index_ >= 0 && selected_skill_index_ < static_cast<int>(skill_result_.skills.size())) {
                install_skill(skill_result_.skills[selected_skill_index_], install_target_ == 0);
            }
            
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void MarketplacePanel::render_skill_list() {
    if (detail_state_ == FetchState::Error) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Error:");
        ImGui::TextWrapped("%s", skill_error_.c_str());
        return;
    }
    
    if (detail_state_ == FetchState::Idle && skill_result_.skills.empty()) {
        ImGui::TextDisabled("Click 'Browse All' or search to find skills");
        return;
    }
    
    if (detail_state_ == FetchState::Loading && skill_result_.skills.empty()) {
        ImGui::TextDisabled("Loading...");
        return;
    }
    
    for (size_t i = 0; i < skill_result_.skills.size(); ++i) {
        const auto& skill = skill_result_.skills[i];
        ImGui::PushID(skill.id.c_str());
        
        bool is_selected = static_cast<int>(i) == selected_skill_index_;
        
        std::string label = skill.display_name.empty() ? skill.slug : skill.display_name;
        if (ImGui::Selectable(label.c_str(), is_selected)) {
            select_skill(skill);
        }
        
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(skill.description.c_str());
            ImGui::Text("Uses: %d", skill.total_activations);
            ImGui::EndTooltip();
        }
        
        ImGui::PopID();
    }
    
    ImGui::Separator();
    
    auto& pagination = skill_result_.pagination;
    if (pagination.total_pages > 1) {
        ImGui::Text("Page %d / %d (%d total)", 
            pagination.current_page, pagination.total_pages, pagination.total_count);
        
        if (pagination.current_page > 1) {
            if (ImGui::Button("< Prev")) {
                do_skill_search(pagination.current_page - 1);
            }
            ImGui::SameLine();
        }
        
        if (pagination.current_page < pagination.total_pages) {
            if (ImGui::Button("Next >")) {
                do_skill_search(pagination.current_page + 1);
            }
        }
    } else if (pagination.total_count > 0) {
        ImGui::Text("%d skills found", pagination.total_count);
    }
}

void MarketplacePanel::render_skill_detail() {
    if (selected_skill_index_ < 0 || selected_skill_index_ >= static_cast<int>(skill_result_.skills.size())) {
        ImGui::TextDisabled("Select a skill to view details");
        return;
    }
    
    const auto& skill = skill_result_.skills[selected_skill_index_];
    
    ImGui::TextWrapped("%s", skill.display_name.empty() ? skill.slug.c_str() : skill.display_name.c_str());
    ImGui::Separator();
    ImGui::TextWrapped("%s", skill.description.c_str());
    
    ImGui::Spacing();
    ImGui::Text("Namespace: %s", skill.namespace_name.c_str());
    ImGui::Text("Slug: %s", skill.slug.c_str());
    ImGui::Text("Activations: %d", skill.total_activations);
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    if (ImGui::Button("Install", ImVec2(-1, 30))) {
        show_install_popup_ = true;
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    std::string cli_log;
    {
        std::lock_guard<std::mutex> lock(install_mutex_);
        cli_log = cli_log_;
    }
    ImGui::Text("Smithery CLI Output:");
    ImGui::BeginChild("SmitheryCliLog", ImVec2(0, 80), true, ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextWrapped("%s", cli_log.empty() ? "(no output)" : cli_log.c_str());
    ImGui::EndChild();
    
    if (!skill.git_url.empty()) {
        if (ImGui::Button("View Repo", ImVec2(-1, 0))) {
#ifdef __APPLE__
            std::string cmd = "open \"" + skill.git_url + "\"";
            system(cmd.c_str());
#endif
        }
    }
}

void MarketplacePanel::do_search(int page) {
    list_state_ = FetchState::Loading;
    list_error_.clear();
    
    client_.search_servers_async(search_query_, page, 15,
        [this](bool success, McpSearchResult result, std::string error) {
            if (success) {
                search_result_ = result;
                list_state_ = FetchState::Success;
            } else {
                list_error_ = error;
                list_state_ = FetchState::Error;
            }
        });
}

void MarketplacePanel::do_skill_search(int page) {
    detail_state_ = FetchState::Loading;
    skill_error_.clear();
    
    client_.search_skills_async(search_query_, page, 15,
        [this](bool success, SkillSearchResult result, std::string error) {
            if (success) {
                skill_result_ = result;
                detail_state_ = FetchState::Success;
            } else {
                skill_error_ = error;
                detail_state_ = FetchState::Error;
            }
        });
}

void MarketplacePanel::select_server(const McpServerEntry& server) {
    selected_server_ = server;
    detail_state_ = FetchState::Loading;
    detail_error_.clear();
    
    client_.get_server_detail_async(server.qualified_name,
        [this, server](bool success, McpServerEntry detail, std::string error) {
            if (success) {
                selected_server_ = detail;
                detail_state_ = FetchState::Success;
                return;
            }
            
            std::string retry_error;
            auto fallback = client_.get_server_detail_by_id(server.id, retry_error);
            if (retry_error.empty()) {
                selected_server_ = fallback;
                detail_state_ = FetchState::Success;
            } else {
                detail_error_ = retry_error.empty() ? error : retry_error;
                detail_state_ = FetchState::Error;
            }
        });
}

void MarketplacePanel::select_skill(const SkillEntry& skill) {
    selected_skill_index_ = -1;
    for (size_t i = 0; i < skill_result_.skills.size(); ++i) {
        if (skill_result_.skills[i].id == skill.id) {
            selected_skill_index_ = static_cast<int>(i);
            break;
        }
    }
}

void MarketplacePanel::install_server(const McpServerEntry& server, bool project_scope) {
    std::string config_path;
    
    if (project_scope) {
        if (project_directory_.empty()) {
            status_message_ = "Error: No project directory set";
            status_is_error_ = true;
            status_time_ = 3.0f;
            return;
        }
        config_path = project_directory_ + "/.mcp.json";
    } else {
        const char* home = getenv("HOME");
        if (!home) {
            status_message_ = "Error: Cannot find home directory";
            status_is_error_ = true;
            status_time_ = 3.0f;
            return;
        }
        config_path = std::string(home) + "/.claude.json";
    }
    
    nlohmann::json config;
    
    std::ifstream in(config_path);
    if (in.good()) {
        try {
            in >> config;
        } catch (...) {
            config = nlohmann::json::object();
        }
    }
    in.close();
    
    if (!config.contains("mcpServers")) {
        config["mcpServers"] = nlohmann::json::object();
    }
    
    std::string deployment_url = server.deployment_url;
    if (deployment_url.empty()) {
        deployment_url = "https://server.smithery.ai/" + server.qualified_name + "/mcp";
    }
    
    nlohmann::json server_config;
    server_config["type"] = "http";
    server_config["url"] = deployment_url;
    
    config["mcpServers"][server.qualified_name] = server_config;
    
    std::ofstream out(config_path);
    if (!out.good()) {
        status_message_ = "Error: Cannot write to " + config_path;
        status_is_error_ = true;
        status_time_ = 3.0f;
        return;
    }
    
    out << config.dump(2);
    out.close();
    
    status_message_ = "Installed: " + server.display_name;
    status_is_error_ = false;
    status_time_ = 3.0f;
    
    if (install_callback_) {
        McpConnectionConfig conn;
        conn.type = "http";
        conn.url = deployment_url;
        install_callback_(server.qualified_name, conn);
    }
}

void MarketplacePanel::install_server_codex(const McpServerEntry& server) {
    const char* home = getenv("HOME");
    if (!home) {
        status_message_ = "Error: Cannot find home directory";
        status_is_error_ = true;
        status_time_ = 3.0f;
        return;
    }
    
    std::string config_path = std::string(home) + "/.codex/config.toml";
    toml::table tbl;
    
    try {
        tbl = toml::parse_file(config_path);
    } catch (...) {
        tbl = toml::table{};
    }
    
    std::string deployment_url = server.deployment_url;
    if (deployment_url.empty()) {
        deployment_url = "https://server.smithery.ai/" + server.qualified_name + "/mcp";
    }
    
    toml::table mcp_tbl = toml::table{};
    if (auto existing = tbl["mcp_servers"].as_table()) {
        mcp_tbl = *existing;
    }
    
    toml::table server_tbl;
    server_tbl.insert_or_assign("url", deployment_url);
    server_tbl.insert_or_assign("enabled", true);
    mcp_tbl.insert_or_assign(server.qualified_name, server_tbl);
    tbl.insert_or_assign("mcp_servers", mcp_tbl);
    
    std::ofstream out(config_path);
    if (!out.good()) {
        status_message_ = "Error: Cannot write to " + config_path;
        status_is_error_ = true;
        status_time_ = 3.0f;
        return;
    }
    
    out << tbl;
    out.close();
    
    status_message_ = "Installed for Codex: " + server.display_name;
    status_is_error_ = false;
    status_time_ = 3.0f;
}

void MarketplacePanel::install_server_cli(const McpServerEntry& server, bool use_claude) {
    std::string key = api_key_buffer_.data();
    if (key.empty()) {
        std::lock_guard<std::mutex> lock(install_mutex_);
        status_message_ = "Error: Smithery API key is required";
        status_is_error_ = true;
        status_time_ = 3.0f;
        return;
    }
    {
        std::lock_guard<std::mutex> lock(install_mutex_);
        if (install_in_progress_) {
            status_message_ = "Install already in progress";
            status_is_error_ = true;
            status_time_ = 3.0f;
            return;
        }
        install_in_progress_ = true;
        cli_log_.clear();
        status_message_ = "Installing via Smithery CLI...";
        status_is_error_ = false;
        status_time_ = 3.0f;
    }
    
    std::string client = use_claude ? "claude-code" : "codex";
    std::string pkg = !server.qualified_name.empty() ? server.qualified_name : server.id;
    std::string cmd = "SMITHERY_API_KEY='" + key + "' npx -y @smithery/cli@latest install " + pkg + " --client " + client;
#ifdef __APPLE__
    cmd = "bash -lc \"" + cmd + "\"";
#endif
    cmd += " 2>&1";
    
    std::string display_name = server.display_name.empty() ? pkg : server.display_name;
    std::thread([this, cmd, display_name]() {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            std::lock_guard<std::mutex> lock(install_mutex_);
            status_message_ = "Error: Failed to start Smithery CLI";
            status_is_error_ = true;
            status_time_ = 4.0f;
            install_in_progress_ = false;
            return;
        }
        
        std::array<char, 256> buffer{};
        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
            append_cli_log(buffer.data());
        }
        
        int result = pclose(pipe);
        std::lock_guard<std::mutex> lock(install_mutex_);
        if (result != 0) {
            status_message_ = "Error: Smithery CLI install failed (exit " + std::to_string(result) + ")";
            status_is_error_ = true;
            status_time_ = 4.0f;
            install_in_progress_ = false;
            return;
        }
        
        status_message_ = "Installed via Smithery CLI: " + display_name;
        status_is_error_ = false;
        status_time_ = 3.0f;
        install_in_progress_ = false;
    }).detach();
}

void MarketplacePanel::append_cli_log(const std::string& line) {
    std::lock_guard<std::mutex> lock(install_mutex_);
    cli_log_ += line;
    if (cli_log_.size() > 4096) {
        cli_log_.erase(0, cli_log_.size() - 4096);
    }
}

void MarketplacePanel::install_skill(const SkillEntry& skill, bool use_claude) {
    cli_log_.clear();
    append_cli_log("Installing skill...\n");

    std::string base_dir;
    const char* home = getenv("HOME");
    if (!home) {
        status_message_ = "Error: Cannot find home directory";
        status_is_error_ = true;
        status_time_ = 3.0f;
        append_cli_log("Error: HOME not set\n");
        return;
    }
    
    if (use_claude) {
        base_dir = std::string(home) + "/.claude/skills";
    } else {
        base_dir = std::string(home) + "/.codex/skills";
    }
    
    std::string skill_name = skill.slug.empty() ? skill.id : skill.slug;
    std::string skill_dir = base_dir + "/" + skill_name;
    std::string skill_path = skill_dir + "/SKILL.md";
    
    std::error_code ec;
    std::filesystem::create_directories(skill_dir, ec);
    if (ec) {
        status_message_ = "Error: Cannot create skill directory";
        status_is_error_ = true;
        status_time_ = 3.0f;
        append_cli_log("Error: Cannot create directory " + skill_dir + "\n");
        return;
    }
    
    std::ofstream out(skill_path);
    if (!out.good()) {
        status_message_ = "Error: Cannot write to " + skill_path;
        status_is_error_ = true;
        status_time_ = 3.0f;
        append_cli_log("Error: Cannot write " + skill_path + "\n");
        return;
    }
    
    out << "---\n";
    out << "name: " << skill_name << "\n";
    if (!skill.description.empty()) {
        out << "description: " << skill.description << "\n";
    }
    out << "---\n\n";
    if (!skill.prompt.empty()) {
        out << skill.prompt << "\n";
    }
    out.close();
    
    append_cli_log("Wrote " + skill_path + "\n");
    status_message_ = "Installed: " + (skill.display_name.empty() ? skill_name : skill.display_name);
    status_is_error_ = false;
    status_time_ = 3.0f;
}

}
