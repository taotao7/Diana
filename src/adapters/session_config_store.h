#pragma once

#include "terminal/terminal_session.h"
#include <string>
#include <vector>

namespace diana {

struct SavedSessionConfig {
    std::string name;
    AppKind app = AppKind::ClaudeCode;
    std::string working_dir;
};

class SessionConfigStore {
public:
    SessionConfigStore();
    explicit SessionConfigStore(const std::string& config_path);
    
    void save(const std::vector<SavedSessionConfig>& sessions);
    std::vector<SavedSessionConfig> load();
    
private:
    std::string config_path_;
};

}
