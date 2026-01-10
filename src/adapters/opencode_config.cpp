#include "opencode_config.h"

namespace diana {

void to_json(nlohmann::json& j, const OpenCodeProviderConfig& p) {
    j = nlohmann::json::object();
    if (!p.api_key.empty()) j["apiKey"] = p.api_key;
    if (!p.base_url.empty()) j["baseURL"] = p.base_url;
    if (!p.models.empty()) j["models"] = p.models;
    if (!p.options.empty()) j["options"] = p.options;
}

void from_json(const nlohmann::json& j, OpenCodeProviderConfig& p) {
    if (j.contains("apiKey") && j["apiKey"].is_string()) {
        p.api_key = j["apiKey"].get<std::string>();
    }
    if (j.contains("baseURL") && j["baseURL"].is_string()) {
        p.base_url = j["baseURL"].get<std::string>();
    }
    if (j.contains("models") && j["models"].is_object()) {
        p.models = j["models"].get<std::map<std::string, nlohmann::json>>();
    }
    if (j.contains("options") && j["options"].is_object()) {
        p.options = j["options"];
    }
}

void to_json(nlohmann::json& j, const OpenCodeAgentConfig& a) {
    j = nlohmann::json::object();
    if (!a.model.empty()) j["model"] = a.model;
    if (a.disabled) j["disabled"] = a.disabled;
    if (!a.description.empty()) j["description"] = a.description;
    if (!a.prompt.empty()) j["prompt"] = a.prompt;
    if (!a.tools.empty()) j["tools"] = a.tools;
    if (!a.options.empty()) j["options"] = a.options;
}

void from_json(const nlohmann::json& j, OpenCodeAgentConfig& a) {
    if (j.contains("model") && j["model"].is_string()) {
        a.model = j["model"].get<std::string>();
    }
    if (j.contains("disabled") && j["disabled"].is_boolean()) {
        a.disabled = j["disabled"].get<bool>();
    }
    if (j.contains("description") && j["description"].is_string()) {
        a.description = j["description"].get<std::string>();
    }
    if (j.contains("prompt") && j["prompt"].is_string()) {
        a.prompt = j["prompt"].get<std::string>();
    }
    if (j.contains("tools") && j["tools"].is_object()) {
        a.tools = j["tools"].get<std::map<std::string, bool>>();
    }
    if (j.contains("options") && j["options"].is_object()) {
        a.options = j["options"];
    }
}

void to_json(nlohmann::json& j, const OpenCodeTuiConfig& t) {
    j = nlohmann::json::object();
    if (t.scroll_speed != 1) j["scroll_speed"] = t.scroll_speed;
    if (t.scroll_acceleration) {
        j["scroll_acceleration"] = nlohmann::json::object();
        j["scroll_acceleration"]["enabled"] = true;
    }
    if (t.diff_style != "auto") j["diff_style"] = t.diff_style;
}

void from_json(const nlohmann::json& j, OpenCodeTuiConfig& t) {
    if (j.contains("scroll_speed") && j["scroll_speed"].is_number()) {
        t.scroll_speed = j["scroll_speed"].get<int>();
    }
    if (j.contains("scroll_acceleration") && j["scroll_acceleration"].is_object()) {
        auto& sa = j["scroll_acceleration"];
        if (sa.contains("enabled") && sa["enabled"].is_boolean()) {
            t.scroll_acceleration = sa["enabled"].get<bool>();
        }
    }
    if (j.contains("diff_style") && j["diff_style"].is_string()) {
        t.diff_style = j["diff_style"].get<std::string>();
    }
}

void to_json(nlohmann::json& j, const OpenCodeCompactionConfig& c) {
    j = nlohmann::json::object();
    j["auto"] = c.auto_compact;
    j["prune"] = c.prune;
}

void from_json(const nlohmann::json& j, OpenCodeCompactionConfig& c) {
    if (j.contains("auto") && j["auto"].is_boolean()) {
        c.auto_compact = j["auto"].get<bool>();
    }
    if (j.contains("prune") && j["prune"].is_boolean()) {
        c.prune = j["prune"].get<bool>();
    }
}

void to_json(nlohmann::json& j, const OpenCodeMcpServer& m) {
    j = nlohmann::json::object();
    if (!m.type.empty()) j["type"] = m.type;
    if (!m.url.empty()) j["url"] = m.url;
    if (!m.command.empty()) j["command"] = m.command;
    if (!m.args.empty()) j["args"] = m.args;
    if (!m.environment.empty()) j["environment"] = m.environment;
    if (!m.enabled) j["enabled"] = false;
}

void from_json(const nlohmann::json& j, OpenCodeMcpServer& m) {
    if (j.contains("type") && j["type"].is_string()) {
        m.type = j["type"].get<std::string>();
    }
    if (j.contains("url") && j["url"].is_string()) {
        m.url = j["url"].get<std::string>();
    }
    if (j.contains("command") && j["command"].is_string()) {
        m.command = j["command"].get<std::string>();
    }
    if (j.contains("args") && j["args"].is_array()) {
        m.args = j["args"].get<std::vector<std::string>>();
    }
    if (j.contains("environment") && j["environment"].is_object()) {
        m.environment = j["environment"].get<std::map<std::string, std::string>>();
    }
    if (j.contains("enabled") && j["enabled"].is_boolean()) {
        m.enabled = j["enabled"].get<bool>();
    }
}

void to_json(nlohmann::json& j, const OpenCodePermissions& p) {
    j = nlohmann::json::object();
    if (!p.allowed_commands.empty()) j["allowedCommands"] = p.allowed_commands;
    if (!p.denied_commands.empty()) j["deniedCommands"] = p.denied_commands;
    j["autoApproveReads"] = p.auto_approve_reads;
    j["autoApproveWrites"] = p.auto_approve_writes;
    j["autoApproveCommands"] = p.auto_approve_commands;
    if (!p.allowed_paths.empty()) j["allowedPaths"] = p.allowed_paths;
    if (!p.denied_paths.empty()) j["deniedPaths"] = p.denied_paths;
}

void from_json(const nlohmann::json& j, OpenCodePermissions& p) {
    if (j.contains("allowedCommands") && j["allowedCommands"].is_array()) {
        p.allowed_commands = j["allowedCommands"].get<std::vector<std::string>>();
    }
    if (j.contains("deniedCommands") && j["deniedCommands"].is_array()) {
        p.denied_commands = j["deniedCommands"].get<std::vector<std::string>>();
    }
    if (j.contains("autoApproveReads") && j["autoApproveReads"].is_boolean()) {
        p.auto_approve_reads = j["autoApproveReads"].get<bool>();
    }
    if (j.contains("autoApproveWrites") && j["autoApproveWrites"].is_boolean()) {
        p.auto_approve_writes = j["autoApproveWrites"].get<bool>();
    }
    if (j.contains("autoApproveCommands") && j["autoApproveCommands"].is_boolean()) {
        p.auto_approve_commands = j["autoApproveCommands"].get<bool>();
    }
    if (j.contains("allowedPaths") && j["allowedPaths"].is_array()) {
        p.allowed_paths = j["allowedPaths"].get<std::vector<std::string>>();
    }
    if (j.contains("deniedPaths") && j["deniedPaths"].is_array()) {
        p.denied_paths = j["deniedPaths"].get<std::vector<std::string>>();
    }
}

OpenCodeConfig OpenCodeConfig::from_json(const nlohmann::json& j) {
    OpenCodeConfig config;
    
    if (j.contains("model") && j["model"].is_string()) {
        config.model = j["model"].get<std::string>();
    }
    if (j.contains("small_model") && j["small_model"].is_string()) {
        config.small_model = j["small_model"].get<std::string>();
    }
    if (j.contains("theme") && j["theme"].is_string()) {
        config.theme = j["theme"].get<std::string>();
    }
    if (j.contains("default_agent") && j["default_agent"].is_string()) {
        config.default_agent = j["default_agent"].get<std::string>();
    }
    
    if (j.contains("provider") && j["provider"].is_object()) {
        for (auto& [key, value] : j["provider"].items()) {
            OpenCodeProviderConfig pc;
            pc.id = key;
            diana::from_json(value, pc);
            config.providers[key] = pc;
        }
    }
    
    if (j.contains("agent") && j["agent"].is_object()) {
        for (auto& [key, value] : j["agent"].items()) {
            OpenCodeAgentConfig ac;
            diana::from_json(value, ac);
            config.agents[key] = ac;
        }
    }
    
    if (j.contains("permission") && j["permission"].is_object()) {
        for (auto& [key, value] : j["permission"].items()) {
            if (value.is_string()) {
                config.permission_tools[key] = value.get<std::string>();
            }
        }
    }
    
    if (j.contains("tools") && j["tools"].is_object()) {
        config.tools = j["tools"].get<std::map<std::string, bool>>();
    }
    
    if (j.contains("instructions")) {
        if (j["instructions"].is_array()) {
            config.instructions = j["instructions"].get<std::vector<std::string>>();
        } else if (j["instructions"].is_string()) {
            config.instructions.push_back(j["instructions"].get<std::string>());
        }
    }
    
    if (j.contains("tui") && j["tui"].is_object()) {
        diana::from_json(j["tui"], config.tui);
    }
    
    if (j.contains("compaction") && j["compaction"].is_object()) {
        diana::from_json(j["compaction"], config.compaction);
    }
    
    if (j.contains("watcher") && j["watcher"].is_object()) {
        auto& w = j["watcher"];
        if (w.contains("ignore") && w["ignore"].is_array()) {
            config.watcher_ignore = w["ignore"].get<std::vector<std::string>>();
        }
    }
    
    if (j.contains("mcp") && j["mcp"].is_object()) {
        for (auto& [key, value] : j["mcp"].items()) {
            OpenCodeMcpServer server;
            diana::from_json(value, server);
            config.mcp[key] = server;
        }
    }
    
    if (j.contains("share") && j["share"].is_string()) {
        config.share = j["share"].get<std::string>();
    }
    
    if (j.contains("autoupdate")) {
        if (j["autoupdate"].is_boolean()) {
            config.autoupdate = j["autoupdate"].get<bool>() ? "true" : "false";
        } else if (j["autoupdate"].is_string()) {
            config.autoupdate = j["autoupdate"].get<std::string>();
        }
    }
    
    if (j.contains("disabled_providers") && j["disabled_providers"].is_array()) {
        config.disabled_providers = j["disabled_providers"].get<std::vector<std::string>>();
    }
    
    if (j.contains("enabled_providers") && j["enabled_providers"].is_array()) {
        config.enabled_providers = j["enabled_providers"].get<std::vector<std::string>>();
    }
    
    if (j.contains("keybinds") && j["keybinds"].is_object()) {
        config.keybinds = j["keybinds"].get<std::map<std::string, std::string>>();
    }
    
    if (j.contains("experimental") && j["experimental"].is_object()) {
        config.experimental = j["experimental"];
    }
    
    static const std::vector<std::string> known_keys = {
        "model", "small_model", "theme", "default_agent", "provider", "agent",
        "permission", "tools", "instructions", "tui", "compaction", "watcher",
        "mcp", "share", "autoupdate", "disabled_providers", "enabled_providers",
        "keybinds", "experimental", "$schema"
    };
    
    for (auto& [key, value] : j.items()) {
        bool known = false;
        for (const auto& k : known_keys) {
            if (key == k) {
                known = true;
                break;
            }
        }
        if (!known) {
            config.extra_fields[key] = value;
        }
    }
    
    return config;
}

nlohmann::json OpenCodeConfig::to_json() const {
    nlohmann::json j = extra_fields;
    
    j["$schema"] = "https://opencode.ai/config.json";
    
    if (!model.empty()) j["model"] = model;
    if (!small_model.empty()) j["small_model"] = small_model;
    if (!theme.empty()) j["theme"] = theme;
    if (!default_agent.empty()) j["default_agent"] = default_agent;
    
    if (!providers.empty()) {
        nlohmann::json provider_obj = nlohmann::json::object();
        for (const auto& [key, value] : providers) {
            nlohmann::json pj;
            diana::to_json(pj, value);
            provider_obj[key] = pj;
        }
        j["provider"] = provider_obj;
    }
    
    if (!agents.empty()) {
        nlohmann::json agent_obj = nlohmann::json::object();
        for (const auto& [key, value] : agents) {
            nlohmann::json aj;
            diana::to_json(aj, value);
            agent_obj[key] = aj;
        }
        j["agent"] = agent_obj;
    }
    
    if (!permission_tools.empty()) {
        j["permission"] = permission_tools;
    }
    
    if (!tools.empty()) {
        j["tools"] = tools;
    }
    
    if (!instructions.empty()) {
        j["instructions"] = instructions;
    }
    
    nlohmann::json tui_j;
    diana::to_json(tui_j, tui);
    if (!tui_j.empty()) {
        j["tui"] = tui_j;
    }
    
    if (!compaction.auto_compact || !compaction.prune) {
        nlohmann::json comp_j;
        diana::to_json(comp_j, compaction);
        j["compaction"] = comp_j;
    }
    
    if (!watcher_ignore.empty()) {
        j["watcher"] = nlohmann::json::object();
        j["watcher"]["ignore"] = watcher_ignore;
    }
    
    if (!mcp.empty()) {
        nlohmann::json mcp_obj = nlohmann::json::object();
        for (const auto& [key, value] : mcp) {
            nlohmann::json mj;
            diana::to_json(mj, value);
            mcp_obj[key] = mj;
        }
        j["mcp"] = mcp_obj;
    }
    
    if (share != "manual") {
        j["share"] = share;
    }
    
    if (autoupdate != "true") {
        if (autoupdate == "false") {
            j["autoupdate"] = false;
        } else {
            j["autoupdate"] = autoupdate;
        }
    }
    
    if (!disabled_providers.empty()) {
        j["disabled_providers"] = disabled_providers;
    }
    
    if (!enabled_providers.empty()) {
        j["enabled_providers"] = enabled_providers;
    }
    
    if (!keybinds.empty()) {
        j["keybinds"] = keybinds;
    }
    
    if (!experimental.empty()) {
        j["experimental"] = experimental;
    }
    
    return j;
}

void to_json(nlohmann::json& j, const OpenCodeProfile& p) {
    j = nlohmann::json::object();
    j["name"] = p.name;
    j["description"] = p.description;
    j["config"] = p.config.to_json();
    j["created_at"] = p.created_at;
    j["updated_at"] = p.updated_at;
}

void from_json(const nlohmann::json& j, OpenCodeProfile& p) {
    if (j.contains("name") && j["name"].is_string()) {
        p.name = j["name"].get<std::string>();
    }
    if (j.contains("description") && j["description"].is_string()) {
        p.description = j["description"].get<std::string>();
    }
    if (j.contains("config") && j["config"].is_object()) {
        p.config = OpenCodeConfig::from_json(j["config"]);
    }
    if (j.contains("created_at") && j["created_at"].is_number()) {
        p.created_at = j["created_at"].get<int64_t>();
    }
    if (j.contains("updated_at") && j["updated_at"].is_number()) {
        p.updated_at = j["updated_at"].get<int64_t>();
    }
}

}
