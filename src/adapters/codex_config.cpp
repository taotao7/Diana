#include "codex_config.h"
#include <chrono>
#include <sstream>

namespace diana {

CodexConfig CodexConfig::from_toml(const toml::table& tbl) {
    CodexConfig cfg;
    
    if (auto v = tbl["approval_policy"].value<std::string>()) cfg.approval_policy = *v;
    if (auto v = tbl["model"].value<std::string>()) cfg.model = *v;
    if (auto v = tbl["model_provider"].value<std::string>()) cfg.model_provider = *v;
    
    if (auto v = tbl["chatgpt_base_url"].value<std::string>()) cfg.chatgpt_base_url = *v;
    if (auto v = tbl["check_for_update_on_startup"].value<bool>()) cfg.check_for_update_on_startup = *v;
    if (auto v = tbl["cli_auth_credentials_store"].value<std::string>()) cfg.cli_auth_credentials_store = *v;
    if (auto v = tbl["compact_prompt"].value<std::string>()) cfg.compact_prompt = *v;
    if (auto v = tbl["developer_instructions"].value<std::string>()) cfg.developer_instructions = *v;
    if (auto v = tbl["disable_paste_burst"].value<bool>()) cfg.disable_paste_burst = *v;
    if (auto v = tbl["experimental_compact_prompt_file"].value<std::string>()) cfg.experimental_compact_prompt_file = *v;
    if (auto v = tbl["experimental_instructions_file"].value<std::string>()) cfg.experimental_instructions_file = *v;
    if (auto v = tbl["file_opener"].value<std::string>()) cfg.file_opener = *v;
    if (auto v = tbl["forced_chatgpt_workspace_id"].value<std::string>()) cfg.forced_chatgpt_workspace_id = *v;
    if (auto v = tbl["forced_login_method"].value<std::string>()) cfg.forced_login_method = *v;
    if (auto v = tbl["hide_agent_reasoning"].value<bool>()) cfg.hide_agent_reasoning = *v;
    if (auto v = tbl["instructions"].value<std::string>()) cfg.instructions = *v;
    if (auto v = tbl["mcp_oauth_credentials_store"].value<std::string>()) cfg.mcp_oauth_credentials_store = *v;
    if (auto v = tbl["model_auto_compact_token_limit"].value<int64_t>()) cfg.model_auto_compact_token_limit = static_cast<int>(*v);
    if (auto v = tbl["model_context_window"].value<int64_t>()) cfg.model_context_window = static_cast<int>(*v);
    if (auto v = tbl["model_reasoning_effort"].value<std::string>()) cfg.model_reasoning_effort = *v;
    if (auto v = tbl["model_reasoning_summary"].value<std::string>()) cfg.model_reasoning_summary = *v;
    if (auto v = tbl["model_supports_reasoning_summaries"].value<bool>()) cfg.model_supports_reasoning_summaries = *v;
    if (auto v = tbl["model_verbosity"].value<std::string>()) cfg.model_verbosity = *v;
    if (auto v = tbl["oss_provider"].value<std::string>()) cfg.oss_provider = *v;
    if (auto v = tbl["profile"].value<std::string>()) cfg.profile = *v;
    if (auto v = tbl["project_doc_max_bytes"].value<int64_t>()) cfg.project_doc_max_bytes = static_cast<int>(*v);
    if (auto v = tbl["review_model"].value<std::string>()) cfg.review_model = *v;
    if (auto v = tbl["sandbox_mode"].value<std::string>()) cfg.sandbox_mode = *v;
    if (auto v = tbl["show_raw_agent_reasoning"].value<bool>()) cfg.show_raw_agent_reasoning = *v;
    if (auto v = tbl["tool_output_token_limit"].value<int64_t>()) cfg.tool_output_token_limit = static_cast<int>(*v);
    if (auto v = tbl["windows_wsl_setup_acknowledged"].value<bool>()) cfg.windows_wsl_setup_acknowledged = *v;
    
    if (auto v = tbl["experimental_use_freeform_apply_patch"].value<bool>()) cfg.experimental_use_freeform_apply_patch = *v;
    if (auto v = tbl["experimental_use_unified_exec_tool"].value<bool>()) cfg.experimental_use_unified_exec_tool = *v;
    if (auto v = tbl["include_apply_patch_tool"].value<bool>()) cfg.include_apply_patch_tool = *v;
    
    if (auto notify_arr = tbl["notify"].as_array()) {
        for (auto&& elem : *notify_arr) {
            if (auto s = elem.value<std::string>()) cfg.notify.push_back(*s);
        }
    }
    
    if (auto arr = tbl["project_doc_fallback_filenames"].as_array()) {
        for (auto&& elem : *arr) {
            if (auto s = elem.value<std::string>()) cfg.project_doc_fallback_filenames.push_back(*s);
        }
    }
    
    if (auto arr = tbl["project_root_markers"].as_array()) {
        for (auto&& elem : *arr) {
            if (auto s = elem.value<std::string>()) cfg.project_root_markers.push_back(*s);
        }
    }
    
    if (auto features_tbl = tbl["features"].as_table()) {
        if (auto v = (*features_tbl)["apply_patch_freeform"].value<bool>()) cfg.features.apply_patch_freeform = *v;
        if (auto v = (*features_tbl)["elevated_windows_sandbox"].value<bool>()) cfg.features.elevated_windows_sandbox = *v;
        if (auto v = (*features_tbl)["exec_policy"].value<bool>()) cfg.features.exec_policy = *v;
        if (auto v = (*features_tbl)["experimental_windows_sandbox"].value<bool>()) cfg.features.experimental_windows_sandbox = *v;
        if (auto v = (*features_tbl)["powershell_utf8"].value<bool>()) cfg.features.powershell_utf8 = *v;
        if (auto v = (*features_tbl)["remote_compaction"].value<bool>()) cfg.features.remote_compaction = *v;
        if (auto v = (*features_tbl)["remote_models"].value<bool>()) cfg.features.remote_models = *v;
        if (auto v = (*features_tbl)["shell_snapshot"].value<bool>()) cfg.features.shell_snapshot = *v;
        if (auto v = (*features_tbl)["shell_tool"].value<bool>()) cfg.features.shell_tool = *v;
        if (auto v = (*features_tbl)["tui2"].value<bool>()) cfg.features.tui2 = *v;
        if (auto v = (*features_tbl)["unified_exec"].value<bool>()) cfg.features.unified_exec = *v;
        if (auto v = (*features_tbl)["web_search_request"].value<bool>()) cfg.features.web_search_request = *v;
    }
    
    if (auto feedback_tbl = tbl["feedback"].as_table()) {
        if (auto v = (*feedback_tbl)["enabled"].value<bool>()) cfg.feedback.enabled = *v;
    }
    
    if (auto history_tbl = tbl["history"].as_table()) {
        if (auto v = (*history_tbl)["max_bytes"].value<int64_t>()) cfg.history.max_bytes = *v;
        if (auto v = (*history_tbl)["persistence"].value<std::string>()) cfg.history.persistence = *v;
    }
    
    if (auto tui_tbl = tbl["tui"].as_table()) {
        if (auto v = (*tui_tbl)["animations"].value<bool>()) cfg.tui.animations = *v;
        if (auto v = (*tui_tbl)["notifications"].value<bool>()) cfg.tui.notifications = *v;
        if (auto v = (*tui_tbl)["scroll_events_per_tick"].value<int64_t>()) cfg.tui.scroll_events_per_tick = static_cast<int>(*v);
        if (auto v = (*tui_tbl)["scroll_invert"].value<bool>()) cfg.tui.scroll_invert = *v;
        if (auto v = (*tui_tbl)["scroll_mode"].value<std::string>()) cfg.tui.scroll_mode = *v;
        if (auto v = (*tui_tbl)["scroll_trackpad_accel_events"].value<int64_t>()) cfg.tui.scroll_trackpad_accel_events = static_cast<int>(*v);
        if (auto v = (*tui_tbl)["scroll_trackpad_accel_max"].value<int64_t>()) cfg.tui.scroll_trackpad_accel_max = static_cast<int>(*v);
        if (auto v = (*tui_tbl)["scroll_trackpad_lines"].value<int64_t>()) cfg.tui.scroll_trackpad_lines = static_cast<int>(*v);
        if (auto v = (*tui_tbl)["scroll_wheel_like_max_duration_ms"].value<int64_t>()) cfg.tui.scroll_wheel_like_max_duration_ms = static_cast<int>(*v);
        if (auto v = (*tui_tbl)["scroll_wheel_lines"].value<int64_t>()) cfg.tui.scroll_wheel_lines = static_cast<int>(*v);
        if (auto v = (*tui_tbl)["scroll_wheel_tick_detect_max_ms"].value<int64_t>()) cfg.tui.scroll_wheel_tick_detect_max_ms = static_cast<int>(*v);
        if (auto v = (*tui_tbl)["show_tooltips"].value<bool>()) cfg.tui.show_tooltips = *v;
    }
    
    if (auto tools_tbl = tbl["tools"].as_table()) {
        if (auto v = (*tools_tbl)["web_search"].value<bool>()) cfg.tools.web_search = *v;
    }
    
    if (auto sandbox_tbl = tbl["sandbox_workspace_write"].as_table()) {
        if (auto v = (*sandbox_tbl)["exclude_slash_tmp"].value<bool>()) cfg.sandbox_workspace_write.exclude_slash_tmp = *v;
        if (auto v = (*sandbox_tbl)["exclude_tmpdir_env_var"].value<bool>()) cfg.sandbox_workspace_write.exclude_tmpdir_env_var = *v;
        if (auto v = (*sandbox_tbl)["network_access"].value<bool>()) cfg.sandbox_workspace_write.network_access = *v;
        
        if (auto arr = (*sandbox_tbl)["writable_roots"].as_array()) {
            for (auto&& elem : *arr) {
                if (auto s = elem.value<std::string>()) cfg.sandbox_workspace_write.writable_roots.push_back(*s);
            }
        }
    }
    
    if (auto providers_tbl = tbl["model_providers"].as_table()) {
        for (auto&& [key, value] : *providers_tbl) {
            if (auto provider_tbl = value.as_table()) {
                CodexModelProvider provider;
                if (auto v = (*provider_tbl)["name"].value<std::string>()) provider.name = *v;
                if (auto v = (*provider_tbl)["base_url"].value<std::string>()) provider.base_url = *v;
                if (auto v = (*provider_tbl)["env_key"].value<std::string>()) provider.env_key = *v;
                if (auto v = (*provider_tbl)["env_key_instructions"].value<std::string>()) provider.env_key_instructions = *v;
                if (auto v = (*provider_tbl)["experimental_bearer_token"].value<std::string>()) provider.experimental_bearer_token = *v;
                if (auto v = (*provider_tbl)["wire_api"].value<std::string>()) provider.wire_api = *v;
                if (auto v = (*provider_tbl)["request_max_retries"].value<int64_t>()) provider.request_max_retries = static_cast<int>(*v);
                if (auto v = (*provider_tbl)["stream_max_retries"].value<int64_t>()) provider.stream_max_retries = static_cast<int>(*v);
                if (auto v = (*provider_tbl)["stream_idle_timeout_ms"].value<int64_t>()) provider.stream_idle_timeout_ms = static_cast<int>(*v);
                if (auto v = (*provider_tbl)["requires_openai_auth"].value<bool>()) provider.requires_openai_auth = *v;
                
                if (auto headers_tbl = (*provider_tbl)["http_headers"].as_table()) {
                    for (auto&& [k, val] : *headers_tbl) {
                        if (auto s = val.value<std::string>()) provider.http_headers[std::string(k)] = *s;
                    }
                }
                
                if (auto headers_tbl = (*provider_tbl)["env_http_headers"].as_table()) {
                    for (auto&& [k, val] : *headers_tbl) {
                        if (auto s = val.value<std::string>()) provider.env_http_headers[std::string(k)] = *s;
                    }
                }
                
                if (auto params_tbl = (*provider_tbl)["query_params"].as_table()) {
                    for (auto&& [k, val] : *params_tbl) {
                        if (auto s = val.value<std::string>()) provider.query_params[std::string(k)] = *s;
                    }
                }
                
                cfg.model_providers[std::string(key)] = provider;
            }
        }
    }
    
    if (auto mcp_tbl = tbl["mcp_servers"].as_table()) {
        for (auto&& [key, value] : *mcp_tbl) {
            if (auto server_tbl = value.as_table()) {
                CodexMcpServer server;
                if (auto v = (*server_tbl)["command"].value<std::string>()) server.command = *v;
                if (auto v = (*server_tbl)["cwd"].value<std::string>()) server.cwd = *v;
                if (auto v = (*server_tbl)["enabled"].value<bool>()) server.enabled = *v;
                if (auto v = (*server_tbl)["url"].value<std::string>()) server.url = *v;
                if (auto v = (*server_tbl)["bearer_token_env_var"].value<std::string>()) server.bearer_token_env_var = *v;
                if (auto v = (*server_tbl)["startup_timeout_sec"].value<int64_t>()) server.startup_timeout_sec = static_cast<int>(*v);
                if (auto v = (*server_tbl)["tool_timeout_sec"].value<int64_t>()) server.tool_timeout_sec = static_cast<int>(*v);
                
                if (auto arr = (*server_tbl)["args"].as_array()) {
                    for (auto&& elem : *arr) {
                        if (auto s = elem.value<std::string>()) server.args.push_back(*s);
                    }
                }
                
                if (auto arr = (*server_tbl)["env_vars"].as_array()) {
                    for (auto&& elem : *arr) {
                        if (auto s = elem.value<std::string>()) server.env_vars.push_back(*s);
                    }
                }
                
                if (auto arr = (*server_tbl)["enabled_tools"].as_array()) {
                    for (auto&& elem : *arr) {
                        if (auto s = elem.value<std::string>()) server.enabled_tools.push_back(*s);
                    }
                }
                
                if (auto arr = (*server_tbl)["disabled_tools"].as_array()) {
                    for (auto&& elem : *arr) {
                        if (auto s = elem.value<std::string>()) server.disabled_tools.push_back(*s);
                    }
                }
                
                if (auto env_tbl = (*server_tbl)["env"].as_table()) {
                    for (auto&& [k, val] : *env_tbl) {
                        if (auto s = val.value<std::string>()) server.env[std::string(k)] = *s;
                    }
                }
                
                if (auto headers_tbl = (*server_tbl)["http_headers"].as_table()) {
                    for (auto&& [k, val] : *headers_tbl) {
                        if (auto s = val.value<std::string>()) server.http_headers[std::string(k)] = *s;
                    }
                }
                
                if (auto headers_tbl = (*server_tbl)["env_http_headers"].as_table()) {
                    for (auto&& [k, val] : *headers_tbl) {
                        if (auto s = val.value<std::string>()) server.env_http_headers[std::string(k)] = *s;
                    }
                }
                
                cfg.mcp_servers[std::string(key)] = server;
            }
        }
    }
    
    if (auto projects_tbl = tbl["projects"].as_table()) {
        for (auto&& [key, value] : *projects_tbl) {
            if (auto project_tbl = value.as_table()) {
                if (auto v = (*project_tbl)["trust_level"].value<std::string>()) {
                    cfg.projects[std::string(key)] = *v;
                }
            }
        }
    }
    
    cfg.extra_fields = tbl;
    
    return cfg;
}

toml::table CodexConfig::to_toml() const {
    toml::table tbl = extra_fields;
    
    if (!approval_policy.empty()) tbl.insert_or_assign("approval_policy", approval_policy);
    if (!model.empty()) tbl.insert_or_assign("model", model);
    if (!model_provider.empty()) tbl.insert_or_assign("model_provider", model_provider);
    
    if (!chatgpt_base_url.empty()) tbl.insert_or_assign("chatgpt_base_url", chatgpt_base_url);
    if (!check_for_update_on_startup) tbl.insert_or_assign("check_for_update_on_startup", check_for_update_on_startup);
    if (cli_auth_credentials_store != "auto") tbl.insert_or_assign("cli_auth_credentials_store", cli_auth_credentials_store);
    if (!compact_prompt.empty()) tbl.insert_or_assign("compact_prompt", compact_prompt);
    if (!developer_instructions.empty()) tbl.insert_or_assign("developer_instructions", developer_instructions);
    if (disable_paste_burst) tbl.insert_or_assign("disable_paste_burst", disable_paste_burst);
    if (!experimental_compact_prompt_file.empty()) tbl.insert_or_assign("experimental_compact_prompt_file", experimental_compact_prompt_file);
    if (!experimental_instructions_file.empty()) tbl.insert_or_assign("experimental_instructions_file", experimental_instructions_file);
    if (file_opener != "vscode") tbl.insert_or_assign("file_opener", file_opener);
    if (!forced_chatgpt_workspace_id.empty()) tbl.insert_or_assign("forced_chatgpt_workspace_id", forced_chatgpt_workspace_id);
    if (!forced_login_method.empty()) tbl.insert_or_assign("forced_login_method", forced_login_method);
    if (hide_agent_reasoning) tbl.insert_or_assign("hide_agent_reasoning", hide_agent_reasoning);
    if (!instructions.empty()) tbl.insert_or_assign("instructions", instructions);
    if (mcp_oauth_credentials_store != "auto") tbl.insert_or_assign("mcp_oauth_credentials_store", mcp_oauth_credentials_store);
    if (model_auto_compact_token_limit) tbl.insert_or_assign("model_auto_compact_token_limit", *model_auto_compact_token_limit);
    if (model_context_window) tbl.insert_or_assign("model_context_window", *model_context_window);
    if (!model_reasoning_effort.empty()) tbl.insert_or_assign("model_reasoning_effort", model_reasoning_effort);
    if (!model_reasoning_summary.empty()) tbl.insert_or_assign("model_reasoning_summary", model_reasoning_summary);
    if (model_supports_reasoning_summaries) tbl.insert_or_assign("model_supports_reasoning_summaries", model_supports_reasoning_summaries);
    if (!model_verbosity.empty()) tbl.insert_or_assign("model_verbosity", model_verbosity);
    if (!oss_provider.empty()) tbl.insert_or_assign("oss_provider", oss_provider);
    if (!profile.empty()) tbl.insert_or_assign("profile", profile);
    if (project_doc_max_bytes) tbl.insert_or_assign("project_doc_max_bytes", *project_doc_max_bytes);
    if (!review_model.empty()) tbl.insert_or_assign("review_model", review_model);
    if (!sandbox_mode.empty()) tbl.insert_or_assign("sandbox_mode", sandbox_mode);
    if (show_raw_agent_reasoning) tbl.insert_or_assign("show_raw_agent_reasoning", show_raw_agent_reasoning);
    if (tool_output_token_limit) tbl.insert_or_assign("tool_output_token_limit", *tool_output_token_limit);
    if (windows_wsl_setup_acknowledged) tbl.insert_or_assign("windows_wsl_setup_acknowledged", windows_wsl_setup_acknowledged);
    
    if (experimental_use_freeform_apply_patch) tbl.insert_or_assign("experimental_use_freeform_apply_patch", experimental_use_freeform_apply_patch);
    if (experimental_use_unified_exec_tool) tbl.insert_or_assign("experimental_use_unified_exec_tool", experimental_use_unified_exec_tool);
    if (include_apply_patch_tool) tbl.insert_or_assign("include_apply_patch_tool", include_apply_patch_tool);
    
    if (!notify.empty()) {
        toml::array arr;
        for (const auto& s : notify) arr.push_back(s);
        tbl.insert_or_assign("notify", arr);
    }
    
    if (!project_doc_fallback_filenames.empty()) {
        toml::array arr;
        for (const auto& s : project_doc_fallback_filenames) arr.push_back(s);
        tbl.insert_or_assign("project_doc_fallback_filenames", arr);
    }
    
    if (!project_root_markers.empty()) {
        toml::array arr;
        for (const auto& s : project_root_markers) arr.push_back(s);
        tbl.insert_or_assign("project_root_markers", arr);
    }
    
    toml::table features_tbl;
    if (features.apply_patch_freeform) features_tbl.insert_or_assign("apply_patch_freeform", features.apply_patch_freeform);
    if (features.elevated_windows_sandbox) features_tbl.insert_or_assign("elevated_windows_sandbox", features.elevated_windows_sandbox);
    if (!features.exec_policy) features_tbl.insert_or_assign("exec_policy", features.exec_policy);
    if (features.experimental_windows_sandbox) features_tbl.insert_or_assign("experimental_windows_sandbox", features.experimental_windows_sandbox);
    if (features.powershell_utf8) features_tbl.insert_or_assign("powershell_utf8", features.powershell_utf8);
    if (!features.remote_compaction) features_tbl.insert_or_assign("remote_compaction", features.remote_compaction);
    if (features.remote_models) features_tbl.insert_or_assign("remote_models", features.remote_models);
    if (features.shell_snapshot) features_tbl.insert_or_assign("shell_snapshot", features.shell_snapshot);
    if (!features.shell_tool) features_tbl.insert_or_assign("shell_tool", features.shell_tool);
    if (features.tui2) features_tbl.insert_or_assign("tui2", features.tui2);
    if (features.unified_exec) features_tbl.insert_or_assign("unified_exec", features.unified_exec);
    if (features.web_search_request) features_tbl.insert_or_assign("web_search_request", features.web_search_request);
    if (!features_tbl.empty()) tbl.insert_or_assign("features", features_tbl);
    
    toml::table feedback_tbl;
    if (!feedback.enabled) feedback_tbl.insert_or_assign("enabled", feedback.enabled);
    if (!feedback_tbl.empty()) tbl.insert_or_assign("feedback", feedback_tbl);
    
    toml::table history_tbl;
    if (history.max_bytes) history_tbl.insert_or_assign("max_bytes", *history.max_bytes);
    if (history.persistence != "save-all") history_tbl.insert_or_assign("persistence", history.persistence);
    if (!history_tbl.empty()) tbl.insert_or_assign("history", history_tbl);
    
    toml::table tui_tbl;
    if (!tui.animations) tui_tbl.insert_or_assign("animations", tui.animations);
    if (tui.notifications) tui_tbl.insert_or_assign("notifications", *tui.notifications);
    if (tui.scroll_events_per_tick) tui_tbl.insert_or_assign("scroll_events_per_tick", *tui.scroll_events_per_tick);
    if (tui.scroll_invert) tui_tbl.insert_or_assign("scroll_invert", tui.scroll_invert);
    if (tui.scroll_mode != "auto") tui_tbl.insert_or_assign("scroll_mode", tui.scroll_mode);
    if (tui.scroll_trackpad_accel_events) tui_tbl.insert_or_assign("scroll_trackpad_accel_events", *tui.scroll_trackpad_accel_events);
    if (tui.scroll_trackpad_accel_max) tui_tbl.insert_or_assign("scroll_trackpad_accel_max", *tui.scroll_trackpad_accel_max);
    if (tui.scroll_trackpad_lines) tui_tbl.insert_or_assign("scroll_trackpad_lines", *tui.scroll_trackpad_lines);
    if (tui.scroll_wheel_like_max_duration_ms) tui_tbl.insert_or_assign("scroll_wheel_like_max_duration_ms", *tui.scroll_wheel_like_max_duration_ms);
    if (tui.scroll_wheel_lines) tui_tbl.insert_or_assign("scroll_wheel_lines", *tui.scroll_wheel_lines);
    if (tui.scroll_wheel_tick_detect_max_ms) tui_tbl.insert_or_assign("scroll_wheel_tick_detect_max_ms", *tui.scroll_wheel_tick_detect_max_ms);
    if (!tui.show_tooltips) tui_tbl.insert_or_assign("show_tooltips", tui.show_tooltips);
    if (!tui_tbl.empty()) tbl.insert_or_assign("tui", tui_tbl);
    
    toml::table tools_tbl;
    if (tools.web_search) tools_tbl.insert_or_assign("web_search", tools.web_search);
    if (!tools_tbl.empty()) tbl.insert_or_assign("tools", tools_tbl);
    
    toml::table sandbox_tbl;
    if (sandbox_workspace_write.exclude_slash_tmp) sandbox_tbl.insert_or_assign("exclude_slash_tmp", sandbox_workspace_write.exclude_slash_tmp);
    if (sandbox_workspace_write.exclude_tmpdir_env_var) sandbox_tbl.insert_or_assign("exclude_tmpdir_env_var", sandbox_workspace_write.exclude_tmpdir_env_var);
    if (sandbox_workspace_write.network_access) sandbox_tbl.insert_or_assign("network_access", sandbox_workspace_write.network_access);
    if (!sandbox_workspace_write.writable_roots.empty()) {
        toml::array arr;
        for (const auto& s : sandbox_workspace_write.writable_roots) arr.push_back(s);
        sandbox_tbl.insert_or_assign("writable_roots", arr);
    }
    if (!sandbox_tbl.empty()) tbl.insert_or_assign("sandbox_workspace_write", sandbox_tbl);
    
    if (!model_providers.empty()) {
        toml::table providers_tbl;
        for (const auto& [key, provider] : model_providers) {
            toml::table provider_tbl;
            if (!provider.name.empty()) provider_tbl.insert_or_assign("name", provider.name);
            if (!provider.base_url.empty()) provider_tbl.insert_or_assign("base_url", provider.base_url);
            if (!provider.env_key.empty()) provider_tbl.insert_or_assign("env_key", provider.env_key);
            if (!provider.env_key_instructions.empty()) provider_tbl.insert_or_assign("env_key_instructions", provider.env_key_instructions);
            if (!provider.experimental_bearer_token.empty()) provider_tbl.insert_or_assign("experimental_bearer_token", provider.experimental_bearer_token);
            if (!provider.wire_api.empty()) provider_tbl.insert_or_assign("wire_api", provider.wire_api);
            if (provider.request_max_retries) provider_tbl.insert_or_assign("request_max_retries", *provider.request_max_retries);
            if (provider.stream_max_retries) provider_tbl.insert_or_assign("stream_max_retries", *provider.stream_max_retries);
            if (provider.stream_idle_timeout_ms) provider_tbl.insert_or_assign("stream_idle_timeout_ms", *provider.stream_idle_timeout_ms);
            if (provider.requires_openai_auth) provider_tbl.insert_or_assign("requires_openai_auth", provider.requires_openai_auth);
            
            if (!provider.http_headers.empty()) {
                toml::table headers;
                for (const auto& [k, v] : provider.http_headers) headers.insert_or_assign(k, v);
                provider_tbl.insert_or_assign("http_headers", headers);
            }
            
            if (!provider.env_http_headers.empty()) {
                toml::table headers;
                for (const auto& [k, v] : provider.env_http_headers) headers.insert_or_assign(k, v);
                provider_tbl.insert_or_assign("env_http_headers", headers);
            }
            
            if (!provider.query_params.empty()) {
                toml::table params;
                for (const auto& [k, v] : provider.query_params) params.insert_or_assign(k, v);
                provider_tbl.insert_or_assign("query_params", params);
            }
            
            if (!provider_tbl.empty()) providers_tbl.insert_or_assign(key, provider_tbl);
        }
        if (!providers_tbl.empty()) tbl.insert_or_assign("model_providers", providers_tbl);
    }
    
    if (!mcp_servers.empty()) {
        toml::table mcp_tbl;
        for (const auto& [key, server] : mcp_servers) {
            toml::table server_tbl;
            if (!server.command.empty()) server_tbl.insert_or_assign("command", server.command);
            if (!server.cwd.empty()) server_tbl.insert_or_assign("cwd", server.cwd);
            if (!server.enabled) server_tbl.insert_or_assign("enabled", server.enabled);
            if (!server.url.empty()) server_tbl.insert_or_assign("url", server.url);
            if (!server.bearer_token_env_var.empty()) server_tbl.insert_or_assign("bearer_token_env_var", server.bearer_token_env_var);
            if (server.startup_timeout_sec) server_tbl.insert_or_assign("startup_timeout_sec", *server.startup_timeout_sec);
            if (server.tool_timeout_sec) server_tbl.insert_or_assign("tool_timeout_sec", *server.tool_timeout_sec);
            
            if (!server.args.empty()) {
                toml::array arr;
                for (const auto& s : server.args) arr.push_back(s);
                server_tbl.insert_or_assign("args", arr);
            }
            
            if (!server.env_vars.empty()) {
                toml::array arr;
                for (const auto& s : server.env_vars) arr.push_back(s);
                server_tbl.insert_or_assign("env_vars", arr);
            }
            
            if (!server.enabled_tools.empty()) {
                toml::array arr;
                for (const auto& s : server.enabled_tools) arr.push_back(s);
                server_tbl.insert_or_assign("enabled_tools", arr);
            }
            
            if (!server.disabled_tools.empty()) {
                toml::array arr;
                for (const auto& s : server.disabled_tools) arr.push_back(s);
                server_tbl.insert_or_assign("disabled_tools", arr);
            }
            
            if (!server.env.empty()) {
                toml::table env;
                for (const auto& [k, v] : server.env) env.insert_or_assign(k, v);
                server_tbl.insert_or_assign("env", env);
            }
            
            if (!server.http_headers.empty()) {
                toml::table headers;
                for (const auto& [k, v] : server.http_headers) headers.insert_or_assign(k, v);
                server_tbl.insert_or_assign("http_headers", headers);
            }
            
            if (!server.env_http_headers.empty()) {
                toml::table headers;
                for (const auto& [k, v] : server.env_http_headers) headers.insert_or_assign(k, v);
                server_tbl.insert_or_assign("env_http_headers", headers);
            }
            
            if (!server_tbl.empty()) mcp_tbl.insert_or_assign(key, server_tbl);
        }
        if (!mcp_tbl.empty()) tbl.insert_or_assign("mcp_servers", mcp_tbl);
    }
    
    if (!projects.empty()) {
        toml::table projects_tbl;
        for (const auto& [key, trust_level] : projects) {
            toml::table project_tbl;
            project_tbl.insert_or_assign("trust_level", trust_level);
            projects_tbl.insert_or_assign(key, project_tbl);
        }
        tbl.insert_or_assign("projects", projects_tbl);
    }
    
    return tbl;
}

void to_json(nlohmann::json& j, const CodexProfile& p) {
    j = nlohmann::json::object();
    j["name"] = p.name;
    j["description"] = p.description;
    
    std::ostringstream oss;
    oss << p.config.to_toml();
    j["config_toml"] = oss.str();
    
    j["created_at"] = p.created_at;
    j["updated_at"] = p.updated_at;
}

void from_json(const nlohmann::json& j, CodexProfile& p) {
    if (j.contains("name") && j["name"].is_string()) {
        p.name = j["name"].get<std::string>();
    }
    if (j.contains("description") && j["description"].is_string()) {
        p.description = j["description"].get<std::string>();
    }
    if (j.contains("config_toml") && j["config_toml"].is_string()) {
        try {
            auto tbl = toml::parse(j["config_toml"].get<std::string>());
            p.config = CodexConfig::from_toml(tbl);
        } catch (...) {
        }
    }
    if (j.contains("created_at") && j["created_at"].is_number()) {
        p.created_at = j["created_at"].get<int64_t>();
    }
    if (j.contains("updated_at") && j["updated_at"].is_number()) {
        p.updated_at = j["updated_at"].get<int64_t>();
    }
}

}
