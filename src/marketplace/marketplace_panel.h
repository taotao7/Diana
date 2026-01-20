#pragma once

#include "marketplace_client.h"
#include "marketplace_types.h"
#include "adapters/marketplace_settings.h"
#include "process/process_runner.h"
#include <imgui.h>
#include <array>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <memory>
#include <array>


namespace diana {

struct SkillEntry;
class VTerminal;

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
    struct InstalledMcpItem {
        std::string name;
        McpConnectionConfig config;
        bool use_claude = true;
        std::string project_dir;
    };

    struct InstalledSkillItem {
        std::string name;
        std::string path;
        bool use_claude = true;
    };

    void render_search_bar();
    void render_server_list();
    void render_server_detail();
    void render_skill_list();
    void render_skill_detail();
    void render_install_popup();
    void render_installed_mcp_list();
    void render_installed_mcp_detail();
    void render_installed_skill_list();
    void render_installed_skill_detail();
    
    void do_search(int page = 1);
    void do_skill_search(int page = 1);
    void select_server(const McpServerEntry& server);
    void select_skill(const SkillEntry& skill);
    void install_server(const McpServerEntry& server, bool project_scope);
    void install_server_codex(const McpServerEntry& server);
    void install_server_cli(const McpServerEntry& server, bool use_claude);
    void append_cli_log(const std::string& line);
    void install_skill(const SkillEntry& skill, bool use_claude);
    void refresh_installed_mcp();
    void refresh_installed_skills();
    void remove_installed_mcp(const InstalledMcpItem& item);
    void remove_installed_skill(const InstalledSkillItem& item);
    void start_install_queue();
    void start_install_command(const std::string& cmd);
    void render_install_terminal();
    
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
    bool show_installed_mcp_ = false;
    bool show_installed_skills_ = false;
    
    std::string status_message_;
    float status_time_ = 0.0f;
    bool status_is_error_ = false;
    std::string cli_log_;
    std::array<char, 512> install_dir_buffer_{};
    ProcessRunner install_process_;
    std::unique_ptr<VTerminal> install_terminal_;
    bool install_terminal_user_scrolled_ = false;
    bool focus_install_terminal_ = false;
    std::vector<std::string> install_command_queue_;
    std::string install_display_name_;
    std::string install_workdir_;
    int last_marketplace_tab_ = -1;
    bool focus_search_input_ = false;
    std::vector<InstalledMcpItem> installed_mcp_items_;
    int selected_installed_mcp_index_ = -1;
    std::vector<InstalledSkillItem> installed_skill_items_;
    int selected_installed_skill_index_ = -1;
    
    std::string project_directory_;
    InstallCallback install_callback_;
    
    float mcp_detail_width_ = 300.0f;
    float skill_detail_width_ = 320.0f;
    bool initial_fetch_done_ = false;
    std::mutex install_mutex_;
    bool install_in_progress_ = false;
};

}
