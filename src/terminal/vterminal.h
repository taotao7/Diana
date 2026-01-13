#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace diana {

constexpr int TERMINAL_MAX_CHARS_PER_CELL = 6;

struct TerminalCell {
    uint32_t chars[TERMINAL_MAX_CHARS_PER_CELL];
    uint8_t width;
    uint32_t fg;
    uint32_t bg;
    bool bold;
    bool italic;
    bool underline;
    bool strike;
    bool reverse;
};

struct CursorInfo {
    int row;
    int col;
    bool visible;
    int shape;
};

class VTerminalImpl;

class VTerminal {
public:
    VTerminal(int rows, int cols);
    ~VTerminal();
    
    VTerminal(const VTerminal&) = delete;
    VTerminal& operator=(const VTerminal&) = delete;
    
    void resize(int rows, int cols);
    void write(const char* data, size_t len);
    
    int rows() const { return rows_; }
    int cols() const { return cols_; }
    
    TerminalCell get_cell(int row, int col) const;
    CursorInfo get_cursor() const;
    
    std::string get_output();
    
    // Keyboard input - generates correct escape sequences based on terminal mode
    void keyboard_key(int key);  // Use VTermKey enum values
    void keyboard_unichar(uint32_t c, int modifiers = 0);
    void keyboard_start_paste();
    void keyboard_end_paste();
    
    void set_default_colors(uint32_t fg, uint32_t bg);
    
    using ScrollbackCallback = std::function<void(const std::vector<TerminalCell>&)>;
    void set_scrollback_callback(ScrollbackCallback cb);
    
    const std::vector<std::vector<TerminalCell>>& scrollback() const { return scrollback_; }
    size_t scrollback_size() const { return scrollback_.size(); }

private:
    friend class VTerminalImpl;
    
    std::unique_ptr<VTerminalImpl> impl_;
    int rows_;
    int cols_;
    
    CursorInfo cursor_{0, 0, true, 1};
    
    std::vector<std::vector<TerminalCell>> scrollback_;
    static constexpr size_t MAX_SCROLLBACK = 10000;
    
    ScrollbackCallback scrollback_cb_;
    
    uint32_t default_fg_ = 0xFFD4D4D4;
    uint32_t default_bg_ = 0xFF211D1A;
};

}
