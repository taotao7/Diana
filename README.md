# Diana

**Your Agent's Best Handler.**

Diana is the ultimate mission control for your AI agents. Just as every Agent 47 needs a Diana, every LLM needs a handler to manage directives (configuration), monitor results (outputs), and keep the budget in check (token calculation).

## Features

- Multi-tab terminal emulator for running AI agents
- Supports Claude Code, Codex, and OpenCode
- Provider/model configuration switching with atomic file updates
- Real-time token usage monitoring with charts
- Config import/export for backup and sharing

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

### Profiles Panel (Left)
- View and edit provider/model settings for each agent
- Type any custom provider name or select from presets dropdown
- Click "Apply" to save changes (requires agent restart)
- Use "Export All..." / "Import..." to backup or restore configs

### Token Metrics Panel (Right)
- Monitors token usage from `~/.claude/*.jsonl` files
- Displays real-time rates (tok/sec, tok/min)
- Shows cumulative totals and costs
- Bar chart visualization of token rate over last 60 seconds

## Project Structure

```
src/
├── app/           # Application shell and docking
├── terminal/      # Terminal buffer, session, panel
├── process/       # Process spawning and session controller
├── adapters/      # Config adapters for each agent
├── metrics/       # Token monitoring and storage
└── ui/            # UI panels (profiles, metrics)
tests/
├── core/          # EventQueue tests
├── metrics/       # MetricsStore tests
└── adapters/      # ConfigExporter tests
```

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
