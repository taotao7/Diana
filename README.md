# Diana

<img width="2752" height="1536" alt="Gemini_Generated_Image_8fewwy8fewwy8few" src="https://github.com/user-attachments/assets/c15fc35f-d90d-45c5-ad33-d17954a7f951" />

**Your Agent's Best Handler.**

Diana is the ultimate mission control for your AI agents. Just as every Agent 47 needs a Diana, every LLM needs a handler to manage directives (configuration), monitor results (outputs), and keep the budget in check (token calculation). It focuses on unified agent config management, at-a-glance token visibility, and fast in-app edits so you can adjust settings without juggling files.

> [!IMPORTANT]
> **This project is under active development.** Currently only macOS is tested and supported. Other platforms (Linux, Windows) have not been tested.

> [!IMPORTANT]
> **Always select a project directory when starting an agent session.**
>
> Token metrics are tracked per project directory. Running multiple agents on the same project directory will cause conflicts and inaccurate metrics.
>
> **Best Practice:** Use `git worktree` to create separate directories for parallel work on the same repository:
>
> ```bash
> git worktree add ../my-project-feature feature-branch
> ```
>
> This way, each agent operates in its own directory with isolated token tracking.

## Features

- Multi-tab terminal emulator for running AI agents (VT100/xterm via libvterm)
- Supports Claude Code, Codex, and OpenCode
- Provider/model configuration switching with atomic file updates
- Real-time token usage monitoring with charts
- Per-project token metrics tracking
- Config import/export for backup and sharing
- Claude Code multi-profile configuration management
- OpenCode multi-profile configuration management
- Unified Agent Config panel with tab switching and fast in-app edits
- GitHub-style activity heatmap for token usage

## Supported Agents

| Agent       | Config Location                    | Format |
| ----------- | ---------------------------------- | ------ |
| Claude Code | `settings.json` (auto-detected)    | JSON   |
| Codex       | `~/.codex/config.toml`             | TOML   |
| OpenCode    | `~/.config/opencode/opencode.json` | JSONC  |

Providers/models can be entered directly in the configuration fields.

## Screenshots

<img width="3840" height="2224" alt="image" src="https://github.com/user-attachments/assets/dcb56de9-0299-4a36-84e1-34572bba22fc" />
<img width="3840" height="2224" alt="image" src="https://github.com/user-attachments/assets/79c3494f-cf73-499d-9c69-926936efd8a7" />

## Technology Stack

| Category         | Technology                                                                     | Purpose                            |
| ---------------- | ------------------------------------------------------------------------------ | ---------------------------------- |
| **Language**     | C++17                                                                          | Core implementation                |
| **Build System** | CMake 3.20+                                                                    | Cross-platform build configuration |
| **UI Framework** | [Dear ImGui](https://github.com/ocornut/imgui) (docking)                       | Immediate-mode GUI                 |
| **Charting**     | [ImPlot](https://github.com/epezent/implot)                                    | Real-time data visualization       |
| **Windowing**    | [GLFW](https://github.com/glfw/glfw) 3.4                                       | Cross-platform window/input        |
| **Graphics**     | OpenGL 3.3+                                                                    | Hardware-accelerated rendering     |
| **Terminal**     | libvterm                                                                       | VT100/xterm emulation              |
| **JSON**         | [nlohmann/json](https://github.com/nlohmann/json) 3.11.3                       | Config parsing, JSONL processing   |
| **TOML**         | [tomlplusplus](https://github.com/marzer/tomlplusplus) 3.4.0                   | Codex config parsing               |
| **File Dialog**  | [nativefiledialog-extended](https://github.com/btzy/nativefiledialog-extended) | Native OS file dialogs             |
| **Testing**      | [GoogleTest](https://github.com/google/googletest) 1.14.0                      | Unit testing framework             |

## Requirements

### macOS

- Xcode Command Line Tools
- CMake 3.20+

```bash
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

### Package DMG (macOS)

Create a distributable `.app` bundle and DMG:

```bash
./scripts/package-dmg.sh
```

Output: `build/Diana-<version>-<arch>.dmg`

### Signed Release (Optional)

For distribution outside the App Store, code signing and notarization are required:

```bash
export CODESIGN_IDENTITY="Developer ID Application: Your Name (XXXXXXXXXX)"
export APPLE_ID="your@email.com"
export APPLE_TEAM_ID="XXXXXXXXXX"
export APPLE_APP_PASSWORD="xxxx-xxxx-xxxx-xxxx"
export VERSION="0.1.0"

./scripts/package-dmg.sh
```

Generate an app-specific password at [appleid.apple.com](https://appleid.apple.com/account/manage).

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

### OpenCode Panel (Left)

- Multi-profile configuration management
- Create, edit, rename, delete profiles
- Import current config from `opencode.json`
- Radio button to activate profile (syncs to `opencode.json`)
- Full config editing:
  - Basic: model, small_model, theme, default_agent, instructions
  - Providers: API keys, base URLs, custom models with full configuration (name, limits, modalities, variants)
  - Agents: custom agents with model, prompt, tool restrictions
  - Tools: enable/disable individual tools
  - Permissions: per-tool approval settings (ask/allow)
  - MCP Servers: local and remote MCP server configuration
  - TUI: scroll speed, acceleration, diff style
  - Advanced: share mode, auto-update, compaction, watcher ignore, plugins

### Agent Config Panel (Left)

The Agent Config panel provides unified access to both Claude Code and OpenCode configuration with tab switching at the top of the panel.

### Token Metrics Panel (Right)

- Monitors token usage from Claude JSONL logs (projects/transcripts) and OpenCode storage (`$XDG_DATA_HOME/opencode/storage/message/`)
- Displays real-time rates (tok/sec, tok/min)
- Shows cumulative totals and costs
- Bar chart visualization of token rate over last 60 seconds
- Per-session scope selector

### Agent Token Stats Panel (Right)

- Aggregates token usage per agent type (Claude Code, Codex, OpenCode)
- Scans Claude Code project/transcript logs and `$XDG_DATA_HOME/opencode/storage/message/` (fallback: `~/.local/share/opencode/storage/message/`)
- Displays total tokens, cost, and token breakdown (input/output/cache)
- Session list with subagent detection
- GitHub-style activity heatmap (last 365 days)

## Architecture

```
diana/
├── CMakeLists.txt
├── src/
│   ├── main.cpp                      # Entry point, GLFW/OpenGL setup
│   ├── app/
│   │   ├── app_shell.h/cpp           # Main application orchestrator
│   │   └── dockspace.h/cpp           # ImGui docking layout manager
│   ├── core/
│   │   ├── types.h                   # Shared enums (AppKind)
│   │   ├── event_queue.h             # Thread-safe lock-free event queue
│   │   └── session_events.h          # Event type definitions
│   ├── terminal/
│   │   ├── vterminal.h/cpp           # libvterm wrapper (VT100/xterm)
│   │   ├── terminal_session.h/cpp    # Per-tab session state machine
│   │   └── terminal_panel.h/cpp      # Multi-tab terminal UI
│   ├── process/
│   │   ├── process_runner.h/cpp      # PTY/fork process spawning (POSIX)
│   │   └── session_controller.h/cpp  # Agent lifecycle management
│   ├── adapters/
│   │   ├── app_adapter.h             # Abstract adapter interface
│   │   ├── claude_code_adapter.h/cpp # Claude Code settings.json
│   │   ├── claude_code_config.h/cpp  # Full Claude Code config struct
│   │   ├── claude_profile_store.h/cpp# Multi-profile CRUD operations
│   │   ├── codex_adapter.h/cpp       # Codex config.toml
│   │   ├── opencode_adapter.h/cpp    # OpenCode config adapter
│   │   ├── opencode_config.h/cpp     # Full OpenCode config struct
│   │   ├── opencode_profile_store.h/cpp # OpenCode multi-profile CRUD
│   │   ├── config_manager.h/cpp      # Unified config access facade
│   │   └── config_exporter.h/cpp     # JSON export/import utilities
│   ├── metrics/
│   │   ├── metrics_store.h/cpp       # Token metrics with EMA smoothing
│   │   ├── multi_metrics_store.h/cpp # Per-project metrics hub
│   │   ├── claude_usage_collector.h/cpp # Claude JSONL file watcher
│   │   ├── opencode_usage_collector.h/cpp # OpenCode storage parser
│   │   └── agent_token_store.h/cpp   # Per-agent token aggregation
│   └── ui/
│       ├── theme.h/cpp               # Catppuccin theme + system detection
│       ├── theme_macos.mm            # macOS appearance bridge
│       ├── claude_code_panel.h/cpp   # Claude Code profile list + config editor
│       ├── opencode_panel.h/cpp      # OpenCode profile list + config editor
│       ├── agent_config_panel.h/cpp  # Unified tab panel for agent configs
│       ├── metrics_panel.h/cpp       # Real-time token charts
│       └── agent_token_panel.h/cpp   # Agent stats + heatmap
├── tests/
│   ├── test_main.cpp
│   ├── core/test_event_queue.cpp
│   ├── metrics/
│   │   ├── test_metrics_store.cpp
│   │   ├── test_multi_metrics_store.cpp
│   │   ├── test_agent_token_store.cpp
│   │   └── test_claude_usage_collector.cpp
│   └── adapters/
│       ├── test_config_exporter.cpp
│       ├── test_opencode_config.cpp
│       └── test_opencode_profile_store.cpp
├── packaging/
│   ├── Info.plist.in                 # macOS bundle metadata
│   └── entitlements.plist            # macOS signing entitlements
├── scripts/
│   ├── package-dmg.sh                # DMG packaging
│   └── generate-appicon.sh           # App icon generator
├── third_party/
│   └── libvterm/                     # Vendored terminal emulation library
└── resources/
    └── fonts/
        ├── IoskeleyMono-*.ttf        # Primary UI font family
        └── unifont.otf               # Unicode fallback
```

### Component Diagram

```
┌──────────────────────────────────────────────────────────────────────┐
│                               AppShell                               │
│  ┌───────────────────┐  ┌─────────────────┐  ┌─────────────────────┐ │
│  │ AgentConfigPanel  │  │  TerminalPanel  │  │   MetricsPanel      │ │
│  │ (Claude+OpenCode) │  │  (Multi-tab)    │  │   AgentTokenPanel   │ │
│  └────────┬──────────┘  └────────┬────────┘  └──────────┬──────────┘ │
└───────────┼──────────────────────┼──────────────────────┼────────────┘
            │                      │                      │
            ▼                      ▼                      ▼
┌────────────────────┐  ┌──────────────────┐  ┌─────────────────────────────┐
│  ProfileStore      │  │ SessionController│  │  ClaudeUsageCollector       │
│  (Claude+OpenCode) │  │  ProcessRunner   │  │  OpenCodeUsageCollector     │
│  ConfigManager     │  │  VTerminal       │  │  MultiMetricsStore          │
│  ConfigExporter    │  └────────┬─────────┘  │  AgentTokenStore            │
└─────────┬──────────┘           │            └───────────┬─────────────────┘
          │                      │                        │
          ▼                      ▼                        ▼
    Claude Code config      PTY/fork               Claude Code logs
    ~/.config/opencode/     subprocess             $XDG_DATA_HOME/opencode/storage/
    ~/.config/diana/
```

### Data Flow

```
                    ┌─────────────────┐
                    │   User Input    │
                    └────────┬────────┘
                             │
                             ▼
┌────────────────────────────────────────────────────────────┐
│                     TerminalPanel                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                  │
│  │  Tab 1   │  │  Tab 2   │  │  Tab N   │                  │
│  │ Session  │  │ Session  │  │ Session  │                  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘                  │
└───────┼─────────────┼─────────────┼────────────────────────┘
        │             │             │
        └─────────────┼─────────────┘
                      ▼
            ┌─────────────────┐
            │SessionController│
            └────────┬────────┘
                     │
        ┌────────────┼────────────┐
        ▼            ▼            ▼
┌───────────┐ ┌───────────┐ ┌───────────┐
│ProcessRun │ │ VTerminal │ │ AppAdapter│
│   (PTY)   │ │ (libvterm)│ │  (config) │
└─────┬─────┘ └─────┬─────┘ └─────┬─────┘
      │             │             │
      ▼             ▼             ▼
   Agent         Screen       Config
   Process       Buffer       Files
```

### Metrics Collection Flow

```
Claude Code JSONL logs (projects/transcripts)
$XDG_DATA_HOME/opencode/storage/{project,session,message}/
            │
            ├─────────────────────────────┐
            ▼                             ▼
┌────────────────────────────┐    ┌───────────────────┐
│ ClaudeUsageCollector       │    │ AgentTokenStore   │
│ OpenCodeUsageCollector     │    │ (Claude+OpenCode) │
└───────────┬────────────────┘    └──────────┬────────┘
            │                               │
            ▼                               ▼
┌─────────────────┐                 ┌─────────────────┐
│MultiMetricsStore│                 │ AgentTokenPanel │
└────────┬────────┘                 └─────────────────┘
         │
         ▼
┌─────────────────┐
│ MetricsPanel    │
└─────────────────┘
```

## Testing

The project includes 87 unit tests covering core functionality:

```bash
./diana_tests
```

| Test Suite                          | Tests | Coverage                                          |
| ----------------------------------- | ----- | ------------------------------------------------- |
| EventQueueTest                      | 5     | Thread-safe queue operations                      |
| MetricsStoreTest                    | 5     | Token aggregation, EMA rates                      |
| MultiMetricsStoreTest               | 7     | Per-project storage                               |
| AgentTokenStoreTest                 | 10    | JSONL parsing, session tracking                   |
| ClaudeUsageCollectorTest            | 12    | File watching, incremental parsing                |
| ConfigExporterTest                  | 5     | JSON export/import                                |
| OpenCodeConfigTest                  | 25    | OpenCode JSON serialization, empty field handling |
| OpenCodeProfileTest                 | 1     | Profile serialization                             |
| OpenCodeProfileStoreTest            | 8     | Profile store operations                          |
| OpenCodeProfileStoreIntegrationTest | 6     | CRUD integration tests                            |
| AgentTokenUsageTest                 | 1     | Token calculation                                 |
| DailyTokenDataTest                  | 1     | Date formatting                                   |
| AgentTypeNameTest                   | 1     | Enum to string                                    |

## Roadmap

- [ ] Claude Code Skill/MCP marketplace browser
- [x] OpenCode configuration panel (similar to Claude Code)
- [ ] Codex configuration panel (similar to Claude Code)
- [x] Agent token stats panel with per-agent aggregation
- [x] Per-project token metrics tracking
- [x] GitHub-style activity heatmap
- [ ] Session history and replay
- [ ] Cost estimation and budget alerts

## Dependencies

All dependencies are fetched automatically via CMake FetchContent:

| Dependency                | Version        | Purpose                      |
| ------------------------- | -------------- | ---------------------------- |
| Dear ImGui                | docking branch | Immediate-mode GUI framework |
| GLFW                      | 3.4            | Cross-platform windowing     |
| ImPlot                    | master         | Real-time plotting           |
| nlohmann/json             | 3.11.3         | JSON/JSONL parsing           |
| tomlplusplus              | 3.4.0          | TOML config parsing          |
| nativefiledialog-extended | 1.2.1          | Native file dialogs          |
| GoogleTest                | 1.14.0         | Unit testing (optional)      |

Vendored:

- **libvterm** (in `third_party/libvterm/`) - Terminal emulation

## Font

Diana loads all `.ttf`, `.otf`, and `.ttc` files from `resources/fonts` at startup and merges them into a single ImGui font atlas. `unifont.otf` ([GNU Unifont](https://unifoundry.com/unifont/)) is used as the Unicode fallback, while the first face with `Regular` in its filename (for example `IoskeleyMono-Regular.ttf`) becomes the primary UI font. Add or replace fonts in that folder to customize the UI without code changes.

## License

MIT
