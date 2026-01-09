#pragma once

#include "app_adapter.h"
#include <nlohmann/json.hpp>

namespace agent47 {

class ClaudeCodeAdapter : public IAppAdapter {
public:
    std::string name() const override { return "Claude Code"; }
    std::string config_path() const override;
    
    std::optional<AppConfig> read_config() override;
    bool write_config(const ProviderConfig& config) override;
    
    bool supports_provider(const std::string& provider) const override;
    std::vector<std::string> supported_providers() const override;

private:
    std::string get_settings_path() const;
    nlohmann::json read_json_file(const std::string& path);
    bool write_json_file(const std::string& path, const nlohmann::json& data);
};

}
