#include <gtest/gtest.h>
#include "metrics/claude_usage_collector.h"
#include "metrics/metrics_store.h"
#include "metrics/multi_metrics_store.h"
#include <fstream>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

class ClaudeUsageCollectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = fs::temp_directory_path() / "diana_collector_test";
        fs::create_directories(test_dir_ / "projects" / "Users-test-workspace-myproject");
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

TEST_F(ClaudeUsageCollectorTest, InitialState) {
    diana::ClaudeUsageCollector collector(test_dir_.string());
    collector.wait_for_init();
    
    EXPECT_EQ(collector.files_processed(), 0);
    EXPECT_EQ(collector.entries_parsed(), 0);
    EXPECT_TRUE(collector.watched_files().empty());
}

TEST_F(ClaudeUsageCollectorTest, PollWithoutStoreDoesNothing) {
    create_jsonl_file(
        test_dir_ / "projects" / "test" / "session.jsonl",
        {R"({"message": {"usage": {"input_tokens": 100, "output_tokens": 50}}})"}
    );
    
    diana::ClaudeUsageCollector collector(test_dir_.string());
    collector.wait_for_init();
    collector.poll();
    
    EXPECT_EQ(collector.entries_parsed(), 1);
}

TEST_F(ClaudeUsageCollectorTest, CollectToMetricsStore) {
    create_jsonl_file(
        test_dir_ / "projects" / "Users-test-workspace-myproject" / "session.jsonl",
        {
            R"({"message": {"usage": {"input_tokens": 100, "output_tokens": 50}}, "costUSD": 0.01})",
            R"({"message": {"usage": {"input_tokens": 200, "output_tokens": 100}}, "costUSD": 0.02})"
        }
    );
    
    diana::MetricsStore store;
    diana::ClaudeUsageCollector collector(test_dir_.string());
    collector.set_metrics_store(&store);
    collector.wait_for_init();
    collector.poll();
    
    EXPECT_GE(collector.files_processed(), 1);
    EXPECT_EQ(collector.entries_parsed(), 2);
    
    auto stats = store.compute_stats();
    EXPECT_EQ(stats.total_input, 300);
    EXPECT_EQ(stats.total_output, 150);
    EXPECT_DOUBLE_EQ(stats.total_cost, 0.03);
}

TEST_F(ClaudeUsageCollectorTest, CollectToMultiStore) {
    create_jsonl_file(
        test_dir_ / "projects" / "Users-test-workspace-projectA" / "session.jsonl",
        {R"({"message": {"usage": {"input_tokens": 100, "output_tokens": 50}}})"}
    );
    create_jsonl_file(
        test_dir_ / "projects" / "Users-test-workspace-projectB" / "session.jsonl",
        {R"({"message": {"usage": {"input_tokens": 200, "output_tokens": 100}}})"}
    );
    
    diana::MultiMetricsStore hub;
    diana::ClaudeUsageCollector collector(test_dir_.string());
    collector.set_multi_store(&hub);
    collector.wait_for_init();
    collector.poll();
    
    auto sources = hub.list_sources();
    EXPECT_EQ(sources.size(), 2);
}

TEST_F(ClaudeUsageCollectorTest, ParseCacheTokens) {
    create_jsonl_file(
        test_dir_ / "projects" / "test" / "session.jsonl",
        {R"({"message": {"usage": {"input_tokens": 100, "output_tokens": 50, "cache_creation_input_tokens": 500, "cache_read_input_tokens": 200}}})"}
    );
    
    diana::MetricsStore store;
    diana::ClaudeUsageCollector collector(test_dir_.string());
    collector.set_metrics_store(&store);
    collector.wait_for_init();
    collector.poll();
    
    auto stats = store.compute_stats();
    EXPECT_EQ(stats.total_input, 800);
    EXPECT_EQ(stats.total_output, 50);
}

TEST_F(ClaudeUsageCollectorTest, IncrementalParsing) {
    auto file_path = test_dir_ / "projects" / "test" / "incremental.jsonl";
    create_jsonl_file(file_path, {
        R"({"message": {"usage": {"input_tokens": 100, "output_tokens": 50}}})"
    });
    
    diana::MetricsStore store;
    diana::ClaudeUsageCollector collector(test_dir_.string());
    collector.set_metrics_store(&store);
    collector.wait_for_init();
    collector.poll();
    EXPECT_EQ(collector.entries_parsed(), 1);
    
    {
        std::ofstream file(file_path, std::ios::app);
        file << R"({"message": {"usage": {"input_tokens": 200, "output_tokens": 100}}})" << "\n";
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(550));
    collector.poll();
    EXPECT_EQ(collector.entries_parsed(), 2);
    
    auto stats = store.compute_stats();
    EXPECT_EQ(stats.total_input, 300);
}

TEST_F(ClaudeUsageCollectorTest, TranscriptsDirectory) {
    create_jsonl_file(
        test_dir_ / "transcripts" / "session.jsonl",
        {R"({"message": {"usage": {"input_tokens": 100, "output_tokens": 50}}})"}
    );
    
    diana::MultiMetricsStore hub;
    diana::ClaudeUsageCollector collector(test_dir_.string());
    collector.set_multi_store(&hub);
    collector.wait_for_init();
    collector.poll();
    
    auto sources = hub.list_sources();
    EXPECT_GE(sources.size(), 1);
}

TEST_F(ClaudeUsageCollectorTest, InvalidJsonIgnored) {
    create_jsonl_file(
        test_dir_ / "projects" / "test" / "session.jsonl",
        {
            "invalid json",
            R"({"message": {"usage": {"input_tokens": 100, "output_tokens": 50}}})",
            "{}",
            R"({"message": {"usage": {"input_tokens": 200, "output_tokens": 100}}})"
        }
    );
    
    diana::MetricsStore store;
    diana::ClaudeUsageCollector collector(test_dir_.string());
    collector.set_metrics_store(&store);
    collector.wait_for_init();
    collector.poll();
    
    EXPECT_EQ(collector.entries_parsed(), 2);
    
    auto stats = store.compute_stats();
    EXPECT_EQ(stats.total_input, 300);
}

TEST_F(ClaudeUsageCollectorTest, AlternativeUsageFormat) {
    create_jsonl_file(
        test_dir_ / "projects" / "test" / "session.jsonl",
        {R"({"usage": {"input_tokens": 150, "output_tokens": 75}, "costUSD": 0.015})"}
    );
    
    diana::MetricsStore store;
    diana::ClaudeUsageCollector collector(test_dir_.string());
    collector.set_metrics_store(&store);
    collector.wait_for_init();
    collector.poll();
    
    auto stats = store.compute_stats();
    EXPECT_EQ(stats.total_input, 150);
    EXPECT_EQ(stats.total_output, 75);
}

TEST_F(ClaudeUsageCollectorTest, WatchedFilesTracking) {
    create_jsonl_file(
        test_dir_ / "projects" / "p1" / "s1.jsonl",
        {R"({"message": {"usage": {"input_tokens": 100, "output_tokens": 50}}})"}
    );
    create_jsonl_file(
        test_dir_ / "projects" / "p2" / "s2.jsonl",
        {R"({"message": {"usage": {"input_tokens": 200, "output_tokens": 100}}})"}
    );
    
    diana::MetricsStore store;
    diana::ClaudeUsageCollector collector(test_dir_.string());
    collector.set_metrics_store(&store);
    collector.wait_for_init();
    collector.poll();
    
    EXPECT_GE(collector.watched_files().size(), 2);
}

TEST_F(ClaudeUsageCollectorTest, ProjectKeyExtraction) {
    create_jsonl_file(
        test_dir_ / "projects" / "-Users-alice-code-myapp" / "session.jsonl",
        {R"({"message": {"usage": {"input_tokens": 100, "output_tokens": 50}}})"}
    );
    
    diana::MultiMetricsStore hub;
    diana::ClaudeUsageCollector collector(test_dir_.string());
    collector.set_multi_store(&hub);
    collector.wait_for_init();
    collector.poll();
    
    auto sources = hub.list_sources();
    EXPECT_EQ(sources.size(), 1);
    EXPECT_GE(collector.entries_parsed(), 1);
}

TEST_F(ClaudeUsageCollectorTest, EmptyDirectory) {
    diana::MetricsStore store;
    diana::ClaudeUsageCollector collector(test_dir_.string());
    collector.set_metrics_store(&store);
    collector.wait_for_init();
    collector.poll();
    
    EXPECT_EQ(collector.files_processed(), 0);
    EXPECT_EQ(collector.entries_parsed(), 0);
}
