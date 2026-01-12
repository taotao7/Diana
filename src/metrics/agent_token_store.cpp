#include "metrics/agent_token_store.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <future>
#include <iomanip>
#include <sstream>
#include <array>
#include <ctime>
#include <cctype>

namespace diana {

namespace {

int agent_index(AgentType type) {
    switch (type) {
        case AgentType::ClaudeCode: return 0;
        case AgentType::Codex: return 1;
        case AgentType::OpenCode: return 2;
        default: return -1;
    }
}

bool parse_iso8601_utc(const std::string& ts, std::chrono::system_clock::time_point& out) {
    if (ts.size() < 19) {
        return false;
    }
    std::tm tm{};
    std::istringstream ss(ts.substr(0, 19));
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (ss.fail()) {
        return false;
    }
#if defined(__APPLE__) || defined(__linux__)
    std::time_t t = timegm(&tm);
#else
    std::time_t t = std::mktime(&tm);
#endif
    if (t == static_cast<std::time_t>(-1)) {
        return false;
    }
    out = std::chrono::system_clock::from_time_t(t);
    auto dot = ts.find('.');
    if (dot != std::string::npos) {
        size_t end = dot + 1;
        while (end < ts.size() && std::isdigit(static_cast<unsigned char>(ts[end]))) {
            end++;
        }
        std::string frac = ts.substr(dot + 1, end - dot - 1);
        if (!frac.empty()) {
            while (frac.size() < 3) {
                frac.push_back('0');
            }
            int ms = std::stoi(frac.substr(0, 3));
            out += std::chrono::milliseconds(ms);
        }
    }
    return true;
}

void add_daily_usage(std::array<std::map<std::string, DailyTokenData>, 3>& totals,
                     std::array<std::unordered_map<std::string, std::set<std::string>>, 3>& session_ids,
                     AgentType type,
                     const std::string& session_id,
                     const AgentTokenUsage& usage,
                     const std::chrono::system_clock::time_point& usage_time) {
    int idx = agent_index(type);
    if (idx < 0) {
        return;
    }
    std::time_t tt = std::chrono::system_clock::to_time_t(usage_time);
    std::tm* tm = std::localtime(&tt);
    if (!tm) {
        return;
    }
    DailyTokenData data;
    data.year = tm->tm_year + 1900;
    data.month = tm->tm_mon + 1;
    data.day = tm->tm_mday;
    data.weekday = tm->tm_wday;
    std::string key = data.date_key();

    auto& day = totals[idx][key];
    if (day.year == 0) {
        day = data;
    }
    day.tokens += usage.total();
    day.cost += usage.cost_usd;
    auto& sessions_for_day = session_ids[idx][key];
    if (sessions_for_day.insert(session_id).second) {
        day.session_count++;
    }
}

}

std::string DailyTokenData::date_key() const {
    std::ostringstream oss;
    oss << year << "-" << std::setfill('0') << std::setw(2) << month 
        << "-" << std::setw(2) << day;
    return oss.str();
}

AgentTokenStore::AgentTokenStore() {
    const char* home = std::getenv("HOME");
    if (home) {
        claude_dir_ = std::string(home) + "/.claude";
        codex_dir_ = std::string(home) + "/.codex";
        
        const char* xdg_data = std::getenv("XDG_DATA_HOME");
        if (xdg_data) {
            opencode_dir_ = std::string(xdg_data) + "/opencode/storage";
        } else {
            opencode_dir_ = std::string(home) + "/.local/share/opencode/storage";
        }
    }
    init_future_ = std::async(std::launch::async, &AgentTokenStore::do_initial_scan, this);
}

AgentTokenStore::AgentTokenStore(const std::string& claude_dir)
    : claude_dir_(claude_dir)
{
    init_future_ = std::async(std::launch::async, &AgentTokenStore::do_initial_scan, this);
}

AgentTokenStore::~AgentTokenStore() {
    if (init_future_.valid()) {
        init_future_.wait();
    }
}

void AgentTokenStore::do_initial_scan() {
    std::lock_guard<std::mutex> lock(mutex_);
    scan_all();
    
    for (auto& file : files_) {
        process_file(file);
    }
    
    last_scan_ = std::chrono::steady_clock::now();
    last_poll_ = std::chrono::steady_clock::now();
    init_done_ = true;
}

void AgentTokenStore::poll() {
    if (claude_dir_.empty() && codex_dir_.empty() && opencode_dir_.empty()) {
        return;
    }
    
    if (!init_done_) {
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_scan_).count() >= 5) {
        std::lock_guard<std::mutex> lock(mutex_);
        scan_all();
        last_scan_ = now;
    }
    
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_poll_).count() >= 500) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& file : files_) {
            process_file(file);
        }
        last_poll_ = now;
    }
}

void AgentTokenStore::scan_all() {
    scan_claude_directory();
    scan_codex_directory();
    scan_opencode_directory();
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

void AgentTokenStore::scan_codex_directory() {
    namespace fs = std::filesystem;
    
    if (codex_dir_.empty() || !fs::exists(codex_dir_)) {
        return;
    }
    
    fs::path sessions_dir = fs::path(codex_dir_) / "sessions";
    if (!fs::exists(sessions_dir)) {
        return;
    }
    
    scan_directory_recursive(sessions_dir, AgentType::Codex);
}

void AgentTokenStore::scan_opencode_directory() {
    namespace fs = std::filesystem;
    
    if (opencode_dir_.empty() || !fs::exists(opencode_dir_)) {
        return;
    }
    
    fs::path message_dir = fs::path(opencode_dir_) / "message";
    if (!fs::exists(message_dir)) {
        return;
    }
    
    for (const auto& session_entry : fs::directory_iterator(message_dir)) {
        if (!session_entry.is_directory()) continue;
        
        std::string session_id = session_entry.path().filename().string();
        
        for (const auto& msg_entry : fs::directory_iterator(session_entry.path())) {
            if (!msg_entry.is_regular_file()) continue;
            if (msg_entry.path().extension() != ".json") continue;
            
            process_opencode_message_file(msg_entry.path(), session_id);
        }
    }
}

void AgentTokenStore::process_opencode_message_file(const std::filesystem::path& path, 
                                                     const std::string& session_id) {
    namespace fs = std::filesystem;
    
    static std::unordered_map<std::string, fs::file_time_type> processed_files;
    
    auto mtime = fs::last_write_time(path);
    auto it = processed_files.find(path.string());
    if (it != processed_files.end() && it->second == mtime) {
        return;
    }
    processed_files[path.string()] = mtime;
    
    std::ifstream file(path);
    if (!file) return;
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    try {
        nlohmann::json j = nlohmann::json::parse(content);
        
        AgentTokenUsage usage;
        if (parse_opencode_message(j, usage)) {
            opencode_session_ids_.insert(session_id);
            
            std::chrono::system_clock::time_point usage_time = std::chrono::system_clock::now();
            bool has_usage_time = false;
            if (j.contains("time") && j["time"].is_object() && j["time"].contains("created")) {
                const auto& created = j["time"]["created"];
                if (created.is_number()) {
                    auto ms = created.get<int64_t>();
                    usage_time = std::chrono::system_clock::time_point{
                        std::chrono::milliseconds(ms)
                    };
                    has_usage_time = true;
                }
            }
            
            auto& session = sessions_[session_id];
            if (session.session_id.empty()) {
                session.session_id = session_id;
                session.first_seen = has_usage_time ? usage_time : std::chrono::system_clock::now();
                session.is_subagent = false;
            }
            
            session.tokens.input_tokens += usage.input_tokens;
            session.tokens.output_tokens += usage.output_tokens;
            session.tokens.cache_creation_tokens += usage.cache_creation_tokens;
            session.tokens.cache_read_tokens += usage.cache_read_tokens;
            session.tokens.cost_usd += usage.cost_usd;
            session.last_seen = has_usage_time ? usage_time : std::chrono::system_clock::now();
            session.message_count++;
            
            if (has_usage_time) {
                add_daily_usage(daily_totals_, daily_session_ids_, AgentType::OpenCode, session_id, usage, usage_time);
            }
            
            ++files_processed_;
        }
    } catch (...) {
    }
}

bool AgentTokenStore::parse_opencode_message(const nlohmann::json& j, AgentTokenUsage& usage) {
    if (!j.contains("tokens")) {
        return false;
    }
    
    const auto& tokens = j["tokens"];
    
    if (tokens.contains("input") && tokens["input"].is_number()) {
        usage.input_tokens = tokens["input"].get<uint64_t>();
    }
    if (tokens.contains("output") && tokens["output"].is_number()) {
        usage.output_tokens = tokens["output"].get<uint64_t>();
    }
    if (tokens.contains("reasoning") && tokens["reasoning"].is_number()) {
        usage.output_tokens += tokens["reasoning"].get<uint64_t>();
    }
    if (tokens.contains("cache") && tokens["cache"].is_object()) {
        const auto& cache = tokens["cache"];
        if (cache.contains("read") && cache["read"].is_number()) {
            usage.cache_read_tokens = cache["read"].get<uint64_t>();
        }
        if (cache.contains("write") && cache["write"].is_number()) {
            usage.cache_creation_tokens = cache["write"].get<uint64_t>();
        }
    }
    
    if (j.contains("cost") && j["cost"].is_number()) {
        usage.cost_usd = j["cost"].get<double>();
    }
    
    return usage.total() > 0;
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
        std::chrono::system_clock::time_point usage_time{};
        bool has_usage_time = false;
        
        bool has_usage = parse_jsonl_line(line, usage, session_id, is_subagent, usage_time, has_usage_time);
        if (!session_id.empty() && state.session_id != session_id) {
            state.session_id = session_id;
        }
        if (has_usage) {
            if (session_id.empty()) {
                session_id = state.session_id;
            }
            
            auto& session = sessions_[session_id];
            if (session.session_id.empty()) {
                session.session_id = session_id;
                session.first_seen = has_usage_time ? usage_time : std::chrono::system_clock::now();
                session.is_subagent = state.is_subagent || is_subagent;
            }
            
            session.tokens.input_tokens += usage.input_tokens;
            session.tokens.output_tokens += usage.output_tokens;
            session.tokens.cache_creation_tokens += usage.cache_creation_tokens;
            session.tokens.cache_read_tokens += usage.cache_read_tokens;
            session.tokens.cost_usd += usage.cost_usd;
            session.last_seen = has_usage_time ? usage_time : std::chrono::system_clock::now();
            session.message_count++;

            if (has_usage_time) {
                add_daily_usage(daily_totals_, daily_session_ids_, state.agent_type, session_id, usage, usage_time);
            }
        }
    }
    
    state.last_pos = file.tellg();
}

bool AgentTokenStore::parse_jsonl_line(const std::string& line, AgentTokenUsage& usage,
                                        std::string& session_id, bool& is_subagent,
                                        std::chrono::system_clock::time_point& usage_time,
                                        bool& has_usage_time) {
    try {
        auto json = nlohmann::json::parse(line);
        
        if (json.contains("timestamp") && json["timestamp"].is_string()) {
            has_usage_time = parse_iso8601_utc(json["timestamp"].get<std::string>(), usage_time);
        }
        
        if (json.contains("type") && json["type"].is_string()) {
            std::string type = json["type"].get<std::string>();
            if (type == "session_meta" && json.contains("payload")) {
                const auto& payload = json["payload"];
                if (payload.contains("id")) {
                    session_id = payload["id"].get<std::string>();
                }
                return false;
            }
            if (type == "event_msg" && json.contains("payload")) {
                const auto& payload = json["payload"];
                if (payload.contains("type") && payload["type"].is_string()) {
                    std::string payload_type = payload["type"].get<std::string>();
                    if (payload_type == "token_count") {
                        if (!payload.contains("info") || payload["info"].is_null()) {
                            return false;
                        }
                        const auto& info = payload["info"];
                        if (!info.contains("last_token_usage")) {
                            return false;
                        }
                        const auto& last = info["last_token_usage"];
                        if (last.contains("input_tokens")) {
                            usage.input_tokens = last["input_tokens"].get<uint64_t>();
                        }
                        if (last.contains("output_tokens")) {
                            usage.output_tokens = last["output_tokens"].get<uint64_t>();
                        }
                        if (last.contains("reasoning_output_tokens")) {
                            usage.output_tokens += last["reasoning_output_tokens"].get<uint64_t>();
                        }
                        if (last.contains("cached_input_tokens")) {
                            usage.cache_read_tokens = last["cached_input_tokens"].get<uint64_t>();
                        }
                        return usage.total() > 0;
                    }
                }
            }
        }
        
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
    if (!init_done_) return {};
    std::lock_guard<std::mutex> lock(mutex_);
    
    AgentTypeStats stats;
    stats.type = type;
    
    auto now = std::chrono::system_clock::now();
    auto active_threshold = now - std::chrono::minutes(5);
    
    for (const auto& [id, session] : sessions_) {
        bool match = false;
        
        if (type == AgentType::OpenCode) {
            match = opencode_session_ids_.count(id) > 0;
        } else {
            for (const auto& file : files_) {
                if (file.session_id == id && file.agent_type == type) {
                    match = true;
                    break;
                }
            }
            
            if (!match && type == AgentType::ClaudeCode && opencode_session_ids_.count(id) == 0) {
                match = true;
            }
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
    if (!init_done_) return {};
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
    if (!init_done_) return {};
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<AgentSession> result;
    
    for (const auto& [id, session] : sessions_) {
        bool match = false;
        
        if (type == AgentType::OpenCode) {
            match = opencode_session_ids_.count(id) > 0;
        } else {
            for (const auto& file : files_) {
                if (file.session_id == id && file.agent_type == type) {
                    match = true;
                    break;
                }
            }
            
            if (!match && type == AgentType::ClaudeCode && opencode_session_ids_.count(id) == 0) {
                match = true;
            }
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

std::vector<DailyTokenData> AgentTokenStore::get_daily_data(AgentType type) const {
    if (!init_done_) return {};
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::map<std::string, DailyTokenData> daily_map;
    
    auto now = std::chrono::system_clock::now();
    
    for (int i = 0; i < 365; ++i) {
        auto day_point = now - std::chrono::hours(24 * (364 - i));
        auto time_t_val = std::chrono::system_clock::to_time_t(day_point);
        std::tm* tm = std::localtime(&time_t_val);
        
        DailyTokenData data;
        data.year = tm->tm_year + 1900;
        data.month = tm->tm_mon + 1;
        data.day = tm->tm_mday;
        data.weekday = tm->tm_wday;
        
        daily_map[data.date_key()] = data;
    }
    
    int idx = agent_index(type);
    if (idx >= 0) {
        for (const auto& [key, data] : daily_totals_[idx]) {
            if (daily_map.count(key)) {
                daily_map[key].tokens += data.tokens;
                daily_map[key].cost += data.cost;
                daily_map[key].session_count += data.session_count;
            }
        }
    }
    
    std::vector<DailyTokenData> result;
    result.reserve(daily_map.size());
    for (auto& [key, data] : daily_map) {
        result.push_back(std::move(data));
    }
    
    return result;
}

void AgentTokenStore::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.clear();
    files_.clear();
    files_processed_ = 0;
    opencode_session_ids_.clear();
    for (auto& daily : daily_totals_) {
        daily.clear();
    }
    for (auto& session_ids : daily_session_ids_) {
        session_ids.clear();
    }
}

}
