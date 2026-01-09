#include "claude_code_config.h"
#include <chrono>

namespace diana {

void to_json(nlohmann::json& j, const ClaudeCodePermissions& p) {
    j = nlohmann::json::object();
    if (!p.allow.empty()) j["allow"] = p.allow;
    if (!p.deny.empty()) j["deny"] = p.deny;
    if (!p.ask.empty()) j["ask"] = p.ask;
    if (!p.additional_directories.empty()) j["additionalDirectories"] = p.additional_directories;
    if (!p.default_mode.empty()) j["defaultMode"] = p.default_mode;
    if (p.disable_bypass_permissions_mode) j["disableBypassPermissionsMode"] = p.disable_bypass_permissions_mode;
}

void from_json(const nlohmann::json& j, ClaudeCodePermissions& p) {
    if (j.contains("allow") && j["allow"].is_array()) {
        p.allow = j["allow"].get<std::vector<std::string>>();
    }
    if (j.contains("deny") && j["deny"].is_array()) {
        p.deny = j["deny"].get<std::vector<std::string>>();
    }
    if (j.contains("ask") && j["ask"].is_array()) {
        p.ask = j["ask"].get<std::vector<std::string>>();
    }
    if (j.contains("additionalDirectories") && j["additionalDirectories"].is_array()) {
        p.additional_directories = j["additionalDirectories"].get<std::vector<std::string>>();
    }
    if (j.contains("defaultMode") && j["defaultMode"].is_string()) {
        p.default_mode = j["defaultMode"].get<std::string>();
    }
    if (j.contains("disableBypassPermissionsMode") && j["disableBypassPermissionsMode"].is_boolean()) {
        p.disable_bypass_permissions_mode = j["disableBypassPermissionsMode"].get<bool>();
    }
}

void to_json(nlohmann::json& j, const ClaudeCodeSandbox& s) {
    j = nlohmann::json::object();
    if (s.enabled) j["enabled"] = s.enabled;
    if (!s.auto_allow_bash_if_sandboxed) j["autoAllowBashIfSandboxed"] = s.auto_allow_bash_if_sandboxed;
    if (!s.excluded_commands.empty()) j["excludedCommands"] = s.excluded_commands;
    if (!s.allow_unsandboxed_commands) j["allowUnsandboxedCommands"] = s.allow_unsandboxed_commands;
    
    nlohmann::json network = nlohmann::json::object();
    if (!s.network_allow_unix_sockets.empty()) network["allowUnixSockets"] = s.network_allow_unix_sockets;
    if (s.network_allow_local_binding) network["allowLocalBinding"] = s.network_allow_local_binding;
    if (s.network_http_proxy_port) network["httpProxyPort"] = *s.network_http_proxy_port;
    if (s.network_socks_proxy_port) network["socksProxyPort"] = *s.network_socks_proxy_port;
    if (!network.empty()) j["network"] = network;
}

void from_json(const nlohmann::json& j, ClaudeCodeSandbox& s) {
    if (j.contains("enabled") && j["enabled"].is_boolean()) {
        s.enabled = j["enabled"].get<bool>();
    }
    if (j.contains("autoAllowBashIfSandboxed") && j["autoAllowBashIfSandboxed"].is_boolean()) {
        s.auto_allow_bash_if_sandboxed = j["autoAllowBashIfSandboxed"].get<bool>();
    }
    if (j.contains("excludedCommands") && j["excludedCommands"].is_array()) {
        s.excluded_commands = j["excludedCommands"].get<std::vector<std::string>>();
    }
    if (j.contains("allowUnsandboxedCommands") && j["allowUnsandboxedCommands"].is_boolean()) {
        s.allow_unsandboxed_commands = j["allowUnsandboxedCommands"].get<bool>();
    }
    if (j.contains("network") && j["network"].is_object()) {
        const auto& net = j["network"];
        if (net.contains("allowUnixSockets") && net["allowUnixSockets"].is_array()) {
            s.network_allow_unix_sockets = net["allowUnixSockets"].get<std::vector<std::string>>();
        }
        if (net.contains("allowLocalBinding") && net["allowLocalBinding"].is_boolean()) {
            s.network_allow_local_binding = net["allowLocalBinding"].get<bool>();
        }
        if (net.contains("httpProxyPort") && net["httpProxyPort"].is_number_integer()) {
            s.network_http_proxy_port = net["httpProxyPort"].get<int>();
        }
        if (net.contains("socksProxyPort") && net["socksProxyPort"].is_number_integer()) {
            s.network_socks_proxy_port = net["socksProxyPort"].get<int>();
        }
    }
}

void to_json(nlohmann::json& j, const ClaudeCodeAttribution& a) {
    j = nlohmann::json::object();
    if (!a.commit.empty()) j["commit"] = a.commit;
    if (!a.pr.empty()) j["pr"] = a.pr;
}

void from_json(const nlohmann::json& j, ClaudeCodeAttribution& a) {
    if (j.contains("commit") && j["commit"].is_string()) {
        a.commit = j["commit"].get<std::string>();
    }
    if (j.contains("pr") && j["pr"].is_string()) {
        a.pr = j["pr"].get<std::string>();
    }
}

static const std::vector<std::string> KNOWN_KEYS = {
    "model", "apiKeyHelper", "cleanupPeriodDays", "env", "permissions",
    "sandbox", "attribution", "alwaysThinkingEnabled", "language",
    "outputStyle", "forceLoginMethod", "forceLoginOrgUUID", "disableAllHooks",
    "respectGitignore", "hooks", "includeCoAuthoredBy", "companyAnnouncements",
    "statusLine", "fileSuggestion", "enableAllProjectMcpServers",
    "enabledMcpjsonServers", "disabledMcpjsonServers", "awsAuthRefresh",
    "awsCredentialExport", "otelHeadersHelper"
};

ClaudeCodeConfig ClaudeCodeConfig::from_json(const nlohmann::json& j) {
    ClaudeCodeConfig cfg;
    
    if (j.contains("model") && j["model"].is_string()) {
        cfg.model = j["model"].get<std::string>();
    }
    if (j.contains("apiKeyHelper") && j["apiKeyHelper"].is_string()) {
        cfg.api_key_helper = j["apiKeyHelper"].get<std::string>();
    }
    if (j.contains("cleanupPeriodDays") && j["cleanupPeriodDays"].is_number_integer()) {
        cfg.cleanup_period_days = j["cleanupPeriodDays"].get<int>();
    }
    if (j.contains("env") && j["env"].is_object()) {
        for (auto& [key, val] : j["env"].items()) {
            if (val.is_string()) {
                cfg.env[key] = val.get<std::string>();
            }
        }
    }
    if (j.contains("permissions") && j["permissions"].is_object()) {
        cfg.permissions = j["permissions"].get<ClaudeCodePermissions>();
    }
    if (j.contains("sandbox") && j["sandbox"].is_object()) {
        cfg.sandbox = j["sandbox"].get<ClaudeCodeSandbox>();
    }
    if (j.contains("attribution") && j["attribution"].is_object()) {
        cfg.attribution = j["attribution"].get<ClaudeCodeAttribution>();
    }
    if (j.contains("alwaysThinkingEnabled") && j["alwaysThinkingEnabled"].is_boolean()) {
        cfg.always_thinking_enabled = j["alwaysThinkingEnabled"].get<bool>();
    }
    if (j.contains("language") && j["language"].is_string()) {
        cfg.language = j["language"].get<std::string>();
    }
    if (j.contains("outputStyle") && j["outputStyle"].is_string()) {
        cfg.output_style = j["outputStyle"].get<std::string>();
    }
    if (j.contains("forceLoginMethod") && j["forceLoginMethod"].is_string()) {
        cfg.force_login_method = j["forceLoginMethod"].get<std::string>();
    }
    if (j.contains("forceLoginOrgUUID") && j["forceLoginOrgUUID"].is_string()) {
        cfg.force_login_org_uuid = j["forceLoginOrgUUID"].get<std::string>();
    }
    if (j.contains("disableAllHooks") && j["disableAllHooks"].is_boolean()) {
        cfg.disable_all_hooks = j["disableAllHooks"].get<bool>();
    }
    if (j.contains("respectGitignore") && j["respectGitignore"].is_boolean()) {
        cfg.respect_gitignore = j["respectGitignore"].get<bool>();
    }
    
    for (auto& [key, val] : j.items()) {
        bool known = false;
        for (const auto& k : KNOWN_KEYS) {
            if (key == k) { known = true; break; }
        }
        if (!known) {
            cfg.extra_fields[key] = val;
        }
    }
    
    return cfg;
}

nlohmann::json ClaudeCodeConfig::to_json() const {
    nlohmann::json j = extra_fields;
    
    if (!model.empty()) j["model"] = model;
    if (!api_key_helper.empty()) j["apiKeyHelper"] = api_key_helper;
    if (cleanup_period_days) j["cleanupPeriodDays"] = *cleanup_period_days;
    if (!env.empty()) j["env"] = env;
    
    nlohmann::json perm_json;
    diana::to_json(perm_json, permissions);
    if (!perm_json.empty()) j["permissions"] = perm_json;
    
    nlohmann::json sandbox_json;
    diana::to_json(sandbox_json, sandbox);
    if (!sandbox_json.empty()) j["sandbox"] = sandbox_json;
    
    nlohmann::json attr_json;
    diana::to_json(attr_json, attribution);
    if (!attr_json.empty()) j["attribution"] = attr_json;
    
    if (always_thinking_enabled) j["alwaysThinkingEnabled"] = always_thinking_enabled;
    if (!language.empty()) j["language"] = language;
    if (!output_style.empty()) j["outputStyle"] = output_style;
    if (!force_login_method.empty()) j["forceLoginMethod"] = force_login_method;
    if (!force_login_org_uuid.empty()) j["forceLoginOrgUUID"] = force_login_org_uuid;
    if (disable_all_hooks) j["disableAllHooks"] = disable_all_hooks;
    if (!respect_gitignore) j["respectGitignore"] = respect_gitignore;
    
    return j;
}

void to_json(nlohmann::json& j, const ClaudeCodeProfile& p) {
    j = nlohmann::json{
        {"name", p.name},
        {"description", p.description},
        {"config", p.config.to_json()},
        {"created_at", p.created_at},
        {"updated_at", p.updated_at}
    };
}

void from_json(const nlohmann::json& j, ClaudeCodeProfile& p) {
    if (j.contains("name")) p.name = j["name"].get<std::string>();
    if (j.contains("description")) p.description = j["description"].get<std::string>();
    if (j.contains("config")) p.config = ClaudeCodeConfig::from_json(j["config"]);
    if (j.contains("created_at")) p.created_at = j["created_at"].get<int64_t>();
    if (j.contains("updated_at")) p.updated_at = j["updated_at"].get<int64_t>();
}

}
