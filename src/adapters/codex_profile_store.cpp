#include "codex_profile_store.h"
#include <fstream>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <sstream>

namespace diana {

CodexProfileStore::CodexProfileStore() {
    const char* home = std::getenv("HOME");
    if (home) {
        store_path_ = std::string(home) + "/.config/diana/codex_profiles.json";
        config_path_ = std::string(home) + "/.codex/config.toml";
    }
}

std::string CodexProfileStore::store_path() const {
    return store_path_;
}

std::string CodexProfileStore::config_path() const {
    return config_path_;
}

int64_t CodexProfileStore::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
}

bool CodexProfileStore::ensure_diana_config_dir() const {
    const char* home = std::getenv("HOME");
    if (!home) return false;
    
    std::string diana_dir = std::string(home) + "/.config/diana";
    return std::filesystem::create_directories(diana_dir) || 
           std::filesystem::exists(diana_dir);
}

bool CodexProfileStore::load() {
    if (store_path_.empty()) return false;
    
    std::ifstream file(store_path_);
    if (!file.is_open()) {
        return true;
    }
    
    try {
        nlohmann::json j;
        file >> j;
        
        if (j.contains("profiles") && j["profiles"].is_array()) {
            profiles_.clear();
            for (const auto& pj : j["profiles"]) {
                CodexProfile profile;
                from_json(pj, profile);
                profiles_.push_back(std::move(profile));
            }
        }
        
        if (j.contains("active_profile") && j["active_profile"].is_string()) {
            active_profile_ = j["active_profile"].get<std::string>();
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

bool CodexProfileStore::save() {
    if (store_path_.empty()) return false;
    
    if (!ensure_diana_config_dir()) return false;
    
    nlohmann::json j;
    j["profiles"] = nlohmann::json::array();
    for (const auto& profile : profiles_) {
        nlohmann::json pj;
        to_json(pj, profile);
        j["profiles"].push_back(pj);
    }
    j["active_profile"] = active_profile_;
    
    std::string temp_path = store_path_ + ".tmp";
    
    std::ofstream file(temp_path);
    if (!file.is_open()) {
        return false;
    }
    
    file << j.dump(2);
    file.close();
    
    if (!file.good()) {
        std::remove(temp_path.c_str());
        return false;
    }
    
    if (std::rename(temp_path.c_str(), store_path_.c_str()) != 0) {
        std::remove(temp_path.c_str());
        return false;
    }
    
    return true;
}

std::optional<CodexProfile> CodexProfileStore::get_profile(const std::string& name) const {
    for (const auto& profile : profiles_) {
        if (profile.name == name) {
            return profile;
        }
    }
    return std::nullopt;
}

bool CodexProfileStore::add_profile(const CodexProfile& profile) {
    for (const auto& p : profiles_) {
        if (p.name == profile.name) {
            return false;
        }
    }
    
    CodexProfile new_profile = profile;
    new_profile.created_at = current_timestamp();
    new_profile.updated_at = new_profile.created_at;
    
    profiles_.push_back(std::move(new_profile));
    return save();
}

bool CodexProfileStore::update_profile(const std::string& name, const CodexProfile& profile) {
    for (auto& p : profiles_) {
        if (p.name == name) {
            p.config = profile.config;
            p.description = profile.description;
            p.updated_at = current_timestamp();
            
            if (name == active_profile_) {
                write_current_config(p.config);
            }
            
            return save();
        }
    }
    return false;
}

bool CodexProfileStore::delete_profile(const std::string& name) {
    auto it = std::find_if(profiles_.begin(), profiles_.end(),
        [&name](const CodexProfile& p) { return p.name == name; });
    
    if (it == profiles_.end()) {
        return false;
    }
    
    profiles_.erase(it);
    
    if (active_profile_ == name) {
        active_profile_.clear();
    }
    
    return save();
}

bool CodexProfileStore::rename_profile(const std::string& old_name, const std::string& new_name) {
    if (old_name == new_name) return true;
    
    for (const auto& p : profiles_) {
        if (p.name == new_name) {
            return false;
        }
    }
    
    for (auto& p : profiles_) {
        if (p.name == old_name) {
            p.name = new_name;
            p.updated_at = current_timestamp();
            
            if (active_profile_ == old_name) {
                active_profile_ = new_name;
            }
            
            return save();
        }
    }
    
    return false;
}

bool CodexProfileStore::set_active_profile(const std::string& name) {
    auto profile_opt = get_profile(name);
    if (!profile_opt) {
        return false;
    }
    
    if (!write_current_config(profile_opt->config)) {
        return false;
    }
    
    active_profile_ = name;
    return save();
}

void CodexProfileStore::detect_active_profile() {
    CodexConfig current = read_current_config();
    
    for (const auto& profile : profiles_) {
        if (configs_match(current, profile.config)) {
            active_profile_ = profile.name;
            return;
        }
    }
    
    active_profile_.clear();
}

CodexConfig CodexProfileStore::read_current_config() {
    if (config_path_.empty()) {
        return CodexConfig{};
    }
    
    try {
        auto tbl = toml::parse_file(config_path_);
        return CodexConfig::from_toml(tbl);
    } catch (...) {
        return CodexConfig{};
    }
}

bool CodexProfileStore::write_current_config(const CodexConfig& config) {
    if (config_path_.empty()) return false;
    
    std::filesystem::path config_dir = std::filesystem::path(config_path_).parent_path();
    std::filesystem::create_directories(config_dir);
    
    toml::table tbl = config.to_toml();
    
    std::string temp_path = config_path_ + ".tmp";
    
    std::ofstream file(temp_path);
    if (!file.is_open()) {
        return false;
    }
    
    file << tbl;
    file.close();
    
    if (!file.good()) {
        std::remove(temp_path.c_str());
        return false;
    }
    
    if (std::rename(temp_path.c_str(), config_path_.c_str()) != 0) {
        std::remove(temp_path.c_str());
        return false;
    }
    
    return true;
}

bool CodexProfileStore::configs_match(const CodexConfig& a, const CodexConfig& b) const {
    return a.model == b.model && a.model_provider == b.model_provider;
}

}
