#include "metrics/agent_token_store.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdlib>

namespace agent47 {

AgentTokenStore::AgentTokenStore() {
    const char* home = std::getenv("HOME");
    if (home) {
        claude_dir_ = std::string(home) + "/.claude";
        codex_dir_ = std::string(home) + "/.codex";
        opencode_dir_ = std::string(home) + "/.config/opencode";
    }
}

AgentTokenStore::AgentTokenStore(const std::string& claude_dir)
    : claude_dir_(claude_dir)
{
}

void AgentTokenStore::poll() {
    if (claude_dir_.empty()) {
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto since_last_scan = now - last_scan_;
    
    if (std::chrono::duration_cast<std::chrono::seconds>(since_last_scan).count() >= 5) {
        scan_all();
        last_scan_ = now;
    }
    
    for (auto& file : files_) {
        process_file(file);
    }
}

void AgentTokenStore::scan_all() {
    scan_claude_directory();
}

void AgentTokenStore::scan_claude_directory() {
    namespace fs = std::filesystem;
    
    if (claude_dir_.empty() || !fs::exists(claude_dir_)) {
        return;
    }
    
    fs::path projects_dir = fs::path(claude_dir_) / "projects";
    if (fs::exists(projects_dir)) {
        scan_directory_recursive(projects_dir, AgentType::ClaudeCode);
    }
    
    fs::path transcripts_dir = fs::path(claude_dir_) / "transcripts";
    if (fs::exists(transcripts_dir)) {
        scan_directory_recursive(transcripts_dir, AgentType::ClaudeCode);
    }
}

void AgentTokenStore::scan_directory_recursive(const std::filesystem::path& dir, AgentType type) {
    namespace fs = std::filesystem;
    
    if (!fs::exists(dir) || !fs::is_directory(dir)) {
        return;
    }
    
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_directory()) {
            scan_directory_recursive(entry.path(), type);
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
                state.agent_type = detect_agent_type(path);
                state.session_id = extract_session_id(path);
                state.is_subagent = path.string().find("/subagents/") != std::string::npos;
                state.last_pos = 0;
                state.last_mtime = fs::file_time_type::min();
                
                files_.push_back(std::move(state));
                ++files_processed_;
            }
        }
    }
}

AgentType AgentTokenStore::detect_agent_type(const std::filesystem::path& path) {
    std::string path_str = path.string();
    
    if (path_str.find(".claude") != std::string::npos) {
        return AgentType::ClaudeCode;
    }
    if (path_str.find(".codex") != std::string::npos) {
        return AgentType::Codex;
    }
    if (path_str.find("opencode") != std::string::npos) {
        return AgentType::OpenCode;
    }
    
    return AgentType::Unknown;
}

std::string AgentTokenStore::extract_session_id(const std::filesystem::path& path) {
    std::string filename = path.stem().string();
    
    if (filename.find("ses_") == 0 || filename.find("agent-") == 0) {
        return filename;
    }
    
    if (filename.length() == 36 && filename[8] == '-' && filename[13] == '-') {
        return filename;
    }
    
    return path.filename().string();
}

void AgentTokenStore::process_file(FileState& state) {
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
        
        AgentTokenUsage usage;
        std::string session_id;
        bool is_subagent = false;
        
        if (parse_jsonl_line(line, usage, session_id, is_subagent)) {
            std::lock_guard<std::mutex> lock(mutex_);
            
            if (session_id.empty()) {
                session_id = state.session_id;
            }
            
            auto& session = sessions_[session_id];
            if (session.session_id.empty()) {
                session.session_id = session_id;
                session.first_seen = std::chrono::system_clock::now();
                session.is_subagent = state.is_subagent || is_subagent;
            }
            
            session.tokens.input_tokens += usage.input_tokens;
            session.tokens.output_tokens += usage.output_tokens;
            session.tokens.cache_creation_tokens += usage.cache_creation_tokens;
            session.tokens.cache_read_tokens += usage.cache_read_tokens;
            session.tokens.cost_usd += usage.cost_usd;
            session.last_seen = std::chrono::system_clock::now();
            session.message_count++;
        }
    }
    
    state.last_pos = file.tellg();
}

bool AgentTokenStore::parse_jsonl_line(const std::string& line, AgentTokenUsage& usage,
                                        std::string& session_id, bool& is_subagent) {
    try {
        auto json = nlohmann::json::parse(line);
        
        if (json.contains("sessionId")) {
            session_id = json["sessionId"].get<std::string>();
        }
        
        if (json.contains("agentId")) {
            is_subagent = true;
        }
        
        const nlohmann::json* usage_ptr = nullptr;
        
        if (json.contains("message") && json["message"].contains("usage")) {
            usage_ptr = &json["message"]["usage"];
        } else if (json.contains("usage")) {
            usage_ptr = &json["usage"];
        }
        
        if (usage_ptr) {
            const auto& u = *usage_ptr;
            
            if (u.contains("input_tokens")) {
                usage.input_tokens = u["input_tokens"].get<uint64_t>();
            }
            if (u.contains("output_tokens")) {
                usage.output_tokens = u["output_tokens"].get<uint64_t>();
            }
            if (u.contains("cache_creation_input_tokens")) {
                usage.cache_creation_tokens = u["cache_creation_input_tokens"].get<uint64_t>();
            }
            if (u.contains("cache_read_input_tokens")) {
                usage.cache_read_tokens = u["cache_read_input_tokens"].get<uint64_t>();
            }
            
            if (json.contains("costUSD")) {
                usage.cost_usd = json["costUSD"].get<double>();
            }
            
            return usage.total() > 0;
        }
        
    } catch (...) {
    }
    
    return false;
}

AgentTypeStats AgentTokenStore::get_stats(AgentType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    AgentTypeStats stats;
    stats.type = type;
    
    auto now = std::chrono::system_clock::now();
    auto active_threshold = now - std::chrono::minutes(5);
    
    for (const auto& [id, session] : sessions_) {
        bool match = false;
        for (const auto& file : files_) {
            if (file.session_id == id && file.agent_type == type) {
                match = true;
                break;
            }
        }
        
        if (!match && type == AgentType::ClaudeCode) {
            match = true;
        }
        
        if (match) {
            stats.total_tokens.input_tokens += session.tokens.input_tokens;
            stats.total_tokens.output_tokens += session.tokens.output_tokens;
            stats.total_tokens.cache_creation_tokens += session.tokens.cache_creation_tokens;
            stats.total_tokens.cache_read_tokens += session.tokens.cache_read_tokens;
            stats.total_tokens.cost_usd += session.tokens.cost_usd;
            stats.session_count++;
            
            if (session.last_seen > active_threshold) {
                stats.active_sessions++;
            }
            
            if (session.last_seen > stats.last_activity) {
                stats.last_activity = session.last_seen;
            }
        }
    }
    
    return stats;
}

std::vector<AgentTypeStats> AgentTokenStore::get_all_stats() const {
    std::vector<AgentTypeStats> all_stats;
    all_stats.push_back(get_stats(AgentType::ClaudeCode));
    all_stats.push_back(get_stats(AgentType::Codex));
    all_stats.push_back(get_stats(AgentType::OpenCode));
    return all_stats;
}

AgentTokenUsage AgentTokenStore::get_total_usage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    AgentTokenUsage total;
    for (const auto& [id, session] : sessions_) {
        total.input_tokens += session.tokens.input_tokens;
        total.output_tokens += session.tokens.output_tokens;
        total.cache_creation_tokens += session.tokens.cache_creation_tokens;
        total.cache_read_tokens += session.tokens.cache_read_tokens;
        total.cost_usd += session.tokens.cost_usd;
    }
    return total;
}

std::vector<AgentSession> AgentTokenStore::get_sessions(AgentType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<AgentSession> result;
    
    for (const auto& [id, session] : sessions_) {
        bool match = false;
        for (const auto& file : files_) {
            if (file.session_id == id && file.agent_type == type) {
                match = true;
                break;
            }
        }
        
        if (!match && type == AgentType::ClaudeCode) {
            match = true;
        }
        
        if (match) {
            result.push_back(session);
        }
    }
    
    std::sort(result.begin(), result.end(), [](const AgentSession& a, const AgentSession& b) {
        return a.last_seen > b.last_seen;
    });
    
    return result;
}

void AgentTokenStore::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.clear();
    files_.clear();
    files_processed_ = 0;
}

}
