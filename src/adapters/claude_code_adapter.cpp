#include "claude_code_adapter.h"
#include <fstream>
#include <cstdlib>
#include <sys/stat.h>

namespace diana {

std::string ClaudeCodeAdapter::config_path() const {
    return get_settings_path();
}

std::string ClaudeCodeAdapter::get_settings_path() const {
    const char* home = std::getenv("HOME");
    if (!home) return "";
    return std::string(home) + "/.claude/settings.json";
}

nlohmann::json ClaudeCodeAdapter::read_json_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return nlohmann::json::object();
    }
    
    try {
        return nlohmann::json::parse(file);
    } catch (...) {
        return nlohmann::json::object();
    }
}

bool ClaudeCodeAdapter::write_json_file(const std::string& path, const nlohmann::json& data) {
    std::string temp_path = path + ".tmp";
    
    std::ofstream file(temp_path);
    if (!file.is_open()) {
        return false;
    }
    
    file << data.dump(2);
    file.close();
    
    if (!file.good()) {
        std::remove(temp_path.c_str());
        return false;
    }
    
    if (std::rename(temp_path.c_str(), path.c_str()) != 0) {
        std::remove(temp_path.c_str());
        return false;
    }
    
    return true;
}

std::optional<AppConfig> ClaudeCodeAdapter::read_config() {
    std::string path = get_settings_path();
    if (path.empty()) return std::nullopt;
    
    nlohmann::json json = read_json_file(path);
    
    AppConfig config;
    
    if (json.contains("model") && json["model"].is_string()) {
        config.active.model = json["model"].get<std::string>();
    }
    
    if (json.contains("provider") && json["provider"].is_string()) {
        config.active.provider = json["provider"].get<std::string>();
    } else {
        config.active.provider = "anthropic";
    }
    
    for (const auto& provider : supported_providers()) {
        ProviderConfig pc;
        pc.provider = provider;
        config.available_providers.push_back(pc);
    }
    
    return config;
}

bool ClaudeCodeAdapter::write_config(const ProviderConfig& config) {
    std::string path = get_settings_path();
    if (path.empty()) return false;
    
    nlohmann::json json = read_json_file(path);
    
    if (!config.model.empty()) {
        json["model"] = config.model;
    }
    
    if (!config.provider.empty()) {
        json["provider"] = config.provider;
    }
    
    return write_json_file(path, json);
}

bool ClaudeCodeAdapter::supports_provider(const std::string& provider) const {
    auto providers = supported_providers();
    return std::find(providers.begin(), providers.end(), provider) != providers.end();
}

std::vector<std::string> ClaudeCodeAdapter::supported_providers() const {
    return {"anthropic", "bedrock", "vertex"};
}

}
