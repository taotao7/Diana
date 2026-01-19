#pragma once

#include <string>
#include <vector>
#include <optional>
#include <map>
#include <nlohmann/json.hpp>

namespace diana {

struct SkillEntry;
struct SkillSearchResult;

struct McpServerEntry {
    std::string id;
    std::string qualified_name;
    std::string display_name;
    std::string description;
    std::string icon_url;
    bool verified = false;
    int use_count = 0;
    bool remote = false;
    bool is_deployed = false;
    std::string created_at;
    std::string homepage;
    
    std::string deployment_url;
    
    static McpServerEntry from_json(const nlohmann::json& j);
};

struct Pagination {
    int current_page = 1;
    int page_size = 10;
    int total_pages = 0;
    int total_count = 0;
    
    static Pagination from_json(const nlohmann::json& j);
};

struct McpSearchResult {
    std::vector<McpServerEntry> servers;
    Pagination pagination;
    
    static McpSearchResult from_json(const nlohmann::json& j);
};

struct SkillEntry {
    std::string id;
    std::string namespace_name;
    std::string slug;
    std::string display_name;
    std::string description;
    std::string prompt;
    int quality_score = 0;
    bool listed = false;
    std::string created_at;
    int external_stars = 0;
    int external_forks = 0;
    int total_activations = 0;
    int unique_users = 0;
    std::vector<std::string> categories;
    std::vector<std::string> servers;
    std::string git_url;
    
    static SkillEntry from_json(const nlohmann::json& j);
};

struct SkillSearchResult {
    std::vector<SkillEntry> skills;
    Pagination pagination;
    
    static SkillSearchResult from_json(const nlohmann::json& j);
};

struct McpConnectionConfig {
    std::string type = "http";
    std::string url;
    std::string command;
    std::vector<std::string> args;
    std::map<std::string, std::string> env;
    std::map<std::string, std::string> headers;
    
    nlohmann::json to_json() const;
};

struct InstalledMcpServer {
    std::string name;
    McpConnectionConfig config;
    bool is_project_scope = false;
};

enum class FetchState {
    Idle,
    Loading,
    Success,
    Error
};

inline McpServerEntry McpServerEntry::from_json(const nlohmann::json& j) {
    McpServerEntry entry;
    
    if (j.contains("id") && j["id"].is_string()) {
        entry.id = j["id"].get<std::string>();
    }
    if (j.contains("qualifiedName") && j["qualifiedName"].is_string()) {
        entry.qualified_name = j["qualifiedName"].get<std::string>();
    }
    if (j.contains("displayName") && j["displayName"].is_string()) {
        entry.display_name = j["displayName"].get<std::string>();
    }
    if (j.contains("description") && j["description"].is_string()) {
        entry.description = j["description"].get<std::string>();
    }
    if (j.contains("iconUrl") && j["iconUrl"].is_string()) {
        entry.icon_url = j["iconUrl"].get<std::string>();
    }
    if (j.contains("verified") && j["verified"].is_boolean()) {
        entry.verified = j["verified"].get<bool>();
    }
    if (j.contains("useCount") && j["useCount"].is_number()) {
        entry.use_count = j["useCount"].get<int>();
    }
    if (j.contains("remote") && j["remote"].is_boolean()) {
        entry.remote = j["remote"].get<bool>();
    }
    if (j.contains("isDeployed") && j["isDeployed"].is_boolean()) {
        entry.is_deployed = j["isDeployed"].get<bool>();
    }
    if (j.contains("createdAt") && j["createdAt"].is_string()) {
        entry.created_at = j["createdAt"].get<std::string>();
    }
    if (j.contains("homepage") && j["homepage"].is_string()) {
        entry.homepage = j["homepage"].get<std::string>();
    }
    if (j.contains("deploymentUrl") && j["deploymentUrl"].is_string()) {
        entry.deployment_url = j["deploymentUrl"].get<std::string>();
    }
    
    return entry;
}

inline Pagination Pagination::from_json(const nlohmann::json& j) {
    Pagination p;
    
    if (j.contains("currentPage") && j["currentPage"].is_number()) {
        p.current_page = j["currentPage"].get<int>();
    }
    if (j.contains("pageSize") && j["pageSize"].is_number()) {
        p.page_size = j["pageSize"].get<int>();
    }
    if (j.contains("totalPages") && j["totalPages"].is_number()) {
        p.total_pages = j["totalPages"].get<int>();
    }
    if (j.contains("totalCount") && j["totalCount"].is_number()) {
        p.total_count = j["totalCount"].get<int>();
    }
    
    return p;
}

inline McpSearchResult McpSearchResult::from_json(const nlohmann::json& j) {
    McpSearchResult result;
    
    if (j.contains("servers") && j["servers"].is_array()) {
        for (const auto& server : j["servers"]) {
            result.servers.push_back(McpServerEntry::from_json(server));
        }
    }
    
    if (j.contains("pagination") && j["pagination"].is_object()) {
        result.pagination = Pagination::from_json(j["pagination"]);
    }
    
    return result;
}

inline SkillEntry SkillEntry::from_json(const nlohmann::json& j) {
    SkillEntry entry;
    
    if (j.contains("id") && j["id"].is_string()) {
        entry.id = j["id"].get<std::string>();
    }
    if (j.contains("namespace") && j["namespace"].is_string()) {
        entry.namespace_name = j["namespace"].get<std::string>();
    }
    if (j.contains("slug") && j["slug"].is_string()) {
        entry.slug = j["slug"].get<std::string>();
    }
    if (j.contains("displayName") && j["displayName"].is_string()) {
        entry.display_name = j["displayName"].get<std::string>();
    }
    if (j.contains("description") && j["description"].is_string()) {
        entry.description = j["description"].get<std::string>();
    }
    if (j.contains("prompt") && j["prompt"].is_string()) {
        entry.prompt = j["prompt"].get<std::string>();
    }
    if (j.contains("qualityScore") && j["qualityScore"].is_number()) {
        entry.quality_score = j["qualityScore"].get<int>();
    }
    if (j.contains("listed") && j["listed"].is_boolean()) {
        entry.listed = j["listed"].get<bool>();
    }
    if (j.contains("createdAt") && j["createdAt"].is_string()) {
        entry.created_at = j["createdAt"].get<std::string>();
    }
    if (j.contains("externalStars") && j["externalStars"].is_number()) {
        entry.external_stars = j["externalStars"].get<int>();
    }
    if (j.contains("externalForks") && j["externalForks"].is_number()) {
        entry.external_forks = j["externalForks"].get<int>();
    }
    if (j.contains("totalActivations") && j["totalActivations"].is_number()) {
        entry.total_activations = j["totalActivations"].get<int>();
    }
    if (j.contains("uniqueUsers") && j["uniqueUsers"].is_number()) {
        entry.unique_users = j["uniqueUsers"].get<int>();
    }
    if (j.contains("categories") && j["categories"].is_array()) {
        entry.categories = j["categories"].get<std::vector<std::string>>();
    }
    if (j.contains("servers") && j["servers"].is_array()) {
        entry.servers = j["servers"].get<std::vector<std::string>>();
    }
    if (j.contains("gitUrl") && j["gitUrl"].is_string()) {
        entry.git_url = j["gitUrl"].get<std::string>();
    }
    
    return entry;
}

inline SkillSearchResult SkillSearchResult::from_json(const nlohmann::json& j) {
    SkillSearchResult result;
    
    if (j.contains("skills") && j["skills"].is_array()) {
        for (const auto& skill : j["skills"]) {
            result.skills.push_back(SkillEntry::from_json(skill));
        }
    }
    
    if (j.contains("pagination") && j["pagination"].is_object()) {
        result.pagination = Pagination::from_json(j["pagination"]);
    }
    
    return result;
}

inline nlohmann::json McpConnectionConfig::to_json() const {
    nlohmann::json j;
    j["type"] = type;
    
    if (!url.empty()) {
        j["url"] = url;
    }
    if (!command.empty()) {
        j["command"] = command;
    }
    if (!args.empty()) {
        j["args"] = args;
    }
    if (!env.empty()) {
        j["env"] = env;
    }
    if (!headers.empty()) {
        j["headers"] = headers;
    }
    
    return j;
}

}
