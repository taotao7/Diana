#include <gtest/gtest.h>
#include "adapters/config_exporter.h"

TEST(ConfigExporterTest, AppKindConversion) {
    EXPECT_EQ(agent47::ConfigExporter::app_kind_to_string(agent47::AppKind::ClaudeCode), "claude_code");
    EXPECT_EQ(agent47::ConfigExporter::app_kind_to_string(agent47::AppKind::Codex), "codex");
    EXPECT_EQ(agent47::ConfigExporter::app_kind_to_string(agent47::AppKind::OpenCode), "opencode");
    
    auto claude = agent47::ConfigExporter::string_to_app_kind("claude_code");
    ASSERT_TRUE(claude.has_value());
    EXPECT_EQ(claude.value(), agent47::AppKind::ClaudeCode);
    
    auto codex = agent47::ConfigExporter::string_to_app_kind("codex");
    ASSERT_TRUE(codex.has_value());
    EXPECT_EQ(codex.value(), agent47::AppKind::Codex);
    
    auto unknown = agent47::ConfigExporter::string_to_app_kind("invalid");
    EXPECT_FALSE(unknown.has_value());
}

TEST(ConfigExporterTest, ExportToJson) {
    std::vector<agent47::ExportedConfig> configs;
    
    agent47::ExportedConfig cfg1;
    cfg1.app_name = "claude_code";
    cfg1.config.provider = "anthropic";
    cfg1.config.model = "claude-3-opus";
    configs.push_back(cfg1);
    
    agent47::ExportedConfig cfg2;
    cfg2.app_name = "opencode";
    cfg2.config.provider = "openai";
    cfg2.config.model = "gpt-4";
    configs.push_back(cfg2);
    
    std::string json = agent47::ConfigExporter::export_to_json(configs);
    
    EXPECT_NE(json.find("claude_code"), std::string::npos);
    EXPECT_NE(json.find("anthropic"), std::string::npos);
    EXPECT_NE(json.find("claude-3-opus"), std::string::npos);
    EXPECT_NE(json.find("opencode"), std::string::npos);
    EXPECT_NE(json.find("version"), std::string::npos);
    EXPECT_NE(json.find("exported_at"), std::string::npos);
}

TEST(ConfigExporterTest, ImportFromJson) {
    std::string json = R"({
        "version": "1.0",
        "exported_at": "2024-01-01T00:00:00Z",
        "configs": [
            {"app": "claude_code", "provider": "anthropic", "model": "claude-3-opus"},
            {"app": "codex", "provider": "openai", "model": "gpt-4"}
        ]
    })";
    
    auto bundle = agent47::ConfigExporter::import_from_json(json);
    
    ASSERT_TRUE(bundle.has_value());
    EXPECT_EQ(bundle->version, "1.0");
    EXPECT_EQ(bundle->configs.size(), 2);
    
    EXPECT_EQ(bundle->configs[0].app_name, "claude_code");
    EXPECT_EQ(bundle->configs[0].config.provider, "anthropic");
    EXPECT_EQ(bundle->configs[0].config.model, "claude-3-opus");
    
    EXPECT_EQ(bundle->configs[1].app_name, "codex");
    EXPECT_EQ(bundle->configs[1].config.provider, "openai");
    EXPECT_EQ(bundle->configs[1].config.model, "gpt-4");
}

TEST(ConfigExporterTest, ImportInvalidJson) {
    auto result = agent47::ConfigExporter::import_from_json("invalid json");
    EXPECT_FALSE(result.has_value());
    
    auto result2 = agent47::ConfigExporter::import_from_json("{}");
    EXPECT_FALSE(result2.has_value());
}

TEST(ConfigExporterTest, RoundTrip) {
    std::vector<agent47::ExportedConfig> original;
    
    agent47::ExportedConfig cfg;
    cfg.app_name = "codex";
    cfg.config.provider = "openai";
    cfg.config.model = "gpt-4-turbo";
    original.push_back(cfg);
    
    std::string json = agent47::ConfigExporter::export_to_json(original);
    auto imported = agent47::ConfigExporter::import_from_json(json);
    
    ASSERT_TRUE(imported.has_value());
    ASSERT_EQ(imported->configs.size(), 1);
    EXPECT_EQ(imported->configs[0].app_name, "codex");
    EXPECT_EQ(imported->configs[0].config.provider, "openai");
    EXPECT_EQ(imported->configs[0].config.model, "gpt-4-turbo");
}
