#include "opencode_profile_store.h"
#include <fstream>
#include <cstdlib>
#include <chrono>
#include <sys/stat.h>
#include <filesystem>

namespace diana {

OpenCodeProfileStore::OpenCodeProfileStore() {
    const char* home = std::getenv("HOME");
    if (home) {
        store_path_ = std::string(home) + "/.config/diana/opencode_profiles.json";
        config_path_ = std::string(home) + "/.config/opencode/opencode.json";
    }
}

std::string OpenCodeProfileStore::store_path() const {
    return store_path_;
}

std::string OpenCodeProfileStore::config_path() const {
    return config_path_;
}

int64_t OpenCodeProfileStore::current_timestamp() const {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
}

bool OpenCodeProfileStore::ensure_diana_config_dir() const {
    const char* home = std::getenv("HOME");
    if (!home) return false;
    
    std::string diana_dir = std::string(home) + "/.config/diana";
    return std::filesystem::create_directories(diana_dir) || 
           std::filesystem::exists(diana_dir);
}

std::string OpenCodeProfileStore::strip_jsonc_comments(const std::string& content) const {
    std::string cleaned;
    bool in_string = false;
    bool escaped = false;
    size_t i = 0;
    
    while (i < content.size()) {
        char c = content[i];
        
        if (escaped) {
            cleaned += c;
            escaped = false;
            ++i;
            continue;
        }
        
        if (c == '\\' && in_string) {
            escaped = true;
            cleaned += c;
            ++i;
            continue;
        }
        
        if (c == '"') {
            in_string = !in_string;
            cleaned += c;
            ++i;
            continue;
        }
        
        if (!in_string && c == '/' && i + 1 < content.size()) {
            if (content[i + 1] == '/') {
                while (i < content.size() && content[i] != '\n') ++i;
                continue;
            }
            if (content[i + 1] == '*') {
                i += 2;
                while (i + 1 < content.size() && !(content[i] == '*' && content[i + 1] == '/')) ++i;
                i += 2;
                continue;
            }
        }
        
        cleaned += c;
        ++i;
    }
    
    return cleaned;
}

bool OpenCodeProfileStore::load() {
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
                OpenCodeProfile profile;
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

bool OpenCodeProfileStore::save() {
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

std::optional<OpenCodeProfile> OpenCodeProfileStore::get_profile(const std::string& name) const {
    for (const auto& profile : profiles_) {
        if (profile.name == name) {
            return profile;
        }
    }
    return std::nullopt;
}

bool OpenCodeProfileStore::add_profile(const OpenCodeProfile& profile) {
    for (const auto& p : profiles_) {
        if (p.name == profile.name) {
            return false;
        }
    }
    
    OpenCodeProfile new_profile = profile;
    new_profile.created_at = current_timestamp();
    new_profile.updated_at = new_profile.created_at;
    
    profiles_.push_back(std::move(new_profile));
    return save();
}

bool OpenCodeProfileStore::update_profile(const std::string& name, const OpenCodeProfile& profile) {
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

bool OpenCodeProfileStore::delete_profile(const std::string& name) {
    auto it = std::find_if(profiles_.begin(), profiles_.end(),
        [&name](const OpenCodeProfile& p) { return p.name == name; });
    
    if (it == profiles_.end()) {
        return false;
    }
    
    profiles_.erase(it);
    
    if (active_profile_ == name) {
        active_profile_.clear();
    }
    
    return save();
}

bool OpenCodeProfileStore::rename_profile(const std::string& old_name, const std::string& new_name) {
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

bool OpenCodeProfileStore::set_active_profile(const std::string& name) {
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

void OpenCodeProfileStore::detect_active_profile() {
    OpenCodeConfig current = read_current_config();
    
    for (const auto& profile : profiles_) {
        if (configs_match(current, profile.config)) {
            active_profile_ = profile.name;
            return;
        }
    }
    
    active_profile_.clear();
}

OpenCodeConfig OpenCodeProfileStore::read_current_config() {
    if (config_path_.empty()) {
        return OpenCodeConfig{};
    }
    
    std::ifstream file(config_path_);
    if (!file.is_open()) {
        return OpenCodeConfig{};
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    std::string cleaned = strip_jsonc_comments(content);
    
    try {
        nlohmann::json j = nlohmann::json::parse(cleaned);
        return OpenCodeConfig::from_json(j);
    } catch (...) {
        return OpenCodeConfig{};
    }
}

bool OpenCodeProfileStore::write_current_config(const OpenCodeConfig& config) {
    if (config_path_.empty()) return false;
    
    std::filesystem::path config_dir = std::filesystem::path(config_path_).parent_path();
    std::filesystem::create_directories(config_dir);
    
    nlohmann::json j = config.to_json();
    
    std::string temp_path = config_path_ + ".tmp";
    
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
    
    if (std::rename(temp_path.c_str(), config_path_.c_str()) != 0) {
        std::remove(temp_path.c_str());
        return false;
    }
    
    return true;
}

bool OpenCodeProfileStore::configs_match(const OpenCodeConfig& a, const OpenCodeConfig& b) const {
    return a.model == b.model && a.theme == b.theme;
}

}
