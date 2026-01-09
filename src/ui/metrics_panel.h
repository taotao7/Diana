#pragma once

#include "metrics/metrics_store.h"
#include "metrics/claude_usage_collector.h"
#include <memory>

namespace agent47 {

class MetricsPanel {
public:
    MetricsPanel();
    
    void render();
    void update();

private:
    std::unique_ptr<MetricsStore> store_;
    std::unique_ptr<ClaudeUsageCollector> collector_;
    
    std::array<float, 60> rate_history_{};
};

}
