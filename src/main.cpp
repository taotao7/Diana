#include <cstdio>
#include <cstdlib>

#define GL_SILENCE_DEPRECATION
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

#include <GLFW/glfw3.h>

#include "app/app_shell.h"
#include "ui/theme.h"

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif

    GLFWwindow* window = glfwCreateWindow(1600, 900, "Diana - AI Agent Monitor", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui::StyleColorsDark();
    agent47::update_system_theme();

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImFontConfig font_config;
    font_config.OversampleH = 2;
    font_config.OversampleV = 1;
    
    static const ImWchar ranges[] = {
        0x0020, 0x00FF,
        0x2000, 0x206F,
        0x2100, 0x214F,
        0x2190, 0x21FF,
        0x2200, 0x22FF,
        0x2300, 0x23FF,
        0x2500, 0x257F,
        0x2580, 0x259F,
        0x25A0, 0x25FF,
        0x2600, 0x26FF,
        0x2700, 0x27BF,
        0x2B00, 0x2BFF,
        0x3000, 0x303F,
        0x3040, 0x309F,
        0x30A0, 0x30FF,
        0x4E00, 0x9FFF,
        0xF000, 0xF0FF,
        0xFE00, 0xFE0F,
        0xFF00, 0xFFEF,
        0x1F300, 0x1F9FF,
        0,
    };
    
    ImFont* font = io.Fonts->AddFontFromFileTTF("resources/fonts/unifont.otf", 16.0f, &font_config, ranges);
    if (!font) {
        io.Fonts->AddFontDefault();
    }

    const char* glsl_version = "#version 150";
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    agent47::AppShell app_shell;
    app_shell.init();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        agent47::update_system_theme();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        app_shell.render();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }

    app_shell.shutdown();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
