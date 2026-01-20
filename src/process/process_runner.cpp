#include "process_runner.h"

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <poll.h>
#include <cstring>
#include <cerrno>
#include <termios.h>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <algorithm>

#if defined(__APPLE__)
#include <util.h>
#elif defined(__linux__)
#include <pty.h>
#endif

namespace diana {

namespace {

std::string trim_ws(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
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
    return trim_ws(line);
}

std::string find_nvm_version_dir(const std::string& home_dir, const std::string& alias) {
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

std::vector<std::string> collect_nvm_bin_paths(const std::string& home_dir) {
    std::vector<std::string> paths;
    std::filesystem::path versions_dir = std::filesystem::path(home_dir) / ".nvm/versions/node";
    std::error_code ec;
    if (!std::filesystem::exists(versions_dir, ec) || ec) return paths;
    
    std::string default_alias = read_nvm_default_version(home_dir);
    std::string default_bin = find_nvm_version_dir(home_dir, default_alias);
    if (!default_bin.empty()) {
        paths.push_back(default_bin);
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(versions_dir, ec)) {
        if (ec || !entry.is_directory()) continue;
        std::filesystem::path bin = entry.path() / "bin";
        if (std::filesystem::exists(bin, ec) && !ec) {
            std::string bin_str = bin.string();
            if (std::find(paths.begin(), paths.end(), bin_str) == paths.end()) {
                paths.push_back(bin_str);
            }
        }
    }
    return paths;
}

}

ProcessRunner::ProcessRunner() = default;

ProcessRunner::~ProcessRunner() {
    stop();
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
}

bool ProcessRunner::start(const ProcessConfig& config) {
    if (running_.load()) {
        return false;
    }

    if (io_thread_.joinable()) {
        io_thread_.join();
    }
    
    struct winsize ws;
    ws.ws_col = static_cast<unsigned short>(config.cols);
    ws.ws_row = static_cast<unsigned short>(config.rows);
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    
    pid_ = forkpty(&pty_fd_, nullptr, nullptr, &ws);
    
    if (pid_ < 0) {
        return false;
    }
    
    if (pid_ == 0) {
        setpgid(0, 0);
        
        if (!config.working_dir.empty()) {
            if (chdir(config.working_dir.c_str()) != 0) {
                fprintf(stderr, "Failed to change directory to %s: %s\n", config.working_dir.c_str(), strerror(errno));
                _exit(127);
            }
        }
        
        const char* current_path = std::getenv("PATH");
        std::string new_path = "/usr/local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin";
        const char* home = std::getenv("HOME");
        if (home && *home) {
            std::string home_dir = home;
            new_path += ":" + home_dir + "/.local/bin";
            new_path += ":" + home_dir + "/.cargo/bin";
            new_path += ":" + home_dir + "/.npm-global/bin";
            new_path += ":" + home_dir + "/.volta/bin";
            new_path += ":" + home_dir + "/.asdf/shims";
            
            for (const auto& nvm_bin : collect_nvm_bin_paths(home_dir)) {
                new_path += ":" + nvm_bin;
            }
        }
        if (current_path && *current_path) {
            new_path += ":";
            new_path += current_path;
        }
        setenv("PATH", new_path.c_str(), 1);
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        setenv("LANG", "en_US.UTF-8", 1);
        
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(config.executable.c_str()));
        for (const auto& arg : config.args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        
        execvp(config.executable.c_str(), argv.data());
        
        fprintf(stderr, "Failed to execute '%s': %s\n", config.executable.c_str(), strerror(errno));
        _exit(127);
    }
    
    fcntl(pty_fd_, F_SETFL, fcntl(pty_fd_, F_GETFL) | O_NONBLOCK);
    
    running_.store(true);
    stop_requested_.store(false);
    
    io_thread_ = std::thread(&ProcessRunner::io_thread_func, this);
    
    return true;
}

void ProcessRunner::stop() {
    if (!running_.load()) return;
    
    stop_requested_.store(true);
    
    if (pid_ > 0) {
        ::kill(-pid_, SIGTERM);
    }
}

void ProcessRunner::kill() {
    if (!running_.load()) return;
    
    stop_requested_.store(true);
    
    if (pid_ > 0) {
        ::kill(pid_, SIGKILL);
    }
}

bool ProcessRunner::write_stdin(const std::string& data) {
    if (!running_.load() || pty_fd_ < 0) {
        return false;
    }
    
    ssize_t written = write(pty_fd_, data.c_str(), data.size());
    return written == static_cast<ssize_t>(data.size());
}

void ProcessRunner::resize(int rows, int cols) {
    if (pty_fd_ < 0) return;
    
    struct winsize ws;
    ws.ws_row = static_cast<unsigned short>(rows);
    ws.ws_col = static_cast<unsigned short>(cols);
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    
    ioctl(pty_fd_, TIOCSWINSZ, &ws);
}

void ProcessRunner::io_thread_func() {
    constexpr size_t BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    
    struct pollfd fds[1];
    fds[0].fd = pty_fd_;
    fds[0].events = POLLIN;
    
    bool exit_reported = false;
    int exit_code = -1;
    
    while (!stop_requested_.load()) {
        int ret = poll(fds, 1, 100);
        
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        
        bool hangup = ret > 0 && (fds[0].revents & POLLHUP);
        if (ret > 0 && (fds[0].revents & (POLLIN | POLLHUP))) {
            while (true) {
                ssize_t n = read(pty_fd_, buffer, BUFFER_SIZE - 1);
                if (n > 0) {
                    buffer[n] = '\0';
                    if (output_callback_) {
                        output_callback_(std::string(buffer, n), false);
                    }
                    continue;
                }
                if (n == 0) {
                    hangup = true;
                    break;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                hangup = true;
                break;
            }
        }
        
        int status;
        pid_t result = waitpid(pid_, &status, WNOHANG);
        if (result == pid_) {
            exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            pid_ = -1;
            
            for (int drain_attempts = 0; drain_attempts < 50; ++drain_attempts) {
                ssize_t n = read(pty_fd_, buffer, BUFFER_SIZE - 1);
                if (n > 0) {
                    buffer[n] = '\0';
                    if (output_callback_) {
                        output_callback_(std::string(buffer, n), false);
                    }
                } else if (n == 0) {
                    break;
                } else {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            break;
        }
        
        if (hangup) {
            break;
        }
    }
    
    if (pid_ > 0 && stop_requested_.load()) {
        close_pty();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        int status;
        pid_t r = waitpid(pid_, &status, WNOHANG);
        if (r == pid_) {
            exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            pid_ = -1;
        } else {
            ::kill(pid_, SIGKILL);
            ::kill(-pid_, SIGKILL);
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            r = waitpid(pid_, &status, WNOHANG);
            if (r == pid_) {
                exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            }
            pid_ = -1;
        }
    }
    
    if (exit_callback_ && !exit_reported) {
        exit_reported = true;
        exit_callback_(exit_code);
    }
    
    close_pty();
    running_.store(false);
}

void ProcessRunner::close_pty() {
    if (pty_fd_ >= 0) { 
        close(pty_fd_); 
        pty_fd_ = -1; 
    }
}

}
