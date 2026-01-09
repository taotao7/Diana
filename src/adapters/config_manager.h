#pragma once

#include "app_adapter.h"
#include "claude_code_adapter.h"
#include "codex_adapter.h"
#include "opencode_adapter.h"
#include "core/types.h"
#include <memory>
#include <string>

namespace diana {

struct SwitchResult {
    bool success;
    std::string error;
    ProviderConfig previous;
};

class ConfigManager {
public:
    ConfigManager();
    
    IAppAdapter* get_adapter(AppKind kind);
    
    SwitchResult switch_config(AppKind kind, const ProviderConfig& new_config);
    
    std::optional<AppConfig> read_config(AppKind kind);
    
    bool backfill_config(AppKind kind, const ProviderConfig& original);

private:
    std::unique_ptr<ClaudeCodeAdapter> claude_adapter_;
    std::unique_ptr<CodexAdapter> codex_adapter_;
    std::unique_ptr<OpenCodeAdapter> opencode_adapter_;
};

}
