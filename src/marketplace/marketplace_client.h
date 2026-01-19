#pragma once

#include "marketplace_types.h"
#include <functional>
#include <string>
#include <future>
#include <mutex>

namespace diana {

class MarketplaceClient {
public:
    MarketplaceClient();
    ~MarketplaceClient();
    
    void set_api_key(const std::string& api_key) { api_key_ = api_key; }
    
    using SearchCallback = std::function<void(bool success, McpSearchResult result, std::string error)>;
    using DetailCallback = std::function<void(bool success, McpServerEntry server, std::string error)>;
    using SkillSearchCallback = std::function<void(bool success, SkillSearchResult result, std::string error)>;
    
    void search_servers_async(const std::string& query, int page, int page_size, SearchCallback callback);
    void get_server_detail_async(const std::string& qualified_name, DetailCallback callback);
    void search_skills_async(const std::string& query, int page, int page_size, SkillSearchCallback callback);
    
    McpSearchResult search_servers(const std::string& query, int page, int page_size, std::string& error_out);
    McpServerEntry get_server_detail(const std::string& qualified_name, std::string& error_out);
    McpServerEntry get_server_detail_by_id(const std::string& id, std::string& error_out);
    SkillSearchResult search_skills(const std::string& query, int page, int page_size, std::string& error_out);
    
    void poll();
    
private:
    std::string http_get(const std::string& url, std::string& error_out);
    static std::string url_encode(const std::string& value);
    
    struct AsyncTask {
        std::future<void> future;
        bool completed = false;
    };
    
    std::vector<AsyncTask> pending_tasks_;
    std::mutex tasks_mutex_;
    std::string api_key_;
};

}
