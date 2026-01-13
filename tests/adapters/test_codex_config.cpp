#include <gtest/gtest.h>
#include "adapters/codex_config.h"
#include <sstream>

using namespace diana;

TEST(CodexConfigTest, BasicTomlRoundtrip) {
    CodexConfig config;
    config.model = "gpt-5-codex";
    config.model_provider = "openai";
    config.approval_policy = "on-request";
    config.sandbox_mode = "workspace-write";
    
    toml::table tbl = config.to_toml();
    CodexConfig restored = CodexConfig::from_toml(tbl);
    
    EXPECT_EQ(restored.model, "gpt-5-codex");
    EXPECT_EQ(restored.model_provider, "openai");
    EXPECT_EQ(restored.approval_policy, "on-request");
    EXPECT_EQ(restored.sandbox_mode, "workspace-write");
}

TEST(CodexConfigTest, FeaturesRoundtrip) {
    CodexConfig config;
    config.features.apply_patch_freeform = true;
    config.features.exec_policy = false;
    config.features.web_search_request = true;
    
    toml::table tbl = config.to_toml();
    CodexConfig restored = CodexConfig::from_toml(tbl);
    
    EXPECT_TRUE(restored.features.apply_patch_freeform);
    EXPECT_FALSE(restored.features.exec_policy);
    EXPECT_TRUE(restored.features.web_search_request);
}

TEST(CodexConfigTest, ModelProvidersRoundtrip) {
    CodexConfig config;
    
    CodexModelProvider provider;
    provider.name = "OpenAI";
    provider.base_url = "https://api.openai.com/v1";
    provider.env_key = "OPENAI_API_KEY";
    provider.wire_api = "chat";
    provider.requires_openai_auth = true;
    
    config.model_providers["openai"] = provider;
    
    toml::table tbl = config.to_toml();
    CodexConfig restored = CodexConfig::from_toml(tbl);
    
    ASSERT_EQ(restored.model_providers.size(), 1);
    ASSERT_TRUE(restored.model_providers.find("openai") != restored.model_providers.end());
    
    const auto& restored_provider = restored.model_providers.at("openai");
    EXPECT_EQ(restored_provider.name, "OpenAI");
    EXPECT_EQ(restored_provider.base_url, "https://api.openai.com/v1");
    EXPECT_EQ(restored_provider.env_key, "OPENAI_API_KEY");
    EXPECT_EQ(restored_provider.wire_api, "chat");
    EXPECT_TRUE(restored_provider.requires_openai_auth);
}

TEST(CodexConfigTest, McpServersRoundtrip) {
    CodexConfig config;
    
    CodexMcpServer server;
    server.command = "npx";
    server.args = {"-y", "@modelcontextprotocol/server-filesystem"};
    server.cwd = "/tmp";
    server.enabled = true;
    
    config.mcp_servers["filesystem"] = server;
    
    toml::table tbl = config.to_toml();
    CodexConfig restored = CodexConfig::from_toml(tbl);
    
    ASSERT_EQ(restored.mcp_servers.size(), 1);
    ASSERT_TRUE(restored.mcp_servers.find("filesystem") != restored.mcp_servers.end());
    
    const auto& restored_server = restored.mcp_servers.at("filesystem");
    EXPECT_EQ(restored_server.command, "npx");
    ASSERT_EQ(restored_server.args.size(), 2);
    EXPECT_EQ(restored_server.args[0], "-y");
    EXPECT_EQ(restored_server.args[1], "@modelcontextprotocol/server-filesystem");
    EXPECT_EQ(restored_server.cwd, "/tmp");
    EXPECT_TRUE(restored_server.enabled);
}

TEST(CodexConfigTest, TuiConfigRoundtrip) {
    CodexConfig config;
    config.tui.animations = false;
    config.tui.scroll_mode = "trackpad";
    config.tui.scroll_invert = true;
    config.tui.show_tooltips = false;
    
    toml::table tbl = config.to_toml();
    CodexConfig restored = CodexConfig::from_toml(tbl);
    
    EXPECT_FALSE(restored.tui.animations);
    EXPECT_EQ(restored.tui.scroll_mode, "trackpad");
    EXPECT_TRUE(restored.tui.scroll_invert);
    EXPECT_FALSE(restored.tui.show_tooltips);
}

TEST(CodexConfigTest, HistoryConfigRoundtrip) {
    CodexConfig config;
    config.history.max_bytes = 10485760;
    config.history.persistence = "none";
    
    toml::table tbl = config.to_toml();
    CodexConfig restored = CodexConfig::from_toml(tbl);
    
    ASSERT_TRUE(restored.history.max_bytes.has_value());
    EXPECT_EQ(*restored.history.max_bytes, 10485760);
    EXPECT_EQ(restored.history.persistence, "none");
}

TEST(CodexConfigTest, ToolsConfigRoundtrip) {
    CodexConfig config;
    config.tools.web_search = true;
    
    toml::table tbl = config.to_toml();
    CodexConfig restored = CodexConfig::from_toml(tbl);
    
    EXPECT_TRUE(restored.tools.web_search);
}

TEST(CodexConfigTest, SandboxConfigRoundtrip) {
    CodexConfig config;
    config.sandbox_workspace_write.exclude_slash_tmp = true;
    config.sandbox_workspace_write.network_access = true;
    config.sandbox_workspace_write.writable_roots = {"/home/user/workspace", "/tmp"};
    
    toml::table tbl = config.to_toml();
    CodexConfig restored = CodexConfig::from_toml(tbl);
    
    EXPECT_TRUE(restored.sandbox_workspace_write.exclude_slash_tmp);
    EXPECT_TRUE(restored.sandbox_workspace_write.network_access);
    ASSERT_EQ(restored.sandbox_workspace_write.writable_roots.size(), 2);
    EXPECT_EQ(restored.sandbox_workspace_write.writable_roots[0], "/home/user/workspace");
    EXPECT_EQ(restored.sandbox_workspace_write.writable_roots[1], "/tmp");
}

TEST(CodexProfileTest, JsonSerialization) {
    CodexProfile profile;
    profile.name = "test-profile";
    profile.description = "A test profile";
    profile.config.model = "gpt-5-codex";
    profile.created_at = 1234567890;
    profile.updated_at = 1234567900;
    
    nlohmann::json j;
    to_json(j, profile);
    
    CodexProfile restored;
    from_json(j, restored);
    
    EXPECT_EQ(restored.name, "test-profile");
    EXPECT_EQ(restored.description, "A test profile");
    EXPECT_EQ(restored.config.model, "gpt-5-codex");
    EXPECT_EQ(restored.created_at, 1234567890);
    EXPECT_EQ(restored.updated_at, 1234567900);
}
