#include "adapters/config_exporter.h"
#include "adapters/config_exporter.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <cstdlib>

namespace diana {

namespace {

std::string diana_config_dir() {
    const char* home = std::getenv("HOME");
    if (!home) return {};
    return std::string(home) + "/.config/diana";
}

nlohmann::json build_export_root(const std::vector<ExportedConfig>& configs,
                                const std::vector<ExportedFile>& diana_files) {
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

    nlohmann::json files_array = nlohmann::json::array();
    for (const auto& file : diana_files) {
        nlohmann::json item;
        item["path"] = file.path;
        item["content"] = file.content;
        files_array.push_back(item);
    }
    root["diana_files"] = files_array;

    return root;
}

bool is_safe_relative_path(const std::filesystem::path& path) {
    if (path.is_absolute()) return false;
    for (const auto& part : path) {
        if (part == "..") return false;
    }
    return true;
}

}

std::string ConfigExporter::app_kind_to_string(AppKind kind) {
    switch (kind) {
        case AppKind::ClaudeCode: return "claude_code";
        case AppKind::Codex: return "codex";
        case AppKind::OpenCode: return "opencode";
        case AppKind::Shell: return "shell";
    }
    return "unknown";
}

std::optional<AppKind> ConfigExporter::string_to_app_kind(const std::string& str) {
    if (str == "claude_code") return AppKind::ClaudeCode;
    if (str == "codex") return AppKind::Codex;
    if (str == "opencode") return AppKind::OpenCode;
    if (str == "shell") return AppKind::Shell;
    return std::nullopt;
}

std::string ConfigExporter::export_to_json(const std::vector<ExportedConfig>& configs) {
    return export_to_json(configs, {});
}

std::string ConfigExporter::export_to_json(const std::vector<ExportedConfig>& configs,
                                          const std::vector<ExportedFile>& diana_files) {
    return build_export_root(configs, diana_files).dump(2);
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
        
        bool has_configs = root.contains("configs") && root["configs"].is_array();
        bool has_diana_files = root.contains("diana_files") && root["diana_files"].is_array();
        if (!has_configs && !has_diana_files) {
            return std::nullopt;
        }

        if (has_configs) {
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
        }

        if (has_diana_files) {
            for (const auto& item : root["diana_files"]) {
                ExportedFile file;
                if (item.contains("path")) {
                    file.path = item["path"].get<std::string>();
                }
                if (item.contains("content")) {
                    file.content = item["content"].get<std::string>();
                }
                if (!file.path.empty()) {
                    bundle.diana_files.push_back(std::move(file));
                }
            }
        }

        return bundle;
    } catch (...) {
        return std::nullopt;
    }
}

bool ConfigExporter::export_to_file(const std::string& path, const std::vector<ExportedConfig>& configs) {
    return export_to_file(path, configs, {});
}

bool ConfigExporter::export_to_file(const std::string& path, const std::vector<ExportedConfig>& configs,
                                   const std::vector<ExportedFile>& diana_files) {
    std::ofstream file(path);
    if (!file) {
        return false;
    }

    file << export_to_json(configs, diana_files);
    return file.good();
}

std::vector<ExportedFile> ConfigExporter::collect_diana_config_files() {
    std::vector<ExportedFile> files;
    std::string base = diana_config_dir();
    if (base.empty()) return files;

    namespace fs = std::filesystem;
    fs::path base_path(base);
    if (!fs::exists(base_path)) return files;

    for (const auto& entry : fs::recursive_directory_iterator(base_path)) {
        if (!entry.is_regular_file()) continue;
        const fs::path& path = entry.path();
        if (path.extension() == ".tmp") continue;

        std::ifstream file(path, std::ios::binary);
        if (!file) continue;

        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        ExportedFile exported;
        exported.path = fs::relative(path, base_path).generic_string();
        exported.content = std::move(content);
        files.push_back(std::move(exported));
    }

    return files;
}

bool ConfigExporter::restore_diana_config_files(const std::vector<ExportedFile>& files) {
    std::string base = diana_config_dir();
    if (base.empty()) return false;

    namespace fs = std::filesystem;
    fs::path base_path(base);
    std::error_code ec;
    fs::create_directories(base_path, ec);
    if (ec) return false;

    for (const auto& file : files) {
        if (file.path.empty()) continue;
        fs::path rel_path(file.path);
        if (!is_safe_relative_path(rel_path)) continue;

        fs::path target = base_path / rel_path;
        ec.clear();
        fs::create_directories(target.parent_path(), ec);
        if (ec) return false;

        std::ofstream out(target, std::ios::binary);
        if (!out) return false;
        out << file.content;
        if (!out.good()) return false;
    }

    return true;
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
