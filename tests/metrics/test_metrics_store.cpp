#include <gtest/gtest.h>
#include "metrics/metrics_store.h"
#include <thread>

TEST(MetricsStoreTest, RecordAndCount) {
    agent47::MetricsStore store;
    
    EXPECT_EQ(store.sample_count(), 0);
    
    store.record_usage(100, 50, 0.01);
    EXPECT_EQ(store.sample_count(), 1);
    
    store.record_usage(200, 100, 0.02);
    EXPECT_EQ(store.sample_count(), 2);
}

TEST(MetricsStoreTest, ComputeStats) {
    agent47::MetricsStore store;
    
    store.record_usage(100, 50, 0.01);
    store.record_usage(200, 100, 0.02);
    
    auto stats = store.compute_stats();
    
    EXPECT_EQ(stats.total_input, 300);
    EXPECT_EQ(stats.total_output, 150);
    EXPECT_EQ(stats.total_tokens, 450);
    EXPECT_DOUBLE_EQ(stats.total_cost, 0.03);
}

TEST(MetricsStoreTest, Clear) {
    agent47::MetricsStore store;
    
    store.record_usage(100, 50, 0.01);
    store.record_usage(200, 100, 0.02);
    
    EXPECT_EQ(store.sample_count(), 2);
    
    store.clear();
    
    EXPECT_EQ(store.sample_count(), 0);
    auto stats = store.compute_stats();
    EXPECT_EQ(stats.total_tokens, 0);
}

TEST(MetricsStoreTest, RateHistory) {
    agent47::MetricsStore store;
    
    store.record_usage(100, 50, 0.0);
    
    auto history = store.get_rate_history();
    
    float sum = 0;
    for (float v : history) {
        sum += v;
    }
    EXPECT_GT(sum, 0);
}

TEST(MetricsStoreTest, ThreadSafety) {
    agent47::MetricsStore store;
    const int num_records = 100;
    
    std::thread writer([&store, num_records]() {
        for (int i = 0; i < num_records; ++i) {
            store.record_usage(10, 5, 0.001);
        }
    });
    
    std::thread reader([&store, num_records]() {
        for (int i = 0; i < num_records; ++i) {
            auto stats = store.compute_stats();
            (void)stats;
        }
    });
    
    writer.join();
    reader.join();
    
    EXPECT_EQ(store.sample_count(), num_records);
}
