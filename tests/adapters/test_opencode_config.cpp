#include <gtest/gtest.h>
#include "adapters/opencode_config.h"

using namespace diana;

TEST(OpenCodeConfigTest, FromJsonBasicFields) {
    nlohmann::json j = {
        {"model", "anthropic/claude-sonnet-4-20250514"},
        {"small_model", "anthropic/claude-haiku-4-5"},
        {"theme", "catppuccin"},
        {"default_agent", "build"}
    };
    
    auto config = OpenCodeConfig::from_json(j);
    
    EXPECT_EQ(config.model, "anthropic/claude-sonnet-4-20250514");
    EXPECT_EQ(config.small_model, "anthropic/claude-haiku-4-5");
    EXPECT_EQ(config.theme, "catppuccin");
    EXPECT_EQ(config.default_agent, "build");
}

TEST(OpenCodeConfigTest, ToJsonBasicFields) {
    OpenCodeConfig config;
    config.model = "openai/gpt-4o";
    config.small_model = "openai/gpt-4o-mini";
    config.theme = "opencode";
    config.default_agent = "plan";
    
    auto j = config.to_json();
    
    EXPECT_EQ(j["model"], "openai/gpt-4o");
    EXPECT_EQ(j["small_model"], "openai/gpt-4o-mini");
    EXPECT_EQ(j["theme"], "opencode");
    EXPECT_EQ(j["default_agent"], "plan");
    EXPECT_EQ(j["$schema"], "https://opencode.ai/config.json");
}

TEST(OpenCodeConfigTest, ProviderConfig) {
    nlohmann::json j = {
        {"provider", {
            {"anthropic", {
                {"apiKey", "{env:ANTHROPIC_API_KEY}"},
                {"baseURL", "https://api.anthropic.com"}
            }},
            {"openai", {
                {"apiKey", "sk-test"}
            }}
        }}
    };
    
    auto config = OpenCodeConfig::from_json(j);
    
    ASSERT_EQ(config.providers.size(), 2);
    EXPECT_EQ(config.providers["anthropic"].api_key, "{env:ANTHROPIC_API_KEY}");
    EXPECT_EQ(config.providers["anthropic"].base_url, "https://api.anthropic.com");
    EXPECT_EQ(config.providers["openai"].api_key, "sk-test");
}

TEST(OpenCodeConfigTest, AgentConfig) {
    nlohmann::json j = {
        {"agent", {
            {"code-reviewer", {
                {"model", "anthropic/claude-sonnet-4-5"},
                {"description", "Reviews code"},
                {"prompt", "You are a code reviewer."},
                {"disabled", false},
                {"tools", {{"write", false}, {"edit", false}}}
            }}
        }}
    };
    
    auto config = OpenCodeConfig::from_json(j);
    
    ASSERT_EQ(config.agents.size(), 1);
    auto& agent = config.agents["code-reviewer"];
    EXPECT_EQ(agent.model, "anthropic/claude-sonnet-4-5");
    EXPECT_EQ(agent.description, "Reviews code");
    EXPECT_EQ(agent.prompt, "You are a code reviewer.");
    EXPECT_FALSE(agent.disabled);
    EXPECT_EQ(agent.tools.size(), 2);
    EXPECT_FALSE(agent.tools["write"]);
}

TEST(OpenCodeConfigTest, ToolsConfig) {
    nlohmann::json j = {
        {"tools", {
            {"write", false},
            {"bash", false},
            {"read", true}
        }}
    };
    
    auto config = OpenCodeConfig::from_json(j);
    
    ASSERT_EQ(config.tools.size(), 3);
    EXPECT_FALSE(config.tools["write"]);
    EXPECT_FALSE(config.tools["bash"]);
    EXPECT_TRUE(config.tools["read"]);
}

TEST(OpenCodeConfigTest, PermissionTools) {
    nlohmann::json j = {
        {"permission", {
            {"edit", "ask"},
            {"bash", "ask"}
        }}
    };
    
    auto config = OpenCodeConfig::from_json(j);
    
    EXPECT_EQ(config.permission_tools["edit"], "ask");
    EXPECT_EQ(config.permission_tools["bash"], "ask");
}

TEST(OpenCodeConfigTest, InstructionsArray) {
    nlohmann::json j = {
        {"instructions", {"CONTRIBUTING.md", "docs/guidelines.md"}}
    };
    
    auto config = OpenCodeConfig::from_json(j);
    
    ASSERT_EQ(config.instructions.size(), 2);
    EXPECT_EQ(config.instructions[0], "CONTRIBUTING.md");
    EXPECT_EQ(config.instructions[1], "docs/guidelines.md");
}

TEST(OpenCodeConfigTest, InstructionsSingleString) {
    nlohmann::json j = {
        {"instructions", "README.md"}
    };
    
    auto config = OpenCodeConfig::from_json(j);
    
    ASSERT_EQ(config.instructions.size(), 1);
    EXPECT_EQ(config.instructions[0], "README.md");
}

TEST(OpenCodeConfigTest, TuiConfig) {
    nlohmann::json j = {
        {"tui", {
            {"scroll_speed", 3},
            {"scroll_acceleration", {{"enabled", true}}},
            {"diff_style", "stacked"}
        }}
    };
    
    auto config = OpenCodeConfig::from_json(j);
    
    EXPECT_EQ(config.tui.scroll_speed, 3);
    EXPECT_TRUE(config.tui.scroll_acceleration);
    EXPECT_EQ(config.tui.diff_style, "stacked");
}

TEST(OpenCodeConfigTest, CompactionConfig) {
    nlohmann::json j = {
        {"compaction", {
            {"auto", false},
            {"prune", false}
        }}
    };
    
    auto config = OpenCodeConfig::from_json(j);
    
    EXPECT_FALSE(config.compaction.auto_compact);
    EXPECT_FALSE(config.compaction.prune);
}

TEST(OpenCodeConfigTest, McpServers) {
    nlohmann::json j = {
        {"mcp", {
            {"filesystem", {
                {"type", "local"},
                {"command", "npx"},
                {"args", {"-y", "@modelcontextprotocol/server-filesystem"}},
                {"enabled", true}
            }},
            {"jira", {
                {"type", "remote"},
                {"url", "https://jira.example.com/mcp"},
                {"enabled", false}
            }}
        }}
    };
    
    auto config = OpenCodeConfig::from_json(j);
    
    ASSERT_EQ(config.mcp.size(), 2);
    
    auto& fs = config.mcp["filesystem"];
    EXPECT_EQ(fs.type, "local");
    EXPECT_EQ(fs.command, "npx");
    ASSERT_EQ(fs.args.size(), 2);
    EXPECT_TRUE(fs.enabled);
    
    auto& jira = config.mcp["jira"];
    EXPECT_EQ(jira.type, "remote");
    EXPECT_EQ(jira.url, "https://jira.example.com/mcp");
    EXPECT_FALSE(jira.enabled);
}

TEST(OpenCodeConfigTest, WatcherIgnore) {
    nlohmann::json j = {
        {"watcher", {
            {"ignore", {"node_modules/**", "dist/**", ".git/**"}}
        }}
    };
    
    auto config = OpenCodeConfig::from_json(j);
    
    ASSERT_EQ(config.watcher_ignore.size(), 3);
    EXPECT_EQ(config.watcher_ignore[0], "node_modules/**");
}

TEST(OpenCodeConfigTest, ShareModes) {
    nlohmann::json j1 = {{"share", "auto"}};
    auto config1 = OpenCodeConfig::from_json(j1);
    EXPECT_EQ(config1.share, "auto");
    
    nlohmann::json j2 = {{"share", "disabled"}};
    auto config2 = OpenCodeConfig::from_json(j2);
    EXPECT_EQ(config2.share, "disabled");
}

TEST(OpenCodeConfigTest, AutoupdateModes) {
    nlohmann::json j1 = {{"autoupdate", true}};
    auto config1 = OpenCodeConfig::from_json(j1);
    EXPECT_EQ(config1.autoupdate, "true");
    
    nlohmann::json j2 = {{"autoupdate", false}};
    auto config2 = OpenCodeConfig::from_json(j2);
    EXPECT_EQ(config2.autoupdate, "false");
    
    nlohmann::json j3 = {{"autoupdate", "notify"}};
    auto config3 = OpenCodeConfig::from_json(j3);
    EXPECT_EQ(config3.autoupdate, "notify");
}

TEST(OpenCodeConfigTest, ProviderFilters) {
    nlohmann::json j = {
        {"disabled_providers", {"openai", "google"}},
        {"enabled_providers", {"anthropic"}}
    };
    
    auto config = OpenCodeConfig::from_json(j);
    
    ASSERT_EQ(config.disabled_providers.size(), 2);
    EXPECT_EQ(config.disabled_providers[0], "openai");
    ASSERT_EQ(config.enabled_providers.size(), 1);
    EXPECT_EQ(config.enabled_providers[0], "anthropic");
}

TEST(OpenCodeConfigTest, RoundTrip) {
    OpenCodeConfig original;
    original.model = "anthropic/claude-sonnet-4-20250514";
    original.small_model = "anthropic/claude-haiku-4-5";
    original.theme = "opencode";
    original.default_agent = "build";
    original.share = "auto";
    original.autoupdate = "notify";
    
    OpenCodeProviderConfig provider;
    provider.id = "anthropic";
    provider.api_key = "{env:ANTHROPIC_API_KEY}";
    original.providers["anthropic"] = provider;
    
    original.tools["bash"] = false;
    original.permission_tools["edit"] = "ask";
    original.instructions = {"README.md", "CONTRIBUTING.md"};
    original.disabled_providers = {"openai"};
    
    auto j = original.to_json();
    auto restored = OpenCodeConfig::from_json(j);
    
    EXPECT_EQ(restored.model, original.model);
    EXPECT_EQ(restored.small_model, original.small_model);
    EXPECT_EQ(restored.theme, original.theme);
    EXPECT_EQ(restored.default_agent, original.default_agent);
    EXPECT_EQ(restored.share, original.share);
    EXPECT_EQ(restored.autoupdate, original.autoupdate);
    EXPECT_EQ(restored.providers["anthropic"].api_key, "{env:ANTHROPIC_API_KEY}");
    EXPECT_FALSE(restored.tools["bash"]);
    EXPECT_EQ(restored.permission_tools["edit"], "ask");
    EXPECT_EQ(restored.instructions.size(), 2);
    EXPECT_EQ(restored.disabled_providers.size(), 1);
}

TEST(OpenCodeConfigTest, ExtraFieldsPreserved) {
    nlohmann::json j = {
        {"model", "anthropic/claude-sonnet-4-20250514"},
        {"custom_field", "custom_value"},
        {"another_unknown", 42}
    };
    
    auto config = OpenCodeConfig::from_json(j);
    auto output = config.to_json();
    
    EXPECT_EQ(output["custom_field"], "custom_value");
    EXPECT_EQ(output["another_unknown"], 42);
}

TEST(OpenCodeConfigTest, ProviderModelsWithComplexStructure) {
    nlohmann::json j = {
        {"$schema", "https://opencode.ai/config.json"},
        {"plugin", {"oh-my-opencode", "opencode-antigravity-auth@beta"}},
        {"provider", {
            {"google", {
                {"models", {
                    {"antigravity-gemini-3-pro", {
                        {"name", "Gemini 3 Pro (Antigravity)"},
                        {"limit", {{"context", 1048576}, {"output", 65535}}},
                        {"modalities", {
                            {"input", {"text", "image", "pdf"}},
                            {"output", {"text"}}
                        }},
                        {"variants", {
                            {"low", {{"thinkingLevel", "low"}}},
                            {"high", {{"thinkingLevel", "high"}}}
                        }}
                    }}
                }}
            }}
        }}
    };
    
    auto config = OpenCodeConfig::from_json(j);
    auto output = config.to_json();
    
    ASSERT_TRUE(output.contains("plugin"));
    ASSERT_EQ(output["plugin"].size(), 2);
    EXPECT_EQ(output["plugin"][0], "oh-my-opencode");
    
    ASSERT_TRUE(output.contains("provider"));
    ASSERT_TRUE(output["provider"].contains("google"));
    ASSERT_TRUE(output["provider"]["google"].contains("models"));
    
    auto& models = output["provider"]["google"]["models"];
    ASSERT_TRUE(models.contains("antigravity-gemini-3-pro"));
    
    auto& model = models["antigravity-gemini-3-pro"];
    EXPECT_EQ(model["name"], "Gemini 3 Pro (Antigravity)");
    EXPECT_EQ(model["limit"]["context"], 1048576);
    EXPECT_EQ(model["limit"]["output"], 65535);
    
    ASSERT_TRUE(model.contains("modalities"));
    EXPECT_EQ(model["modalities"]["input"].size(), 3);
    EXPECT_EQ(model["modalities"]["output"][0], "text");
    
    ASSERT_TRUE(model.contains("variants"));
    EXPECT_EQ(model["variants"]["low"]["thinkingLevel"], "low");
    EXPECT_EQ(model["variants"]["high"]["thinkingLevel"], "high");
}

TEST(OpenCodeProfileTest, Serialization) {
    OpenCodeProfile profile;
    profile.name = "test-profile";
    profile.description = "A test profile";
    profile.config.model = "anthropic/claude-sonnet-4-20250514";
    profile.created_at = 1234567890;
    profile.updated_at = 1234567900;
    
    nlohmann::json j;
    to_json(j, profile);
    
    OpenCodeProfile restored;
    from_json(j, restored);
    
    EXPECT_EQ(restored.name, "test-profile");
    EXPECT_EQ(restored.description, "A test profile");
    EXPECT_EQ(restored.config.model, "anthropic/claude-sonnet-4-20250514");
    EXPECT_EQ(restored.created_at, 1234567890);
    EXPECT_EQ(restored.updated_at, 1234567900);
}

TEST(OpenCodeConfigTest, EmptyFieldsNotWritten) {
    OpenCodeConfig config;
    config.model = "test-model";
    
    auto j = config.to_json();
    
    EXPECT_TRUE(j.contains("model"));
    EXPECT_FALSE(j.contains("small_model"));
    EXPECT_FALSE(j.contains("theme"));
    EXPECT_FALSE(j.contains("default_agent"));
    EXPECT_FALSE(j.contains("provider"));
    EXPECT_FALSE(j.contains("agent"));
    EXPECT_FALSE(j.contains("permission"));
    EXPECT_FALSE(j.contains("tools"));
    EXPECT_FALSE(j.contains("instructions"));
    EXPECT_FALSE(j.contains("tui"));
    EXPECT_FALSE(j.contains("compaction"));
    EXPECT_FALSE(j.contains("watcher"));
    EXPECT_FALSE(j.contains("mcp"));
    EXPECT_FALSE(j.contains("share"));
    EXPECT_FALSE(j.contains("autoupdate"));
    EXPECT_FALSE(j.contains("disabled_providers"));
    EXPECT_FALSE(j.contains("enabled_providers"));
}

TEST(OpenCodeConfigTest, EmptyProviderNotWritten) {
    OpenCodeConfig config;
    config.model = "test-model";
    
    OpenCodeProviderConfig empty_provider;
    empty_provider.id = "empty";
    config.providers["empty"] = empty_provider;
    
    auto j = config.to_json();
    
    EXPECT_FALSE(j.contains("provider"));
}

TEST(OpenCodeConfigTest, EmptyAgentNotWritten) {
    OpenCodeConfig config;
    config.model = "test-model";
    
    OpenCodeAgentConfig empty_agent;
    config.agents["empty"] = empty_agent;
    
    auto j = config.to_json();
    
    EXPECT_FALSE(j.contains("agent"));
}

TEST(OpenCodeConfigTest, DefaultPermissionsNotWritten) {
    OpenCodeConfig config;
    config.model = "test-model";
    
    auto j = config.to_json();
    
    EXPECT_FALSE(j.contains("permissions"));
}

TEST(OpenCodeConfigTest, NonDefaultPermissionsWritten) {
    OpenCodePermissions p;
    p.auto_approve_writes = true;
    
    nlohmann::json j;
    to_json(j, p);
    
    EXPECT_TRUE(j.contains("autoApproveWrites"));
    EXPECT_TRUE(j["autoApproveWrites"].get<bool>());
    EXPECT_FALSE(j.contains("autoApproveReads"));
    EXPECT_FALSE(j.contains("autoApproveCommands"));
}

TEST(OpenCodeConfigTest, DefaultCompactionNotWritten) {
    OpenCodeCompactionConfig c;
    
    nlohmann::json j;
    to_json(j, c);
    
    EXPECT_TRUE(j.empty());
}

TEST(OpenCodeConfigTest, NonDefaultCompactionWritten) {
    OpenCodeCompactionConfig c;
    c.auto_compact = false;
    c.prune = false;
    
    nlohmann::json j;
    to_json(j, c);
    
    EXPECT_TRUE(j.contains("auto"));
    EXPECT_TRUE(j.contains("prune"));
    EXPECT_FALSE(j["auto"].get<bool>());
    EXPECT_FALSE(j["prune"].get<bool>());
}
