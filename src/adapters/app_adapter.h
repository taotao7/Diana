#pragma once

#include <string>
#include <optional>
#include <vector>

namespace diana {

struct ProviderConfig {
    std::string provider;
    std::string model;
    std::string api_key;
};

struct AppConfig {
    ProviderConfig active;
    std::vector<ProviderConfig> available_providers;
};

class IAppAdapter {
public:
    virtual ~IAppAdapter() = default;
    
    virtual std::string name() const = 0;
    virtual std::string config_path() const = 0;
    
    virtual std::optional<AppConfig> read_config() = 0;
    virtual bool write_config(const ProviderConfig& config) = 0;
    
    virtual bool supports_provider(const std::string& provider) const = 0;
    virtual std::vector<std::string> supported_providers() const = 0;
};

}
