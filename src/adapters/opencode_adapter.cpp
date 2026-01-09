#include "opencode_adapter.h"
#include <fstream>
#include <cstdlib>

namespace agent47 {

std::string OpenCodeAdapter::config_path() const {
    return get_config_path();
}

std::string OpenCodeAdapter::get_config_path() const {
    const char* home = std::getenv("HOME");
    if (!home) return "";
    return std::string(home) + "/.config/opencode/opencode.json";
}

nlohmann::json OpenCodeAdapter::read_json_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return nlohmann::json::object();
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    std::string cleaned;
    bool in_string = false;
    bool escaped = false;
    size_t i = 0;
    
    while (i < content.size()) {
        char c = content[i];
        
        if (escaped) {
            cleaned += c;
            escaped = false;
            ++i;
            continue;
        }
        
        if (c == '\\' && in_string) {
            escaped = true;
            cleaned += c;
            ++i;
            continue;
        }
        
        if (c == '"') {
            in_string = !in_string;
            cleaned += c;
            ++i;
            continue;
        }
        
        if (!in_string && c == '/' && i + 1 < content.size()) {
            if (content[i + 1] == '/') {
                while (i < content.size() && content[i] != '\n') ++i;
                continue;
            }
            if (content[i + 1] == '*') {
                i += 2;
                while (i + 1 < content.size() && !(content[i] == '*' && content[i + 1] == '/')) ++i;
                i += 2;
                continue;
            }
        }
        
        cleaned += c;
        ++i;
    }
    
    try {
        return nlohmann::json::parse(cleaned);
    } catch (...) {
        return nlohmann::json::object();
    }
}

bool OpenCodeAdapter::write_json_file(const std::string& path, const nlohmann::json& data) {
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

std::optional<AppConfig> OpenCodeAdapter::read_config() {
    std::string path = get_config_path();
    if (path.empty()) return std::nullopt;
    
    nlohmann::json json = read_json_file(path);
    
    AppConfig config;
    
    // OpenCode uses "model" field in "provider/model" format
    // e.g., "anthropic/claude-sonnet-4-5", "google/gemini-3-pro-high"
    if (json.contains("model") && json["model"].is_string()) {
        std::string model_str = json["model"].get<std::string>();
        size_t slash_pos = model_str.find('/');
        if (slash_pos != std::string::npos) {
            config.active.provider = model_str.substr(0, slash_pos);
            config.active.model = model_str.substr(slash_pos + 1);
        } else {
            // Fallback: treat entire string as model, use default provider
            config.active.model = model_str;
            config.active.provider = "anthropic";
        }
    }
    
    // "provider" field is an object containing provider configurations
    // e.g., { "google": { "models": {...}, "options": {...} } }
    if (json.contains("provider") && json["provider"].is_object()) {
        for (auto& [provider_id, provider_config] : json["provider"].items()) {
            ProviderConfig pc;
            pc.provider = provider_id;
            // We don't extract individual models here - just track available providers
            config.available_providers.push_back(pc);
        }
    }
    
    // Also add built-in providers that might not be in config
    for (const auto& builtin : supported_providers()) {
        bool found = false;
        for (const auto& p : config.available_providers) {
            if (p.provider == builtin) {
                found = true;
                break;
            }
        }
        if (!found) {
            ProviderConfig pc;
            pc.provider = builtin;
            config.available_providers.push_back(pc);
        }
    }
    
    return config;
}

bool OpenCodeAdapter::write_config(const ProviderConfig& config) {
    std::string path = get_config_path();
    if (path.empty()) return false;
    
    nlohmann::json json = read_json_file(path);
    
    if (!config.provider.empty() && !config.model.empty()) {
        json["model"] = config.provider + "/" + config.model;
    } else if (!config.model.empty()) {
        if (config.model.find('/') != std::string::npos) {
            json["model"] = config.model;
        } else {
            std::string existing_provider = "anthropic";
            if (json.contains("model") && json["model"].is_string()) {
                std::string existing = json["model"].get<std::string>();
                size_t slash = existing.find('/');
                if (slash != std::string::npos) {
                    existing_provider = existing.substr(0, slash);
                }
            }
            json["model"] = existing_provider + "/" + config.model;
        }
    }
    
    return write_json_file(path, json);
}

bool OpenCodeAdapter::supports_provider(const std::string& provider) const {
    auto providers = supported_providers();
    return std::find(providers.begin(), providers.end(), provider) != providers.end();
}

std::vector<std::string> OpenCodeAdapter::supported_providers() const {
    return {"anthropic", "openai", "google", "azure", "xai", "groq", "openrouter", "copilot"};
}

}
