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

void ClaudeUsageCollector::scan_directory() {
    namespace fs = std::filesystem;
    
    if (!fs::exists(claude_dir_) || !fs::is_directory(claude_dir_)) {
        return;
    }
    
    for (const auto& entry : fs::directory_iterator(claude_dir_)) {
        if (!entry.is_regular_file()) continue;
        
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
            state.last_pos = 0;
            state.last_mtime = fs::file_time_type::min();
            files_.push_back(std::move(state));
            ++files_processed_;
        }
    }
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
        
        if (json.contains("usage")) {
            const auto& usage = json["usage"];
            
            if (usage.contains("input_tokens")) {
                sample.input_tokens = usage["input_tokens"].get<uint64_t>();
            }
            if (usage.contains("output_tokens")) {
                sample.output_tokens = usage["output_tokens"].get<uint64_t>();
            }
            sample.total_tokens = sample.input_tokens + sample.output_tokens;
            
            if (json.contains("costUSD")) {
                sample.cost_usd = json["costUSD"].get<double>();
            }
            
            sample.timestamp = std::chrono::steady_clock::now();
            return true;
        }
        
        if (json.contains("inputTokens") || json.contains("input_tokens")) {
            if (json.contains("inputTokens")) {
                sample.input_tokens = json["inputTokens"].get<uint64_t>();
            } else {
                sample.input_tokens = json["input_tokens"].get<uint64_t>();
            }
            
            if (json.contains("outputTokens")) {
                sample.output_tokens = json["outputTokens"].get<uint64_t>();
            } else if (json.contains("output_tokens")) {
                sample.output_tokens = json["output_tokens"].get<uint64_t>();
            }
            
            sample.total_tokens = sample.input_tokens + sample.output_tokens;
            
            if (json.contains("costUSD")) {
                sample.cost_usd = json["costUSD"].get<double>();
            }
            
            sample.timestamp = std::chrono::steady_clock::now();
            return true;
        }
        
    } catch (...) {
    }
    
    return false;
}

}
