# Diana

<img width="2752" height="1536" alt="Gemini_Generated_Image_8fewwy8fewwy8few" src="https://github.com/user-attachments/assets/c15fc35f-d90d-45c5-ad33-d17954a7f951" />

**Your Agent's Best Handler.**

Diana is the ultimate mission control for your AI agents. Just as every Agent 47 needs a Diana, every LLM needs a handler to manage directives (configuration), monitor results (outputs), and keep the budget in check (token calculation).

> [!IMPORTANT]
> **Always select a project directory when starting an agent session.**
>
> Token metrics are tracked per project directory. Running multiple agents on the same project directory will cause conflicts and inaccurate metrics.
>
> **Best Practice:** Use `git worktree` to create separate directories for parallel work on the same repository:
> ```bash
> git worktree add ../my-project-feature feature-branch
> ```
> This way, each agent operates in its own directory with isolated token tracking.

## Features

- Multi-tab terminal emulator for running AI agents (VT100/xterm via libvterm)
- Supports Claude Code, Codex, and OpenCode
- Provider/model configuration switching with atomic file updates
- Real-time token usage monitoring with charts
- Config import/export for backup and sharing
- Claude Code multi-profile configuration management

## Supported Agents

| Agent | Config Location | Format |
|-------|----------------|--------|
| Claude Code | `~/.claude/settings.json` | JSON |
| Codex | `~/.codex/config.toml` | TOML |
| OpenCode | `~/.config/opencode/opencode.json` | JSONC |

Providers/models can be entered directly in the configuration fields.

## Example
<img width="4330" height="2224" alt="image" src="https://github.com/user-attachments/assets/de4dcfea-fd5a-43b0-a7e4-bd77ffc2c96e" />
<img width="4330" height="2224" alt="image" src="https://github.com/user-attachments/assets/5baa322c-8b91-4714-90ce-b240bff293ae" />


## Requirements

### macOS
- Xcode Command Line Tools
- CMake 3.20+

```bash
# Install via Homebrew
brew install cmake
xcode-select --install
```

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

### Agent Token Stats Panel (Right)
- Aggregates token usage per agent type (Claude Code, Codex, OpenCode)
- Scans `~/.claude/projects/` and `~/.claude/transcripts/` directories
- Displays total tokens, cost, and token breakdown (input/output/cache)
- Session list with subagent detection
- Real-time rate chart (tokens/sec over last 60 seconds)

## Architecture

```
src/
├── main.cpp                         # Entry point, GLFW/OpenGL setup
├── app/
│   ├── app_shell.h/cpp              # Main application orchestrator
│   └── dockspace.h/cpp              # ImGui docking layout
├── core/
│   ├── types.h                      # Shared enums (AppKind)
│   ├── event_queue.h                # Thread-safe event queue
│   └── session_events.h             # Event type definitions
├── terminal/
│   ├── vterminal.h/cpp              # libvterm wrapper for VT100/xterm emulation
│   ├── terminal_session.h/cpp       # Per-tab session state
│   └── terminal_panel.h/cpp         # Multi-tab terminal UI
├── process/
│   ├── process_runner.h/cpp         # PTY/fork process spawning (POSIX)
│   └── session_controller.h/cpp     # Agent lifecycle management
├── adapters/
│   ├── app_adapter.h                # Abstract adapter interface
│   ├── claude_code_adapter.h/cpp    # Claude Code settings.json adapter
│   ├── claude_code_config.h/cpp     # Full Claude Code config struct
│   ├── claude_profile_store.h/cpp   # Claude Code multi-profile CRUD
│   ├── codex_adapter.h/cpp          # Codex config.toml adapter
│   ├── opencode_adapter.h/cpp       # OpenCode config adapter
│   ├── config_manager.h/cpp         # Unified config access
│   └── config_exporter.h/cpp        # JSON export/import
├── metrics/
│   ├── metrics_store.h/cpp          # Token metrics aggregation
│   ├── claude_usage_collector.h/cpp # JSONL file watcher
│   └── agent_token_store.h/cpp      # Per-agent token aggregation
└── ui/
    ├── claude_code_panel.h/cpp      # Profile list + config editor UI
    ├── metrics_panel.h/cpp          # Token usage charts
    └── agent_token_panel.h/cpp      # Agent token stats panel
```

### Terminal Emulation

The terminal panel uses **libvterm** to provide VT100/xterm-compatible emulation (escape sequences, cursor movement, and color handling).

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
- [x] Agent token stats panel with per-agent aggregation
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

Vendored:
- libvterm (in `third_party/libvterm/`)

## Font

Diana uses [GNU Unifont](https://unifoundry.com/unifont/) (`unifont.otf`) for text rendering. Unifont is a Unicode font with near-complete coverage of the Basic Multilingual Plane (BMP), supporting virtually all written languages including CJK (Chinese, Japanese, Korean), Cyrillic, Arabic, Hebrew, Thai, and many more. This ensures consistent text display regardless of language or script.

## License

MIT
