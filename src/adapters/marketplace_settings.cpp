#include "adapters/marketplace_settings.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdlib>
#include <filesystem>

namespace diana {

namespace {

std::string default_config_path() {
    const char* home = std::getenv("HOME");
    if (!home) return "";
    return std::string(home) + "/.config/diana/marketplace_settings.json";
}

}

MarketplaceSettingsStore::MarketplaceSettingsStore()
    : config_path_(default_config_path())
{
}

MarketplaceSettingsStore::MarketplaceSettingsStore(const std::string& config_path)
    : config_path_(config_path)
{
}

MarketplaceSettings MarketplaceSettingsStore::load() const {
    MarketplaceSettings settings;
    if (config_path_.empty()) return settings;

    std::ifstream file(config_path_);
    if (!file) return settings;

    try {
        nlohmann::json j = nlohmann::json::parse(file);
        if (j.contains("smithery_api_key") && j["smithery_api_key"].is_string()) {
            settings.smithery_api_key = j["smithery_api_key"].get<std::string>();
        }
    } catch (...) {
    }

    return settings;
}

void MarketplaceSettingsStore::save(const MarketplaceSettings& settings) const {
    if (config_path_.empty()) return;

    namespace fs = std::filesystem;
    fs::path path(config_path_);
    fs::create_directories(path.parent_path());

    nlohmann::json j = nlohmann::json::object();
    j["smithery_api_key"] = settings.smithery_api_key;

    std::ofstream file(config_path_);
    if (file) {
        file << j.dump(2);
    }
}

}
