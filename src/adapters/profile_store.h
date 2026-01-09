#pragma once

#include "claude_code_config.h"
#include <string>
#include <vector>
#include <optional>
#include <functional>

namespace agent47 {

class ProfileStore {
public:
    ProfileStore();
    
    bool load();
    bool save();
    
    const std::vector<ClaudeCodeProfile>& profiles() const { return profiles_; }
    const std::string& active_profile() const { return active_profile_; }
    
    std::optional<ClaudeCodeProfile> get_profile(const std::string& name) const;
    bool add_profile(const ClaudeCodeProfile& profile);
    bool update_profile(const std::string& name, const ClaudeCodeProfile& profile);
    bool delete_profile(const std::string& name);
    bool rename_profile(const std::string& old_name, const std::string& new_name);
    
    bool set_active_profile(const std::string& name);
    void detect_active_profile();
    
    ClaudeCodeConfig read_current_config();
    bool write_current_config(const ClaudeCodeConfig& config);
    
    std::string store_path() const;
    std::string settings_path() const;

private:
    std::vector<ClaudeCodeProfile> profiles_;
    std::string active_profile_;
    std::string store_path_;
    std::string settings_path_;
    
    int64_t current_timestamp() const;
    bool configs_match(const ClaudeCodeConfig& a, const ClaudeCodeConfig& b) const;
};

}
