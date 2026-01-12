#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <set>

namespace diana {

// Supported agent/app types
enum class AgentType {
    ClaudeCode,
    Codex,
    OpenCode,
    Unknown
};

// Token usage for a single agent session
struct AgentTokenUsage {
    uint64_t input_tokens = 0;
    uint64_t output_tokens = 0;
    uint64_t cache_creation_tokens = 0;
    uint64_t cache_read_tokens = 0;
    double cost_usd = 0.0;
    
    uint64_t total() const { 
        return input_tokens + output_tokens + cache_creation_tokens + cache_read_tokens; 
    }
};

// Session info for an agent
struct AgentSession {
    std::string session_id;
    std::string project_path;
    std::chrono::system_clock::time_point first_seen;
    std::chrono::system_clock::time_point last_seen;
    AgentTokenUsage tokens;
    size_t message_count = 0;
    bool is_subagent = false;
    std::string parent_session_id;  // For subagents
};

// Aggregate stats for an agent type
struct AgentTypeStats {
    AgentType type = AgentType::Unknown;
    AgentTokenUsage total_tokens;
    size_t session_count = 0;
    size_t active_sessions = 0;
    std::chrono::system_clock::time_point last_activity;
};

struct DailyTokenData {
    int year = 0;
    int month = 0;
    int day = 0;
    int weekday = 0;
    uint64_t tokens = 0;
    double cost = 0.0;
    size_t session_count = 0;
    
    std::string date_key() const;
};

// Store for aggregating token usage per agent type
class AgentTokenStore {
public:
    AgentTokenStore();
    explicit AgentTokenStore(const std::string& claude_dir);
    
    // Scan and collect data
    void poll();
    void scan_all();
    
    // Get stats
    AgentTypeStats get_stats(AgentType type) const;
    std::vector<AgentTypeStats> get_all_stats() const;
    AgentTokenUsage get_total_usage() const;
    
    // Get session list for a specific agent type
    std::vector<AgentSession> get_sessions(AgentType type) const;
    
    // Get daily token data for heatmap (last 365 days)
    std::vector<DailyTokenData> get_daily_data(AgentType type) const;
    
    // Clear all data
    void clear();
    
    // Get file counts
    size_t files_processed() const { return files_processed_; }
    size_t sessions_tracked() const { return sessions_.size(); }

private:
    struct FileState {
        std::filesystem::path path;
        std::streampos last_pos = 0;
        std::filesystem::file_time_type last_mtime{};
        AgentType agent_type = AgentType::Unknown;
        std::string session_id;
        bool is_subagent = false;
    };
    
    void scan_claude_directory();
    void scan_opencode_directory();
    void scan_directory_recursive(const std::filesystem::path& dir, AgentType type);
    void process_file(FileState& state);
    void process_opencode_message_file(const std::filesystem::path& path, const std::string& session_id);
    bool parse_jsonl_line(const std::string& line, AgentTokenUsage& usage, 
                          std::string& session_id, bool& is_subagent);
    bool parse_opencode_message(const nlohmann::json& j, AgentTokenUsage& usage);
    
    AgentType detect_agent_type(const std::filesystem::path& path);
    std::string extract_session_id(const std::filesystem::path& path);
    
    std::string claude_dir_;
    std::string codex_dir_;
    std::string opencode_dir_;
    
    mutable std::mutex mutex_;
    std::vector<FileState> files_;
    std::unordered_map<std::string, AgentSession> sessions_;
    std::set<std::string> opencode_session_ids_;
    
    std::chrono::steady_clock::time_point last_scan_{};
    std::chrono::steady_clock::time_point last_poll_{};
    size_t files_processed_ = 0;
};

// Helper function to get agent type name
inline const char* agent_type_name(AgentType type) {
    switch (type) {
        case AgentType::ClaudeCode: return "Claude Code";
        case AgentType::Codex: return "Codex";
        case AgentType::OpenCode: return "OpenCode";
        default: return "Unknown";
    }
}

}
