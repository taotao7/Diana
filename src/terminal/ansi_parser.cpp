#include "ansi_parser.h"
#include <sstream>
#include <cstdlib>

namespace agent47 {

namespace {

enum AnsiBasicColor : uint32_t {
    BLACK   = 0xFF000000,
    RED     = 0xFF0000CD,
    GREEN   = 0xFF00CD00,
    YELLOW  = 0xFF00CDCD,
    BLUE    = 0xFFCD0000,
    MAGENTA = 0xFFCD00CD,
    CYAN    = 0xFFCDCD00,
    WHITE   = 0xFFE5E5E5,
};

enum AnsiBrightColor : uint32_t {
    BRIGHT_BLACK   = 0xFF7F7F7F,
    BRIGHT_RED     = 0xFF0000FF,
    BRIGHT_GREEN   = 0xFF00FF00,
    BRIGHT_YELLOW  = 0xFF00FFFF,
    BRIGHT_BLUE    = 0xFFFF0000,
    BRIGHT_MAGENTA = 0xFFFF00FF,
    BRIGHT_CYAN    = 0xFFFFFF00,
    BRIGHT_WHITE   = 0xFFFFFFFF,
};

constexpr uint32_t BASIC_COLORS[] = {
    BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE
};

constexpr uint32_t BRIGHT_COLORS[] = {
    BRIGHT_BLACK, BRIGHT_RED, BRIGHT_GREEN, BRIGHT_YELLOW,
    BRIGHT_BLUE, BRIGHT_MAGENTA, BRIGHT_CYAN, BRIGHT_WHITE
};

}

AnsiParser::AnsiParser() {
    reset();
}

void AnsiParser::reset() {
    current_color_ = default_color_;
}

uint32_t AnsiParser::make_color(uint8_t r, uint8_t g, uint8_t b) const {
    return 0xFF000000 | (b << 16) | (g << 8) | r;
}

std::vector<ColoredSpan> AnsiParser::parse_line(const std::string& input) {
    std::vector<ColoredSpan> spans;
    std::string current_text;
    
    size_t i = 0;
    while (i < input.size()) {
        if (input[i] == '\x1b' && i + 1 < input.size() && input[i + 1] == '[') {
            if (!current_text.empty()) {
                spans.push_back({current_text, current_color_});
                current_text.clear();
            }
            
            i += 2;
            std::string params;
            while (i < input.size() && input[i] != 'm' && input[i] != 'H' && 
                   input[i] != 'J' && input[i] != 'K' && input[i] != 'A' &&
                   input[i] != 'B' && input[i] != 'C' && input[i] != 'D' &&
                   input[i] != 'h' && input[i] != 'l' && input[i] != '?') {
                params += input[i];
                ++i;
            }
            
            if (i < input.size()) {
                char cmd = input[i];
                ++i;
                
                if (cmd == 'm') {
                    process_sgr(params);
                }
            }
        } else {
            current_text += input[i];
            ++i;
        }
    }
    
    if (!current_text.empty()) {
        spans.push_back({current_text, current_color_});
    }
    
    if (spans.empty()) {
        spans.push_back({"", current_color_});
    }
    
    return spans;
}

void AnsiParser::process_sgr(const std::string& params) {
    if (params.empty()) {
        current_color_ = default_color_;
        return;
    }
    
    std::vector<int> codes;
    std::stringstream ss(params);
    std::string token;
    while (std::getline(ss, token, ';')) {
        if (!token.empty()) {
            codes.push_back(std::atoi(token.c_str()));
        }
    }
    
    size_t i = 0;
    while (i < codes.size()) {
        int code = codes[i];
        
        if (code == 0) {
            current_color_ = default_color_;
        }
        else if (code >= 30 && code <= 37) {
            current_color_ = BASIC_COLORS[code - 30];
        }
        else if (code >= 90 && code <= 97) {
            current_color_ = BRIGHT_COLORS[code - 90];
        }
        else if (code == 38) {
            if (i + 1 < codes.size() && codes[i + 1] == 5 && i + 2 < codes.size()) {
                int idx = codes[i + 2];
                if (idx < 8) {
                    current_color_ = BASIC_COLORS[idx];
                } else if (idx < 16) {
                    current_color_ = BRIGHT_COLORS[idx - 8];
                } else if (idx < 232) {
                    int n = idx - 16;
                    int r = (n / 36) * 51;
                    int g = ((n / 6) % 6) * 51;
                    int b = (n % 6) * 51;
                    current_color_ = make_color(r, g, b);
                } else {
                    int gray = (idx - 232) * 10 + 8;
                    current_color_ = make_color(gray, gray, gray);
                }
                i += 2;
            }
            else if (i + 1 < codes.size() && codes[i + 1] == 2 && i + 4 < codes.size()) {
                int r = codes[i + 2];
                int g = codes[i + 3];
                int b = codes[i + 4];
                current_color_ = make_color(r, g, b);
                i += 4;
            }
        }
        else if (code == 39) {
            current_color_ = default_color_;
        }
        
        ++i;
    }
}

}
