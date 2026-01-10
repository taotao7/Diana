#include <gtest/gtest.h>
#include "metrics/agent_token_store.h"
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

class AgentTokenStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "diana_agent_token_test";
        fs::create_directories(test_dir_ / "projects" / "test-project");
        fs::create_directories(test_dir_ / "transcripts");
    }
    
    void TearDown() override {
        fs::remove_all(test_dir_);
    }
    
    void create_jsonl_file(const fs::path& path, const std::vector<std::string>& lines) {
        fs::create_directories(path.parent_path());
        std::ofstream file(path);
        for (const auto& line : lines) {
            file << line << "\n";
        }
    }
    
    fs::path test_dir_;
};

TEST_F(AgentTokenStoreTest, InitialState) {
    diana::AgentTokenStore store(test_dir_.string());
    
    EXPECT_EQ(store.files_processed(), 0);
    EXPECT_EQ(store.sessions_tracked(), 0);
    
    auto stats = store.get_stats(diana::AgentType::ClaudeCode);
    EXPECT_EQ(stats.total_tokens.total(), 0);
    EXPECT_EQ(stats.session_count, 0);
}

TEST_F(AgentTokenStoreTest, ParseSimpleUsage) {
    create_jsonl_file(
        test_dir_ / "projects" / "test-project" / "session1.jsonl",
        {
            R"({"message": {"usage": {"input_tokens": 100, "output_tokens": 50}}, "costUSD": 0.01})",
            R"({"message": {"usage": {"input_tokens": 200, "output_tokens": 100}}, "costUSD": 0.02})"
        }
    );
    
    diana::AgentTokenStore store(test_dir_.string());
    store.scan_all();
    store.poll();
    
    EXPECT_GE(store.files_processed(), 1);
    
    auto stats = store.get_stats(diana::AgentType::ClaudeCode);
    EXPECT_EQ(stats.total_tokens.input_tokens, 300);
    EXPECT_EQ(stats.total_tokens.output_tokens, 150);
    EXPECT_DOUBLE_EQ(stats.total_tokens.cost_usd, 0.03);
}

TEST_F(AgentTokenStoreTest, ParseCacheTokens) {
    create_jsonl_file(
        test_dir_ / "projects" / "test-project" / "session2.jsonl",
        {
            R"({"message": {"usage": {"input_tokens": 100, "output_tokens": 50, "cache_creation_input_tokens": 1000, "cache_read_input_tokens": 500}}, "costUSD": 0.05})"
        }
    );
    
    diana::AgentTokenStore store(test_dir_.string());
    store.scan_all();
    store.poll();
    
    auto stats = store.get_stats(diana::AgentType::ClaudeCode);
    EXPECT_EQ(stats.total_tokens.input_tokens, 100);
    EXPECT_EQ(stats.total_tokens.output_tokens, 50);
    EXPECT_EQ(stats.total_tokens.cache_creation_tokens, 1000);
    EXPECT_EQ(stats.total_tokens.cache_read_tokens, 500);
    EXPECT_EQ(stats.total_tokens.total(), 1650);
}

TEST_F(AgentTokenStoreTest, SessionTracking) {
    create_jsonl_file(
        test_dir_ / "projects" / "proj1" / "ses_abc123.jsonl",
        {R"({"sessionId": "ses_abc123", "message": {"usage": {"input_tokens": 100, "output_tokens": 50}}})"}
    );
    create_jsonl_file(
        test_dir_ / "projects" / "proj2" / "ses_def456.jsonl",
        {R"({"sessionId": "ses_def456", "message": {"usage": {"input_tokens": 200, "output_tokens": 100}}})"}
    );
    
    diana::AgentTokenStore store(test_dir_.string());
    store.scan_all();
    store.poll();
    
    auto sessions = store.get_sessions(diana::AgentType::ClaudeCode);
    EXPECT_EQ(sessions.size(), 2);
    
    bool found_abc = false, found_def = false;
    for (const auto& s : sessions) {
        if (s.session_id == "ses_abc123") {
            found_abc = true;
            EXPECT_EQ(s.tokens.input_tokens, 100);
        }
        if (s.session_id == "ses_def456") {
            found_def = true;
            EXPECT_EQ(s.tokens.input_tokens, 200);
        }
    }
    EXPECT_TRUE(found_abc);
    EXPECT_TRUE(found_def);
}

TEST_F(AgentTokenStoreTest, SubagentDetection) {
    fs::create_directories(test_dir_ / "projects" / "proj" / "subagents");
    create_jsonl_file(
        test_dir_ / "projects" / "proj" / "subagents" / "agent-001.jsonl",
        {R"({"message": {"usage": {"input_tokens": 50, "output_tokens": 25}}})"}
    );
    
    diana::AgentTokenStore store(test_dir_.string());
    store.scan_all();
    store.poll();
    
    auto sessions = store.get_sessions(diana::AgentType::ClaudeCode);
    ASSERT_GE(sessions.size(), 1);
    
    bool found_subagent = false;
    for (const auto& s : sessions) {
        if (s.is_subagent) {
            found_subagent = true;
            EXPECT_EQ(s.tokens.input_tokens, 50);
        }
    }
    EXPECT_TRUE(found_subagent);
}

TEST_F(AgentTokenStoreTest, GetTotalUsage) {
    create_jsonl_file(
        test_dir_ / "projects" / "p1" / "s1.jsonl",
        {R"({"message": {"usage": {"input_tokens": 100, "output_tokens": 50}}})"}
    );
    create_jsonl_file(
        test_dir_ / "projects" / "p2" / "s2.jsonl",
        {R"({"message": {"usage": {"input_tokens": 200, "output_tokens": 100}}})"}
    );
    
    diana::AgentTokenStore store(test_dir_.string());
    store.scan_all();
    store.poll();
    
    auto total = store.get_total_usage();
    EXPECT_EQ(total.input_tokens, 300);
    EXPECT_EQ(total.output_tokens, 150);
}

TEST_F(AgentTokenStoreTest, Clear) {
    create_jsonl_file(
        test_dir_ / "projects" / "p" / "s.jsonl",
        {R"({"message": {"usage": {"input_tokens": 100, "output_tokens": 50}}})"}
    );
    
    diana::AgentTokenStore store(test_dir_.string());
    store.scan_all();
    store.poll();
    
    EXPECT_GE(store.sessions_tracked(), 1);
    EXPECT_GE(store.files_processed(), 1);
    
    store.clear();
    
    EXPECT_EQ(store.sessions_tracked(), 0);
    EXPECT_EQ(store.files_processed(), 0);
}

TEST_F(AgentTokenStoreTest, DailyDataGeneration) {
    diana::AgentTokenStore store(test_dir_.string());
    
    auto daily = store.get_daily_data(diana::AgentType::ClaudeCode);
    EXPECT_EQ(daily.size(), 365);
    
    for (const auto& d : daily) {
        EXPECT_GE(d.year, 2025);
        EXPECT_GE(d.month, 1);
        EXPECT_LE(d.month, 12);
        EXPECT_GE(d.day, 1);
        EXPECT_LE(d.day, 31);
        EXPECT_GE(d.weekday, 0);
        EXPECT_LE(d.weekday, 6);
    }
}

TEST_F(AgentTokenStoreTest, MultipleEntriesInFile) {
    auto file_path = test_dir_ / "projects" / "p" / "multi.jsonl";
    create_jsonl_file(file_path, {
        R"({"message": {"usage": {"input_tokens": 100, "output_tokens": 50}}})",
        R"({"message": {"usage": {"input_tokens": 200, "output_tokens": 100}}})",
        R"({"message": {"usage": {"input_tokens": 300, "output_tokens": 150}}})"
    });
    
    diana::AgentTokenStore store(test_dir_.string());
    store.scan_all();
    store.poll();
    
    auto stats = store.get_stats(diana::AgentType::ClaudeCode);
    EXPECT_EQ(stats.total_tokens.input_tokens, 600);
    EXPECT_EQ(stats.total_tokens.output_tokens, 300);
}

TEST_F(AgentTokenStoreTest, InvalidJsonIgnored) {
    create_jsonl_file(
        test_dir_ / "projects" / "p" / "invalid.jsonl",
        {
            "not valid json",
            R"({"message": {"usage": {"input_tokens": 100, "output_tokens": 50}}})",
            "{incomplete",
            R"({"message": {"usage": {"input_tokens": 200, "output_tokens": 100}}})"
        }
    );
    
    diana::AgentTokenStore store(test_dir_.string());
    store.scan_all();
    store.poll();
    
    auto stats = store.get_stats(diana::AgentType::ClaudeCode);
    EXPECT_EQ(stats.total_tokens.input_tokens, 300);
    EXPECT_EQ(stats.total_tokens.output_tokens, 150);
}

TEST(AgentTokenUsageTest, Total) {
    diana::AgentTokenUsage usage;
    usage.input_tokens = 100;
    usage.output_tokens = 50;
    usage.cache_creation_tokens = 1000;
    usage.cache_read_tokens = 500;
    
    EXPECT_EQ(usage.total(), 1650);
}

TEST(DailyTokenDataTest, DateKey) {
    diana::DailyTokenData data;
    data.year = 2025;
    data.month = 1;
    data.day = 5;
    
    EXPECT_EQ(data.date_key(), "2025-01-05");
    
    data.month = 12;
    data.day = 25;
    EXPECT_EQ(data.date_key(), "2025-12-25");
}

TEST(AgentTypeNameTest, Names) {
    EXPECT_STREQ(diana::agent_type_name(diana::AgentType::ClaudeCode), "Claude Code");
    EXPECT_STREQ(diana::agent_type_name(diana::AgentType::Codex), "Codex");
    EXPECT_STREQ(diana::agent_type_name(diana::AgentType::OpenCode), "OpenCode");
    EXPECT_STREQ(diana::agent_type_name(diana::AgentType::Unknown), "Unknown");
}
