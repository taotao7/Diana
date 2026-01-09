#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace agent47 {

struct ColoredSpan {
    std::string text;
    uint32_t color = 0xFFDCDCDC;
};

class AnsiParser {
public:
    AnsiParser();
    
    std::vector<ColoredSpan> parse_line(const std::string& input);
    
    void reset();
    
    uint32_t current_color() const { return current_color_; }
    
    void set_default_color(uint32_t color);
    void update_palette_from_theme();

private:
    void process_sgr(const std::string& params);
    uint32_t make_color(uint8_t r, uint8_t g, uint8_t b) const;
    
    uint32_t current_color_ = 0xFFDCDCDC;
    uint32_t default_color_ = 0xFFDCDCDC;
    
    uint32_t basic_colors_[8];
    uint32_t bright_colors_[8];
};

}
