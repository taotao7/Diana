
#include "metrics/codex_usage_collector.h"
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <sstream>

namespace {

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

std::string get_home_dir() {
    const char* home = std::getenv("HOME");
    return home ? std::string(home) : std::string();
}

}

namespace diana {

CodexUsageCollector::CodexUsageCollector() {
    std::string home = get_home_dir();
    if (!home.empty()) {
        codex_dir_ = home + "/.codex/sessions";
    }
    init_future_ = std::async(std::launch::async, &CodexUsageCollector::do_initial_scan, this);
}

CodexUsageCollector::CodexUsageCollector(const std::string& codex_dir)
    : codex_dir_(codex_dir) {
    init_future_ = std::async(std::launch::async, &CodexUsageCollector::do_initial_scan, this);
}

CodexUsageCollector::~CodexUsageCollector() {
    if (init_future_.valid()) {
        init_future_.wait();
    }
}

void CodexUsageCollector::do_initial_scan() {
    {
        std::lock_guard<std::mutex> lock(files_mutex_);
        scan_directories();
        for (auto& file : files_) {
            process_file(file);
        }
    }
    last_scan_ = std::chrono::steady_clock::now();
    last_poll_ = std::chrono::steady_clock::now();
    init_done_ = true;
}

void CodexUsageCollector::poll() {
    if (codex_dir_.empty()) {
        return;
    }
    if (!store_ && !hub_) {
        return;
    }
    if (!init_done_) {
        return;
    }

    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_scan_).count() >= 5) {
        std::lock_guard<std::mutex> lock(files_mutex_);
        scan_directories();
        last_scan_ = now;
    }

    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_poll_).count() >= 500) {
        std::lock_guard<std::mutex> lock(files_mutex_);
        for (auto& file : files_) {
            process_file(file);
        }
        last_poll_ = now;
    }
}

void CodexUsageCollector::scan_directory_recursive(const std::filesystem::path& dir) {
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

void CodexUsageCollector::scan_directories() {
    namespace fs = std::filesystem;

    if (codex_dir_.empty()) {
        return;
    }
    fs::path sessions_dir = fs::path(codex_dir_);
    scan_directory_recursive(sessions_dir);
}

void CodexUsageCollector::process_file(FileState& state) {
    namespace fs = std::filesystem;

    if (!fs::exists(state.path)) return;

    auto file_size = static_cast<std::streamoff>(fs::file_size(state.path));
    if (file_size < state.last_pos) {
        state.last_pos = 0;
    }
    if (file_size == state.last_pos) {
        return;
    }

    std::ifstream file(state.path);
    if (!file) return;

    if (state.last_pos > 0) {
        file.seekg(state.last_pos);
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        TokenSample sample;
        bool has_sample = false;
        if (parse_json_line(line, state, sample, has_sample) && has_sample) {
            if (hub_) {
                hub_->record(state.project_key, sample);
            }
            if (store_) {
                store_->record_sample(sample);
            }
            ++entries_parsed_;
        }
    }

    auto pos = file.tellg();
    if (pos < 0 || pos > file_size) {
        state.last_pos = file_size;
    } else {
        state.last_pos = pos;
    }
}

bool CodexUsageCollector::parse_json_line(const std::string& line, FileState& state, TokenSample& sample, bool& has_sample) {
    has_sample = false;

    auto json = nlohmann::json::parse(line, nullptr, false);
    if (json.is_discarded()) {
        return false;
    }

    if (json.contains("type") && json["type"].is_string()) {
        std::string type = json["type"].get<std::string>();
        if (type == "turn_context" && json.contains("payload") && json["payload"].is_object()) {
            const auto& payload = json["payload"];
            if (payload.contains("cwd") && payload["cwd"].is_string()) {
                state.cwd = payload["cwd"].get<std::string>();
                state.project_key = normalize_project_key(state.cwd);
            }
            return true;
        }

        if (type == "event_msg" && json.contains("payload") && json["payload"].is_object()) {
            const auto& payload = json["payload"];
            if (payload.contains("type") && payload["type"].is_string() && payload["type"].get<std::string>() == "token_count") {
                if (!payload.contains("info") || payload["info"].is_null()) {
                    return true;
                }
                const auto& info = payload["info"];
                if (!info.contains("last_token_usage") || !info["last_token_usage"].is_object()) {
                    return true;
                }

                const auto& last = info["last_token_usage"];
                uint64_t input_tokens = last.value("input_tokens", 0ull);
                uint64_t cached_input_tokens = last.value("cached_input_tokens", 0ull);
                uint64_t output_tokens = last.value("output_tokens", 0ull);
                uint64_t reasoning_tokens = last.value("reasoning_output_tokens", 0ull);

                uint64_t total_input = input_tokens + cached_input_tokens;
                uint64_t total_output = output_tokens + reasoning_tokens;

                if (total_input == 0 && total_output == 0) {
                    return true;
                }

                if (total_input == state.last_input_tokens && total_output == state.last_output_tokens) {
                    return true;
                }

                state.last_input_tokens = total_input;
                state.last_output_tokens = total_output;

                std::chrono::system_clock::time_point ts = std::chrono::system_clock::now();
                if (json.contains("timestamp") && json["timestamp"].is_string()) {
                    parse_iso8601_utc(json["timestamp"].get<std::string>(), ts);
                }

                sample.timestamp = ts;
                sample.input_tokens = total_input;
                sample.output_tokens = total_output;
                sample.total_tokens = total_input + total_output;
                has_sample = true;

                if (state.project_key.empty()) {
                    state.project_key = normalize_project_key(state.cwd);
                }
                return true;
            }
        }
    }

    return true;
}

std::string CodexUsageCollector::normalize_project_key(const std::string& cwd) const {
    std::string result = cwd;
    if (result.empty()) {
        result = get_home_dir();
    }
    if (result.empty()) {
        return "unknown";
    }

    for (char& c : result) {
        if (c == '/' || c == '_') {
            c = '-';
        }
    }
    if (!result.empty() && result[0] == '-') {
        result = result.substr(1);
    }
    return "codex:" + result;
}

}
