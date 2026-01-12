#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <toml++/toml.hpp>
#include <nlohmann/json.hpp>

namespace diana {

struct CodexMcpServer {
    std::string command;
    std::vector<std::string> args;
    std::string cwd;
    std::map<std::string, std::string> env;
    std::vector<std::string> env_vars;
    bool enabled = true;
    std::vector<std::string> enabled_tools;
    std::vector<std::string> disabled_tools;
    std::optional<int> startup_timeout_sec;
    std::optional<int> tool_timeout_sec;
    
    std::string url;
    std::string bearer_token_env_var;
    std::map<std::string, std::string> http_headers;
    std::map<std::string, std::string> env_http_headers;
};

struct CodexModelProvider {
    std::string name;
    std::string base_url;
    std::string env_key;
    std::string env_key_instructions;
    std::string experimental_bearer_token;
    std::map<std::string, std::string> http_headers;
    std::map<std::string, std::string> env_http_headers;
    std::map<std::string, std::string> query_params;
    std::optional<int> request_max_retries;
    std::optional<int> stream_max_retries;
    std::optional<int> stream_idle_timeout_ms;
    bool requires_openai_auth = false;
    std::string wire_api;
};

struct CodexFeatures {
    bool apply_patch_freeform = false;
    bool elevated_windows_sandbox = false;
    bool exec_policy = true;
    bool experimental_windows_sandbox = false;
    bool powershell_utf8 = false;
    bool remote_compaction = true;
    bool remote_models = false;
    bool shell_snapshot = false;
    bool shell_tool = true;
    bool tui2 = false;
    bool unified_exec = false;
    bool web_search_request = false;
};

struct CodexFeedback {
    bool enabled = true;
};

struct CodexHistory {
    std::optional<int64_t> max_bytes;
    std::string persistence = "save-all";
};

struct CodexTui {
    bool animations = true;
    std::optional<bool> notifications;
    std::optional<int> scroll_events_per_tick;
    bool scroll_invert = false;
    std::string scroll_mode = "auto";
    std::optional<int> scroll_trackpad_accel_events;
    std::optional<int> scroll_trackpad_accel_max;
    std::optional<int> scroll_trackpad_lines;
    std::optional<int> scroll_wheel_like_max_duration_ms;
    std::optional<int> scroll_wheel_lines;
    std::optional<int> scroll_wheel_tick_detect_max_ms;
    bool show_tooltips = true;
};

struct CodexTools {
    bool web_search = false;
};

struct CodexSandboxWorkspaceWrite {
    bool exclude_slash_tmp = false;
    bool exclude_tmpdir_env_var = false;
    bool network_access = false;
    std::vector<std::string> writable_roots;
};

struct CodexConfig {
    std::string approval_policy;
    std::string model;
    std::string model_provider;
    
    std::string chatgpt_base_url;
    bool check_for_update_on_startup = true;
    std::string cli_auth_credentials_store = "auto";
    std::string compact_prompt;
    std::string developer_instructions;
    bool disable_paste_burst = false;
    std::string experimental_compact_prompt_file;
    std::string experimental_instructions_file;
    std::string file_opener = "vscode";
    std::string forced_chatgpt_workspace_id;
    std::string forced_login_method;
    bool hide_agent_reasoning = false;
    std::string instructions;
    std::string mcp_oauth_credentials_store = "auto";
    std::optional<int> model_auto_compact_token_limit;
    std::optional<int> model_context_window;
    std::string model_reasoning_effort;
    std::string model_reasoning_summary;
    bool model_supports_reasoning_summaries = false;
    std::string model_verbosity;
    std::vector<std::string> notify;
    std::string oss_provider;
    std::string profile;
    std::vector<std::string> project_doc_fallback_filenames;
    std::optional<int> project_doc_max_bytes;
    std::vector<std::string> project_root_markers;
    std::string review_model;
    std::string sandbox_mode;
    bool show_raw_agent_reasoning = false;
    std::optional<int> tool_output_token_limit;
    bool windows_wsl_setup_acknowledged = false;
    
    bool experimental_use_freeform_apply_patch = false;
    bool experimental_use_unified_exec_tool = false;
    bool include_apply_patch_tool = false;
    
    CodexFeatures features;
    CodexFeedback feedback;
    CodexHistory history;
    CodexTui tui;
    CodexTools tools;
    CodexSandboxWorkspaceWrite sandbox_workspace_write;
    
    std::map<std::string, CodexModelProvider> model_providers;
    std::map<std::string, CodexMcpServer> mcp_servers;
    std::map<std::string, std::string> projects;
    
    std::map<std::string, bool> notice_flags;
    std::map<std::string, std::string> notice_model_migrations;
    
    toml::table extra_fields;
    
    static CodexConfig from_toml(const toml::table& tbl);
    toml::table to_toml() const;
};

struct CodexProfile {
    std::string name;
    std::string description;
    CodexConfig config;
    int64_t created_at = 0;
    int64_t updated_at = 0;
};

void to_json(nlohmann::json& j, const CodexProfile& p);
void from_json(const nlohmann::json& j, CodexProfile& p);

}
