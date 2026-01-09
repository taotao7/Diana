#pragma once

#include "adapters/config_manager.h"
#include "adapters/config_exporter.h"
#include <array>
#include <string>
#include <functional>

namespace agent47 {

using OnConfigApplied = std::function<void(AppKind, const ProviderConfig&)>;

class ProfilesPanel {
public:
    ProfilesPanel();
    
    void set_config_manager(ConfigManager* mgr) { config_manager_ = mgr; }
    void set_on_config_applied(OnConfigApplied callback) { on_config_applied_ = callback; }
    
    void render();
    void refresh_all();

private:
    static constexpr int kNumApps = 3;
    
    struct AppState {
        AppKind kind;
        std::string name;
        bool loaded = false;
        bool modified = false;
        std::string status;
        
        ProviderConfig current;
        
        std::array<char, 128> provider_buf{};
        std::array<char, 256> model_buf{};
        
        std::vector<std::string> providers;
        int selected_provider_idx = 0;
    };
    
    void render_app_section(AppState& app);
    void render_export_import();
    void load_config(AppState& app);
    void apply_config(AppState& app);
    void do_export();
    void do_import();
    
    ConfigManager* config_manager_ = nullptr;
    OnConfigApplied on_config_applied_;
    std::array<AppState, kNumApps> apps_;
    
    std::array<char, 512> file_path_buf_{};
    std::string export_import_status_;
};

}
