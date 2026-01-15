#include "app/dockspace.h"
#include "app/dockspace.h"
#include "ui/theme.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <cstdlib>

#ifndef DIANA_VERSION
#define DIANA_VERSION "0.1.0"
#endif

extern "C" void diana_request_exit();

namespace diana {

static bool show_about_dialog = false;

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
            if (ImGui::MenuItem("Import Config...", "Ctrl+I")) {}
            if (ImGui::MenuItem("Export Config...", "Ctrl+E")) {}
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
