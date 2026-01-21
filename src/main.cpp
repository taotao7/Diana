#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

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

static GLFWwindow* g_main_window = nullptr;
namespace fs = std::filesystem;

static bool is_font_file(const fs::path& path) {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return ext == ".ttf" || ext == ".otf" || ext == ".ttc";
}

static fs::path find_fonts_directory(const char* argv0) {
    std::vector<fs::path> candidates;

    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    if (!ec) {
        candidates.push_back(cwd / "resources");
        candidates.push_back(cwd / "resources" / "fonts");
    }

    if (argv0 && argv0[0] != '\0') {
        fs::path exe_path = fs::path(argv0);
        if (exe_path.is_relative() && !cwd.empty()) {
            exe_path = cwd / exe_path;
        }
        const fs::path exe_dir = exe_path.parent_path();
        if (!exe_dir.empty()) {
            candidates.push_back(exe_dir / "resources");
            candidates.push_back(exe_dir / "resources" / "fonts");
            candidates.push_back(exe_dir.parent_path() / "Resources" / "resources");
            candidates.push_back(exe_dir.parent_path() / "Resources" / "resources" / "fonts");
        }
    }

    for (const auto& candidate : candidates) {
        std::error_code exists_ec;
        if (!fs::exists(candidate, exists_ec) || exists_ec) continue;

        std::error_code dir_ec;
        if (fs::is_directory(candidate, dir_ec) && !dir_ec) {
            std::error_code canonical_ec;
            const fs::path canonical = fs::weakly_canonical(candidate, canonical_ec);
            return canonical_ec ? candidate : canonical;
        }
    }

    return {};
}

static std::vector<fs::path> collect_font_files(const fs::path& fonts_dir) {
    std::vector<fs::path> files;
    if (fonts_dir.empty()) {
        return files;
    }

    std::error_code ec;
    if (!fs::exists(fonts_dir, ec) || ec || !fs::is_directory(fonts_dir, ec)) {
        return files;
    }

    for (const auto& entry : fs::recursive_directory_iterator(fonts_dir)) {
        if (!entry.is_regular_file()) continue;
        if (is_font_file(entry.path())) {
            files.push_back(entry.path());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

static std::vector<fs::path> build_font_load_order(const std::vector<fs::path>& font_files) {
    if (font_files.empty()) {
        return font_files;
    }

    auto to_lower = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    };

    const auto find_first_matching = [&](const std::string& needle) -> std::optional<fs::path> {
        for (const auto& font : font_files) {
            const std::string name = to_lower(font.stem().string());
            if (name.find(needle) != std::string::npos) {
                return font;
            }
        }
        return std::nullopt;
    };

    std::vector<fs::path> ordered;
    ordered.reserve(2);
    
    auto regular = find_first_matching("regular");
    auto unifont = find_first_matching("unifont");
    
    if (regular) {
        ordered.push_back(*regular);
    }
    if (unifont) {
        ordered.push_back(*unifont);
    }
    
    if (ordered.empty() && !font_files.empty()) {
        ordered.push_back(font_files.front());
    }

    return ordered;
}

static bool is_unifont_font(const fs::path& font_path) {
    std::string name = font_path.stem().string();
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return name.find("unifont") != std::string::npos;
}

static std::vector<fs::path> collect_system_cjk_fonts() {
    std::vector<fs::path> fonts;
#if defined(__APPLE__)
    const char* candidates[] = {
        "/System/Library/Fonts/PingFang.ttc",
        "/System/Library/Fonts/Supplemental/PingFang.ttc",
        "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
        "/System/Library/Fonts/Supplemental/STHeiti Light.ttc",
        "/System/Library/Fonts/Supplemental/STHeiti Medium.ttc",
        "/System/Library/Fonts/Supplemental/Heiti SC.ttc",
    };
    for (const auto* path : candidates) {
        std::error_code ec;
        fs::path font_path(path);
        if (fs::exists(font_path, ec) && !ec) {
            fonts.push_back(font_path);
        }
    }
#endif
    return fonts;
}

extern "C" bool diana_is_ctrl_pressed() {
    if (!g_main_window) return false;
    return glfwGetKey(g_main_window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
           glfwGetKey(g_main_window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
}

extern "C" void diana_request_exit() {
    if (g_main_window) {
        glfwSetWindowShouldClose(g_main_window, GLFW_TRUE);
    }
}

int main(int argc, char** argv) {
    (void)argc;

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

    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Diana - AI Agent Monitor", nullptr, nullptr);
    if (!window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return 1;
    }
    g_main_window = window;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniSavingRate = 1.0f;

    static std::string ini_path;
    const char* home = std::getenv("HOME");
    if (home) {
        ini_path = std::string(home) + "/.config/diana/imgui.ini";
        fs::create_directories(fs::path(ini_path).parent_path());
        io.IniFilename = ini_path.c_str();
    }

    ImGui::StyleColorsDark();
    diana::update_system_theme();

    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImFontConfig font_config;
    font_config.OversampleH = 1;
    font_config.OversampleV = 1;
    
    static const ImWchar primary_ranges[] = {
        0x0020, 0x00FF,
        0x2500, 0x257F,
        0x2580, 0x259F,
        0x25A0, 0x25FF,
        0,
    };
    
    static const ImWchar fallback_ranges[] = {
        0x0020, 0x00FF,
        0x2000, 0x206F,
        0x2100, 0x214F,
        0x2190, 0x21FF,
        0x2500, 0x257F,
        0x2580, 0x259F,
        0x25A0, 0x25FF,
        0x2600, 0x26FF,
        0x3000, 0x303F,
        0x3040, 0x309F,
        0x30A0, 0x30FF,
        0x3400, 0x4DBF,
        0x4E00, 0x9FFF,
        0xF900, 0xFAFF,
        0xFF00, 0xFFEF,
        0,
    };
    
    const float font_size = 16.0f;
    const fs::path fonts_dir = find_fonts_directory(argv ? argv[0] : "");
    std::vector<fs::path> load_order = build_font_load_order(collect_font_files(fonts_dir));
    const std::vector<fs::path> system_cjk_fonts = collect_system_cjk_fonts();
    const bool has_system_cjk = !system_cjk_fonts.empty();

    ImFont* font = nullptr;
    if (!load_order.empty()) {
        ImFontConfig primary_config = font_config;
        font = io.Fonts->AddFontFromFileTTF(load_order.front().string().c_str(), font_size, &primary_config, primary_ranges);
        
        if (font) {
            ImFontConfig merge_config = font_config;
            merge_config.MergeMode = true;
            for (size_t i = 1; i < load_order.size(); ++i) {
                if (has_system_cjk && is_unifont_font(load_order[i])) {
                    continue;
                }
                io.Fonts->AddFontFromFileTTF(load_order[i].string().c_str(), font_size, &merge_config, fallback_ranges);
            }

            for (const auto& system_font : system_cjk_fonts) {
                io.Fonts->AddFontFromFileTTF(system_font.string().c_str(), font_size, &merge_config, fallback_ranges);
            }

            if (has_system_cjk) {
                for (size_t i = 1; i < load_order.size(); ++i) {
                    if (!is_unifont_font(load_order[i])) {
                        continue;
                    }
                    io.Fonts->AddFontFromFileTTF(load_order[i].string().c_str(), font_size, &merge_config, fallback_ranges);
                }
            }
        }
    }

    if (!font) {
        font = io.Fonts->AddFontFromFileTTF("resources/fonts/unifont.otf", font_size, &font_config, fallback_ranges);
        if (!font) {
            io.Fonts->AddFontDefault();
        }
    }

    const char* glsl_version = "#version 150";
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    diana::AppShell app_shell;
    app_shell.init();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        diana::update_system_theme();

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
    g_main_window = nullptr;
    glfwTerminate();

    return 0;
}
