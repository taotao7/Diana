#pragma once

#include "marketplace_client.h"
#include "marketplace_types.h"
#include "adapters/marketplace_settings.h"
#include <imgui.h>
#include <string>
#include <vector>
#include <functional>
#include <mutex>


namespace diana {

struct SkillEntry;

class MarketplacePanel {
public:
    MarketplacePanel();
    
    void render();
    void render_content();
    
    using InstallCallback = std::function<void(const std::string& name, const McpConnectionConfig& config)>;
    void set_install_callback(InstallCallback callback) { install_callback_ = callback; }
    
    void set_project_directory(const std::string& dir) { project_directory_ = dir; }
    void set_api_key(const std::string& key) { client_.set_api_key(key); }
    
private:
    void render_search_bar();
    void render_server_list();
    void render_server_detail();
    void render_skill_list();
    void render_skill_detail();
    void render_install_popup();
    
    void do_search(int page = 1);
    void do_skill_search(int page = 1);
    void select_server(const McpServerEntry& server);
    void select_skill(const SkillEntry& skill);
    void install_server(const McpServerEntry& server, bool project_scope);
    void install_server_codex(const McpServerEntry& server);
    void install_server_cli(const McpServerEntry& server, bool use_claude);
    void append_cli_log(const std::string& line);
    void install_skill(const SkillEntry& skill, bool use_claude);
    
    MarketplaceClient client_;
    MarketplaceSettingsStore settings_store_;
    
    std::string search_query_;
    std::vector<char> search_buffer_;
    std::vector<char> api_key_buffer_;
    
    FetchState list_state_ = FetchState::Idle;
    FetchState detail_state_ = FetchState::Idle;
    
    McpSearchResult search_result_;
    std::string list_error_;
    
    SkillSearchResult skill_result_;
    std::string skill_error_;
    
    McpServerEntry selected_server_;
    std::string detail_error_;
    
    int selected_skill_index_ = -1;
    
    bool show_install_popup_ = false;
    int install_target_ = 0;
    int marketplace_tab_ = 0;
    
    std::string status_message_;
    float status_time_ = 0.0f;
    bool status_is_error_ = false;
    std::string cli_log_;
    
    std::string project_directory_;
    InstallCallback install_callback_;
    
    float mcp_detail_width_ = 300.0f;
    float skill_detail_width_ = 320.0f;
    bool initial_fetch_done_ = false;
    std::mutex install_mutex_;
    bool install_in_progress_ = false;
};

}
