#include "config_manager.h"

namespace diana {

ConfigManager::ConfigManager()
    : claude_adapter_(std::make_unique<ClaudeCodeAdapter>())
    , codex_adapter_(std::make_unique<CodexAdapter>())
    , opencode_adapter_(std::make_unique<OpenCodeAdapter>())
{
}

IAppAdapter* ConfigManager::get_adapter(AppKind kind) {
    switch (kind) {
        case AppKind::ClaudeCode: return claude_adapter_.get();
        case AppKind::Codex:      return codex_adapter_.get();
        case AppKind::OpenCode:   return opencode_adapter_.get();
        case AppKind::Shell:      return nullptr;
    }
    return nullptr;
}

std::optional<AppConfig> ConfigManager::read_config(AppKind kind) {
    auto* adapter = get_adapter(kind);
    if (!adapter) return std::nullopt;
    return adapter->read_config();
}

SwitchResult ConfigManager::switch_config(AppKind kind, const ProviderConfig& new_config) {
    SwitchResult result;
    result.success = false;
    
    auto* adapter = get_adapter(kind);
    if (!adapter) {
        result.error = "Unknown app kind";
        return result;
    }
    
    auto current = adapter->read_config();
    if (current) {
        result.previous = current->active;
    }
    
    if (!adapter->write_config(new_config)) {
        result.error = "Failed to write config";
        return result;
    }
    
    result.success = true;
    return result;
}

bool ConfigManager::backfill_config(AppKind kind, const ProviderConfig& original) {
    auto* adapter = get_adapter(kind);
    if (!adapter) return false;
    return adapter->write_config(original);
}

}
