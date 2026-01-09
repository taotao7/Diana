#include "session_controller.h"
#include <cstdlib>
#include <cstring>

namespace agent47 {

namespace {

const char* get_executable_for_app(AppKind app) {
    switch (app) {
        case AppKind::ClaudeCode: return "claude";
        case AppKind::Codex:      return "codex";
        case AppKind::OpenCode:   return "opencode";
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
    for (auto& [id, runner] : runners_) {
        if (runner && runner->is_running()) {
            runner->stop();
        }
    }
}

void SessionController::start_session(TerminalSession& session) {
    if (runners_.count(session.id()) && runners_[session.id()]->is_running()) {
        return;
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
    config.executable = get_executable_for_app(session.config().app);
    config.working_dir = session.config().working_dir.empty() 
        ? get_working_dir() 
        : session.config().working_dir;
    config.rows = session.terminal().rows();
    config.cols = session.terminal().cols();
    return config;
}

}
