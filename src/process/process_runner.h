#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <thread>
#include <sys/types.h>

namespace agent47 {

struct ProcessConfig {
    std::string executable;
    std::vector<std::string> args;
    std::string working_dir;
    int rows = 24;
    int cols = 80;
};

using OutputCallback = std::function<void(const std::string& data, bool is_stderr)>;
using ExitCallback = std::function<void(int exit_code)>;

class ProcessRunner {
public:
    ProcessRunner();
    ~ProcessRunner();
    
    ProcessRunner(const ProcessRunner&) = delete;
    ProcessRunner& operator=(const ProcessRunner&) = delete;
    
    bool start(const ProcessConfig& config);
    void stop();
    void kill();
    
    bool write_stdin(const std::string& data);
    void resize(int rows, int cols);
    
    bool is_running() const { return running_.load(); }
    pid_t pid() const { return pid_; }
    
    void set_output_callback(OutputCallback cb) { output_callback_ = std::move(cb); }
    void set_exit_callback(ExitCallback cb) { exit_callback_ = std::move(cb); }

private:
    void io_thread_func();
    void close_pty();
    
    pid_t pid_ = -1;
    int pty_fd_ = -1;
    
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::thread io_thread_;
    
    OutputCallback output_callback_;
    ExitCallback exit_callback_;
};

}
