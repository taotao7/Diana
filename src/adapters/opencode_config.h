#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <nlohmann/json.hpp>

namespace diana {

struct OpenCodeProviderConfig {
    std::string id;
    std::string api_key;
    std::string base_url;
    std::map<std::string, nlohmann::json> models;
    nlohmann::json options;
};

struct OpenCodeAgentConfig {
    std::string model;
    bool disabled = false;
    std::string description;
    std::string prompt;
    std::map<std::string, bool> tools;
    nlohmann::json options;
};

struct OpenCodeTuiConfig {
    int scroll_speed = 1;
    bool scroll_acceleration = false;
    std::string diff_style = "auto";
};

struct OpenCodeCompactionConfig {
    bool auto_compact = true;
    bool prune = true;
};

struct OpenCodeMcpServer {
    std::string type;
    std::string url;
    std::string command;
    std::vector<std::string> args;
    std::map<std::string, std::string> environment;
    bool enabled = true;
};

struct OpenCodePermissions {
    std::vector<std::string> allowed_commands;
    std::vector<std::string> denied_commands;
    bool auto_approve_reads = true;
    bool auto_approve_writes = false;
    bool auto_approve_commands = false;
    std::vector<std::string> allowed_paths;
    std::vector<std::string> denied_paths;
};

struct OpenCodeConfig {
    std::string model;
    std::string small_model;
    std::string theme;
    std::string default_agent;
    
    std::map<std::string, OpenCodeProviderConfig> providers;
    std::map<std::string, OpenCodeAgentConfig> agents;
    
    OpenCodePermissions permissions;
    std::map<std::string, std::string> permission_tools;
    
    std::map<std::string, bool> tools;
    
    std::vector<std::string> instructions;
    
    OpenCodeTuiConfig tui;
    
    OpenCodeCompactionConfig compaction;
    
    std::vector<std::string> watcher_ignore;
    
    std::map<std::string, OpenCodeMcpServer> mcp;
    
    std::string share = "manual";
    
    std::string autoupdate = "true";
    
    std::vector<std::string> disabled_providers;
    std::vector<std::string> enabled_providers;
    
    std::map<std::string, std::string> keybinds;
    
    nlohmann::json experimental;
    
    nlohmann::json extra_fields;
    
    static OpenCodeConfig from_json(const nlohmann::json& j);
    nlohmann::json to_json() const;
};

void to_json(nlohmann::json& j, const OpenCodeTuiConfig& t);
void from_json(const nlohmann::json& j, OpenCodeTuiConfig& t);

void to_json(nlohmann::json& j, const OpenCodeCompactionConfig& c);
void from_json(const nlohmann::json& j, OpenCodeCompactionConfig& c);

void to_json(nlohmann::json& j, const OpenCodeMcpServer& m);
void from_json(const nlohmann::json& j, OpenCodeMcpServer& m);

struct OpenCodeProfile {
    std::string name;
    std::string description;
    OpenCodeConfig config;
    int64_t created_at = 0;
    int64_t updated_at = 0;
};

void to_json(nlohmann::json& j, const OpenCodeProviderConfig& p);
void from_json(const nlohmann::json& j, OpenCodeProviderConfig& p);

void to_json(nlohmann::json& j, const OpenCodeAgentConfig& a);
void from_json(const nlohmann::json& j, OpenCodeAgentConfig& a);

void to_json(nlohmann::json& j, const OpenCodePermissions& p);
void from_json(const nlohmann::json& j, OpenCodePermissions& p);

void to_json(nlohmann::json& j, const OpenCodeProfile& p);
void from_json(const nlohmann::json& j, OpenCodeProfile& p);

}
