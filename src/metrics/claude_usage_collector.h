#pragma once

#include "metrics_store.h"
#include <string>
#include <chrono>
#include <vector>
#include <filesystem>

namespace agent47 {

class ClaudeUsageCollector {
public:
    ClaudeUsageCollector();
    explicit ClaudeUsageCollector(const std::string& claude_dir);
    
    void set_metrics_store(MetricsStore* store) { store_ = store; }
    
    void poll();
    
    size_t files_processed() const { return files_processed_; }
    size_t entries_parsed() const { return entries_parsed_; }

private:
    struct FileState {
        std::filesystem::path path;
        std::streampos last_pos = 0;
        std::filesystem::file_time_type last_mtime{};
    };
    
    void scan_directory();
    void process_file(FileState& state);
    bool parse_jsonl_line(const std::string& line, TokenSample& sample);
    
    std::string claude_dir_;
    std::vector<FileState> files_;
    MetricsStore* store_ = nullptr;
    
    std::chrono::steady_clock::time_point last_scan_{};
    size_t files_processed_ = 0;
    size_t entries_parsed_ = 0;
};

}
