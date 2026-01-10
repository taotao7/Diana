#include <gtest/gtest.h>
#include "metrics/multi_metrics_store.h"
#include <thread>

TEST(MultiMetricsStoreTest, RecordAndRetrieve) {
    diana::MultiMetricsStore store;
    
    diana::TokenSample sample;
    sample.timestamp = std::chrono::steady_clock::now();
    sample.input_tokens = 100;
    sample.output_tokens = 50;
    sample.total_tokens = 150;
    sample.cost_usd = 0.01;
    
    store.record("project-a", sample);
    
    EXPECT_TRUE(store.has_source("project-a"));
    EXPECT_FALSE(store.has_source("project-b"));
    
    auto* metrics = store.get_source_store("project-a");
    ASSERT_NE(metrics, nullptr);
    EXPECT_EQ(metrics->sample_count(), 1);
    
    auto stats = metrics->compute_stats();
    EXPECT_EQ(stats.total_input, 100);
    EXPECT_EQ(stats.total_output, 50);
}

TEST(MultiMetricsStoreTest, MultipleProjects) {
    diana::MultiMetricsStore store;
    
    diana::TokenSample sample_a;
    sample_a.timestamp = std::chrono::steady_clock::now();
    sample_a.input_tokens = 100;
    sample_a.output_tokens = 50;
    sample_a.total_tokens = 150;
    
    diana::TokenSample sample_b;
    sample_b.timestamp = std::chrono::steady_clock::now();
    sample_b.input_tokens = 200;
    sample_b.output_tokens = 100;
    sample_b.total_tokens = 300;
    
    store.record("project-a", sample_a);
    store.record("project-b", sample_b);
    store.record("project-a", sample_a);
    
    EXPECT_TRUE(store.has_source("project-a"));
    EXPECT_TRUE(store.has_source("project-b"));
    
    auto* metrics_a = store.get_source_store("project-a");
    auto* metrics_b = store.get_source_store("project-b");
    
    ASSERT_NE(metrics_a, nullptr);
    ASSERT_NE(metrics_b, nullptr);
    
    EXPECT_EQ(metrics_a->sample_count(), 2);
    EXPECT_EQ(metrics_b->sample_count(), 1);
    
    auto stats_a = metrics_a->compute_stats();
    auto stats_b = metrics_b->compute_stats();
    
    EXPECT_EQ(stats_a.total_input, 200);
    EXPECT_EQ(stats_b.total_input, 200);
}

TEST(MultiMetricsStoreTest, ListSources) {
    diana::MultiMetricsStore store;
    
    diana::TokenSample sample;
    sample.timestamp = std::chrono::steady_clock::now();
    sample.input_tokens = 100;
    sample.output_tokens = 50;
    sample.total_tokens = 150;
    
    store.record("project-x", sample);
    store.record("project-y", sample);
    store.record("project-z", sample);
    
    auto sources = store.list_sources();
    EXPECT_EQ(sources.size(), 3);
    
    bool found_x = false, found_y = false, found_z = false;
    for (const auto& info : sources) {
        if (info.key == "project-x") found_x = true;
        if (info.key == "project-y") found_y = true;
        if (info.key == "project-z") found_z = true;
        EXPECT_EQ(info.total_tokens, 150);
    }
    EXPECT_TRUE(found_x);
    EXPECT_TRUE(found_y);
    EXPECT_TRUE(found_z);
}

TEST(MultiMetricsStoreTest, Clear) {
    diana::MultiMetricsStore store;
    
    diana::TokenSample sample;
    sample.timestamp = std::chrono::steady_clock::now();
    sample.input_tokens = 100;
    sample.output_tokens = 50;
    sample.total_tokens = 150;
    
    store.record("project-a", sample);
    store.record("project-b", sample);
    
    EXPECT_EQ(store.list_sources().size(), 2);
    
    store.clear();
    
    EXPECT_EQ(store.list_sources().size(), 0);
    EXPECT_FALSE(store.has_source("project-a"));
    EXPECT_FALSE(store.has_source("project-b"));
}

TEST(MultiMetricsStoreTest, ClearSource) {
    diana::MultiMetricsStore store;
    
    diana::TokenSample sample;
    sample.timestamp = std::chrono::steady_clock::now();
    sample.input_tokens = 100;
    sample.output_tokens = 50;
    sample.total_tokens = 150;
    
    store.record("project-a", sample);
    store.record("project-b", sample);
    
    store.clear_source("project-a");
    
    EXPECT_TRUE(store.has_source("project-a"));
    auto* metrics_a = store.get_source_store("project-a");
    ASSERT_NE(metrics_a, nullptr);
    EXPECT_EQ(metrics_a->sample_count(), 0);
    
    auto* metrics_b = store.get_source_store("project-b");
    ASSERT_NE(metrics_b, nullptr);
    EXPECT_EQ(metrics_b->sample_count(), 1);
}

TEST(MultiMetricsStoreTest, NonExistentSource) {
    diana::MultiMetricsStore store;
    
    EXPECT_FALSE(store.has_source("nonexistent"));
    EXPECT_EQ(store.get_source_store("nonexistent"), nullptr);
    
    const diana::MultiMetricsStore& const_store = store;
    EXPECT_EQ(const_store.get_source_store("nonexistent"), nullptr);
}

TEST(MultiMetricsStoreTest, ThreadSafety) {
    diana::MultiMetricsStore store;
    const int num_records = 100;
    
    std::thread writer1([&store, num_records]() {
        diana::TokenSample sample;
        sample.timestamp = std::chrono::steady_clock::now();
        sample.input_tokens = 10;
        sample.output_tokens = 5;
        sample.total_tokens = 15;
        
        for (int i = 0; i < num_records; ++i) {
            store.record("project-1", sample);
        }
    });
    
    std::thread writer2([&store, num_records]() {
        diana::TokenSample sample;
        sample.timestamp = std::chrono::steady_clock::now();
        sample.input_tokens = 20;
        sample.output_tokens = 10;
        sample.total_tokens = 30;
        
        for (int i = 0; i < num_records; ++i) {
            store.record("project-2", sample);
        }
    });
    
    std::thread reader([&store, num_records]() {
        for (int i = 0; i < num_records; ++i) {
            auto sources = store.list_sources();
            (void)sources;
        }
    });
    
    writer1.join();
    writer2.join();
    reader.join();
    
    auto* m1 = store.get_source_store("project-1");
    auto* m2 = store.get_source_store("project-2");
    
    ASSERT_NE(m1, nullptr);
    ASSERT_NE(m2, nullptr);
    EXPECT_EQ(m1->sample_count(), num_records);
    EXPECT_EQ(m2->sample_count(), num_records);
}
