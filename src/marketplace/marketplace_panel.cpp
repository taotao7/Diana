#include "marketplace_panel.h"
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <toml++/toml.hpp>
#include <array>
#include <cstdio>
#include <mutex>
#include <algorithm>
#include "terminal/vterminal.h"
#include "ui/theme.h"
#include <nfd.h>

namespace diana {

static void RenderSplitter(const char* id, float* width_ptr, float min_width) {
    ImGui::InvisibleButton(id, ImVec2(4.0f, -1.0f));
    if (ImGui::IsItemActive()) {
        *width_ptr -= ImGui::GetIO().MouseDelta.x;
        if (*width_ptr < min_width) *width_ptr = min_width;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
}

static McpConnectionConfig parse_mcp_config(const nlohmann::json& j) {
    McpConnectionConfig config;
    if (!j.is_object()) {
        return config;
    }
    if (j.contains("type") && j["type"].is_string()) {
        config.type = j["type"].get<std::string>();
    }
    if (j.contains("url") && j["url"].is_string()) {
        config.url = j["url"].get<std::string>();
    }
    if (j.contains("command") && j["command"].is_string()) {
        config.command = j["command"].get<std::string>();
    }
    if (j.contains("args") && j["args"].is_array()) {
        for (const auto& arg : j["args"]) {
            if (arg.is_string()) {
                config.args.push_back(arg.get<std::string>());
            }
        }
    }
    if (j.contains("env") && j["env"].is_object()) {
        for (const auto& [key, value] : j["env"].items()) {
            if (value.is_string()) {
                config.env[key] = value.get<std::string>();
            }
        }
    }
    if (j.contains("headers") && j["headers"].is_object()) {
        for (const auto& [key, value] : j["headers"].items()) {
            if (value.is_string()) {
                config.headers[key] = value.get<std::string>();
            }
        }
    }
    return config;
}

static std::string strip_ansi_sequences(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        if (c == 0x1b) {
            if (i + 1 >= input.size()) {
                break;
            }
            char next = input[i + 1];
            if (next == '[') {
                i += 2;
                while (i < input.size()) {
                    char ch = input[i];
                    if ((ch >= '@' && ch <= '~')) {
                        break;
                    }
                    ++i;
                }
                continue;
            }
            if (next == ']') {
                i += 2;
                while (i < input.size()) {
                    char ch = input[i];
                    if (ch == '\\') {
                        break;
                    }
                    if (ch == '\a') {
                        break;
                    }
                    ++i;
                }
                continue;
            }
            continue;
        }
        if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') {
            continue;
        }
        out.push_back(static_cast<char>(c));
    }
    return out;
}

static void utf8_encode(uint32_t codepoint, char* out, int* len) {
    if (codepoint <= 0x7F) {
        out[0] = static_cast<char>(codepoint);
        *len = 1;
    } else if (codepoint <= 0x7FF) {
        out[0] = static_cast<char>(0xC0 | (codepoint >> 6));
        out[1] = static_cast<char>(0x80 | (codepoint & 0x3F));
        *len = 2;
    } else if (codepoint <= 0xFFFF) {
        out[0] = static_cast<char>(0xE0 | (codepoint >> 12));
        out[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
        *len = 3;
    } else {
        out[0] = static_cast<char>(0xF0 | (codepoint >> 18));
        out[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        out[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
        *len = 4;
    }
}

static void cell_to_utf8(const uint32_t* chars, std::string& out) {
    char utf8[8];
    int len = 0;
    for (int j = 0; j < TERMINAL_MAX_CHARS_PER_CELL; ++j) {
        uint32_t cp = chars[j];
        if (cp == 0) break;
        utf8_encode(cp, utf8, &len);
        out.append(utf8, utf8 + len);
    }
}

static bool is_light_color(uint32_t abgr) {
    uint8_t r = abgr & 0xFF;
    uint8_t g = (abgr >> 8) & 0xFF;
    uint8_t b = (abgr >> 16) & 0xFF;
    int brightness = (r * 299 + g * 587 + b * 114) / 1000;
    return brightness > 180;
}

static uint32_t adjust_fg_for_light_theme(uint32_t fg, bool is_light_theme) {
    if (!is_light_theme) return fg;
    if (!is_light_color(fg)) return fg;
    uint8_t r = fg & 0xFF;
    uint8_t g = (fg >> 8) & 0xFF;
    uint8_t b = (fg >> 16) & 0xFF;
    uint8_t a = (fg >> 24) & 0xFF;
    r = static_cast<uint8_t>(r * 0.4f);
    g = static_cast<uint8_t>(g * 0.4f);
    b = static_cast<uint8_t>(b * 0.4f);
    return r | (g << 8) | (b << 16) | (a << 24);
}

static void render_terminal_line(const TerminalCell* cells, int count, float line_height, ImDrawList* draw_list, const ImVec2& start_pos, float char_w, bool is_light_theme) {
    float cell_x = start_pos.x;
    for (int col = 0; col < count; ++col) {
        const TerminalCell& cell = cells[col];
        if (cell.width == 0) continue;

        if (cell.chars[0] != 0 && cell.chars[0] != ' ' && cell.chars[0] <= 0x10FFFF) {
            char utf8_buf[32];
            int utf8_len = 0;
            for (int j = 0; j < TERMINAL_MAX_CHARS_PER_CELL && cell.chars[j] != 0; ++j) {
                uint32_t cp = cell.chars[j];
                if (cp > 0x10FFFF) break;
                char tmp[8];
                int len;
                utf8_encode(cp, tmp, &len);
                if (utf8_len + len < 31) {
                    memcpy(utf8_buf + utf8_len, tmp, static_cast<size_t>(len));
                    utf8_len += len;
                }
            }
            utf8_buf[utf8_len] = '\0';

            if (utf8_len > 0) {
                uint32_t fg = adjust_fg_for_light_theme(cell.fg, is_light_theme);
                draw_list->AddText(ImVec2(cell_x, start_pos.y), fg, utf8_buf);
            }
        }

        cell_x += cell.width * char_w;
    }

    ImGui::Dummy(ImVec2(count * char_w, line_height));
}

MarketplacePanel::MarketplacePanel() {
    search_buffer_.resize(256, '\0');
    api_key_buffer_.resize(256, '\0');
    install_terminal_ = std::make_unique<VTerminal>(24, 80);
    
    auto settings = settings_store_.load();
    if (!settings.smithery_api_key.empty()) {
        std::strncpy(api_key_buffer_.data(), settings.smithery_api_key.c_str(), api_key_buffer_.size() - 1);
        client_.set_api_key(settings.smithery_api_key);
    }
}

void MarketplacePanel::render() {
    ImGui::SetNextWindowSizeConstraints(ImVec2(600, 400), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("MCP Marketplace");
    render_content();
    ImGui::End();
}

void MarketplacePanel::render_content() {
    client_.poll();
    
    if (!initial_fetch_done_ && !api_key_buffer_.empty() && api_key_buffer_[0] != '\0') {
        initial_fetch_done_ = true;
        search_query_.clear();
        do_search(1);
        do_skill_search(1);
    }
    
    std::string status_message;
    bool status_is_error = false;
    float status_time = 0.0f;
    {
        std::lock_guard<std::mutex> lock(install_mutex_);
        if (status_time_ > 0) {
            status_time_ -= ImGui::GetIO().DeltaTime;
        }
        status_message = status_message_;
        status_is_error = status_is_error_;
        status_time = status_time_;
    }
    
    if (status_time > 0) {
        const auto& theme = get_current_theme();
        ImVec4 color = status_is_error
            ? ImGui::ColorConvertU32ToFloat4(theme.error)
            : ImGui::ColorConvertU32ToFloat4(theme.accent);
        ImGui::TextColored(color, "%s", status_message.c_str());
    }
    
    if (ImGui::BeginTabBar("MarketplaceTabs")) {
        if (ImGui::BeginTabItem("MCP")) {
            marketplace_tab_ = 0;
            if (last_marketplace_tab_ != marketplace_tab_) {
                focus_search_input_ = true;
            }
            render_search_bar();
            
            float avail_width = ImGui::GetContentRegionAvail().x;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float min_list_width = 200.0f;
            float min_detail_width = 200.0f;
            float splitter_width = 4.0f;
            
            float max_detail_width = avail_width - min_list_width - splitter_width - (spacing * 2.0f);
            if (max_detail_width < min_detail_width) max_detail_width = min_detail_width;
            
            if (mcp_detail_width_ < min_detail_width) mcp_detail_width_ = min_detail_width;
            if (mcp_detail_width_ > max_detail_width) mcp_detail_width_ = max_detail_width;
            
            float list_width = avail_width - mcp_detail_width_ - splitter_width - (spacing * 2.0f);
            
            ImGui::BeginChild("ServerList", ImVec2(list_width, 0), true);
            if (show_installed_mcp_) {
                render_installed_mcp_list();
            } else {
                render_server_list();
            }
            ImGui::EndChild();
            
            ImGui::SameLine();
            RenderSplitter("##mcp_splitter", &mcp_detail_width_, min_detail_width);
            ImGui::SameLine();
            
            ImGui::BeginChild("ServerDetail", ImVec2(mcp_detail_width_, 0), true);
            if (show_installed_mcp_) {
                render_installed_mcp_detail();
            } else {
                render_server_detail();
            }
            ImGui::EndChild();
            
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Skills")) {
            marketplace_tab_ = 1;
            if (last_marketplace_tab_ != marketplace_tab_) {
                focus_search_input_ = true;
            }
            render_search_bar();
            
            float avail_width = ImGui::GetContentRegionAvail().x;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float min_list_width = 200.0f;
            float min_detail_width = 200.0f;
            float splitter_width = 4.0f;
            
            float max_detail_width = avail_width - min_list_width - splitter_width - (spacing * 2.0f);
            if (max_detail_width < min_detail_width) max_detail_width = min_detail_width;
            
            if (skill_detail_width_ < min_detail_width) skill_detail_width_ = min_detail_width;
            if (skill_detail_width_ > max_detail_width) skill_detail_width_ = max_detail_width;
            
            float list_width = avail_width - skill_detail_width_ - splitter_width - (spacing * 2.0f);
            
            ImGui::BeginChild("SkillList", ImVec2(list_width, 0), true);
            if (show_installed_skills_) {
                render_installed_skill_list();
            } else {
                render_skill_list();
            }
            ImGui::EndChild();
            
            ImGui::SameLine();
            RenderSplitter("##skill_splitter", &skill_detail_width_, min_detail_width);
            ImGui::SameLine();
            
            ImGui::BeginChild("SkillDetail", ImVec2(skill_detail_width_, 0), true);
            if (show_installed_skills_) {
                render_installed_skill_detail();
            } else {
                render_skill_detail();
            }
            ImGui::EndChild();
            
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }

    last_marketplace_tab_ = marketplace_tab_;
    
    render_install_popup();
}

void MarketplacePanel::render_search_bar() {
    const bool is_mcp = marketplace_tab_ == 0;
    ImGui::PushID(is_mcp ? "mcp_search" : "skill_search");
    ImGui::Text(is_mcp ? "Search MCP Servers:" : "Search Skills:");
    ImGui::SameLine();
    
    ImGui::SetNextItemWidth(300);
    if (focus_search_input_ && (!show_installed_mcp_ && !show_installed_skills_)) {
        ImGui::SetKeyboardFocusHere();
        focus_search_input_ = false;
    }
    bool enter_pressed = ImGui::InputTextWithHint(
        "##Search", 
        is_mcp ? "e.g. github, notion, exa..." : "e.g. summarizer, reviewer...", 
        search_buffer_.data(), 
        search_buffer_.size(),
        ImGuiInputTextFlags_EnterReturnsTrue
    );
    
    ImGui::SameLine();
    
    bool search_clicked = ImGui::Button("Search");
    
    if (enter_pressed || search_clicked) {
        search_query_ = search_buffer_.data();
        if (is_mcp) {
            do_search(1);
        } else {
            do_skill_search(1);
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Browse All")) {
        search_query_.clear();
        std::fill(search_buffer_.begin(), search_buffer_.end(), '\0');
        if (is_mcp) {
            do_search(1);
        } else {
            do_skill_search(1);
        }
    }

    ImGui::SameLine();
    bool show_installed = is_mcp ? show_installed_mcp_ : show_installed_skills_;
    const char* toggle_label = show_installed ? "Marketplace" : "Installed";
    if (ImGui::Button(toggle_label)) {
        if (is_mcp) {
            show_installed_mcp_ = !show_installed_mcp_;
            if (show_installed_mcp_) {
                refresh_installed_mcp();
            }
        } else {
            show_installed_skills_ = !show_installed_skills_;
            if (show_installed_skills_) {
                refresh_installed_skills();
            }
        }
    }
    
    
    if (list_state_ == FetchState::Loading || detail_state_ == FetchState::Loading) {
        ImGui::SameLine();
        ImGui::TextDisabled("Loading...");
    }

    ImGui::Spacing();
    ImGui::Text("Smithery Key:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300);
    if (ImGui::InputText("##SmitheryKey", api_key_buffer_.data(), api_key_buffer_.size(), ImGuiInputTextFlags_Password)) {
        client_.set_api_key(api_key_buffer_.data());
        MarketplaceSettings settings;
        settings.smithery_api_key = api_key_buffer_.data();
        settings_store_.save(settings);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Get API Key")) {
#ifdef __APPLE__
        system("open \"https://smithery.ai/account/api-keys\"");
#endif
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Open https://smithery.ai/account/api-keys");
    }
    ImGui::PopID();
}

void MarketplacePanel::render_server_list() {
    if (list_state_ == FetchState::Error) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Error:");
        ImGui::TextWrapped("%s", list_error_.c_str());
        return;
    }
    
    if (list_state_ == FetchState::Idle && search_result_.servers.empty()) {
        ImGui::TextDisabled("Click 'Browse All' or search to find MCP servers");
        return;
    }
    
    if (list_state_ == FetchState::Loading && search_result_.servers.empty()) {
        ImGui::TextDisabled("Loading...");
        return;
    }
    
    for (const auto& server : search_result_.servers) {
        ImGui::PushID(server.id.c_str());
        
        bool is_selected = (selected_server_.id == server.id);
        
        std::string label = server.display_name;
        if (server.verified) {
            label += " [Verified]";
        }
        
        if (ImGui::Selectable(label.c_str(), is_selected)) {
            selected_server_ = server;
            detail_state_ = FetchState::Success;
        }
        
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(server.description.c_str());
            ImGui::Text("Uses: %d", server.use_count);
            ImGui::EndTooltip();
        }
        
        ImGui::PopID();
    }
    
    ImGui::Separator();
    
    auto& pagination = search_result_.pagination;
    if (pagination.total_pages > 1) {
        ImGui::Text("Page %d / %d (%d total)", 
            pagination.current_page, pagination.total_pages, pagination.total_count);
        
        if (pagination.current_page > 1) {
            if (ImGui::Button("< Prev")) {
                do_search(pagination.current_page - 1);
            }
            ImGui::SameLine();
        }
        
        if (pagination.current_page < pagination.total_pages) {
            if (ImGui::Button("Next >")) {
                do_search(pagination.current_page + 1);
            }
        }
    } else if (pagination.total_count > 0) {
        ImGui::Text("%d servers found", pagination.total_count);
    }
}

void MarketplacePanel::render_server_detail() {
    if (selected_server_.id.empty()) {
        ImGui::TextDisabled("Select a server to view details");
        return;
    }
    
    ImGui::TextWrapped("%s", selected_server_.display_name.c_str());
    
    if (selected_server_.verified) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1), "[Verified]");
    }
    
    ImGui::Separator();
    
    ImGui::TextWrapped("%s", selected_server_.description.c_str());
    
    ImGui::Spacing();
    ImGui::Text("Uses: %d", selected_server_.use_count);
    ImGui::Text("ID: %s", selected_server_.qualified_name.c_str());
    
    if (selected_server_.is_deployed) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1), "Remote: Ready to use");
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    if (ImGui::Button("Install", ImVec2(-1, 30))) {
        show_install_popup_ = true;
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    render_install_terminal();
    
    if (!selected_server_.homepage.empty()) {
        if (ImGui::Button("View on Smithery", ImVec2(-1, 0))) {
#ifdef __APPLE__
            std::string cmd = "open \"" + selected_server_.homepage + "\"";
            system(cmd.c_str());
#endif
        }
    }
}

void MarketplacePanel::render_install_popup() {
    if (show_install_popup_) {
        ImGui::OpenPopup("Install Item");
        show_install_popup_ = false;
    }
    
    if (ImGui::BeginPopupModal("Install Item", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        const bool is_mcp = marketplace_tab_ == 0;
        const char* title = is_mcp ? selected_server_.display_name.c_str()
                                   : (selected_skill_index_ >= 0 && selected_skill_index_ < static_cast<int>(skill_result_.skills.size())
                                       ? skill_result_.skills[selected_skill_index_].display_name.c_str() : "");
        ImGui::Text("Install \"%s\"", title);
        ImGui::Separator();
        
        ImGui::Text("Install to:");
        ImGui::RadioButton("Claude Code (global)", &install_target_, 0);
        ImGui::RadioButton("Codex (global)", &install_target_, 1);

        if (is_mcp) {
            if (install_dir_buffer_[0] == '\0' && !project_directory_.empty()) {
                std::strncpy(install_dir_buffer_.data(), project_directory_.c_str(), install_dir_buffer_.size() - 1);
            }
            ImGui::Spacing();
            ImGui::Text("Install directory:");
            ImGui::SetNextItemWidth(360);
            ImGui::InputText("##InstallDir", install_dir_buffer_.data(), install_dir_buffer_.size());
            ImGui::SameLine();
            if (ImGui::Button("Browse...")) {
                nfdchar_t* out_path = nullptr;
                nfdresult_t result = NFD_PickFolder(&out_path, nullptr);
                if (result == NFD_OKAY && out_path) {
                    std::strncpy(install_dir_buffer_.data(), out_path, install_dir_buffer_.size() - 1);
                    install_dir_buffer_[install_dir_buffer_.size() - 1] = '\0';
                }
                if (out_path) {
                    NFD_FreePath(out_path);
                }
            }
        }
        
        ImGui::Spacing();
        
        if (ImGui::Button("Install", ImVec2(120, 0))) {
            if (is_mcp) {
                install_server_cli(selected_server_, install_target_ == 0);
            } else if (selected_skill_index_ >= 0 && selected_skill_index_ < static_cast<int>(skill_result_.skills.size())) {
                install_skill(skill_result_.skills[selected_skill_index_], install_target_ == 0);
            }
            
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void MarketplacePanel::render_skill_list() {
    if (detail_state_ == FetchState::Error) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Error:");
        ImGui::TextWrapped("%s", skill_error_.c_str());
        return;
    }
    
    if (detail_state_ == FetchState::Idle && skill_result_.skills.empty()) {
        ImGui::TextDisabled("Click 'Browse All' or search to find skills");
        return;
    }
    
    if (detail_state_ == FetchState::Loading && skill_result_.skills.empty()) {
        ImGui::TextDisabled("Loading...");
        return;
    }
    
    for (size_t i = 0; i < skill_result_.skills.size(); ++i) {
        const auto& skill = skill_result_.skills[i];
        ImGui::PushID(skill.id.c_str());
        
        bool is_selected = static_cast<int>(i) == selected_skill_index_;
        
        std::string label = skill.display_name.empty() ? skill.slug : skill.display_name;
        if (ImGui::Selectable(label.c_str(), is_selected)) {
            select_skill(skill);
        }
        
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(skill.description.c_str());
            ImGui::Text("Uses: %d", skill.total_activations);
            ImGui::EndTooltip();
        }
        
        ImGui::PopID();
    }
    
    ImGui::Separator();
    
    auto& pagination = skill_result_.pagination;
    if (pagination.total_pages > 1) {
        ImGui::Text("Page %d / %d (%d total)", 
            pagination.current_page, pagination.total_pages, pagination.total_count);
        
        if (pagination.current_page > 1) {
            if (ImGui::Button("< Prev")) {
                do_skill_search(pagination.current_page - 1);
            }
            ImGui::SameLine();
        }
        
        if (pagination.current_page < pagination.total_pages) {
            if (ImGui::Button("Next >")) {
                do_skill_search(pagination.current_page + 1);
            }
        }
    } else if (pagination.total_count > 0) {
        ImGui::Text("%d skills found", pagination.total_count);
    }
}

void MarketplacePanel::render_skill_detail() {
    if (selected_skill_index_ < 0 || selected_skill_index_ >= static_cast<int>(skill_result_.skills.size())) {
        ImGui::TextDisabled("Select a skill to view details");
        return;
    }
    
    const auto& skill = skill_result_.skills[selected_skill_index_];
    
    ImGui::TextWrapped("%s", skill.display_name.empty() ? skill.slug.c_str() : skill.display_name.c_str());
    ImGui::Separator();
    ImGui::TextWrapped("%s", skill.description.c_str());
    
    ImGui::Spacing();
    ImGui::Text("Namespace: %s", skill.namespace_name.c_str());
    ImGui::Text("Slug: %s", skill.slug.c_str());
    ImGui::Text("Activations: %d", skill.total_activations);
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    if (ImGui::Button("Install", ImVec2(-1, 30))) {
        show_install_popup_ = true;
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    render_install_terminal();
    
    if (!skill.git_url.empty()) {
        if (ImGui::Button("View Repo", ImVec2(-1, 0))) {
#ifdef __APPLE__
            std::string cmd = "open \"" + skill.git_url + "\"";
            system(cmd.c_str());
#endif
        }
    }
}

void MarketplacePanel::do_search(int page) {
    list_state_ = FetchState::Loading;
    list_error_.clear();
    
    client_.search_servers_async(search_query_, page, 15,
        [this](bool success, McpSearchResult result, std::string error) {
            if (success) {
                search_result_ = result;
                list_state_ = FetchState::Success;
            } else {
                list_error_ = error;
                list_state_ = FetchState::Error;
            }
        });
}

void MarketplacePanel::do_skill_search(int page) {
    detail_state_ = FetchState::Loading;
    skill_error_.clear();
    
    client_.search_skills_async(search_query_, page, 15,
        [this](bool success, SkillSearchResult result, std::string error) {
            if (success) {
                skill_result_ = result;
                detail_state_ = FetchState::Success;
            } else {
                skill_error_ = error;
                detail_state_ = FetchState::Error;
            }
        });
}

void MarketplacePanel::select_server(const McpServerEntry& server) {
    selected_server_ = server;
    detail_state_ = FetchState::Loading;
    detail_error_.clear();
    
    client_.get_server_detail_async(server.qualified_name,
        [this, server](bool success, McpServerEntry detail, std::string error) {
            if (success) {
                selected_server_ = detail;
                detail_state_ = FetchState::Success;
                return;
            }
            
            std::string retry_error;
            auto fallback = client_.get_server_detail_by_id(server.id, retry_error);
            if (retry_error.empty()) {
                selected_server_ = fallback;
                detail_state_ = FetchState::Success;
            } else {
                detail_error_ = retry_error.empty() ? error : retry_error;
                detail_state_ = FetchState::Error;
            }
        });
}

void MarketplacePanel::select_skill(const SkillEntry& skill) {
    selected_skill_index_ = -1;
    for (size_t i = 0; i < skill_result_.skills.size(); ++i) {
        if (skill_result_.skills[i].id == skill.id) {
            selected_skill_index_ = static_cast<int>(i);
            break;
        }
    }
}

void MarketplacePanel::install_server(const McpServerEntry& server, bool project_scope) {
    std::string config_path;
    
    if (project_scope) {
        if (project_directory_.empty()) {
            status_message_ = "Error: No project directory set";
            status_is_error_ = true;
            status_time_ = 3.0f;
            return;
        }
        config_path = project_directory_ + "/.mcp.json";
    } else {
        const char* home = getenv("HOME");
        if (!home) {
            status_message_ = "Error: Cannot find home directory";
            status_is_error_ = true;
            status_time_ = 3.0f;
            return;
        }
        config_path = std::string(home) + "/.claude.json";
    }
    
    nlohmann::json config;
    
    std::ifstream in(config_path);
    if (in.good()) {
        try {
            in >> config;
        } catch (...) {
            config = nlohmann::json::object();
        }
    }
    in.close();
    
    if (!config.contains("mcpServers")) {
        config["mcpServers"] = nlohmann::json::object();
    }
    
    std::string deployment_url = server.deployment_url;
    if (deployment_url.empty()) {
        deployment_url = "https://server.smithery.ai/" + server.qualified_name + "/mcp";
    }
    
    nlohmann::json server_config;
    server_config["type"] = "http";
    server_config["url"] = deployment_url;
    
    config["mcpServers"][server.qualified_name] = server_config;
    
    std::ofstream out(config_path);
    if (!out.good()) {
        status_message_ = "Error: Cannot write to " + config_path;
        status_is_error_ = true;
        status_time_ = 3.0f;
        return;
    }
    
    out << config.dump(2);
    out.close();
    
    status_message_ = "Install completed: " + server.display_name;
    status_is_error_ = false;
    status_time_ = 3.0f;
    
    if (install_callback_) {
        McpConnectionConfig conn;
        conn.type = "http";
        conn.url = deployment_url;
        install_callback_(server.qualified_name, conn);
    }
}

void MarketplacePanel::install_server_codex(const McpServerEntry& server) {
    const char* home = getenv("HOME");
    if (!home) {
        status_message_ = "Error: Cannot find home directory";
        status_is_error_ = true;
        status_time_ = 3.0f;
        return;
    }
    
    std::string config_path = std::string(home) + "/.codex/config.toml";
    toml::table tbl;
    
    try {
        tbl = toml::parse_file(config_path);
    } catch (...) {
        tbl = toml::table{};
    }
    
    std::string deployment_url = server.deployment_url;
    if (deployment_url.empty()) {
        deployment_url = "https://server.smithery.ai/" + server.qualified_name + "/mcp";
    }
    
    toml::table mcp_tbl = toml::table{};
    if (auto existing = tbl["mcp_servers"].as_table()) {
        mcp_tbl = *existing;
    }
    
    toml::table server_tbl;
    server_tbl.insert_or_assign("url", deployment_url);
    server_tbl.insert_or_assign("enabled", true);
    mcp_tbl.insert_or_assign(server.qualified_name, server_tbl);
    tbl.insert_or_assign("mcp_servers", mcp_tbl);
    
    std::ofstream out(config_path);
    if (!out.good()) {
        status_message_ = "Error: Cannot write to " + config_path;
        status_is_error_ = true;
        status_time_ = 3.0f;
        return;
    }
    
    out << tbl;
    out.close();
    
    status_message_ = "Installed for Codex: " + server.display_name;
    status_is_error_ = false;
    status_time_ = 3.0f;
}

void MarketplacePanel::install_server_cli(const McpServerEntry& server, bool use_claude) {
    std::string key = api_key_buffer_.data();
    if (key.empty()) {
        std::lock_guard<std::mutex> lock(install_mutex_);
        status_message_ = "Error: Smithery API key is required";
        status_is_error_ = true;
        status_time_ = 3.0f;
        return;
    }
    {
        std::lock_guard<std::mutex> lock(install_mutex_);
        if (install_in_progress_) {
            status_message_ = "Install already in progress";
            status_is_error_ = true;
            status_time_ = 3.0f;
            return;
        }
        install_in_progress_ = true;
        cli_log_.clear();
        install_terminal_ = std::make_unique<VTerminal>(24, 80);
        install_terminal_user_scrolled_ = false;
        focus_install_terminal_ = true;
        status_message_ = "Installing via Smithery CLI...";
        status_is_error_ = false;
        status_time_ = 3.0f;
    }
    
    std::string client = use_claude ? "claude-code" : "codex";
    std::string pkg = !server.qualified_name.empty() ? server.qualified_name : server.id;
    std::string cmd = "SMITHERY_API_KEY='" + key + "' npx -y @smithery/cli@latest install " + pkg + " --client " + client;
#ifdef __APPLE__
    cmd = "bash -lc \"" + cmd + "\"";
#endif
    cmd += " 2>&1";

    std::string display_name = server.display_name.empty() ? pkg : server.display_name;
    install_command_queue_.clear();
    install_command_queue_.push_back(cmd);
    install_workdir_.clear();
    if (install_dir_buffer_[0] != '\0') {
        install_workdir_ = install_dir_buffer_.data();
    }
    if (install_workdir_.empty()) {
        install_workdir_ = project_directory_;
    }
    if (install_workdir_.empty()) {
        const char* home = getenv("HOME");
        if (home) {
            install_workdir_ = home;
        }
    }

    install_display_name_ = display_name;
    start_install_queue();
}

void MarketplacePanel::append_cli_log(const std::string& line) {
    std::lock_guard<std::mutex> lock(install_mutex_);
    std::string cleaned = strip_ansi_sequences(line);
    cli_log_ += cleaned;
    if (!install_process_.is_running() && install_terminal_) {
        install_terminal_->write(cleaned.c_str(), cleaned.size());
    }
    if (cli_log_.size() > 4096) {
        cli_log_.erase(0, cli_log_.size() - 4096);
    }
}

void MarketplacePanel::install_skill(const SkillEntry& skill, bool use_claude) {
    {
        std::lock_guard<std::mutex> lock(install_mutex_);
        if (install_in_progress_) {
            status_message_ = "Install already in progress";
            status_is_error_ = true;
            status_time_ = 3.0f;
            return;
        }
        install_in_progress_ = true;
        cli_log_.clear();
        install_terminal_ = std::make_unique<VTerminal>(24, 80);
        install_terminal_user_scrolled_ = false;
        focus_install_terminal_ = true;
        status_message_ = "Installing skill...";
        status_is_error_ = false;
        status_time_ = 3.0f;
    }

    append_cli_log("Installing skill...\n");

    std::string base_dir;
    const char* home = getenv("HOME");
    if (!home) {
        status_message_ = "Error: Cannot find home directory";
        status_is_error_ = true;
        status_time_ = 3.0f;
        append_cli_log("Error: HOME not set\n");
        install_in_progress_ = false;
        return;
    }
    
    if (use_claude) {
        base_dir = std::string(home) + "/.claude/skills";
    } else {
        base_dir = std::string(home) + "/.codex/skills";
    }
    
    std::string skill_name = skill.slug.empty() ? skill.id : skill.slug;
    std::string skill_dir = base_dir + "/" + skill_name;
    std::string skill_path = skill_dir + "/SKILL.md";
    
    std::error_code ec;
    std::filesystem::create_directories(skill_dir, ec);
    if (ec) {
        status_message_ = "Error: Cannot create skill directory";
        status_is_error_ = true;
        status_time_ = 3.0f;
        append_cli_log("Error: Cannot create directory " + skill_dir + "\n");
        install_in_progress_ = false;
        return;
    }
    
    std::ofstream out(skill_path);
    if (!out.good()) {
        status_message_ = "Error: Cannot write to " + skill_path;
        status_is_error_ = true;
        status_time_ = 3.0f;
        append_cli_log("Error: Cannot write " + skill_path + "\n");
        install_in_progress_ = false;
        return;
    }
    
    out << "---\n";
    out << "name: " << skill_name << "\n";
    if (!skill.description.empty()) {
        out << "description: " << skill.description << "\n";
    }
    out << "---\n\n";
    if (!skill.prompt.empty()) {
        out << skill.prompt << "\n";
    }
    out.close();
    
    append_cli_log("Wrote " + skill_path + "\n");
    status_message_ = "Install completed: " + (skill.display_name.empty() ? skill_name : skill.display_name);
    status_is_error_ = false;
    status_time_ = 3.0f;
    install_in_progress_ = false;
}

void MarketplacePanel::render_installed_mcp_list() {
    if (installed_mcp_items_.empty()) {
        ImGui::TextDisabled("No installed MCP servers found");
        return;
    }

    for (size_t i = 0; i < installed_mcp_items_.size(); ++i) {
        const auto& item = installed_mcp_items_[i];
        ImGui::PushID(static_cast<int>(i));
        bool is_selected = static_cast<int>(i) == selected_installed_mcp_index_;
        std::string label = item.use_claude ? "Claude Code: " : "Codex: ";
        label += item.name;
        if (!item.project_dir.empty()) {
            label += " (" + item.project_dir + ")";
        }
        if (ImGui::Selectable(label.c_str(), is_selected)) {
            selected_installed_mcp_index_ = static_cast<int>(i);
        }
        ImGui::PopID();
    }
}

void MarketplacePanel::render_installed_mcp_detail() {
    if (selected_installed_mcp_index_ < 0 || selected_installed_mcp_index_ >= static_cast<int>(installed_mcp_items_.size())) {
        ImGui::TextDisabled("Select an installed MCP to view details");
        return;
    }

    const auto& item = installed_mcp_items_[selected_installed_mcp_index_];
    ImGui::TextWrapped("%s", item.name.c_str());
    ImGui::Text("Target: %s", item.use_claude ? "Claude Code" : "Codex");
    if (!item.project_dir.empty()) {
        ImGui::TextWrapped("Project: %s", item.project_dir.c_str());
    }
    ImGui::Separator();

    if (!item.config.type.empty()) {
        ImGui::Text("Type: %s", item.config.type.c_str());
    }
    if (!item.config.url.empty()) {
        ImGui::TextWrapped("URL: %s", item.config.url.c_str());
    }
    if (!item.config.command.empty()) {
        ImGui::TextWrapped("Command: %s", item.config.command.c_str());
    }
    if (!item.config.args.empty()) {
        std::string args;
        for (size_t i = 0; i < item.config.args.size(); ++i) {
            if (i > 0) args += " ";
            args += item.config.args[i];
        }
        ImGui::TextWrapped("Args: %s", args.c_str());
    }
    if (!item.config.env.empty()) {
        ImGui::Text("Env:");
        for (const auto& [key, value] : item.config.env) {
            ImGui::Text("%s=%s", key.c_str(), value.c_str());
        }
    }
    if (!item.config.headers.empty()) {
        ImGui::Text("Headers:");
        for (const auto& [key, value] : item.config.headers) {
            ImGui::Text("%s: %s", key.c_str(), value.c_str());
        }
    }

    ImGui::Spacing();
    if (ImGui::Button("Remove", ImVec2(-1, 0))) {
        remove_installed_mcp(item);
        refresh_installed_mcp();
    }
}

void MarketplacePanel::render_installed_skill_list() {
    if (installed_skill_items_.empty()) {
        ImGui::TextDisabled("No installed skills found");
        return;
    }

    for (size_t i = 0; i < installed_skill_items_.size(); ++i) {
        const auto& item = installed_skill_items_[i];
        ImGui::PushID(static_cast<int>(i));
        bool is_selected = static_cast<int>(i) == selected_installed_skill_index_;
        std::string label = item.use_claude ? "Claude Code: " : "Codex: ";
        label += item.name;
        if (ImGui::Selectable(label.c_str(), is_selected)) {
            selected_installed_skill_index_ = static_cast<int>(i);
        }
        ImGui::PopID();
    }
}

void MarketplacePanel::render_installed_skill_detail() {
    if (selected_installed_skill_index_ < 0 || selected_installed_skill_index_ >= static_cast<int>(installed_skill_items_.size())) {
        ImGui::TextDisabled("Select an installed skill to view details");
        return;
    }

    const auto& item = installed_skill_items_[selected_installed_skill_index_];
    ImGui::TextWrapped("%s", item.name.c_str());
    ImGui::Text("Target: %s", item.use_claude ? "Claude Code" : "Codex");
    ImGui::TextWrapped("Path: %s", item.path.c_str());

    ImGui::Spacing();
    if (ImGui::Button("Remove", ImVec2(-1, 0))) {
        remove_installed_skill(item);
        refresh_installed_skills();
    }
}

void MarketplacePanel::refresh_installed_mcp() {
    installed_mcp_items_.clear();
    selected_installed_mcp_index_ = -1;

    const char* home = getenv("HOME");
    if (!home) {
        return;
    }

    {
        std::string config_path = std::string(home) + "/.claude.json";
        nlohmann::json config;
        std::ifstream in(config_path);
        if (in.good()) {
            try {
                in >> config;
            } catch (...) {
                config = nlohmann::json::object();
            }
        }
        in.close();

        if (config.contains("mcpServers") && config["mcpServers"].is_object()) {
            for (const auto& [key, value] : config["mcpServers"].items()) {
                InstalledMcpItem item;
                item.name = key;
                item.config = parse_mcp_config(value);
                item.use_claude = true;
                installed_mcp_items_.push_back(item);
            }
        }

        if (config.contains("projects") && config["projects"].is_object()) {
            for (const auto& [proj_key, proj_val] : config["projects"].items()) {
                if (!proj_val.is_object()) {
                    continue;
                }
                if (proj_val.contains("mcpServers") && proj_val["mcpServers"].is_object()) {
                    for (const auto& [key, value] : proj_val["mcpServers"].items()) {
                        InstalledMcpItem item;
                        item.name = key;
                        item.config = parse_mcp_config(value);
                        item.use_claude = true;
                        item.project_dir = proj_key;
                        installed_mcp_items_.push_back(item);
                    }
                }
            }
        }
    }

    {
        std::string config_path = std::string(home) + "/.codex/config.toml";
        toml::table tbl;
        try {
            tbl = toml::parse_file(config_path);
        } catch (...) {
            tbl = toml::table{};
        }

        if (auto mcp_tbl = tbl["mcp_servers"].as_table()) {
            for (auto&& [key, value] : *mcp_tbl) {
                if (auto server_tbl = value.as_table()) {
                    InstalledMcpItem item;
                    item.name = std::string(key);
                    item.use_claude = false;
                    if (auto v = (*server_tbl)["url"].value<std::string>()) {
                        item.config.type = "http";
                        item.config.url = *v;
                    }
                    if (auto v = (*server_tbl)["command"].value<std::string>()) {
                        item.config.type = "stdio";
                        item.config.command = *v;
                    }
                    if (auto arr = (*server_tbl)["args"].as_array()) {
                        for (auto&& elem : *arr) {
                            if (auto s = elem.value<std::string>()) item.config.args.push_back(*s);
                        }
                    }
                    if (auto env_tbl = (*server_tbl)["env"].as_table()) {
                        for (auto&& [k, val] : *env_tbl) {
                            if (auto s = val.value<std::string>()) item.config.env[std::string(k)] = *s;
                        }
                    }
                    if (auto headers_tbl = (*server_tbl)["http_headers"].as_table()) {
                        for (auto&& [k, val] : *headers_tbl) {
                            if (auto s = val.value<std::string>()) item.config.headers[std::string(k)] = *s;
                        }
                    }
                    installed_mcp_items_.push_back(item);
                }
            }
        }
    }

    std::sort(installed_mcp_items_.begin(), installed_mcp_items_.end(),
        [](const InstalledMcpItem& a, const InstalledMcpItem& b) {
            if (a.use_claude != b.use_claude) return a.use_claude > b.use_claude;
            return a.name < b.name;
        });
}

void MarketplacePanel::refresh_installed_skills() {
    installed_skill_items_.clear();
    selected_installed_skill_index_ = -1;

    const char* home = getenv("HOME");
    if (!home) {
        return;
    }

    auto load_dir = [this](const std::string& base_dir, bool use_claude) {
        std::error_code ec;
        if (!std::filesystem::exists(base_dir, ec)) {
            return;
        }
        for (const auto& entry : std::filesystem::directory_iterator(base_dir, ec)) {
            if (ec) {
                return;
            }
            if (!entry.is_directory()) {
                continue;
            }
            InstalledSkillItem item;
            item.name = entry.path().filename().string();
            item.path = entry.path().string();
            item.use_claude = use_claude;
            installed_skill_items_.push_back(item);
        }
    };

    load_dir(std::string(home) + "/.claude/skills", true);
    load_dir(std::string(home) + "/.codex/skills", false);

    std::sort(installed_skill_items_.begin(), installed_skill_items_.end(),
        [](const InstalledSkillItem& a, const InstalledSkillItem& b) {
            if (a.use_claude != b.use_claude) return a.use_claude > b.use_claude;
            return a.name < b.name;
        });
}

void MarketplacePanel::remove_installed_mcp(const InstalledMcpItem& item) {
    const char* home = getenv("HOME");
    if (!home) {
        status_message_ = "Error: Cannot find home directory";
        status_is_error_ = true;
        status_time_ = 3.0f;
        return;
    }

    if (item.use_claude) {
        std::string config_path = std::string(home) + "/.claude.json";
        nlohmann::json config;
        std::ifstream in(config_path);
        if (in.good()) {
            try {
                in >> config;
            } catch (...) {
                config = nlohmann::json::object();
            }
        }
        in.close();

        if (!item.project_dir.empty()) {
            if (config.contains("projects") && config["projects"].is_object()) {
                auto& projects = config["projects"];
                if (projects.contains(item.project_dir) && projects[item.project_dir].is_object()) {
                    auto& proj = projects[item.project_dir];
                    if (proj.contains("mcpServers") && proj["mcpServers"].is_object()) {
                        proj["mcpServers"].erase(item.name);
                    }
                }
            }
        } else if (config.contains("mcpServers") && config["mcpServers"].is_object()) {
            config["mcpServers"].erase(item.name);
        }

        std::ofstream out(config_path);
        if (!out.good()) {
            status_message_ = "Error: Cannot write to " + config_path;
            status_is_error_ = true;
            status_time_ = 3.0f;
            return;
        }
        out << config.dump(2);
        out.close();
    } else {
        std::string config_path = std::string(home) + "/.codex/config.toml";
        toml::table tbl;
        try {
            tbl = toml::parse_file(config_path);
        } catch (...) {
            tbl = toml::table{};
        }

        if (auto mcp_tbl = tbl["mcp_servers"].as_table()) {
            mcp_tbl->erase(item.name);
            if (mcp_tbl->empty()) {
                tbl.erase("mcp_servers");
            }
        }

        std::ofstream out(config_path);
        if (!out.good()) {
            status_message_ = "Error: Cannot write to " + config_path;
            status_is_error_ = true;
            status_time_ = 3.0f;
            return;
        }
        out << tbl;
        out.close();
    }

    status_message_ = "Removed: " + item.name;
    status_is_error_ = false;
    status_time_ = 3.0f;
}

void MarketplacePanel::remove_installed_skill(const InstalledSkillItem& item) {
    std::error_code ec;
    std::filesystem::remove_all(item.path, ec);
    if (ec) {
        status_message_ = "Error: Cannot remove " + item.name;
        status_is_error_ = true;
        status_time_ = 3.0f;
        return;
    }

    status_message_ = "Removed: " + item.name;
    status_is_error_ = false;
    status_time_ = 3.0f;
}

void MarketplacePanel::render_install_terminal() {
    ImGui::Text("Smithery CLI Output:");
    ImGui::BeginChild("InstallTerminal", ImVec2(0, 240), true, ImGuiWindowFlags_HorizontalScrollbar);

    if (focus_install_terminal_) {
        ImGui::SetWindowFocus();
        focus_install_terminal_ = false;
    }
    if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)) {
        ImGui::SetWindowFocus();
    }

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 content_size = ImGui::GetContentRegionAvail();
    ImVec2 char_size = ImGui::CalcTextSize("W");
    float line_height = ImGui::GetTextLineHeight();
    int cols = std::max(80, static_cast<int>(content_size.x / char_size.x));
    int rows = std::max(10, static_cast<int>(std::ceil(content_size.y / line_height)));

    {
        std::lock_guard<std::mutex> lock(install_mutex_);
        if (install_terminal_) {
            if (install_terminal_->cols() != cols || install_terminal_->rows() != rows) {
                install_terminal_->resize(rows, cols);
            }

            const auto& theme = get_current_theme();
            bool is_light_theme = (theme.kind == ThemeKind::Light);
            const auto& scrollback = install_terminal_->scrollback();
            int total_lines = static_cast<int>(scrollback.size()) + install_terminal_->rows();

            ImGuiListClipper clipper;
            clipper.Begin(total_lines, line_height);
            ImDrawList* draw_list = ImGui::GetWindowDrawList();

            while (clipper.Step()) {
                for (int line_idx = clipper.DisplayStart; line_idx < clipper.DisplayEnd; ++line_idx) {
                    ImVec2 start_pos = ImGui::GetCursorScreenPos();
                    if (line_idx < static_cast<int>(scrollback.size())) {
                        const auto& line = scrollback[static_cast<size_t>(line_idx)];
                        render_terminal_line(line.data(), static_cast<int>(line.size()), line_height, draw_list, start_pos, char_size.x, is_light_theme);
                    } else {
                        int screen_row = line_idx - static_cast<int>(scrollback.size());
                        std::vector<TerminalCell> row_cells;
                        row_cells.reserve(static_cast<size_t>(install_terminal_->cols()));
                        for (int col = 0; col < install_terminal_->cols(); ++col) {
                            row_cells.push_back(install_terminal_->get_cell(screen_row, col));
                        }
                        render_terminal_line(row_cells.data(), static_cast<int>(row_cells.size()), line_height, draw_list, start_pos, char_size.x, is_light_theme);
                    }
                }
            }
        }
    }

    float scroll_max = ImGui::GetScrollMaxY();
    float scroll_y = ImGui::GetScrollY();
    if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f) {
        install_terminal_user_scrolled_ = true;
    }
    if (scroll_max <= 0.0f || scroll_y >= scroll_max - 1.0f) {
        install_terminal_user_scrolled_ = false;
    }
    if (!install_terminal_user_scrolled_) {
        ImGui::SetScrollHereY(1.0f);
    }

    bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (install_process_.is_running() && focused) {
        if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
            install_process_.write_stdin("\n");
        } else if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
            install_process_.write_stdin("\x7f");
        } else if (ImGui::IsKeyPressed(ImGuiKey_Tab)) {
            install_process_.write_stdin("\t");
        } else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            install_process_.write_stdin("\x1b[A");
        } else if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            install_process_.write_stdin("\x1b[B");
        } else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
            install_process_.write_stdin("\x1b[D");
        } else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
            install_process_.write_stdin("\x1b[C");
        }

        if (!io.KeyCtrl && !io.KeySuper) {
            for (int i = 0; i < io.InputQueueCharacters.Size; ++i) {
                ImWchar c = io.InputQueueCharacters[i];
                if (c > 0 && c != 127) {
                    char utf8_buf[8];
                    int len = 0;
                    utf8_encode(static_cast<uint32_t>(c), utf8_buf, &len);
                    install_process_.write_stdin(std::string(utf8_buf, utf8_buf + len));
                }
            }
        }
    }

    ImGui::EndChild();
}

void MarketplacePanel::start_install_queue() {
    if (install_process_.is_running() || install_command_queue_.empty()) {
        return;
    }
    start_install_command(install_command_queue_.front());
}

void MarketplacePanel::start_install_command(const std::string& cmd) {
    ProcessConfig config;
    config.executable = "bash";
    config.args = {"-lc", cmd};
    config.working_dir = install_workdir_;
    install_process_.set_output_callback([this](const std::string& data, bool) {
        {
            std::lock_guard<std::mutex> lock(install_mutex_);
            if (install_terminal_) {
                install_terminal_->write(data.c_str(), data.size());
            }
        }
        append_cli_log(data);
    });
    install_process_.set_exit_callback([this](int exit_code) {
        std::string next_cmd;
        {
            std::lock_guard<std::mutex> lock(install_mutex_);
            if (exit_code != 0) {
                status_message_ = "Error: Install command failed (exit " + std::to_string(exit_code) + ")";
                status_is_error_ = true;
                status_time_ = 4.0f;
                install_in_progress_ = false;
                install_command_queue_.clear();
                return;
            }

            if (!install_command_queue_.empty()) {
                install_command_queue_.erase(install_command_queue_.begin());
            }

            if (install_command_queue_.empty()) {
                status_message_ = "Install completed: " + install_display_name_;
                status_is_error_ = false;
                status_time_ = 3.0f;
                install_in_progress_ = false;
                return;
            }

            next_cmd = install_command_queue_.front();
        }
        start_install_command(next_cmd);
    });

    if (!install_process_.start(config)) {
        std::lock_guard<std::mutex> lock(install_mutex_);
        status_message_ = "Error: Failed to start install process";
        status_is_error_ = true;
        status_time_ = 4.0f;
        install_in_progress_ = false;
        install_command_queue_.clear();
    }
}

}
