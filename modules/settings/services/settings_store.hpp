// AriaAgent — SettingsStore: preferences persistence service.
//
// Registered in the ServiceHub (settings module). Pure C++ JSON persistence
// to ~/.ariaagent/settings.json (no Qt QSettings) so mobile shells reuse it.
// Values are also injected into the process environment so the engine's
// lazy LLM client picks them up.
#pragma once

#include <string>
#include <vector>

namespace agent {

struct SettingsValues {
    std::string base_url      = "https://api.deepseek.com";
    std::string api_key;
    std::string model         = "deepseek-chat";
    std::vector<std::string> models{"deepseek-chat"};
    std::string system_prompt;
    int         theme         = 2;   // 0=system 1=light 2=dark
    std::string language      = "zh-CN";
    bool        streaming     = true;
    int         enter_behavior= 0;   // 0=Enter send, 1=Ctrl+Enter
    int         preset        = 0;   // 0=standard 1=creative 2=minimal
};

class SettingsStore {
public:
    SettingsStore();

    /// Load persisted values into `out` (keeps existing defaults where the
    /// file has no entry).
    void load(SettingsValues& out) const;

    /// Persist `v` to JSON and inject into the process environment.
    void save(const SettingsValues& v) const;

    /// Apply LLM values to the current process without rewriting the file.
    void apply_runtime(const SettingsValues& v) const;

private:
    std::string path() const;
};

} // namespace agent
