#include "marketplace_client.h"
#include <sstream>
#include <thread>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <CFNetwork/CFNetwork.h>
#endif

namespace diana {

MarketplaceClient::MarketplaceClient() = default;
MarketplaceClient::~MarketplaceClient() = default;

std::string MarketplaceClient::url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    
    for (char c : value) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
        }
    }
    
    return escaped.str();
}

std::string MarketplaceClient::http_get(const std::string& url, std::string& error_out) {
    
#ifdef __APPLE__
    CFURLRef cf_url = CFURLCreateWithBytes(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(url.c_str()),
        static_cast<CFIndex>(url.length()),
        kCFStringEncodingUTF8,
        nullptr
    );
    
    if (!cf_url) {
        error_out = "Failed to create URL";
        return "";
    }
    
    CFHTTPMessageRef request = CFHTTPMessageCreateRequest(
        kCFAllocatorDefault,
        CFSTR("GET"),
        cf_url,
        kCFHTTPVersion1_1
    );
    CFRelease(cf_url);
    
    if (!request) {
        error_out = "Failed to create HTTP request";
        return "";
    }
    
    CFHTTPMessageSetHeaderFieldValue(request, CFSTR("Accept"), CFSTR("application/json"));
    CFHTTPMessageSetHeaderFieldValue(request, CFSTR("User-Agent"), CFSTR("Diana/1.0"));
    if (!api_key_.empty()) {
        std::string auth_value = "Bearer " + api_key_;
        CFStringRef auth_cf = CFStringCreateWithCString(kCFAllocatorDefault, auth_value.c_str(), kCFStringEncodingUTF8);
        CFHTTPMessageSetHeaderFieldValue(request, CFSTR("Authorization"), auth_cf);
        CFRelease(auth_cf);
    }
    
    CFReadStreamRef stream = CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, request);
    CFRelease(request);
    
    if (!stream) {
        error_out = "Failed to create HTTP stream";
        return "";
    }
    
    CFReadStreamSetProperty(stream, kCFStreamPropertyHTTPShouldAutoredirect, kCFBooleanTrue);
    
    if (!CFReadStreamOpen(stream)) {
        CFRelease(stream);
        error_out = "Failed to open HTTP stream";
        return "";
    }
    
    std::string response_body;
    UInt8 buffer[4096];
    CFIndex bytes_read;
    
    while ((bytes_read = CFReadStreamRead(stream, buffer, sizeof(buffer))) > 0) {
        response_body.append(reinterpret_cast<char*>(buffer), static_cast<size_t>(bytes_read));
    }
    
    if (bytes_read < 0) {
        CFRelease(stream);
        error_out = "Error reading HTTP response";
        return "";
    }
    
    CFHTTPMessageRef response = const_cast<CFHTTPMessageRef>(static_cast<const __CFHTTPMessage*>(
        CFReadStreamCopyProperty(stream, kCFStreamPropertyHTTPResponseHeader)
    ));
    
    if (response) {
        CFIndex status_code = CFHTTPMessageGetResponseStatusCode(response);
        CFRelease(response);
        
        if (status_code < 200 || status_code >= 300) {
            std::string preview = response_body.substr(0, 256);
            error_out = "HTTP error: " + std::to_string(status_code) + " (" + url + ") " + preview;
            std::cerr << "[Smithery] " << error_out << std::endl;
            CFReadStreamClose(stream);
            CFRelease(stream);
            return "";
        }
    }
    
    CFReadStreamClose(stream);
    CFRelease(stream);
    
    return response_body;
#else
    error_out = "HTTP not implemented for this platform";
    return "";
#endif
}

McpSearchResult MarketplaceClient::search_servers(const std::string& query, int page, int page_size, std::string& error_out) {
    std::ostringstream url;
    url << "https://registry.smithery.ai/servers?page=" << page << "&pageSize=" << page_size;
    
    if (!query.empty()) {
        url << "&q=" << url_encode(query);
    }
    
    std::string response = http_get(url.str(), error_out);
    if (!error_out.empty()) {
        return {};
    }
    
    try {
        auto json = nlohmann::json::parse(response);
        return McpSearchResult::from_json(json);
    } catch (const std::exception& e) {
        error_out = std::string("JSON parse error: ") + e.what();
        return {};
    }
}

McpServerEntry MarketplaceClient::get_server_detail(const std::string& qualified_name, std::string& error_out) {
    std::string url = "https://registry.smithery.ai/servers/" + url_encode(qualified_name);
    
    std::string response = http_get(url, error_out);
    if (!error_out.empty()) {
        return {};
    }
    
    try {
        auto json = nlohmann::json::parse(response);
        return McpServerEntry::from_json(json);
    } catch (const std::exception& e) {
        error_out = std::string("JSON parse error: ") + e.what();
        return {};
    }
}

McpServerEntry MarketplaceClient::get_server_detail_by_id(const std::string& id, std::string& error_out) {
    std::string url = "https://registry.smithery.ai/servers/" + url_encode(id);
    
    std::string response = http_get(url, error_out);
    if (!error_out.empty()) {
        return {};
    }
    
    try {
        auto json = nlohmann::json::parse(response);
        return McpServerEntry::from_json(json);
    } catch (const std::exception& e) {
        error_out = std::string("JSON parse error: ") + e.what();
        return {};
    }
}

SkillSearchResult MarketplaceClient::search_skills(const std::string& query, int page, int page_size, std::string& error_out) {
    std::ostringstream url;
    url << "https://registry.smithery.ai/skills?page=" << page << "&pageSize=" << page_size;
    
    if (!query.empty()) {
        url << "&q=" << url_encode(query);
    }
    
    std::string response = http_get(url.str(), error_out);
    if (!error_out.empty()) {
        return {};
    }
    
    try {
        auto json = nlohmann::json::parse(response);
        return SkillSearchResult::from_json(json);
    } catch (const std::exception& e) {
        error_out = std::string("JSON parse error: ") + e.what();
        return {};
    }
}

void MarketplaceClient::search_servers_async(const std::string& query, int page, int page_size, SearchCallback callback) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    auto future = std::async(std::launch::async, [this, query, page, page_size, callback]() {
        std::string error;
        auto result = search_servers(query, page, page_size, error);
        callback(error.empty(), result, error);
    });
    
    pending_tasks_.push_back({std::move(future), false});
}

void MarketplaceClient::get_server_detail_async(const std::string& qualified_name, DetailCallback callback) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    auto future = std::async(std::launch::async, [this, qualified_name, callback]() {
        std::string error;
        auto result = get_server_detail(qualified_name, error);
        callback(error.empty(), result, error);
    });
    
    pending_tasks_.push_back({std::move(future), false});
}

void MarketplaceClient::search_skills_async(const std::string& query, int page, int page_size, SkillSearchCallback callback) {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    auto future = std::async(std::launch::async, [this, query, page, page_size, callback]() {
        std::string error;
        auto result = search_skills(query, page, page_size, error);
        callback(error.empty(), result, error);
    });
    
    pending_tasks_.push_back({std::move(future), false});
}

void MarketplaceClient::poll() {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    
    pending_tasks_.erase(
        std::remove_if(pending_tasks_.begin(), pending_tasks_.end(),
            [](AsyncTask& task) {
                if (task.future.valid()) {
                    auto status = task.future.wait_for(std::chrono::milliseconds(0));
                    return status == std::future_status::ready;
                }
                return true;
            }),
        pending_tasks_.end()
    );
}

}
