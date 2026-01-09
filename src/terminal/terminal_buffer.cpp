#include "terminal_buffer.h"
#include <chrono>
#include <algorithm>

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
    flush_pending(color);

    TerminalLine line;
    line.text = text;
    line.color = color;
    line.timestamp = current_timestamp_ms();
    segments_[active_segment_idx_].lines.push_back(std::move(line));

    trim_if_needed();
}

void TerminalBuffer::append_text(const std::string& text, uint32_t color) {
    for (char c : text) {
        if (c == '\n') {
            TerminalLine line;
            line.text = std::move(pending_text_);
            line.color = color;
            line.timestamp = current_timestamp_ms();
            segments_[active_segment_idx_].lines.push_back(std::move(line));
            pending_text_.clear();
        } else if (c != '\r') {
            pending_text_ += c;
        }
    }

    trim_if_needed();
}

void TerminalBuffer::new_segment(SegmentKind kind) {
    flush_pending(0xFFFFFFFF);

    Segment seg;
    seg.kind = kind;
    segments_.push_back(std::move(seg));
    active_segment_idx_ = segments_.size() - 1;

    while (segments_.size() > max_segments_ && active_segment_idx_ > 0) {
        segments_.erase(segments_.begin());
        active_segment_idx_--;
    }
}

void TerminalBuffer::add_restart_marker(const std::string& message) {
    new_segment(SegmentKind::RestartMarker);
    
    TerminalLine marker_line;
    marker_line.text = "─── " + message + " ───";
    marker_line.color = 0xFF88FFFF;
    marker_line.timestamp = current_timestamp_ms();
    segments_[active_segment_idx_].lines.push_back(std::move(marker_line));
    
    new_segment(SegmentKind::Normal);
}

void TerminalBuffer::clear_current_segment() {
    segments_[active_segment_idx_].lines.clear();
    pending_text_.clear();
}

size_t TerminalBuffer::total_line_count() const {
    size_t total = 0;
    for (const auto& seg : segments_) {
        total += seg.lines.size();
    }
    return total;
}

void TerminalBuffer::flush_pending(uint32_t color) {
    if (pending_text_.empty()) return;
    
    TerminalLine line;
    line.text = std::move(pending_text_);
    line.color = color;
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
