#pragma once

#include <string>
#include <variant>
#include <cstdint>

namespace agent47 {

struct OutputEvent {
    uint32_t session_id;
    std::string data;
    bool is_stderr;
};

struct ExitEvent {
    uint32_t session_id;
    int exit_code;
};

struct InputEvent {
    uint32_t session_id;
    std::string data;
};

struct StartEvent {
    uint32_t session_id;
};

struct StopEvent {
    uint32_t session_id;
};

using SessionEvent = std::variant<OutputEvent, ExitEvent, InputEvent, StartEvent, StopEvent>;

}
