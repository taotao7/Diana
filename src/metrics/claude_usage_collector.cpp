#include "metrics/claude_usage_collector.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdlib>

namespace agent47 {

ClaudeUsageCollector::ClaudeUsageCollector() {
    const char* home = std::getenv("HOME");
    if (home) {
        claude_dir_ = std::string(home) + "/.claude";
    }
}

ClaudeUsageCollector::ClaudeUsageCollector(const std::string& claude_dir)
    : claude_dir_(claude_dir)
{
}

void ClaudeUsageCollector::poll() {
    if (claude_dir_.empty() || !store_) {
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto since_last_scan = now - last_scan_;
    
    if (std::chrono::duration_cast<std::chrono::seconds>(since_last_scan).count() >= 5) {
        scan_directory();
        last_scan_ = now;
    }
    
    for (auto& file : files_) {
        process_file(file);
    }
}

void ClaudeUsageCollector::scan_directory_recursive(const std::filesystem::path& dir) {
    namespace fs = std::filesystem;
    
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        return;
    }
    
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_directory()) {
            scan_directory_recursive(entry.path());
        } else if (entry.is_regular_file()) {
            const auto& path = entry.path();
            if (path.extension() != ".jsonl") continue;
            
            bool found = false;
            for (const auto& f : files_) {
                if (f.path == path) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                FileState state;
                state.path = path;
                
                std::ifstream file(path, std::ios::ate);
                if (file) {
                    state.last_pos = file.tellg();
                } else {
                    state.last_pos = 0;
                }
                state.last_mtime = fs::last_write_time(path);
                
                files_.push_back(std::move(state));
                ++files_processed_;
            }
        }
    }
}

void ClaudeUsageCollector::scan_directory() {
    namespace fs = std::filesystem;
    
    fs::path projects_dir = fs::path(claude_dir_) / "projects";
    scan_directory_recursive(projects_dir);
}

void ClaudeUsageCollector::process_file(FileState& state) {
    namespace fs = std::filesystem;
    
    if (!fs::exists(state.path)) return;
    
    auto mtime = fs::last_write_time(state.path);
    if (mtime == state.last_mtime) {
        return;
    }
    state.last_mtime = mtime;
    
    std::ifstream file(state.path);
    if (!file) return;
    
    file.seekg(state.last_pos);
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        TokenSample sample;
        if (parse_jsonl_line(line, sample)) {
            store_->record_sample(sample);
            ++entries_parsed_;
        }
    }
    
    state.last_pos = file.tellg();
}

bool ClaudeUsageCollector::parse_jsonl_line(const std::string& line, TokenSample& sample) {
    try {
        auto json = nlohmann::json::parse(line);
        
        const nlohmann::json* usage_ptr = nullptr;
        
        if (json.contains("message") && json["message"].contains("usage")) {
            usage_ptr = &json["message"]["usage"];
        } else if (json.contains("usage")) {
            usage_ptr = &json["usage"];
        }
        
        if (usage_ptr) {
            const auto& usage = *usage_ptr;
            
            if (usage.contains("input_tokens")) {
                sample.input_tokens = usage["input_tokens"].get<uint64_t>();
            }
            if (usage.contains("output_tokens")) {
                sample.output_tokens = usage["output_tokens"].get<uint64_t>();
            }
            if (usage.contains("cache_read_input_tokens")) {
                sample.input_tokens += usage["cache_read_input_tokens"].get<uint64_t>();
            }
            if (usage.contains("cache_creation_input_tokens")) {
                sample.input_tokens += usage["cache_creation_input_tokens"].get<uint64_t>();
            }
            
            sample.total_tokens = sample.input_tokens + sample.output_tokens;
            
            if (json.contains("costUSD")) {
                sample.cost_usd = json["costUSD"].get<double>();
            }
            
            sample.timestamp = std::chrono::steady_clock::now();
            return sample.total_tokens > 0;
        }
        
    } catch (...) {
    }
    
    return false;
}

}
