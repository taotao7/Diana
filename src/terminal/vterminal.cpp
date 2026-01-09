#include "vterminal.h"

extern "C" {
#include <vterm.h>
}

#include <cstring>
#include <algorithm>

namespace diana {

uint32_t vterm_color_to_u32(const VTermScreen* screen, const VTermColor* color, uint32_t default_fg, uint32_t default_bg) {
    VTermColor rgb_color = *color;
    
    if (VTERM_COLOR_IS_INDEXED(&rgb_color)) {
        vterm_screen_convert_color_to_rgb(screen, &rgb_color);
    }
    
    if (VTERM_COLOR_IS_DEFAULT_FG(color)) {
        return default_fg;
    }
    if (VTERM_COLOR_IS_DEFAULT_BG(color)) {
        return default_bg;
    }
    
    return 0xFF000000 |
           (static_cast<uint32_t>(rgb_color.rgb.blue) << 16) |
           (static_cast<uint32_t>(rgb_color.rgb.green) << 8) |
           static_cast<uint32_t>(rgb_color.rgb.red);
}

void convert_vterm_cell(const VTermScreen* screen, const VTermScreenCell* src, TerminalCell* dst, uint32_t default_fg, uint32_t default_bg) {
    for (int i = 0; i < TERMINAL_MAX_CHARS_PER_CELL && i < VTERM_MAX_CHARS_PER_CELL; ++i) {
        dst->chars[i] = src->chars[i];
    }
    
    dst->width = static_cast<uint8_t>(src->width);
    dst->fg = vterm_color_to_u32(screen, &src->fg, default_fg, default_bg);
    dst->bg = vterm_color_to_u32(screen, &src->bg, default_fg, default_bg);
    
    dst->bold = src->attrs.bold;
    dst->italic = src->attrs.italic;
    dst->underline = src->attrs.underline != VTERM_UNDERLINE_OFF;
    dst->strike = src->attrs.strike;
    dst->reverse = src->attrs.reverse;
}

class VTerminalImpl {
public:
    VTerm* vt = nullptr;
    VTermScreen* screen = nullptr;
    VTerminal* owner;
    
    VTerminalImpl(VTerminal* o, int rows, int cols) : owner(o) {
        vt = vterm_new(rows, cols);
        vterm_set_utf8(vt, 1);
        
        screen = vterm_obtain_screen(vt);
        
        static const VTermScreenCallbacks screen_cbs = {
            .damage = on_damage,
            .moverect = nullptr,
            .movecursor = on_movecursor,
            .settermprop = nullptr,
            .bell = nullptr,
            .resize = nullptr,
            .sb_pushline = on_sb_pushline,
            .sb_popline = on_sb_popline,
            .sb_clear = nullptr,
        };
        
        vterm_screen_set_callbacks(screen, &screen_cbs, this);
        vterm_screen_set_damage_merge(screen, VTERM_DAMAGE_SCROLL);
        vterm_screen_enable_altscreen(screen, 1);
        vterm_screen_reset(screen, 1);
    }
    
    ~VTerminalImpl() {
        if (vt) {
            vterm_free(vt);
        }
    }
    
    static int on_damage(VTermRect rect, void* user) {
        (void)rect;
        (void)user;
        return 1;
    }
    
    static int on_movecursor(VTermPos pos, VTermPos oldpos, int visible, void* user) {
        (void)oldpos;
        auto* impl = static_cast<VTerminalImpl*>(user);
        impl->owner->cursor_.row = pos.row;
        impl->owner->cursor_.col = pos.col;
        impl->owner->cursor_.visible = visible != 0;
        return 1;
    }
    
    static int on_sb_pushline(int cols, const VTermScreenCell* cells, void* user) {
        auto* impl = static_cast<VTerminalImpl*>(user);
        auto* owner = impl->owner;
        
        std::vector<TerminalCell> line;
        line.reserve(static_cast<size_t>(cols));
        
        for (int i = 0; i < cols; ++i) {
            TerminalCell tc{};
            convert_vterm_cell(impl->screen, &cells[i], &tc, owner->default_fg_, owner->default_bg_);
            line.push_back(tc);
        }
        
        owner->scrollback_.push_back(std::move(line));
        
        if (owner->scrollback_.size() > VTerminal::MAX_SCROLLBACK) {
            owner->scrollback_.erase(owner->scrollback_.begin());
        }
        
        if (owner->scrollback_cb_) {
            owner->scrollback_cb_(owner->scrollback_.back());
        }
        
        return 1;
    }
    
    static int on_sb_popline(int cols, VTermScreenCell* cells, void* user) {
        auto* impl = static_cast<VTerminalImpl*>(user);
        auto* owner = impl->owner;
        
        if (owner->scrollback_.empty()) {
            return 0;
        }
        
        const auto& line = owner->scrollback_.back();
        int copy_cols = std::min(cols, static_cast<int>(line.size()));
        
        for (int i = 0; i < copy_cols; ++i) {
            const auto& tc = line[i];
            std::memset(&cells[i], 0, sizeof(VTermScreenCell));
            
            for (int j = 0; j < VTERM_MAX_CHARS_PER_CELL && tc.chars[j]; ++j) {
                cells[i].chars[j] = tc.chars[j];
            }
            cells[i].width = tc.width ? tc.width : 1;
            
            vterm_color_rgb(&cells[i].fg,
                (tc.fg >> 0) & 0xFF,
                (tc.fg >> 8) & 0xFF,
                (tc.fg >> 16) & 0xFF);
            vterm_color_rgb(&cells[i].bg,
                (tc.bg >> 0) & 0xFF,
                (tc.bg >> 8) & 0xFF,
                (tc.bg >> 16) & 0xFF);
            
            cells[i].attrs.bold = tc.bold;
            cells[i].attrs.italic = tc.italic;
            cells[i].attrs.underline = tc.underline ? VTERM_UNDERLINE_SINGLE : VTERM_UNDERLINE_OFF;
            cells[i].attrs.strike = tc.strike;
            cells[i].attrs.reverse = tc.reverse;
        }
        
        for (int i = copy_cols; i < cols; ++i) {
            std::memset(&cells[i], 0, sizeof(VTermScreenCell));
            cells[i].chars[0] = ' ';
            cells[i].width = 1;
        }
        
        owner->scrollback_.pop_back();
        return 1;
    }
};

VTerminal::VTerminal(int rows, int cols)
    : rows_(rows)
    , cols_(cols)
{
    impl_ = std::make_unique<VTerminalImpl>(this, rows, cols);
    
    VTermColor fg, bg;
    vterm_color_rgb(&fg, 
        (default_fg_ >> 0) & 0xFF,
        (default_fg_ >> 8) & 0xFF, 
        (default_fg_ >> 16) & 0xFF);
    vterm_color_rgb(&bg,
        (default_bg_ >> 0) & 0xFF,
        (default_bg_ >> 8) & 0xFF,
        (default_bg_ >> 16) & 0xFF);
    vterm_screen_set_default_colors(impl_->screen, &fg, &bg);
}

VTerminal::~VTerminal() = default;

void VTerminal::resize(int rows, int cols) {
    if (rows == rows_ && cols == cols_) return;
    
    rows_ = rows;
    cols_ = cols;
    vterm_set_size(impl_->vt, rows, cols);
}

void VTerminal::write(const char* data, size_t len) {
    vterm_input_write(impl_->vt, data, len);
    vterm_screen_flush_damage(impl_->screen);
}

TerminalCell VTerminal::get_cell(int row, int col) const {
    TerminalCell result{};
    
    VTermPos pos = { .row = row, .col = col };
    VTermScreenCell cell;
    
    if (vterm_screen_get_cell(impl_->screen, pos, &cell)) {
        convert_vterm_cell(impl_->screen, &cell, &result, default_fg_, default_bg_);
    }
    
    return result;
}

CursorInfo VTerminal::get_cursor() const {
    return cursor_;
}

std::string VTerminal::get_output() {
    std::string result;
    size_t len = vterm_output_get_buffer_current(impl_->vt);
    if (len > 0) {
        result.resize(len);
        vterm_output_read(impl_->vt, result.data(), len);
    }
    return result;
}

void VTerminal::set_default_colors(uint32_t fg, uint32_t bg) {
    default_fg_ = fg;
    default_bg_ = bg;
    
    VTermColor vfg, vbg;
    vterm_color_rgb(&vfg,
        (fg >> 0) & 0xFF,
        (fg >> 8) & 0xFF,
        (fg >> 16) & 0xFF);
    vterm_color_rgb(&vbg,
        (bg >> 0) & 0xFF,
        (bg >> 8) & 0xFF,
        (bg >> 16) & 0xFF);
    vterm_screen_set_default_colors(impl_->screen, &vfg, &vbg);
}

void VTerminal::set_scrollback_callback(ScrollbackCallback cb) {
    scrollback_cb_ = std::move(cb);
}

void VTerminal::keyboard_key(int key) {
    vterm_keyboard_key(impl_->vt, static_cast<VTermKey>(key), VTERM_MOD_NONE);
}

void VTerminal::keyboard_unichar(uint32_t c, int modifiers) {
    vterm_keyboard_unichar(impl_->vt, c, static_cast<VTermModifier>(modifiers));
}

}
