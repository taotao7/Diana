#include "terminal_buffer.h"
#include <chrono>

namespace agent47 {

namespace {

int64_t current_timestamp_ms() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

}

TerminalBuffer::TerminalBuffer(size_t max_lines_per_segment, size_t max_segments)
    : max_lines_per_segment_(max_lines_per_segment)
    , max_segments_(max_segments)
{
    segments_.emplace_back();
    active_segment_idx_ = 0;
}

void TerminalBuffer::append_line(const std::string& text, uint32_t color) {
    flush_pending();

    TerminalLine line;
    line.spans = ansi_parser_.parse_line(text);
    if (line.spans.size() == 1 && line.spans[0].color == 0xFFDCDCDC) {
        line.spans[0].color = color;
    }
    line.timestamp = current_timestamp_ms();
    segments_[active_segment_idx_].lines.push_back(std::move(line));

    trim_if_needed();
}

void TerminalBuffer::append_text(const std::string& text, uint32_t color) {
    (void)color;
    
    size_t i = 0;
    while (i < text.size()) {
        char c = text[i];
        if (c == '\n') {
            TerminalLine line;
            line.spans = ansi_parser_.parse_line(pending_text_);
            line.timestamp = current_timestamp_ms();
            segments_[active_segment_idx_].lines.push_back(std::move(line));
            pending_text_.clear();
            ++i;
        } else if (c == '\r') {
            ++i;
        } else {
            pending_text_ += c;
            ++i;
        }
    }

    trim_if_needed();
}

void TerminalBuffer::new_segment(SegmentKind kind) {
    flush_pending();

    Segment seg;
    seg.kind = kind;
    segments_.push_back(std::move(seg));
    active_segment_idx_ = segments_.size() - 1;

    while (segments_.size() > max_segments_ && active_segment_idx_ > 0) {
        segments_.erase(segments_.begin());
        active_segment_idx_--;
    }
    
    ansi_parser_.reset();
}

void TerminalBuffer::add_restart_marker(const std::string& message) {
    new_segment(SegmentKind::RestartMarker);
    
    TerminalLine marker_line;
    marker_line.spans.push_back({"\u2500\u2500\u2500 " + message + " \u2500\u2500\u2500", 0xFF88FFFF});
    marker_line.timestamp = current_timestamp_ms();
    segments_[active_segment_idx_].lines.push_back(std::move(marker_line));
    
    new_segment(SegmentKind::Normal);
}

void TerminalBuffer::clear_current_segment() {
    segments_[active_segment_idx_].lines.clear();
    pending_text_.clear();
    ansi_parser_.reset();
}

size_t TerminalBuffer::total_line_count() const {
    size_t total = 0;
    for (const auto& seg : segments_) {
        total += seg.lines.size();
    }
    return total;
}

void TerminalBuffer::flush_pending() {
    if (pending_text_.empty()) return;
    
    TerminalLine line;
    line.spans = ansi_parser_.parse_line(pending_text_);
    line.timestamp = current_timestamp_ms();
    segments_[active_segment_idx_].lines.push_back(std::move(line));
    pending_text_.clear();
}

void TerminalBuffer::trim_if_needed() {
    auto& lines = segments_[active_segment_idx_].lines;
    while (lines.size() > max_lines_per_segment_) {
        lines.pop_front();
    }
}

}
