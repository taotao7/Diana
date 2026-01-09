#pragma once

#include <cstdint>
#include <string>

namespace diana {

// ANSI terminal colors (0-7 normal, 8-15 bright)
struct AnsiColors {
    uint32_t black;
    uint32_t red;
    uint32_t green;
    uint32_t yellow;
    uint32_t blue;
    uint32_t magenta;
    uint32_t cyan;
    uint32_t white;
    uint32_t bright_black;
    uint32_t bright_red;
    uint32_t bright_green;
    uint32_t bright_yellow;
    uint32_t bright_blue;
    uint32_t bright_magenta;
    uint32_t bright_cyan;
    uint32_t bright_white;
};

enum class ThemeKind {
    Dark,
    Light
};

enum class ThemeMode {
    System,
    Dark,
    Light
};

struct Theme {
    std::string name;
    ThemeKind kind;
    
    // Core colors
    uint32_t background;         // Main window background
    uint32_t background_dark;    // Sidebar / darker panels
    uint32_t background_light;   // Lighter panels
    uint32_t foreground;         // Main text color
    uint32_t foreground_dim;     // Dimmed text
    uint32_t accent;             // Primary accent color (orange)
    uint32_t accent_hover;       // Accent on hover
    uint32_t selection;          // Selection background
    uint32_t border;             // Border color
    uint32_t scrollbar;          // Scrollbar color
    uint32_t scrollbar_hover;    // Scrollbar hover
    
    // Terminal colors
    uint32_t terminal_bg;        // Terminal background
    uint32_t terminal_fg;        // Terminal foreground (default text)
    uint32_t terminal_cursor;    // Terminal cursor color
    uint32_t terminal_cursor_glow; // Terminal cursor glow color
    AnsiColors ansi;             // ANSI color palette
    
    // Button colors
    uint32_t button;
    uint32_t button_hover;
    uint32_t button_active;
    
    // Input field colors
    uint32_t input_bg;
    uint32_t input_border;
    
    // Header colors
    uint32_t header;
    uint32_t header_hover;
    uint32_t header_active;
    
    // Tab colors
    uint32_t tab;
    uint32_t tab_hover;
    uint32_t tab_active;
    uint32_t tab_unfocused;
    
    // Status colors (semantic)
    uint32_t success;
    uint32_t warning;
    uint32_t error;
    uint32_t info;
};

// Get predefined themes
Theme get_dark_theme();
Theme get_light_theme();

// Apply theme to ImGui
void apply_theme(const Theme& theme);

// Get current theme
const Theme& get_current_theme();

// Theme mode management
ThemeMode get_theme_mode();
void set_theme_mode(ThemeMode mode);

// Call each frame to check system theme changes (when mode is System)
void update_system_theme();

// Detect system dark mode (platform-specific)
bool is_system_dark_mode();

// Helper: convert hex RGB to ImGui color (ABGR format)
constexpr uint32_t hex_to_imgui(uint32_t hex_rgb) {
    uint8_t r = (hex_rgb >> 16) & 0xFF;
    uint8_t g = (hex_rgb >> 8) & 0xFF;
    uint8_t b = hex_rgb & 0xFF;
    return 0xFF000000 | (b << 16) | (g << 8) | r;
}

}
