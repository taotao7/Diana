#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <nlohmann/json.hpp>

namespace diana {

struct ClaudeCodePermissions {
    std::vector<std::string> allow;
    std::vector<std::string> deny;
    std::vector<std::string> ask;
    std::vector<std::string> additional_directories;
    std::string default_mode;
    bool disable_bypass_permissions_mode = false;
};

struct ClaudeCodeSandbox {
    bool enabled = false;
    bool auto_allow_bash_if_sandboxed = true;
    std::vector<std::string> excluded_commands;
    bool allow_unsandboxed_commands = true;
    std::vector<std::string> network_allow_unix_sockets;
    bool network_allow_local_binding = false;
    std::optional<int> network_http_proxy_port;
    std::optional<int> network_socks_proxy_port;
};

struct ClaudeCodeAttribution {
    std::string commit;
    std::string pr;
};

struct ClaudeCodeConfig {
    std::string model;
    std::string api_key_helper;
    std::optional<int> cleanup_period_days;
    std::map<std::string, std::string> env;
    ClaudeCodePermissions permissions;
    ClaudeCodeSandbox sandbox;
    ClaudeCodeAttribution attribution;
    bool always_thinking_enabled = false;
    std::string language;
    std::string output_style;
    std::string force_login_method;
    std::string force_login_org_uuid;
    bool disable_all_hooks = false;
    bool respect_gitignore = true;
    
    nlohmann::json extra_fields;
    
    static ClaudeCodeConfig from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;
};

struct ClaudeCodeProfile {
    std::string name;
    std::string description;
    ClaudeCodeConfig config;
    int64_t created_at = 0;
    int64_t updated_at = 0;
};

void to_json(nlohmann::json& j, const ClaudeCodePermissions& p);
void from_json(const nlohmann::json& j, ClaudeCodePermissions& p);

void to_json(nlohmann::json& j, const ClaudeCodeSandbox& s);
void from_json(const nlohmann::json& j, ClaudeCodeSandbox& s);

void to_json(nlohmann::json& j, const ClaudeCodeAttribution& a);
void from_json(const nlohmann::json& j, ClaudeCodeAttribution& a);

void to_json(nlohmann::json& j, const ClaudeCodeProfile& p);
void from_json(const nlohmann::json& j, ClaudeCodeProfile& p);

}
