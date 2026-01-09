#pragma once

#include "app_adapter.h"
#include <nlohmann/json.hpp>

namespace agent47 {

class CodexAdapter : public IAppAdapter {
public:
    std::string name() const override { return "Codex"; }
    std::string config_path() const override;
    
    std::optional<AppConfig> read_config() override;
    bool write_config(const ProviderConfig& config) override;
    
    bool supports_provider(const std::string& provider) const override;
    std::vector<std::string> supported_providers() const override;

private:
    std::string get_config_dir() const;
    std::string get_auth_path() const;
    std::string get_config_toml_path() const;
};

}
