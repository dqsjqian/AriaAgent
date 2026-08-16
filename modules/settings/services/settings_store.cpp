// AriaAgent — SettingsStore implementation.
#include "settings_store.hpp"

#include "agent/session_store.hpp"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>

namespace {

// Cross-platform env write (Windows: _putenv_s, POSIX: setenv).
void set_env(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

} // namespace

namespace agent {

SettingsStore::SettingsStore() = default;

std::string SettingsStore::path() const {
    return SessionStore::base_dir() + "/../settings.json";
}

void SettingsStore::load(SettingsValues& out) const {
    std::ifstream f(path());
    if (!f.is_open()) return;
    try {
        const auto doc = nlohmann::json::parse(f);
        if (doc.contains("base_url"))       out.base_url       = doc["base_url"].get<std::string>();
        if (doc.contains("api_key"))        out.api_key        = doc["api_key"].get<std::string>();
        if (doc.contains("model"))          out.model          = doc["model"].get<std::string>();
        if (doc.contains("system_prompt"))  out.system_prompt  = doc["system_prompt"].get<std::string>();
        if (doc.contains("theme"))          out.theme          = doc["theme"].get<int>();
        if (doc.contains("language"))       out.language       = doc["language"].get<std::string>();
        if (doc.contains("streaming"))      out.streaming      = doc["streaming"].get<bool>();
        if (doc.contains("enter_behavior")) out.enter_behavior = doc["enter_behavior"].get<int>();
        if (doc.contains("preset"))         out.preset         = doc["preset"].get<int>();
    } catch (const std::exception&) {
        // Corrupt settings: keep defaults.
    }
}

void SettingsStore::save(const SettingsValues& v) const {
    // Persist JSON.
    {
        nlohmann::json doc;
        doc["base_url"]       = v.base_url;
        doc["api_key"]        = v.api_key;
        doc["model"]          = v.model;
        doc["system_prompt"]  = v.system_prompt;
        doc["theme"]          = v.theme;
        doc["language"]       = v.language;
        doc["streaming"]      = v.streaming;
        doc["enter_behavior"] = v.enter_behavior;
        doc["preset"]         = v.preset;
        std::ofstream f(path(), std::ios::trunc);
        f << doc.dump(2);
    }

    // Inject into the process env so the engine (lazy client) picks it up.
    set_env("ARIA_LLM_BASE_URL", v.base_url);
    set_env("ARIA_LLM_MODEL",    v.model);
    if (!v.api_key.empty()) set_env("ARIA_LLM_API_KEY", v.api_key);
    set_env("ARIA_LLM_SYSTEM_PROMPT", v.system_prompt);
}

} // namespace agent
