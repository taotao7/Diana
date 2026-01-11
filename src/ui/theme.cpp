#include "ui/theme.h"
#include "imgui.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_MAC
extern "C" bool diana_is_dark_mode();
#endif
#endif

namespace diana {

namespace {
    Theme g_current_theme;
    ThemeMode g_theme_mode = ThemeMode::System;
    bool g_theme_initialized = false;
    bool g_last_system_dark = true;
}

Theme get_dark_theme() {
    Theme theme;
    theme.name = "Magnetic Night";
    theme.kind = ThemeKind::Dark;
    
    theme.background       = hex_to_imgui(0x25292e);
    theme.background_dark  = hex_to_imgui(0x1a1d21);
    theme.background_light = hex_to_imgui(0x2d3238);
    theme.foreground       = hex_to_imgui(0xd4d4d4);
    theme.foreground_dim   = hex_to_imgui(0x808080);
    theme.accent           = hex_to_imgui(0xffb86c);
    theme.accent_hover     = hex_to_imgui(0xffc88c);
    theme.selection        = hex_to_imgui(0x3a4a5a);
    theme.border           = hex_to_imgui(0x3a3f45);
    theme.scrollbar        = hex_to_imgui(0x4a4f55);
    theme.scrollbar_hover  = hex_to_imgui(0x5a5f65);
    
    theme.terminal_bg      = hex_to_imgui(0x1a1d21);
    theme.terminal_fg      = hex_to_imgui(0xd4d4d4);
    theme.terminal_cursor  = hex_to_imgui(0xffb86c);
    theme.terminal_cursor_glow = hex_to_imgui(0xffb86c);
    
    theme.ansi.black         = hex_to_imgui(0x000000);
    theme.ansi.red           = hex_to_imgui(0xff5555);
    theme.ansi.green         = hex_to_imgui(0x50fa7b);
    theme.ansi.yellow        = hex_to_imgui(0xffb86c);
    theme.ansi.blue          = hex_to_imgui(0x8be9fd);
    theme.ansi.magenta       = hex_to_imgui(0xbd93f9);
    theme.ansi.cyan          = hex_to_imgui(0x8be9fd);
    theme.ansi.white         = hex_to_imgui(0xe5e5e5);
    theme.ansi.bright_black  = hex_to_imgui(0x7f7f7f);
    theme.ansi.bright_red    = hex_to_imgui(0xff6e6e);
    theme.ansi.bright_green  = hex_to_imgui(0x69ff94);
    theme.ansi.bright_yellow = hex_to_imgui(0xffffa5);
    theme.ansi.bright_blue   = hex_to_imgui(0xd6acff);
    theme.ansi.bright_magenta= hex_to_imgui(0xff92df);
    theme.ansi.bright_cyan   = hex_to_imgui(0xa4ffff);
    theme.ansi.bright_white  = hex_to_imgui(0xffffff);
    
    theme.button        = hex_to_imgui(0x3a3f45);
    theme.button_hover  = hex_to_imgui(0xffb86c);
    theme.button_active = hex_to_imgui(0xe5a35c);
    
    theme.input_bg     = hex_to_imgui(0x1f2327);
    theme.input_border = hex_to_imgui(0x3a3f45);
    
    theme.header        = hex_to_imgui(0x3a3f45);
    theme.header_hover  = hex_to_imgui(0x4a4f55);
    theme.header_active = hex_to_imgui(0xffb86c);
    
    theme.tab           = hex_to_imgui(0x1f2327);
    theme.tab_hover     = hex_to_imgui(0x3a3f45);
    theme.tab_active    = hex_to_imgui(0x25292e);
    theme.tab_unfocused = hex_to_imgui(0x1a1d21);
    
    theme.success = hex_to_imgui(0x50fa7b);
    theme.warning = hex_to_imgui(0xffb86c);
    theme.error   = hex_to_imgui(0xff5555);
    theme.info    = hex_to_imgui(0x8be9fd);
    
    return theme;
}

Theme get_light_theme() {
    Theme theme;
    theme.name = "Beige Terminal";
    theme.kind = ThemeKind::Light;
    
    theme.background       = hex_to_imgui(0xfaf8f4);
    theme.background_dark  = hex_to_imgui(0xede8e0);
    theme.background_light = hex_to_imgui(0xf5f0e8);
    theme.foreground       = hex_to_imgui(0x2d2a27);
    theme.foreground_dim   = hex_to_imgui(0x504945);
    theme.accent           = hex_to_imgui(0xd65d0e);
    theme.accent_hover     = hex_to_imgui(0xe67018);
    theme.selection        = hex_to_imgui(0xd4c8b8);
    theme.border           = hex_to_imgui(0xc4b8a8);
    theme.scrollbar        = hex_to_imgui(0xc4b8a8);
    theme.scrollbar_hover  = hex_to_imgui(0xb4a898);
    
    theme.terminal_bg      = hex_to_imgui(0xf5f0e8);
    theme.terminal_fg      = hex_to_imgui(0x2d2a27);
    theme.terminal_cursor  = hex_to_imgui(0xd65d0e);
    theme.terminal_cursor_glow = hex_to_imgui(0xd65d0e);
    
    theme.ansi.black         = hex_to_imgui(0x2d2a27);
    theme.ansi.red           = hex_to_imgui(0x9d0006);
    theme.ansi.green         = hex_to_imgui(0x79740e);
    theme.ansi.yellow        = hex_to_imgui(0xb57614);
    theme.ansi.blue          = hex_to_imgui(0x076678);
    theme.ansi.magenta       = hex_to_imgui(0x8f3f71);
    theme.ansi.cyan          = hex_to_imgui(0x427b58);
    theme.ansi.white         = hex_to_imgui(0x504945);
    theme.ansi.bright_black  = hex_to_imgui(0x504945);
    theme.ansi.bright_red    = hex_to_imgui(0xcc241d);
    theme.ansi.bright_green  = hex_to_imgui(0x79740e);
    theme.ansi.bright_yellow = hex_to_imgui(0xd65d0e);
    theme.ansi.bright_blue   = hex_to_imgui(0x076678);
    theme.ansi.bright_magenta= hex_to_imgui(0x8f3f71);
    theme.ansi.bright_cyan   = hex_to_imgui(0x427b58);
    theme.ansi.bright_white  = hex_to_imgui(0x3c3836);
    
    theme.button        = hex_to_imgui(0xede8e0);
    theme.button_hover  = hex_to_imgui(0xd65d0e);
    theme.button_active = hex_to_imgui(0xc54d00);
    
    theme.input_bg     = hex_to_imgui(0xffffff);
    theme.input_border = hex_to_imgui(0xc4b8a8);
    
    theme.header        = hex_to_imgui(0xede8e0);
    theme.header_hover  = hex_to_imgui(0xd4c8b8);
    theme.header_active = hex_to_imgui(0xd65d0e);
    
    theme.tab           = hex_to_imgui(0xede8e0);
    theme.tab_hover     = hex_to_imgui(0xd4c8b8);
    theme.tab_active    = hex_to_imgui(0xfaf8f4);
    theme.tab_unfocused = hex_to_imgui(0xe5e0d8);
    
    theme.success = hex_to_imgui(0x79740e);
    theme.warning = hex_to_imgui(0xd65d0e);
    theme.error   = hex_to_imgui(0x9d0006);
    theme.info    = hex_to_imgui(0x076678);
    
    return theme;
}

static ImVec4 u32_to_imvec4(uint32_t color) {
    float r = (color & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = ((color >> 16) & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;
    return ImVec4(r, g, b, a);
}

void apply_theme(const Theme& theme) {
    g_current_theme = theme;
    g_theme_initialized = true;
    
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    colors[ImGuiCol_Text]                  = u32_to_imvec4(theme.foreground);
    colors[ImGuiCol_TextDisabled]          = u32_to_imvec4(theme.foreground_dim);
    colors[ImGuiCol_WindowBg]              = u32_to_imvec4(theme.background);
    colors[ImGuiCol_ChildBg]               = u32_to_imvec4(theme.background);
    colors[ImGuiCol_PopupBg]               = u32_to_imvec4(theme.background_dark);
    colors[ImGuiCol_Border]                = u32_to_imvec4(theme.border);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_FrameBg]               = u32_to_imvec4(theme.input_bg);
    colors[ImGuiCol_FrameBgHovered]        = u32_to_imvec4(theme.selection);
    colors[ImGuiCol_FrameBgActive]         = u32_to_imvec4(theme.selection);
    colors[ImGuiCol_TitleBg]               = u32_to_imvec4(theme.background_dark);
    colors[ImGuiCol_TitleBgActive]         = u32_to_imvec4(theme.background_dark);
    colors[ImGuiCol_TitleBgCollapsed]      = u32_to_imvec4(theme.background_dark);
    colors[ImGuiCol_MenuBarBg]             = u32_to_imvec4(theme.background_dark);
    colors[ImGuiCol_ScrollbarBg]           = u32_to_imvec4(theme.background_dark);
    colors[ImGuiCol_ScrollbarGrab]         = u32_to_imvec4(theme.scrollbar);
    colors[ImGuiCol_ScrollbarGrabHovered]  = u32_to_imvec4(theme.scrollbar_hover);
    colors[ImGuiCol_ScrollbarGrabActive]   = u32_to_imvec4(theme.accent);
    colors[ImGuiCol_CheckMark]             = u32_to_imvec4(theme.accent);
    colors[ImGuiCol_SliderGrab]            = u32_to_imvec4(theme.accent);
    colors[ImGuiCol_SliderGrabActive]      = u32_to_imvec4(theme.accent_hover);
    colors[ImGuiCol_Button]                = u32_to_imvec4(theme.button);
    colors[ImGuiCol_ButtonHovered]         = u32_to_imvec4(theme.button_hover);
    colors[ImGuiCol_ButtonActive]          = u32_to_imvec4(theme.button_active);
    colors[ImGuiCol_Header]                = u32_to_imvec4(theme.header);
    colors[ImGuiCol_HeaderHovered]         = u32_to_imvec4(theme.header_hover);
    colors[ImGuiCol_HeaderActive]          = u32_to_imvec4(theme.header_active);
    colors[ImGuiCol_Separator]             = u32_to_imvec4(theme.border);
    colors[ImGuiCol_SeparatorHovered]      = u32_to_imvec4(theme.accent);
    colors[ImGuiCol_SeparatorActive]       = u32_to_imvec4(theme.accent);
    colors[ImGuiCol_ResizeGrip]            = u32_to_imvec4(theme.border);
    colors[ImGuiCol_ResizeGripHovered]     = u32_to_imvec4(theme.accent);
    colors[ImGuiCol_ResizeGripActive]      = u32_to_imvec4(theme.accent);
    colors[ImGuiCol_Tab]                   = u32_to_imvec4(theme.tab);
    colors[ImGuiCol_TabHovered]            = u32_to_imvec4(theme.tab_hover);
    colors[ImGuiCol_TabActive]             = u32_to_imvec4(theme.tab_active);
    colors[ImGuiCol_TabUnfocused]          = u32_to_imvec4(theme.tab_unfocused);
    colors[ImGuiCol_TabUnfocusedActive]    = u32_to_imvec4(theme.tab);
    colors[ImGuiCol_DockingPreview]        = u32_to_imvec4(theme.accent);
    colors[ImGuiCol_DockingEmptyBg]        = u32_to_imvec4(theme.background_dark);
    colors[ImGuiCol_PlotLines]             = u32_to_imvec4(theme.accent);
    colors[ImGuiCol_PlotLinesHovered]      = u32_to_imvec4(theme.accent_hover);
    colors[ImGuiCol_PlotHistogram]         = u32_to_imvec4(theme.accent);
    colors[ImGuiCol_PlotHistogramHovered]  = u32_to_imvec4(theme.accent_hover);
    colors[ImGuiCol_TableHeaderBg]         = u32_to_imvec4(theme.background_dark);
    colors[ImGuiCol_TableBorderStrong]     = u32_to_imvec4(theme.border);
    colors[ImGuiCol_TableBorderLight]      = u32_to_imvec4(theme.border);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_TableRowBgAlt]         = u32_to_imvec4(theme.background_light);
    colors[ImGuiCol_TextSelectedBg]        = u32_to_imvec4(theme.selection);
    colors[ImGuiCol_DragDropTarget]        = u32_to_imvec4(theme.accent);
    colors[ImGuiCol_NavHighlight]          = u32_to_imvec4(theme.accent);
    colors[ImGuiCol_NavWindowingHighlight] = u32_to_imvec4(theme.accent);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.2f, 0.2f, 0.2f, 0.35f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.2f, 0.2f, 0.2f, 0.35f);
    
    style.WindowRounding    = 4.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 4.0f;
    
    style.WindowPadding     = ImVec2(8, 8);
    style.FramePadding      = ImVec2(6, 4);
    style.ItemSpacing       = ImVec2(8, 4);
    style.ItemInnerSpacing  = ImVec2(4, 4);
    style.ScrollbarSize     = 12.0f;
    style.GrabMinSize       = 10.0f;
    
    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.PopupBorderSize   = 1.0f;
    style.TabBorderSize     = 0.0f;
}

const Theme& get_current_theme() {
    if (!g_theme_initialized) {
        update_system_theme();
    }
    return g_current_theme;
}

ThemeMode get_theme_mode() {
    return g_theme_mode;
}

void set_theme_mode(ThemeMode mode) {
    g_theme_mode = mode;
    
    if (mode == ThemeMode::System) {
        update_system_theme();
    } else if (mode == ThemeMode::Dark) {
        apply_theme(get_dark_theme());
    } else {
        apply_theme(get_light_theme());
    }
}

bool is_system_dark_mode() {
#if defined(__APPLE__) && TARGET_OS_MAC
    return diana_is_dark_mode();
#else
    return true;
#endif
}

void update_system_theme() {
    if (g_theme_mode != ThemeMode::System) {
        return;
    }
    
    bool is_dark = is_system_dark_mode();
    
    if (!g_theme_initialized || is_dark != g_last_system_dark) {
        g_last_system_dark = is_dark;
        if (is_dark) {
            apply_theme(get_dark_theme());
        } else {
            apply_theme(get_light_theme());
        }
    }
}

}
