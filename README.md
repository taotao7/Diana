# Diana

**Your Agent's Best Handler.**

Diana is the ultimate mission control for your AI agents. Just as every Agent 47 needs a Diana, every LLM needs a handler to manage directives (configuration), monitor results (outputs), and keep the budget in check (token calculation).

## Features

- Multi-tab terminal emulator for running AI agents
- Supports Claude Code, Codex, and OpenCode
- Provider/model configuration switching with atomic file updates
- Real-time token usage monitoring with charts
- Config import/export for backup and sharing
- Claude Code multi-profile configuration management

## Supported Agents

| Agent | Config Location | Format | Preset Providers |
|-------|----------------|--------|------------------|
| Claude Code | `~/.claude/settings.json` | JSON | anthropic, bedrock, vertex |
| Codex | `~/.codex/config.toml` | TOML | openai, azure, ollama |
| OpenCode | `~/.config/opencode/opencode.json` | JSONC | anthropic, openai, azure, ollama |

Custom providers can be entered directly in the Provider field.

## Requirements

### macOS
- Xcode Command Line Tools
- CMake 3.20+

### Linux
- GCC 9+ or Clang 10+
- CMake 3.20+
- OpenGL development libraries
- X11 or Wayland development libraries

```bash
# Ubuntu/Debian
sudo apt install build-essential cmake libgl1-mesa-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```

## Build

All dependencies are fetched automatically via CMake FetchContent.

```bash
mkdir build && cd build

# Release build (recommended)
cmake .. -DCMAKE_BUILD_TYPE=Release -DDIANA_BUILD_TESTS=OFF
make -j8

# Run
./diana
```

### With Tests

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DDIANA_BUILD_TESTS=ON
make -j8
./diana_tests
```

## Usage

### Terminal Panel (Center)
- Click "+" to create a terminal session
- Select agent type (Claude Code, Codex, OpenCode)
- Click "Start" to launch the agent
- Type in the input field and press Enter to send commands
- Click "Stop" to terminate the agent

### Claude Code Panel (Left)
- Multi-profile configuration management
- Create, edit, rename, delete profiles
- Import current config from `settings.json`
- Radio button to activate profile (syncs to `settings.json`)
- Full config editing: model, env vars, permissions, sandbox, attribution

### Token Metrics Panel (Right)
- Monitors token usage from `~/.claude/*.jsonl` files
- Displays real-time rates (tok/sec, tok/min)
- Shows cumulative totals and costs
- Bar chart visualization of token rate over last 60 seconds

## Architecture

```
src/
├── main.cpp                    # Entry point, GLFW/OpenGL setup
├── app/
│   ├── app_shell.h/cpp         # Main application orchestrator
│   └── dockspace.h/cpp         # ImGui docking layout
├── terminal/
│   ├── terminal_buffer.h/cpp   # Ring buffer for terminal output
│   ├── terminal_session.h/cpp  # Per-tab session state
│   ├── terminal_panel.h/cpp    # Multi-tab terminal UI
│   └── ansi_parser.h/cpp       # ANSI escape sequence handling
├── process/
│   ├── process_runner.h/cpp    # PTY/fork process spawning (POSIX)
│   └── session_controller.h/cpp # Agent lifecycle management
├── adapters/
│   ├── app_adapter.h           # Abstract adapter interface
│   ├── claude_code_adapter.h/cpp
│   ├── claude_code_config.h/cpp # Full Claude Code settings struct
│   ├── profile_store.h/cpp     # Multi-profile CRUD + active detection
│   ├── codex_adapter.h/cpp
│   ├── opencode_adapter.h/cpp
│   ├── config_manager.h/cpp    # Unified config access
│   └── config_exporter.h/cpp   # JSON export/import
├── metrics/
│   ├── metrics_store.h/cpp     # Token metrics aggregation
│   └── claude_usage_collector.h/cpp # JSONL file watcher
├── ui/
│   ├── claude_code_panel.h/cpp # Profile list + config editor UI
│   ├── metrics_panel.h/cpp     # Token usage charts
│   └── profiles_panel.h/cpp    # (legacy, to be removed)
└── core/
    ├── event_queue.h           # Thread-safe event queue
    └── session_events.h        # Event type definitions
```

### Data Flow

```
User Input → TerminalPanel → SessionController → ProcessRunner (PTY)
                                    ↓
                              AppAdapter (config read/write)
                                    ↓
                              ~/.claude/settings.json
                                    ↓
                              ProfileStore (multi-profile management)
                                    ↓
                              ~/.claude/diana_profiles.json
```

## Roadmap

- [ ] Claude Code Skill/MCP marketplace browser
- [ ] OpenCode configuration panel (similar to Claude Code)
- [ ] Codex configuration panel (similar to Claude Code)
- [ ] Token cost estimation per session
- [ ] Session history and replay

## Dependencies

Fetched automatically:
- [Dear ImGui](https://github.com/ocornut/imgui) (docking branch)
- [GLFW](https://github.com/glfw/glfw) 3.4
- [ImPlot](https://github.com/epezent/implot)
- [nlohmann/json](https://github.com/nlohmann/json) 3.11.3
- [tomlplusplus](https://github.com/marzer/tomlplusplus) 3.4.0
- [nativefiledialog-extended](https://github.com/btzy/nativefiledialog-extended) 1.2.1
- [GoogleTest](https://github.com/google/googletest) 1.14.0 (tests only)

## License

MIT
