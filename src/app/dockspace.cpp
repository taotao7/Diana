#include "app/dockspace.h"
#include "ui/theme.h"

#include "imgui.h"
#include "imgui_internal.h"

extern "C" void diana_request_exit();

namespace diana {

void render_dockspace(bool first_frame) {
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
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

            ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f, nullptr, &dockspace_id);
            ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.25f, nullptr, &dockspace_id);
            ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.3f, nullptr, &dockspace_id);
            ImGuiID dock_id_center = dockspace_id;
            
            ImGuiID dock_id_right_bottom = ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Down, 0.5f, nullptr, &dock_id_right);

            ImGui::DockBuilderDockWindow("Claude Code", dock_id_left);
            ImGui::DockBuilderDockWindow("Terminal", dock_id_center);
            ImGui::DockBuilderDockWindow("Token Metrics", dock_id_right);
            ImGui::DockBuilderDockWindow("Agent Token Stats", dock_id_right_bottom);

            ImGui::DockBuilderFinish(dockspace_id);
        }
    }

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Import Config...")) {}
            if (ImGui::MenuItem("Export Config...")) {}
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) { diana_request_exit(); }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Terminal")) {}
            if (ImGui::MenuItem("Claude Code")) {}
            if (ImGui::MenuItem("Token Metrics")) {}
            if (ImGui::MenuItem("Agent Token Stats")) {}
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
            if (ImGui::MenuItem("About Diana")) {}
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGui::End();
}

}
