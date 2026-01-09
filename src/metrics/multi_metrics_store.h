#pragma once

#include "metrics_store.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <mutex>

namespace diana {

struct SourceInfo {
    std::string key;
    std::chrono::steady_clock::time_point last_activity;
    uint64_t total_tokens = 0;
};

class MultiMetricsStore {
public:
    MultiMetricsStore() = default;
    
    void record(const std::string& source_key, const TokenSample& sample);
    
    MetricsStore* get_source_store(const std::string& source_key);
    const MetricsStore* get_source_store(const std::string& source_key) const;
    
    std::vector<SourceInfo> list_sources() const;
    
    bool has_source(const std::string& source_key) const;
    
    void clear();
    void clear_source(const std::string& source_key);

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, MetricsStore> by_source_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> source_last_activity_;
};

}
