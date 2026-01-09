#include "metrics/metrics_store.h"
#include <algorithm>

namespace agent47 {

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
    sample.timestamp = std::chrono::steady_clock::now();
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
    
    if (count_ == 0) {
        return stats;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto window_start = now - std::chrono::seconds(kRateWindowSeconds);
    
    uint64_t window_input = 0;
    uint64_t window_output = 0;
    
    for (size_t i = 0; i < count_; ++i) {
        size_t idx = (head_ + kMaxSamples - 1 - i) % kMaxSamples;
        const auto& s = samples_[idx];
        
        if (s.timestamp < window_start) {
            break;
        }
        
        window_input += s.input_tokens;
        window_output += s.output_tokens;
    }
    
    double raw_input_per_sec = static_cast<double>(window_input) / kRateWindowSeconds;
    double raw_output_per_sec = static_cast<double>(window_output) / kRateWindowSeconds;
    
    constexpr double kAlpha = 0.15;
    
    if (last_ema_update_.time_since_epoch().count() == 0) {
        ema_input_per_sec_ = raw_input_per_sec;
        ema_output_per_sec_ = raw_output_per_sec;
    } else {
        ema_input_per_sec_ = kAlpha * raw_input_per_sec + (1.0 - kAlpha) * ema_input_per_sec_;
        ema_output_per_sec_ = kAlpha * raw_output_per_sec + (1.0 - kAlpha) * ema_output_per_sec_;
    }
    last_ema_update_ = now;
    
    stats.input_per_sec = ema_input_per_sec_;
    stats.output_per_sec = ema_output_per_sec_;
    stats.tokens_per_sec = stats.input_per_sec + stats.output_per_sec;
    
    stats.input_per_min = stats.input_per_sec * 60.0;
    stats.output_per_min = stats.output_per_sec * 60.0;
    stats.tokens_per_min = stats.tokens_per_sec * 60.0;
    
    return stats;
}

size_t MetricsStore::sample_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return count_;
}

std::array<float, 60> MetricsStore::get_rate_history() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::array<float, 60> history{};
    
    if (count_ == 0) {
        return history;
    }
    
    auto now = std::chrono::steady_clock::now();
    
    for (size_t i = 0; i < count_; ++i) {
        size_t idx = (head_ + kMaxSamples - 1 - i) % kMaxSamples;
        const auto& s = samples_[idx];
        
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - s.timestamp);
        if (age.count() >= 60) {
            break;
        }
        
        size_t bucket = 59 - static_cast<size_t>(age.count());
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
    ema_input_per_sec_ = 0.0;
    ema_output_per_sec_ = 0.0;
    last_ema_update_ = {};
}

}
