#pragma once

#include "metrics/metrics_store.h"
#include "metrics/multi_metrics_store.h"
#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace diana {

class OpencodeUsageCollector {
public:
    OpencodeUsageCollector();
    explicit OpencodeUsageCollector(const std::string& data_dir);

    void set_store(MetricsStore* store) { store_ = store; }
    void set_multi_store(MultiMetricsStore* hub) { hub_ = hub; }

    void poll();

    size_t files_processed() const { return files_processed_; }
    size_t entries_parsed() const { return entries_parsed_; }

private:
    struct MessageTotals {
        uint64_t input_tokens = 0;
        uint64_t output_tokens = 0;
        uint64_t cache_read = 0;
        uint64_t cache_write = 0;
        uint64_t reasoning = 0;
        double cost_usd = 0.0;
    };

    struct SessionInfo {
        std::string project_id;
        std::string directory;
    };

    std::string data_dir_;
    std::string storage_dir_;

    std::chrono::steady_clock::time_point last_scan_{};

    std::vector<std::filesystem::path> message_paths_;
    std::unordered_map<std::string, MessageTotals> last_totals_;
    std::unordered_map<std::string, SessionInfo> session_by_id_;
    std::unordered_map<std::string, std::string> project_worktree_;

    MetricsStore* store_ = nullptr;
    MultiMetricsStore* hub_ = nullptr;

    size_t files_processed_ = 0;
    size_t entries_parsed_ = 0;

    void scan_directories();
    void scan_projects();
    void scan_sessions();
    void scan_messages();
    void process_file(const std::filesystem::path& path);

    std::string make_project_key(const std::string& session_id) const;
    static std::string sanitize_key(const std::string& key);
    static std::string default_data_dir();
};

}
