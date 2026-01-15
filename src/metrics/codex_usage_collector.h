
#include "metrics_store.h"
#include "multi_metrics_store.h"
#include <atomic>
#include <filesystem>
#include <future>
#include <mutex>
#include <string>
#include <vector>

namespace diana {

class CodexUsageCollector {
public:
    CodexUsageCollector();
    explicit CodexUsageCollector(const std::string& codex_dir);
    ~CodexUsageCollector();

    void set_metrics_store(MetricsStore* store) { store_ = store; }
    void set_multi_store(MultiMetricsStore* hub) { hub_ = hub; }

    void poll();

    size_t files_processed() const { return files_processed_; }
    size_t entries_parsed() const { return entries_parsed_; }

    const std::vector<std::filesystem::path>& watched_files() const { return watched_paths_; }

    void wait_for_init() {
        if (init_future_.valid()) {
            init_future_.wait();
        }
    }

private:
    struct FileState {
        std::filesystem::path path;
        std::streamoff last_pos = 0;
        std::string cwd;
        std::string project_key;
        uint64_t last_input_tokens = 0;
        uint64_t last_output_tokens = 0;
    };

    void do_initial_scan();
    void scan_directories();
    void scan_directory_recursive(const std::filesystem::path& dir);
    void process_file(FileState& state);
    bool parse_json_line(const std::string& line, FileState& state, TokenSample& sample, bool& has_sample);
    std::string normalize_project_key(const std::string& cwd) const;

    std::string codex_dir_;
    std::vector<FileState> files_;
    std::vector<std::filesystem::path> watched_paths_;
    MetricsStore* store_ = nullptr;
    MultiMetricsStore* hub_ = nullptr;

    std::chrono::steady_clock::time_point last_scan_{};
    std::chrono::steady_clock::time_point last_poll_{};
    size_t files_processed_ = 0;
    size_t entries_parsed_ = 0;

    std::future<void> init_future_;
    std::atomic<bool> init_done_{false};
    std::mutex files_mutex_;
};

}
