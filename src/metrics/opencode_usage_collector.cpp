#include "opencode_usage_collector.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <future>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>

namespace diana {
namespace {

std::string get_home_dir() {
    const char* home = std::getenv("HOME");
    return home ? std::string(home) : std::string();
}

std::string get_xdg_data_home() {
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && *xdg) return std::string(xdg);
    std::string home = get_home_dir();
    if (!home.empty()) {
        return home + "/.local/share";
    }
    return {};
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

bool parse_epoch_timestamp(double value, std::chrono::system_clock::time_point& out) {
    if (value <= 0.0) {
        return false;
    }
    double ms_value = value;
    if (value > 1e12) {
        ms_value = value;
    } else if (value > 1e9) {
        ms_value = value * 1000.0;
    } else if (value > 1e6) {
        ms_value = value * 1000.0;
    } else {
        return false;
    }

    auto ms = static_cast<int64_t>(ms_value);
    out = std::chrono::system_clock::time_point{std::chrono::milliseconds(ms)};
    return true;
}

bool parse_timestamp_value(const nlohmann::json& value, std::chrono::system_clock::time_point& out) {
    if (value.is_number()) {
        return parse_epoch_timestamp(value.get<double>(), out);
    }
    if (value.is_string()) {
        std::string text = value.get<std::string>();
        if (parse_iso8601_utc(text, out)) {
            return true;
        }
        char* end = nullptr;
        double numeric = std::strtod(text.c_str(), &end);
        if (end && end != text.c_str()) {
            return parse_epoch_timestamp(numeric, out);
        }
    }
    return false;
}

bool extract_usage_timestamp(const nlohmann::json& json, std::chrono::system_clock::time_point& out) {
    if (json.contains("timestamp")) {
        if (parse_timestamp_value(json["timestamp"], out)) {
            return true;
        }
    }
    if (json.contains("created")) {
        if (parse_timestamp_value(json["created"], out)) {
            return true;
        }
    }
    if (json.contains("created_at")) {
        if (parse_timestamp_value(json["created_at"], out)) {
            return true;
        }
    }
    if (json.contains("time")) {
        if (parse_timestamp_value(json["time"], out)) {
            return true;
        }
    }
    return false;
}

}

OpencodeUsageCollector::OpencodeUsageCollector()
    : data_dir_(default_data_dir())
{
    if (!data_dir_.empty()) {
        storage_dir_ = data_dir_ + "/storage";
    }
    init_future_ = std::async(std::launch::async, &OpencodeUsageCollector::do_initial_scan, this);
}

OpencodeUsageCollector::OpencodeUsageCollector(const std::string& data_dir)
    : data_dir_(data_dir)
{
    if (!data_dir_.empty()) {
        storage_dir_ = data_dir_ + "/storage";
    }
    init_future_ = std::async(std::launch::async, &OpencodeUsageCollector::do_initial_scan, this);
}

OpencodeUsageCollector::~OpencodeUsageCollector() {
    if (init_future_.valid()) {
        init_future_.wait();
    }
}

std::string OpencodeUsageCollector::default_data_dir() {
    std::string base = get_xdg_data_home();
    if (base.empty()) return {};
    return base + "/opencode";
}

void OpencodeUsageCollector::do_initial_scan() {
    if (storage_dir_.empty()) {
        init_done_ = true;
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(mutex_);
        scan_directories();
        
        for (const auto& path : message_paths_) {
            process_file(path);
        }
    }
    
    last_scan_ = std::chrono::steady_clock::now();
    last_poll_ = std::chrono::steady_clock::now();
    init_done_ = true;
}

void OpencodeUsageCollector::poll() {
    if (storage_dir_.empty()) return;
    
    if (!init_done_) return;

    auto now = std::chrono::steady_clock::now();
    
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_scan_).count() >= 5) {
        std::lock_guard<std::mutex> lock(mutex_);
        scan_directories();
        last_scan_ = now;
    }
    
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_poll_).count() >= 500) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& path : message_paths_) {
            process_file(path);
        }
        last_poll_ = now;
    }
}

void OpencodeUsageCollector::scan_directories() {
    namespace fs = std::filesystem;

    files_processed_ = 0;
    message_paths_.clear();
    if (!fs::exists(storage_dir_)) return;

    scan_projects();
    scan_sessions();
    scan_messages();
}

void OpencodeUsageCollector::scan_projects() {
    namespace fs = std::filesystem;

    project_worktree_.clear();
    fs::path dir = fs::path(storage_dir_) / "project";
    if (!fs::exists(dir) || !fs::is_directory(dir)) return;

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        std::ifstream file(entry.path());
        if (!file) continue;
        try {
            auto json = nlohmann::json::parse(file, nullptr, false);
            if (!json.is_object()) continue;
            if (json.contains("worktree") && json["worktree"].is_string()) {
                project_worktree_[entry.path().stem().string()] = json["worktree"].get<std::string>();
            }
        } catch (...) {
        }
    }
}

void OpencodeUsageCollector::scan_sessions() {
    namespace fs = std::filesystem;

    session_by_id_.clear();
    fs::path dir = fs::path(storage_dir_) / "session";
    if (!fs::exists(dir) || !fs::is_directory(dir)) return;

    for (const auto& project_dir : fs::directory_iterator(dir)) {
        if (!project_dir.is_directory()) continue;
        std::string project_id = project_dir.path().filename().string();
        for (const auto& session_file : fs::directory_iterator(project_dir.path())) {
            if (!session_file.is_regular_file()) continue;
            std::ifstream file(session_file.path());
            if (!file) continue;
            try {
                auto json = nlohmann::json::parse(file, nullptr, false);
                if (!json.is_object()) continue;
                std::string session_id = session_file.path().stem().string();
                SessionInfo info;
                info.project_id = project_id;
                if (json.contains("directory") && json["directory"].is_string()) {
                    info.directory = json["directory"].get<std::string>();
                }
                session_by_id_[session_id] = info;
            } catch (...) {
            }
        }
    }
}

void OpencodeUsageCollector::scan_messages() {
    namespace fs = std::filesystem;

    fs::path dir = fs::path(storage_dir_) / "message";
    if (!fs::exists(dir) || !fs::is_directory(dir)) return;

    for (const auto& session_dir : fs::directory_iterator(dir)) {
        if (!session_dir.is_directory()) continue;
        for (const auto& msg_file : fs::directory_iterator(session_dir.path())) {
            if (!msg_file.is_regular_file()) continue;
            message_paths_.push_back(msg_file.path());
            ++files_processed_;
        }
    }
}

void OpencodeUsageCollector::process_file(const std::filesystem::path& path) {
    namespace fs = std::filesystem;

    if (!fs::exists(path)) return;

    std::error_code ec;
    auto mtime = fs::last_write_time(path, ec);
    if (ec) return;

    std::string path_str = path.string();
    auto it = file_mtimes_.find(path_str);
    if (it != file_mtimes_.end() && it->second == mtime) {
        return;
    }
    file_mtimes_[path_str] = mtime;

    std::ifstream file(path);
    if (!file) return;

    MessageTotals totals;
    std::chrono::system_clock::time_point usage_time = std::chrono::system_clock::now();
    bool has_usage_time = false;
    try {
        auto json = nlohmann::json::parse(file, nullptr, false);
        if (!json.is_object() || !json.contains("tokens")) return;
        const auto& tokens = json["tokens"];
        if (tokens.contains("input")) totals.input_tokens = tokens["input"].get<uint64_t>();
        if (tokens.contains("output")) totals.output_tokens = tokens["output"].get<uint64_t>();
        if (tokens.contains("reasoning")) totals.reasoning = tokens["reasoning"].get<uint64_t>();
        if (tokens.contains("cache") && tokens["cache"].is_object()) {
            const auto& cache = tokens["cache"];
            if (cache.contains("read")) totals.cache_read = cache["read"].get<uint64_t>();
            if (cache.contains("write")) totals.cache_write = cache["write"].get<uint64_t>();
        }
        if (json.contains("cost")) {
            totals.cost_usd = json["cost"].get<double>();
        }
        has_usage_time = extract_usage_timestamp(json, usage_time);
    } catch (...) {
        return;
    }

    std::string session_id = path.parent_path().filename().string();
    std::string project_key = make_project_key(session_id);
    if (project_key.empty()) return;

    auto totals_it = last_totals_.find(path.string());
    MessageTotals last = totals_it != last_totals_.end() ? totals_it->second : MessageTotals{};

    uint64_t current_input = totals.input_tokens + totals.cache_read + totals.cache_write;
    uint64_t current_output = totals.output_tokens + totals.reasoning;
    uint64_t last_input = last.input_tokens + last.cache_read + last.cache_write;
    uint64_t last_output = last.output_tokens + last.reasoning;

    int64_t delta_input = static_cast<int64_t>(current_input) - static_cast<int64_t>(last_input);
    int64_t delta_output = static_cast<int64_t>(current_output) - static_cast<int64_t>(last_output);
    int64_t delta_total = static_cast<int64_t>(current_input + current_output) -
                          static_cast<int64_t>(last_input + last_output);
    double delta_cost = totals.cost_usd - last.cost_usd;

    if (delta_input < 0) delta_input = 0;
    if (delta_output < 0) delta_output = 0;
    if (delta_total < 0) delta_total = 0;
    if (delta_cost < 0) delta_cost = 0.0;

    if ((delta_input > 0 || delta_output > 0 || delta_total > 0 || delta_cost > 0.0) &&
        (hub_ || store_)) {
        TokenSample sample;
        sample.input_tokens = static_cast<uint64_t>(delta_input);
        sample.output_tokens = static_cast<uint64_t>(delta_output);
        sample.total_tokens = static_cast<uint64_t>(delta_total);
        sample.cost_usd = delta_cost;
        sample.timestamp = has_usage_time ? usage_time : std::chrono::system_clock::now();

        if (hub_) {
            hub_->record(project_key, sample);
        }
        if (store_) {
            store_->record_sample(sample);
        }
        ++entries_parsed_;
    }

    last_totals_[path.string()] = totals;
}

std::string OpencodeUsageCollector::make_project_key(const std::string& session_id) const {
    auto it = session_by_id_.find(session_id);
    std::string directory;
    if (it != session_by_id_.end()) {
        if (!it->second.directory.empty()) {
            directory = it->second.directory;
        } else {
            auto proj_it = project_worktree_.find(it->second.project_id);
            if (proj_it != project_worktree_.end()) {
                directory = proj_it->second;
            }
        }
    }

    if (directory.empty()) {
        directory = session_id;
    }

    return "opencode:" + sanitize_key(directory);
}

std::string OpencodeUsageCollector::sanitize_key(const std::string& key) {
    std::string result = key;
    for (char& c : result) {
        if (c == '/' || c == '_') {
            c = '-';
        }
    }
    if (!result.empty() && result.front() == '-') {
        result = result.substr(1);
    }
    return result;
}

}
