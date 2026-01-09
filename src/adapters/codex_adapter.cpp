#include "codex_adapter.h"
#include <toml++/toml.hpp>
#include <fstream>
#include <cstdlib>

namespace diana {

std::string CodexAdapter::config_path() const {
    return get_config_dir();
}

std::string CodexAdapter::get_config_dir() const {
    const char* home = std::getenv("HOME");
    if (!home) return "";
    return std::string(home) + "/.codex";
}

std::string CodexAdapter::get_auth_path() const {
    std::string dir = get_config_dir();
    return dir.empty() ? "" : dir + "/auth.json";
}

std::string CodexAdapter::get_config_toml_path() const {
    std::string dir = get_config_dir();
    return dir.empty() ? "" : dir + "/config.toml";
}

std::optional<AppConfig> CodexAdapter::read_config() {
    std::string toml_path = get_config_toml_path();
    if (toml_path.empty()) return std::nullopt;
    
    AppConfig config;
    config.active.provider = "openai";
    
    try {
        auto tbl = toml::parse_file(toml_path);
        
        if (tbl.contains("model")) {
            if (auto val = tbl["model"].value<std::string>()) {
                config.active.model = *val;
            }
        }
        
        if (tbl.contains("model_provider")) {
            if (auto val = tbl["model_provider"].value<std::string>()) {
                config.active.provider = *val;
            }
        }
    } catch (...) {
    }
    
    for (const auto& provider : supported_providers()) {
        ProviderConfig pc;
        pc.provider = provider;
        config.available_providers.push_back(pc);
    }
    
    return config;
}

bool CodexAdapter::write_config(const ProviderConfig& config) {
    std::string toml_path = get_config_toml_path();
    if (toml_path.empty()) return false;
    
    toml::table tbl;
    
    try {
        tbl = toml::parse_file(toml_path);
    } catch (...) {
    }
    
    if (!config.model.empty()) {
        tbl.insert_or_assign("model", config.model);
    }
    
    if (!config.provider.empty()) {
        tbl.insert_or_assign("model_provider", config.provider);
    }
    
    std::string temp_path = toml_path + ".tmp";
    std::ofstream file(temp_path);
    if (!file.is_open()) {
        return false;
    }
    
    file << tbl;
    file.close();
    
    if (!file.good()) {
        std::remove(temp_path.c_str());
        return false;
    }
    
    if (std::rename(temp_path.c_str(), toml_path.c_str()) != 0) {
        std::remove(temp_path.c_str());
        return false;
    }
    
    return true;
}

bool CodexAdapter::supports_provider(const std::string& provider) const {
    auto providers = supported_providers();
    return std::find(providers.begin(), providers.end(), provider) != providers.end();
}

std::vector<std::string> CodexAdapter::supported_providers() const {
    return {"openai", "azure", "gemini", "ollama", "openrouter", "mistral", "deepseek", "xai", "groq", "arcjet"};
}

}
