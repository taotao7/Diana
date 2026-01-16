#pragma once

#include "adapters/app_adapter.h"
#include "core/types.h"
#include <string>
#include <vector>
#include <optional>

namespace diana {

struct ExportedConfig {
    std::string app_name;
    ProviderConfig config;
};

struct ExportedFile {
    std::string path;
    std::string content;
};

struct ExportBundle {
    std::string version = "1.0";
    std::string exported_at;
    std::vector<ExportedConfig> configs;
    std::vector<ExportedFile> diana_files;
};

class ConfigExporter {
public:
    static std::string export_to_json(const std::vector<ExportedConfig>& configs);
    static std::string export_to_json(const std::vector<ExportedConfig>& configs,
                                      const std::vector<ExportedFile>& diana_files);
    static std::optional<ExportBundle> import_from_json(const std::string& json_str);
    
    static bool export_to_file(const std::string& path, const std::vector<ExportedConfig>& configs);
    static bool export_to_file(const std::string& path, const std::vector<ExportedConfig>& configs,
                               const std::vector<ExportedFile>& diana_files);
    static std::optional<ExportBundle> import_from_file(const std::string& path);

    static std::vector<ExportedFile> collect_diana_config_files();
    static bool restore_diana_config_files(const std::vector<ExportedFile>& files);
    
    static std::string app_kind_to_string(AppKind kind);
    static std::optional<AppKind> string_to_app_kind(const std::string& str);
};

}
