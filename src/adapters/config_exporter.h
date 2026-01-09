#pragma once

#include "adapters/app_adapter.h"
#include "terminal/terminal_session.h"
#include <string>
#include <vector>
#include <optional>

namespace agent47 {

struct ExportedConfig {
    std::string app_name;
    ProviderConfig config;
};

struct ExportBundle {
    std::string version = "1.0";
    std::string exported_at;
    std::vector<ExportedConfig> configs;
};

class ConfigExporter {
public:
    static std::string export_to_json(const std::vector<ExportedConfig>& configs);
    static std::optional<ExportBundle> import_from_json(const std::string& json_str);
    
    static bool export_to_file(const std::string& path, const std::vector<ExportedConfig>& configs);
    static std::optional<ExportBundle> import_from_file(const std::string& path);
    
    static std::string app_kind_to_string(AppKind kind);
    static std::optional<AppKind> string_to_app_kind(const std::string& str);
};

}
