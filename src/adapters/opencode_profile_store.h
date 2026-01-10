#pragma once

#include "opencode_config.h"
#include <string>
#include <vector>
#include <optional>

namespace diana {

class OpenCodeProfileStore {
public:
    OpenCodeProfileStore();
    
    bool load();
    bool save();
    
    const std::vector<OpenCodeProfile>& profiles() const { return profiles_; }
    const std::string& active_profile() const { return active_profile_; }
    
    std::optional<OpenCodeProfile> get_profile(const std::string& name) const;
    bool add_profile(const OpenCodeProfile& profile);
    bool update_profile(const std::string& name, const OpenCodeProfile& profile);
    bool delete_profile(const std::string& name);
    bool rename_profile(const std::string& old_name, const std::string& new_name);
    
    bool set_active_profile(const std::string& name);
    void detect_active_profile();
    
    OpenCodeConfig read_current_config();
    bool write_current_config(const OpenCodeConfig& config);
    
    std::string store_path() const;
    std::string config_path() const;

private:
    std::vector<OpenCodeProfile> profiles_;
    std::string active_profile_;
    std::string store_path_;
    std::string config_path_;
    
    int64_t current_timestamp() const;
    bool configs_match(const OpenCodeConfig& a, const OpenCodeConfig& b) const;
    bool ensure_diana_config_dir() const;
    std::string strip_jsonc_comments(const std::string& content) const;
};

}
