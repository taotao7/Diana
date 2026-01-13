#include "session_controller.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace diana {

namespace {

std::string trim_whitespace(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

void append_unique(std::vector<std::string>& items, const std::string& value) {
    if (value.empty()) {
        return;
    }
    if (std::find(items.begin(), items.end(), value) == items.end()) {
        items.push_back(value);
    }
}

void append_if_exists(std::vector<std::string>& items, const std::filesystem::path& path) {
    std::error_code ec;
    if (std::filesystem::exists(path, ec) && !ec) {
        append_unique(items, path.string());
    }
}

std::string read_nvm_default_version(const std::string& home_dir) {
    std::filesystem::path alias_file = std::filesystem::path(home_dir) / ".nvm/alias/default";
    std::error_code ec;
    if (!std::filesystem::exists(alias_file, ec) || ec) {
        return {};
    }
    std::ifstream f(alias_file);
    if (!f) return {};
    std::string line;
    std::getline(f, line);
    return trim_whitespace(line);
}

std::string find_nvm_version_bin(const std::string& home_dir, const std::string& alias) {
    if (alias.empty()) return {};
    std::filesystem::path versions_dir = std::filesystem::path(home_dir) / ".nvm/versions/node";
    std::error_code ec;
    if (!std::filesystem::exists(versions_dir, ec) || ec) return {};
    
    std::filesystem::path exact = versions_dir / alias;
    if (std::filesystem::exists(exact / "bin", ec) && !ec) {
        return (exact / "bin").string();
    }
    
    std::string version_prefix = alias;
    if (!version_prefix.empty() && version_prefix[0] != 'v') {
        version_prefix = "v" + version_prefix;
    }
    
    std::string best_match;
    for (const auto& entry : std::filesystem::directory_iterator(versions_dir, ec)) {
        if (ec || !entry.is_directory()) continue;
        std::string name = entry.path().filename().string();
        if (name.rfind(version_prefix, 0) == 0) {
            std::filesystem::path bin = entry.path() / "bin";
            if (std::filesystem::exists(bin, ec) && !ec) {
                if (best_match.empty() || name > best_match) {
                    best_match = bin.string();
                }
            }
        }
    }
    return best_match;
}

std::vector<std::string> split_path_list(const std::string& value) {
    std::vector<std::string> result;
    std::stringstream ss(value);
    std::string item;
    while (std::getline(ss, item, ':')) {
        item = trim_whitespace(item);
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    return result;
}

std::vector<std::string> build_search_paths() {
    std::vector<std::string> paths;

    const char* env_path = std::getenv("PATH");
    if (env_path && *env_path) {
        for (const auto& entry : split_path_list(env_path)) {
            append_unique(paths, entry);
        }
    }

    append_unique(paths, "/usr/local/bin");
    append_unique(paths, "/opt/homebrew/bin");
    append_unique(paths, "/usr/bin");
    append_unique(paths, "/bin");
    append_unique(paths, "/usr/sbin");
    append_unique(paths, "/sbin");

    const char* home = std::getenv("HOME");
    if (home && *home) {
        std::string home_dir = home;
        append_if_exists(paths, std::filesystem::path(home_dir) / ".local/bin");
        append_if_exists(paths, std::filesystem::path(home_dir) / ".cargo/bin");
        append_if_exists(paths, std::filesystem::path(home_dir) / ".npm-global/bin");
        append_if_exists(paths, std::filesystem::path(home_dir) / ".volta/bin");
        append_if_exists(paths, std::filesystem::path(home_dir) / ".asdf/shims");

        std::string default_alias = read_nvm_default_version(home_dir);
        std::string default_bin = find_nvm_version_bin(home_dir, default_alias);
        if (!default_bin.empty()) {
            append_unique(paths, default_bin);
        }
        
        std::filesystem::path nvm_dir = std::filesystem::path(home_dir) / ".nvm/versions/node";
        std::error_code ec;
        if (std::filesystem::exists(nvm_dir, ec) && !ec) {
            for (const auto& entry : std::filesystem::directory_iterator(nvm_dir, ec)) {
                if (ec) {
                    break;
                }
                if (!entry.is_directory()) {
                    continue;
                }
                append_if_exists(paths, entry.path() / "bin");
            }
        }
    }

    return paths;
}

std::string resolve_from_shell(const std::string& executable) {
    if (executable.find('/') != std::string::npos) {
        return executable;
    }
    const char* shell = std::getenv("SHELL");
    std::string shell_path = (shell && *shell) ? shell : "/bin/zsh";
    std::string command = shell_path + " -lc \"command -v " + executable + " 2>/dev/null\"";

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return {};
    }

    std::string output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        output += buffer;
    }
    pclose(pipe);

    output = trim_whitespace(output);
    if (output.empty()) {
        return {};
    }

    std::filesystem::path resolved(output);
    if (!resolved.is_absolute()) {
        return {};
    }

    std::error_code ec;
    if (!std::filesystem::exists(resolved, ec) || ec) {
        return {};
    }

    return output;
}

std::string resolve_executable_path(const std::string& executable) {
    if (executable.find('/') != std::string::npos) {
        return executable;
    }

    for (const auto& dir : build_search_paths()) {
        std::filesystem::path candidate = std::filesystem::path(dir) / executable;
        std::error_code ec;
        if (std::filesystem::exists(candidate, ec) && !ec) {
            return candidate.string();
        }
    }

    std::string shell_resolved = resolve_from_shell(executable);
    if (!shell_resolved.empty()) {
        return shell_resolved;
    }

    return executable;
}

std::string get_executable_for_app(AppKind app) {
    switch (app) {
        case AppKind::ClaudeCode: return "claude";
        case AppKind::Codex:      return "codex";
        case AppKind::OpenCode:   return "opencode";
        case AppKind::Shell:      return std::getenv("SHELL") ? std::getenv("SHELL") : "/bin/bash";
    }
    return "claude";
}

std::string get_working_dir() {
    const char* home = std::getenv("HOME");
    return home ? home : "/tmp";
}

}

SessionController::SessionController() = default;

SessionController::~SessionController() {
    // Clear callbacks before destruction to prevent IO threads from accessing event_queue_
    for (auto& [id, runner] : runners_) {
        if (runner) {
            runner->set_output_callback(nullptr);
            runner->set_exit_callback(nullptr);
            if (runner->is_running()) {
                runner->stop();
            }
        }
    }
    runners_.clear();
}

void SessionController::start_session(TerminalSession& session) {
    auto it = runners_.find(session.id());
    if (it != runners_.end()) {
        if (it->second && it->second->is_running()) {
            return;
        }
        runners_.erase(it);
    }
    
    auto runner = std::make_unique<ProcessRunner>();
    uint32_t session_id = session.id();
    
    runner->set_output_callback([this, session_id](const std::string& data, bool is_stderr) {
        event_queue_.push(OutputEvent{session_id, data, is_stderr});
    });
    
    runner->set_exit_callback([this, session_id](int exit_code) {
        event_queue_.push(ExitEvent{session_id, exit_code});
    });
    
    ProcessConfig config = build_config(session);
    
    std::string start_msg = "\r\n[Starting " + std::string(TerminalSession::app_kind_name(session.config().app)) + "]\r\n";
    session.write_to_terminal(start_msg.data(), start_msg.size());
    session.request_scroll_to_bottom();
    
    if (runner->start(config)) {
        session.set_state(SessionState::Running);
        runners_[session_id] = std::move(runner);
    } else {
        session.set_state(SessionState::Idle);
        const char* err_msg = "\r\n[Failed to start process]\r\n";
        session.write_to_terminal(err_msg, strlen(err_msg));
        session.request_scroll_to_bottom();
    }
}

void SessionController::stop_session(TerminalSession& session) {
    auto it = runners_.find(session.id());
    if (it != runners_.end() && it->second && it->second->is_running()) {
        session.set_state(SessionState::Stopping);
        it->second->stop();
    }
}

void SessionController::send_input(TerminalSession& session, const std::string& input) {
    auto it = runners_.find(session.id());
    if (it != runners_.end() && it->second && it->second->is_running()) {
        it->second->write_stdin(input + "\n");
    }
}

void SessionController::send_raw_key(TerminalSession& session, const std::string& key) {
    auto it = runners_.find(session.id());
    if (it != runners_.end() && it->second && it->second->is_running()) {
        it->second->write_stdin(key);
    }
}

void SessionController::send_key(TerminalSession& session, int vterm_key) {
    auto it = runners_.find(session.id());
    if (it == runners_.end() || !it->second || !it->second->is_running()) {
        return;
    }
    
    session.terminal().keyboard_key(vterm_key);
    std::string output = session.terminal().get_output();
    if (!output.empty()) {
        it->second->write_stdin(output);
    }
}

void SessionController::send_char(TerminalSession& session, uint32_t codepoint) {
    auto it = runners_.find(session.id());
    if (it == runners_.end() || !it->second || !it->second->is_running()) {
        return;
    }
    
    session.terminal().keyboard_unichar(codepoint);
    std::string output = session.terminal().get_output();
    if (!output.empty()) {
        it->second->write_stdin(output);
    }
}

void SessionController::process_events() {
    while (auto event_opt = event_queue_.try_pop()) {
        std::visit([](auto&& evt) {
            (void)evt;
        }, *event_opt);
    }
}

void SessionController::resize_pty(TerminalSession& session, int rows, int cols) {
    auto it = runners_.find(session.id());
    if (it != runners_.end() && it->second && it->second->is_running()) {
        it->second->resize(rows, cols);
    }
    session.terminal().resize(rows, cols);
}

ProcessConfig SessionController::build_config(const TerminalSession& session) {
    ProcessConfig config;
    config.executable = resolve_executable_path(get_executable_for_app(session.config().app));
    config.working_dir = session.config().working_dir.empty() 
        ? get_working_dir() 
        : session.config().working_dir;
    config.rows = session.terminal().rows();
    config.cols = session.terminal().cols();
    return config;
}

}
