#include "app/dockspace.h"
#include "app/dockspace.h"
#include "adapters/config_exporter.h"
#include "adapters/config_manager.h"
#include "ui/theme.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <nfd.h>
#include <cstdlib>
#include <vector>

#ifndef DIANA_VERSION
#define DIANA_VERSION "0.1.0"
#endif

extern "C" void diana_request_exit();

namespace diana {

static bool show_about_dialog = false;

static std::vector<ExportedConfig> collect_export_configs(ConfigManager& manager) {
    std::vector<ExportedConfig> configs;
    for (AppKind kind : {AppKind::ClaudeCode, AppKind::Codex, AppKind::OpenCode}) {
        auto current = manager.read_config(kind);
        if (!current) {
            continue;
        }
        ExportedConfig exported;
        exported.app_name = ConfigExporter::app_kind_to_string(kind);
        exported.config = current->active;
        configs.push_back(exported);
    }
    return configs;
}

static void open_url(const char* url) {
#if defined(__APPLE__)
    std::string cmd = std::string("open \"") + url + "\"";
    std::system(cmd.c_str());
#elif defined(__linux__)
    std::string cmd = std::string("xdg-open \"") + url + "\"";
    std::system(cmd.c_str());
#endif
}

static void render_about_dialog() {
    if (!show_about_dialog) return;
    
    ImGui::OpenPopup("About Diana");
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 280), ImGuiCond_Appearing);
    
    if (ImGui::BeginPopupModal("About Diana", &show_about_dialog, ImGuiWindowFlags_NoResize)) {
        const auto& theme = get_current_theme();
        
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Diana").x) * 0.5f);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(theme.accent), "Diana");
        ImGui::PopFont();
        
        ImGui::Spacing();
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("Your Agent's Best Handler").x) * 0.5f);
        ImGui::TextDisabled("Your Agent's Best Handler");
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::Text("Version:");
        ImGui::SameLine(120);
        ImGui::Text("%s", DIANA_VERSION);
        
        ImGui::Text("Platform:");
        ImGui::SameLine(120);
#if defined(__APPLE__)
        ImGui::Text("macOS");
#elif defined(__linux__)
        ImGui::Text("Linux");
#else
        ImGui::Text("Unknown");
#endif
        
        ImGui::Text("Dear ImGui:");
        ImGui::SameLine(120);
        ImGui::Text("%s", IMGUI_VERSION);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        ImGui::TextDisabled("Mission control for AI coding agents.");
        ImGui::TextDisabled("Unified config, token tracking, multi-tab terminal.");
        
        ImGui::Spacing();
        ImGui::Spacing();
        
        float button_width = 120;
        float total_width = button_width * 2 + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - total_width) * 0.5f);
        
        if (ImGui::Button("GitHub", ImVec2(button_width, 0))) {
            open_url("https://github.com/taotao7/Diana");
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(button_width, 0))) {
            show_about_dialog = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void render_dockspace(bool first_frame, const DockspacePanels& panels) {
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    static ConfigManager config_manager;
    static std::string pending_import_path;
    static bool show_import_confirmation = false;
    static bool show_import_result = false;
    static bool import_success = false;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) {
        window_flags |= ImGuiWindowFlags_NoBackground;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        ImGuiID dockspace_id = ImGui::GetID("DianaDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

        if (first_frame) {
            ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
            // Only build default layout if the dockspace has no existing splits (i.e. not loaded from INI)
            if (!node || node->IsLeafNode()) {
                ImGui::DockBuilderRemoveNode(dockspace_id);
                ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
                ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

                ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f, nullptr, &dockspace_id);
                ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.25f, nullptr, &dockspace_id);
                ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.3f, nullptr, &dockspace_id);
                ImGuiID dock_id_center = dockspace_id;
                
                ImGuiID dock_id_right_bottom = ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Down, 0.5f, nullptr, &dock_id_right);

                ImGui::DockBuilderDockWindow("Agent Config", dock_id_left);
                ImGui::DockBuilderDockWindow("Terminal", dock_id_center);
                ImGui::DockBuilderDockWindow("Token Metrics", dock_id_right);
                ImGui::DockBuilderDockWindow("Agent Token Stats", dock_id_right_bottom);

                ImGui::DockBuilderFinish(dockspace_id);
            }
        }
    }

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            const nfdfilteritem_t filter_items[1] = {{"JSON", "json"}};
            if (ImGui::MenuItem("Import Config...", "Ctrl+I")) {
                nfdchar_t* out_path = nullptr;
                nfdresult_t result = NFD_OpenDialog(&out_path, filter_items, 1, nullptr);
                if (result == NFD_OKAY && out_path) {
                    pending_import_path = std::string(out_path);
                    show_import_confirmation = true;
                    NFD_FreePath(out_path);
                }
            }
            if (ImGui::MenuItem("Export Config...", "Ctrl+E")) {
                nfdchar_t* out_path = nullptr;
                nfdresult_t result = NFD_SaveDialog(&out_path, filter_items, 1, nullptr, "diana-config.json");
                if (result == NFD_OKAY && out_path) {
                    auto configs = collect_export_configs(config_manager);
                    auto diana_files = ConfigExporter::collect_diana_config_files();
                    ConfigExporter::export_to_file(out_path, configs, diana_files);
                    NFD_FreePath(out_path);
                }
            }
            ImGui::Separator();
#if !defined(__APPLE__)
            if (ImGui::MenuItem("Exit", "Alt+F4")) { diana_request_exit(); }
#endif
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (panels.show_terminal) {
                ImGui::MenuItem("Terminal", "Ctrl+1", panels.show_terminal);
            }
            if (panels.show_agent_config) {
                ImGui::MenuItem("Agent Config", "Ctrl+2", panels.show_agent_config);
            }
            if (panels.show_token_metrics) {
                ImGui::MenuItem("Token Metrics", "Ctrl+3", panels.show_token_metrics);
            }
            if (panels.show_agent_token_stats) {
                ImGui::MenuItem("Agent Token Stats", "Ctrl+4", panels.show_agent_token_stats);
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Theme")) {
                ThemeMode mode = get_theme_mode();
                if (ImGui::MenuItem("System", nullptr, mode == ThemeMode::System)) {
                    set_theme_mode(ThemeMode::System);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Dark (Magnetic Night)", nullptr, mode == ThemeMode::Dark)) {
                    set_theme_mode(ThemeMode::Dark);
                }
                if (ImGui::MenuItem("Light (Beige Terminal)", nullptr, mode == ThemeMode::Light)) {
                    set_theme_mode(ThemeMode::Light);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("Documentation")) {
                open_url("https://github.com/taotao7/Diana#readme");
            }
            if (ImGui::MenuItem("Report Issue")) {
                open_url("https://github.com/taotao7/Diana/issues");
            }
            ImGui::Separator();
            if (ImGui::MenuItem("About Diana")) {
                show_about_dialog = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (show_import_confirmation) {
        ImGui::OpenPopup("Confirm Import");
    }

    if (ImGui::BeginPopupModal("Confirm Import", &show_import_confirmation, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "Warning: Overwriting Configuration");
        ImGui::Spacing();
        ImGui::TextWrapped("Importing this bundle will overwrite existing configuration files in:");
        ImGui::Spacing();
        ImGui::TextDisabled("~/.config/diana");
        ImGui::Spacing();
        ImGui::TextWrapped("This action cannot be undone. Continue?");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            show_import_confirmation = false;
            pending_import_path.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Import", ImVec2(120, 0))) {
            import_success = false;
            if (!pending_import_path.empty()) {
                auto bundle = ConfigExporter::import_from_file(pending_import_path);
                if (bundle) {
                    bool restore_ok = true;
                    if (!bundle->diana_files.empty()) {
                        restore_ok = ConfigExporter::restore_diana_config_files(bundle->diana_files);
                    }
                    if (restore_ok) {
                        for (const auto& cfg : bundle->configs) {
                            auto kind = ConfigExporter::string_to_app_kind(cfg.app_name);
                            if (kind) {
                                config_manager.switch_config(*kind, cfg.config);
                            }
                        }
                        import_success = true;
                    }
                }
            }
            show_import_confirmation = false;
            pending_import_path.clear();
            ImGui::CloseCurrentPopup();
            show_import_result = true;
        }

        ImGui::EndPopup();
    }

    if (show_import_result) {
        ImGui::OpenPopup("Import Result");
    }

    if (ImGui::BeginPopupModal("Import Result", &show_import_result, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (import_success) {
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Import Successful");
            ImGui::Spacing();
            ImGui::Text("Configuration imported successfully.");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Import Failed");
            ImGui::Spacing();
            ImGui::Text("Failed to load configuration bundle.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        float button_width = 120.0f;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - button_width) * 0.5f);
        if (ImGui::Button("OK", ImVec2(button_width, 0))) {
            show_import_result = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (!io.WantTextInput && (io.KeyCtrl || io.KeySuper)) {
        if (panels.show_terminal && ImGui::IsKeyPressed(ImGuiKey_1)) {
            *panels.show_terminal = !*panels.show_terminal;
        }
        if (panels.show_agent_config && ImGui::IsKeyPressed(ImGuiKey_2)) {
            *panels.show_agent_config = !*panels.show_agent_config;
        }
        if (panels.show_token_metrics && ImGui::IsKeyPressed(ImGuiKey_3)) {
            *panels.show_token_metrics = !*panels.show_token_metrics;
        }
        if (panels.show_agent_token_stats && ImGui::IsKeyPressed(ImGuiKey_4)) {
            *panels.show_agent_token_stats = !*panels.show_agent_token_stats;
        }
    }
    
    render_about_dialog();

    ImGui::End();
}

}
