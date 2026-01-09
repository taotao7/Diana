#include "metrics/multi_metrics_store.h"
#include <algorithm>

namespace diana {

void MultiMetricsStore::record(const std::string& source_key, const TokenSample& sample) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    by_source_[source_key].record_sample(sample);
    source_last_activity_[source_key] = std::chrono::steady_clock::now();
}

MetricsStore* MultiMetricsStore::get_source_store(const std::string& source_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = by_source_.find(source_key);
    if (it != by_source_.end()) {
        return &it->second;
    }
    return nullptr;
}

const MetricsStore* MultiMetricsStore::get_source_store(const std::string& source_key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = by_source_.find(source_key);
    if (it != by_source_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<SourceInfo> MultiMetricsStore::list_sources() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<SourceInfo> result;
    result.reserve(by_source_.size());
    
    for (const auto& [key, store] : by_source_) {
        SourceInfo info;
        info.key = key;
        
        auto act_it = source_last_activity_.find(key);
        if (act_it != source_last_activity_.end()) {
            info.last_activity = act_it->second;
        }
        
        auto stats = store.compute_stats();
        info.total_tokens = stats.total_tokens;
        
        result.push_back(std::move(info));
    }
    
    std::sort(result.begin(), result.end(), [](const SourceInfo& a, const SourceInfo& b) {
        return a.last_activity > b.last_activity;
    });
    
    return result;
}

bool MultiMetricsStore::has_source(const std::string& source_key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return by_source_.find(source_key) != by_source_.end();
}

void MultiMetricsStore::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    by_source_.clear();
    source_last_activity_.clear();
}

void MultiMetricsStore::clear_source(const std::string& source_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = by_source_.find(source_key);
    if (it != by_source_.end()) {
        it->second.clear();
    }
}

}
