#pragma once

#include <string>

namespace diana {

struct MarketplaceSettings {
    std::string smithery_api_key;
};

class MarketplaceSettingsStore {
public:
    MarketplaceSettingsStore();
    explicit MarketplaceSettingsStore(const std::string& config_path);

    MarketplaceSettings load() const;
    void save(const MarketplaceSettings& settings) const;

private:
    std::string config_path_;
};

}
