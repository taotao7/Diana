#include "ui/profiles_panel.h"
#include "imgui.h"
#include <nfd.h>
#include <cstring>
#include <algorithm>

namespace agent47 {

ProfilesPanel::ProfilesPanel() {
    apps_[0] = {AppKind::ClaudeCode, "Claude Code"};
    apps_[1] = {AppKind::Codex, "Codex"};
    apps_[2] = {AppKind::OpenCode, "OpenCode"};
}

void ProfilesPanel::render() {
    ImGui::Begin("Profiles");
    
    if (!config_manager_) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "ConfigManager not set");
        ImGui::End();
        return;
    }
    
    if (ImGui::Button("Refresh All")) {
        refresh_all();
    }
    
    ImGui::Separator();
    
    for (auto& app : apps_) {
        render_app_section(app);
    }
    
    ImGui::Separator();
    render_export_import();
    
    ImGui::End();
}

void ProfilesPanel::refresh_all() {
    for (auto& app : apps_) {
        load_config(app);
    }
}

void ProfilesPanel::render_app_section(AppState& app) {
    ImGui::PushID(static_cast<int>(app.kind));
    
    bool header_open = ImGui::CollapsingHeader(app.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
    
    if (header_open) {
        if (!app.loaded) {
            load_config(app);
        }
        
        ImGui::Indent();
        
        if (!app.status.empty()) {
            ImVec4 color = app.status.find("Error") != std::string::npos 
                ? ImVec4(1, 0.4f, 0.4f, 1) 
                : ImVec4(0.4f, 1, 0.4f, 1);
            ImGui::TextColored(color, "%s", app.status.c_str());
        }
        
        ImGui::Text("Provider:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(200);
        if (ImGui::InputText("##provider_input", app.provider_buf.data(), 
                             app.provider_buf.size())) {
            app.modified = true;
        }
        
        if (!app.providers.empty()) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(100);
            if (ImGui::BeginCombo("##provider_preset", "Presets")) {
                for (size_t i = 0; i < app.providers.size(); ++i) {
                    if (ImGui::Selectable(app.providers[i].c_str())) {
                        std::strncpy(app.provider_buf.data(), app.providers[i].c_str(), 
                                     app.provider_buf.size() - 1);
                        app.modified = true;
                    }
                }
                ImGui::EndCombo();
            }
        }
        
        ImGui::Text("Model:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(250);
        if (ImGui::InputText("##model", app.model_buf.data(), app.model_buf.size())) {
            app.modified = true;
        }
        
        ImGui::BeginDisabled(!app.modified);
        if (ImGui::Button("Apply")) {
            apply_config(app);
        }
        ImGui::EndDisabled();
        
        ImGui::SameLine();
        if (ImGui::Button("Reload")) {
            load_config(app);
        }
        
        ImGui::Unindent();
    }
    
    ImGui::PopID();
    ImGui::Spacing();
}

void ProfilesPanel::load_config(AppState& app) {
    if (!config_manager_) return;
    
    auto config = config_manager_->read_config(app.kind);
    if (config) {
        app.current = config->active;
        std::strncpy(app.provider_buf.data(), config->active.provider.c_str(), 
                     app.provider_buf.size() - 1);
        std::strncpy(app.model_buf.data(), config->active.model.c_str(), 
                     app.model_buf.size() - 1);
        
        auto* adapter = config_manager_->get_adapter(app.kind);
        if (adapter) {
            app.providers = adapter->supported_providers();
            
            auto it = std::find(app.providers.begin(), app.providers.end(), 
                               config->active.provider);
            if (it != app.providers.end()) {
                app.selected_provider_idx = static_cast<int>(
                    std::distance(app.providers.begin(), it));
            }
        }
        
        app.loaded = true;
        app.modified = false;
        app.status = "Loaded";
    } else {
        app.status = "Error: Could not read config";
        app.loaded = true;
    }
}

void ProfilesPanel::apply_config(AppState& app) {
    if (!config_manager_) return;
    
    ProviderConfig new_config;
    new_config.provider = app.provider_buf.data();
    new_config.model = app.model_buf.data();
    new_config.api_key = app.current.api_key;
    
    auto result = config_manager_->switch_config(app.kind, new_config);
    if (result.success) {
        app.current = new_config;
        app.modified = false;
        app.status = "Applied successfully";
        
        if (on_config_applied_) {
            on_config_applied_(app.kind, new_config);
        }
    } else {
        app.status = "Error: " + result.error;
    }
}

void ProfilesPanel::render_export_import() {
    if (ImGui::CollapsingHeader("Import / Export")) {
        ImGui::Indent();
        
        if (ImGui::Button("Export All...")) {
            nfdchar_t* out_path = nullptr;
            nfdfilteritem_t filters[1] = { { "JSON", "json" } };
            nfdresult_t result = NFD_SaveDialog(&out_path, filters, 1, nullptr, "agent47-profiles.json");
            if (result == NFD_OKAY && out_path) {
                file_path_buf_.fill('\0');
                std::strncpy(file_path_buf_.data(), out_path, file_path_buf_.size() - 1);
                NFD_FreePath(out_path);
                do_export();
            }
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("Import...")) {
            nfdchar_t* out_path = nullptr;
            nfdfilteritem_t filters[1] = { { "JSON", "json" } };
            nfdresult_t result = NFD_OpenDialog(&out_path, filters, 1, nullptr);
            if (result == NFD_OKAY && out_path) {
                file_path_buf_.fill('\0');
                std::strncpy(file_path_buf_.data(), out_path, file_path_buf_.size() - 1);
                NFD_FreePath(out_path);
                do_import();
            }
        }
        
        if (!export_import_status_.empty()) {
            ImVec4 color = export_import_status_.find("Error") != std::string::npos
                ? ImVec4(1, 0.4f, 0.4f, 1)
                : ImVec4(0.4f, 1, 0.4f, 1);
            ImGui::TextColored(color, "%s", export_import_status_.c_str());
        }
        
        ImGui::Unindent();
    }
}

void ProfilesPanel::do_export() {
    std::string path = file_path_buf_.data();
    if (path.empty()) {
        export_import_status_ = "Error: No file path specified";
        return;
    }
    
    std::vector<ExportedConfig> configs;
    for (const auto& app : apps_) {
        if (app.loaded) {
            ExportedConfig ec;
            ec.app_name = ConfigExporter::app_kind_to_string(app.kind);
            ec.config.provider = app.provider_buf.data();
            ec.config.model = app.model_buf.data();
            configs.push_back(ec);
        }
    }
    
    if (ConfigExporter::export_to_file(path, configs)) {
        export_import_status_ = "Exported " + std::to_string(configs.size()) + " configs";
    } else {
        export_import_status_ = "Error: Failed to write file";
    }
}

void ProfilesPanel::do_import() {
    std::string path = file_path_buf_.data();
    if (path.empty()) {
        export_import_status_ = "Error: No file path specified";
        return;
    }
    
    auto bundle = ConfigExporter::import_from_file(path);
    if (!bundle) {
        export_import_status_ = "Error: Failed to parse file";
        return;
    }
    
    int applied = 0;
    for (const auto& cfg : bundle->configs) {
        auto kind = ConfigExporter::string_to_app_kind(cfg.app_name);
        if (!kind) continue;
        
        auto result = config_manager_->switch_config(*kind, cfg.config);
        if (result.success) {
            ++applied;
        }
    }
    
    refresh_all();
    export_import_status_ = "Imported " + std::to_string(applied) + " configs";
}

}
