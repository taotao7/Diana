#pragma once

#include <string>
#include <vector>
#include <deque>
#include <cstdint>

namespace agent47 {

enum class SegmentKind {
    Normal,
    RestartMarker,
    System
};

struct TerminalLine {
    std::string text;
    uint32_t color = 0xFFFFFFFF;
    int64_t timestamp = 0;
};

struct Segment {
    SegmentKind kind = SegmentKind::Normal;
    std::deque<TerminalLine> lines;
};

class TerminalBuffer {
public:
    explicit TerminalBuffer(size_t max_lines_per_segment = 5000, size_t max_segments = 50);

    void append_line(const std::string& text, uint32_t color = 0xFFFFFFFF);
    void append_text(const std::string& text, uint32_t color = 0xFFFFFFFF);

    void new_segment(SegmentKind kind = SegmentKind::Normal);
    void add_restart_marker(const std::string& message);
    void clear_current_segment();

    const std::vector<Segment>& segments() const { return segments_; }
    size_t active_segment_index() const { return active_segment_idx_; }
    const Segment& active_segment() const { return segments_[active_segment_idx_]; }

    size_t total_line_count() const;

private:
    void trim_if_needed();
    void flush_pending(uint32_t color);

    std::vector<Segment> segments_;
    size_t active_segment_idx_ = 0;
    size_t max_lines_per_segment_;
    size_t max_segments_;
    std::string pending_text_;
};

}
