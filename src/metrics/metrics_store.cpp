#include "metrics/metrics_store.h"
#include <algorithm>

namespace diana {

void MetricsStore::record_sample(const TokenSample& sample) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    samples_[head_] = sample;
    head_ = (head_ + 1) % kMaxSamples;
    if (count_ < kMaxSamples) {
        ++count_;
    }
    
    cumulative_input_ += sample.input_tokens;
    cumulative_output_ += sample.output_tokens;
    cumulative_cost_ += sample.cost_usd;
}

void MetricsStore::record_usage(uint64_t input, uint64_t output, double cost) {
    TokenSample sample;
    sample.timestamp = std::chrono::system_clock::now();
    sample.input_tokens = input;
    sample.output_tokens = output;
    sample.total_tokens = input + output;
    sample.cost_usd = cost;
    record_sample(sample);
}

TokenStats MetricsStore::compute_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    TokenStats stats;
    stats.total_input = cumulative_input_;
    stats.total_output = cumulative_output_;
    stats.total_tokens = cumulative_input_ + cumulative_output_;
    stats.total_cost = cumulative_cost_;
    
    return stats;
}

size_t MetricsStore::sample_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
}

std::array<float, MetricsStore::kHistoryHours> MetricsStore::get_rate_history() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::array<float, kHistoryHours> history{};
    
    if (count_ == 0) {
        return history;
    }
    
    auto now = std::chrono::system_clock::now();
    
    for (size_t i = 0; i < count_; ++i) {
        size_t idx = (head_ + kMaxSamples - 1 - i) % kMaxSamples;
        const auto& s = samples_[idx];
        
        if (s.timestamp > now) {
            continue;
        }
        
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - s.timestamp);
        if (static_cast<size_t>(age.count()) >= kHistoryHours) {
            break;
        }
        
        size_t bucket = (kHistoryHours - 1) - static_cast<size_t>(age.count());
        history[bucket] += static_cast<float>(s.total_tokens);
    }
    
    return history;
}

void MetricsStore::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    head_ = 0;
    count_ = 0;
    cumulative_input_ = 0;
    cumulative_output_ = 0;
    cumulative_cost_ = 0.0;
}

}
