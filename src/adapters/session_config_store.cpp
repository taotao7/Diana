#include "adapters/session_config_store.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdlib>
#include <filesystem>

namespace diana {

namespace {

std::string default_config_path() {
    const char* home = std::getenv("HOME");
    if (!home) return "";
    return std::string(home) + "/.config/diana/sessions.json";
}

std::string app_kind_to_string(AppKind kind) {
    switch (kind) {
        case AppKind::ClaudeCode: return "claude_code";
        case AppKind::Codex: return "codex";
        case AppKind::OpenCode: return "opencode";
        case AppKind::Shell: return "shell";
    }
    return "claude_code";
}

AppKind string_to_app_kind(const std::string& s) {
    if (s == "codex") return AppKind::Codex;
    if (s == "opencode") return AppKind::OpenCode;
    if (s == "shell") return AppKind::Shell;
    return AppKind::ClaudeCode;
}

}

SessionConfigStore::SessionConfigStore()
    : config_path_(default_config_path())
{
}

SessionConfigStore::SessionConfigStore(const std::string& config_path)
    : config_path_(config_path)
{
}

void SessionConfigStore::save(const std::vector<SavedSessionConfig>& sessions) {
    if (config_path_.empty()) return;
    
    namespace fs = std::filesystem;
    fs::path path(config_path_);
    fs::create_directories(path.parent_path());
    
    nlohmann::json j = nlohmann::json::array();
    for (const auto& session : sessions) {
        nlohmann::json s;
        s["name"] = session.name;
        s["app"] = app_kind_to_string(session.app);
        s["working_dir"] = session.working_dir;
        j.push_back(s);
    }
    
    std::ofstream file(config_path_);
    if (file) {
        file << j.dump(2);
    }
}

std::vector<SavedSessionConfig> SessionConfigStore::load() {
    std::vector<SavedSessionConfig> result;
    
    if (config_path_.empty()) return result;
    
    std::ifstream file(config_path_);
    if (!file) return result;
    
    try {
        nlohmann::json j = nlohmann::json::parse(file);
        if (!j.is_array()) return result;
        
        for (const auto& item : j) {
            SavedSessionConfig config;
            if (item.contains("name") && item["name"].is_string()) {
                config.name = item["name"].get<std::string>();
            }
            if (item.contains("app") && item["app"].is_string()) {
                config.app = string_to_app_kind(item["app"].get<std::string>());
            }
            if (item.contains("working_dir") && item["working_dir"].is_string()) {
                config.working_dir = item["working_dir"].get<std::string>();
            }
            result.push_back(config);
        }
    } catch (...) {
    }
    
    return result;
}

}
