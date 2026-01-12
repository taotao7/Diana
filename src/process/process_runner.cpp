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

#if defined(__APPLE__)
#include <util.h>
#elif defined(__linux__)
#include <pty.h>
#endif

namespace diana {

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
