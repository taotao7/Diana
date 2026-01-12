#pragma once

#include "metrics_store.h"
#include "multi_metrics_store.h"
#include <string>
#include <chrono>
#include <vector>
#include <filesystem>

namespace diana {

class ClaudeUsageCollector {
public:
    ClaudeUsageCollector();
    explicit ClaudeUsageCollector(const std::string& claude_dir);
    
    void set_metrics_store(MetricsStore* store) { store_ = store; }
    void set_multi_store(MultiMetricsStore* hub) { hub_ = hub; }
    
    void poll();
    
    size_t files_processed() const { return files_processed_; }
    size_t entries_parsed() const { return entries_parsed_; }
    
    const std::vector<std::filesystem::path>& watched_files() const { return watched_paths_; }

private:
    struct FileState {
        std::filesystem::path path;
        std::streamoff last_pos = 0;
    };
    
    void scan_directories();
    void scan_directory_recursive(const std::filesystem::path& dir);
    void process_file(FileState& state);
    bool parse_jsonl_line(const std::string& line, TokenSample& sample);
    std::string extract_project_key(const std::string& file_path) const;
    std::string normalize_project_key(const std::string& key) const;
    
    std::string claude_dir_;
    std::vector<FileState> files_;
    std::vector<std::filesystem::path> watched_paths_;
    MetricsStore* store_ = nullptr;
    MultiMetricsStore* hub_ = nullptr;
    
    std::chrono::steady_clock::time_point last_scan_{};
    std::chrono::steady_clock::time_point last_poll_{};
    size_t files_processed_ = 0;
    size_t entries_parsed_ = 0;
};

}
