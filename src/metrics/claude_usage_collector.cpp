#include "metrics/claude_usage_collector.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdlib>

namespace diana {

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
    if (claude_dir_.empty()) {
        return;
    }
    
    if (!store_ && !hub_) {
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_scan_).count() >= 5) {
        scan_directories();
        last_scan_ = now;
    }
    
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_poll_).count() >= 500) {
        for (auto& file : files_) {
            process_file(file);
        }
        last_poll_ = now;
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
                state.last_pos = 0;
                
                files_.push_back(std::move(state));
                watched_paths_.push_back(path);
                ++files_processed_;
            }
        }
    }
}

void ClaudeUsageCollector::scan_directories() {
    namespace fs = std::filesystem;
    
    fs::path projects_dir = fs::path(claude_dir_) / "projects";
    scan_directory_recursive(projects_dir);
    
    fs::path transcripts_dir = fs::path(claude_dir_) / "transcripts";
    scan_directory_recursive(transcripts_dir);
}

void ClaudeUsageCollector::process_file(FileState& state) {
    namespace fs = std::filesystem;
    
    if (!fs::exists(state.path)) return;
    
    auto file_size = static_cast<std::streamoff>(fs::file_size(state.path));
    
    if (file_size <= state.last_pos) {
        return;
    }
    
    std::ifstream file(state.path);
    if (!file) return;
    
    if (state.last_pos > 0) {
        file.seekg(state.last_pos);
    }
    
    std::string source_key = extract_project_key(state.path.string());
    
    int new_entries = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        TokenSample sample;
        if (parse_jsonl_line(line, sample)) {
            if (hub_) {
                hub_->record(source_key, sample);
            }
            if (store_) {
                store_->record_sample(sample);
            }
            ++entries_parsed_;
            ++new_entries;
        }
    }
    
    state.last_pos = file_size;
}

std::string ClaudeUsageCollector::normalize_project_key(const std::string& key) const {
    if (!key.empty() && key[0] == '-') {
        return key.substr(1);
    }
    return key;
}

std::string ClaudeUsageCollector::extract_project_key(const std::string& file_path) const {
    std::string path = file_path;
    
    std::string projects_marker = "/.claude/projects/";
    auto pos = path.find(projects_marker);
    if (pos != std::string::npos) {
        std::string after = path.substr(pos + projects_marker.length());
        auto slash_pos = after.find('/');
        std::string key;
        if (slash_pos != std::string::npos) {
            key = after.substr(0, slash_pos);
        } else {
            auto dot_pos = after.rfind('.');
            if (dot_pos != std::string::npos) {
                key = after.substr(0, dot_pos);
            } else {
                key = after;
            }
        }
        return normalize_project_key(key);
    }
    
    std::string transcripts_marker = "/.claude/transcripts/";
    pos = path.find(transcripts_marker);
    if (pos != std::string::npos) {
        return "__transcripts__";
    }
    
    return file_path;
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
