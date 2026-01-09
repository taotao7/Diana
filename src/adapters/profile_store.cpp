#include "profile_store.h"
#include <fstream>
#include <cstdlib>
#include <chrono>
#include <sys/stat.h>

namespace agent47 {

ProfileStore::ProfileStore() {
    const char* home = std::getenv("HOME");
    if (home) {
        store_path_ = std::string(home) + "/.claude/diana_profiles.json";
        settings_path_ = std::string(home) + "/.claude/settings.json";
    }
}

std::string ProfileStore::store_path() const {
    return store_path_;
}

std::string ProfileStore::settings_path() const {
    return settings_path_;
}

int64_t ProfileStore::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
}

bool ProfileStore::load() {
    if (store_path_.empty()) return false;
    
    std::ifstream file(store_path_);
    if (!file.is_open()) {
        return true;
    }
    
    try {
        nlohmann::json j = nlohmann::json::parse(file);
        if (j.contains("profiles") && j["profiles"].is_array()) {
            profiles_.clear();
            for (const auto& pj : j["profiles"]) {
                ClaudeCodeProfile profile;
                from_json(pj, profile);
                profiles_.push_back(profile);
            }
        }
        if (j.contains("active") && j["active"].is_string()) {
            active_profile_ = j["active"].get<std::string>();
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool ProfileStore::save() {
    if (store_path_.empty()) return false;
    
    nlohmann::json j;
    j["version"] = 1;
    j["active"] = active_profile_;
    j["profiles"] = nlohmann::json::array();
    for (const auto& p : profiles_) {
        nlohmann::json pj;
        to_json(pj, p);
        j["profiles"].push_back(pj);
    }
    
    std::string temp_path = store_path_ + ".tmp";
    std::ofstream file(temp_path);
    if (!file.is_open()) return false;
    
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

std::optional<ClaudeCodeProfile> ProfileStore::get_profile(const std::string& name) const {
    for (const auto& p : profiles_) {
        if (p.name == name) return p;
    }
    return std::nullopt;
}

bool ProfileStore::add_profile(const ClaudeCodeProfile& profile) {
    for (const auto& p : profiles_) {
        if (p.name == profile.name) return false;
    }
    
    ClaudeCodeProfile new_profile = profile;
    new_profile.created_at = current_timestamp();
    new_profile.updated_at = new_profile.created_at;
    profiles_.push_back(new_profile);
    return save();
}

bool ProfileStore::update_profile(const std::string& name, const ClaudeCodeProfile& profile) {
    for (auto& p : profiles_) {
        if (p.name == name) {
            p.config = profile.config;
            p.description = profile.description;
            p.updated_at = current_timestamp();
            return save();
        }
    }
    return false;
}

bool ProfileStore::delete_profile(const std::string& name) {
    auto it = std::find_if(profiles_.begin(), profiles_.end(),
        [&name](const auto& p) { return p.name == name; });
    
    if (it != profiles_.end()) {
        profiles_.erase(it);
        if (active_profile_ == name) {
            active_profile_.clear();
        }
        return save();
    }
    return false;
}

bool ProfileStore::rename_profile(const std::string& old_name, const std::string& new_name) {
    for (const auto& p : profiles_) {
        if (p.name == new_name) return false;
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

bool ProfileStore::set_active_profile(const std::string& name) {
    if (name.empty()) {
        active_profile_.clear();
        return save();
    }
    
    auto profile = get_profile(name);
    if (!profile) return false;
    
    if (write_current_config(profile->config)) {
        active_profile_ = name;
        return save();
    }
    return false;
}

ClaudeCodeConfig ProfileStore::read_current_config() {
    if (settings_path_.empty()) return ClaudeCodeConfig{};
    
    std::ifstream file(settings_path_);
    if (!file.is_open()) return ClaudeCodeConfig{};
    
    try {
        nlohmann::json j = nlohmann::json::parse(file);
        return ClaudeCodeConfig::from_json(j);
    } catch (...) {
        return ClaudeCodeConfig{};
    }
}

bool ProfileStore::write_current_config(const ClaudeCodeConfig& config) {
    if (settings_path_.empty()) return false;
    
    nlohmann::json j = config.to_json();
    
    std::string temp_path = settings_path_ + ".tmp";
    std::ofstream file(temp_path);
    if (!file.is_open()) return false;
    
    file << j.dump(2);
    file.close();
    
    if (!file.good()) {
        std::remove(temp_path.c_str());
        return false;
    }
    
    if (std::rename(temp_path.c_str(), settings_path_.c_str()) != 0) {
        std::remove(temp_path.c_str());
        return false;
    }
    
    return true;
}

void ProfileStore::detect_active_profile() {
    ClaudeCodeConfig current = read_current_config();
    
    for (const auto& p : profiles_) {
        if (configs_match(current, p.config)) {
            if (active_profile_ != p.name) {
                active_profile_ = p.name;
                save();
            }
            return;
        }
    }
    
    if (!active_profile_.empty()) {
        active_profile_.clear();
        save();
    }
}

bool ProfileStore::configs_match(const ClaudeCodeConfig& a, const ClaudeCodeConfig& b) const {
    return a.model == b.model &&
           a.env == b.env &&
           a.permissions.default_mode == b.permissions.default_mode &&
           a.permissions.allow == b.permissions.allow &&
           a.permissions.deny == b.permissions.deny &&
           a.always_thinking_enabled == b.always_thinking_enabled;
}

}
