#include "adapters/config_exporter.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace agent47 {

std::string ConfigExporter::app_kind_to_string(AppKind kind) {
    switch (kind) {
        case AppKind::ClaudeCode: return "claude_code";
        case AppKind::Codex: return "codex";
        case AppKind::OpenCode: return "opencode";
    }
    return "unknown";
}

std::optional<AppKind> ConfigExporter::string_to_app_kind(const std::string& str) {
    if (str == "claude_code") return AppKind::ClaudeCode;
    if (str == "codex") return AppKind::Codex;
    if (str == "opencode") return AppKind::OpenCode;
    return std::nullopt;
}

std::string ConfigExporter::export_to_json(const std::vector<ExportedConfig>& configs) {
    nlohmann::json root;
    root["version"] = "1.0";
    
    auto now = std::time(nullptr);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&now), "%Y-%m-%dT%H:%M:%SZ");
    root["exported_at"] = oss.str();
    
    nlohmann::json configs_array = nlohmann::json::array();
    for (const auto& cfg : configs) {
        nlohmann::json item;
        item["app"] = cfg.app_name;
        item["provider"] = cfg.config.provider;
        item["model"] = cfg.config.model;
        configs_array.push_back(item);
    }
    root["configs"] = configs_array;
    
    return root.dump(2);
}

std::optional<ExportBundle> ConfigExporter::import_from_json(const std::string& json_str) {
    try {
        auto root = nlohmann::json::parse(json_str);
        
        ExportBundle bundle;
        
        if (root.contains("version")) {
            bundle.version = root["version"].get<std::string>();
        }
        if (root.contains("exported_at")) {
            bundle.exported_at = root["exported_at"].get<std::string>();
        }
        
        if (!root.contains("configs") || !root["configs"].is_array()) {
            return std::nullopt;
        }
        
        for (const auto& item : root["configs"]) {
            ExportedConfig cfg;
            
            if (item.contains("app")) {
                cfg.app_name = item["app"].get<std::string>();
            }
            if (item.contains("provider")) {
                cfg.config.provider = item["provider"].get<std::string>();
            }
            if (item.contains("model")) {
                cfg.config.model = item["model"].get<std::string>();
            }
            
            bundle.configs.push_back(cfg);
        }
        
        return bundle;
    } catch (...) {
        return std::nullopt;
    }
}

bool ConfigExporter::export_to_file(const std::string& path, const std::vector<ExportedConfig>& configs) {
    std::ofstream file(path);
    if (!file) {
        return false;
    }
    
    file << export_to_json(configs);
    return file.good();
}

std::optional<ExportBundle> ConfigExporter::import_from_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    return import_from_json(content);
}

}
