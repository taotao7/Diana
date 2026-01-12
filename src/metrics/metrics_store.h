#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>

namespace diana {

struct TokenSample {
    std::chrono::steady_clock::time_point timestamp;
    uint64_t input_tokens = 0;
    uint64_t output_tokens = 0;
    uint64_t total_tokens = 0;
    double cost_usd = 0.0;
};

struct TokenStats {
    uint64_t total_input = 0;
    uint64_t total_output = 0;
    uint64_t total_tokens = 0;
    double total_cost = 0.0;
};

class MetricsStore {
public:
    static constexpr size_t kMaxSamples = 3600;
    static constexpr size_t kHistoryHours = 60;
    
    MetricsStore() = default;
    
    void record_sample(const TokenSample& sample);
    void record_usage(uint64_t input, uint64_t output, double cost = 0.0);
    
    TokenStats compute_stats() const;
    
    size_t sample_count() const;
    
    std::array<float, kHistoryHours> get_rate_history() const;
    
    void clear();

private:
    mutable std::mutex mutex_;
    std::array<TokenSample, kMaxSamples> samples_{};
    size_t head_ = 0;
    size_t count_ = 0;
    
    uint64_t cumulative_input_ = 0;
    uint64_t cumulative_output_ = 0;
    double cumulative_cost_ = 0.0;
};

}
