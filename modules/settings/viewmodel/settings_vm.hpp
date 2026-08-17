// AriaAgent — SettingsVm: the settings feature module.
//
// Owns preference state as aria Properties (auto-localized via BaseVm),
// persists through the shared SettingsStore service, switches the i18n
// language, and notifies the shell when the theme/language changed so Views
// restyle. Mirrors AiTools' SettingsVm.
#pragma once

#include <string>
#include <vector>

#include <aria/aria.hpp>
#include <aria/detail/typed_signal.hpp>

#include "module_api/BaseVm.h"

#include "settings/services/settings_store.hpp"

class SettingsVm : public BaseVm {
public:
    explicit SettingsVm(agent::SettingsStore& store);

    /// Load persisted values (or defaults); called once at startup.
    void load();

    /// Persist + apply. Writes via the SettingsStore, switches the i18n
    /// language, and emits settings_saved (shell restyles theme/language).
    void save();

    // ── Model page ─────────────────────────────────────────────────────────
    aria::Property<std::string> base_url;
    aria::Property<std::string> api_key;
    aria::Property<std::string> model;
    aria::Property<std::vector<std::string>> models;

    /// Select and persist an already configured model.
    void select_model(const std::string& value);

    // ── General page ───────────────────────────────────────────────────────
    aria::Property<std::string> system_prompt;
    aria::Property<int>         theme;        // 0=system 1=light 2=dark
    aria::Property<std::string> language;     // "zh-CN" / "en"
    aria::Property<bool>        streaming;
    aria::Property<int>         enter_behavior; // 0=Enter send, 1=Ctrl+Enter

    // ── Presets page ───────────────────────────────────────────────────────
    aria::Property<int> preset;               // 0=standard 1=creative 2=minimal

    // ── Localized UI text (auto-refresh on language change via BaseVm) ─────
    aria::Property<std::string> title;
    aria::Property<std::string> nav_general, nav_model, nav_plugins, nav_presets;
    aria::Property<std::string> save_text, cancel_text;
    aria::Property<std::string> label_system_prompt, label_language, label_appearance;
    aria::Property<std::string> label_enter_behavior, label_streaming;
    aria::Property<std::string> label_base_url, label_api_key, label_model_name;
    aria::Property<std::string> hint_model, hint_plugins, hint_presets;
    aria::Property<std::string> label_theme_follow, label_theme_light, label_theme_dark;
    aria::Property<std::string> label_enter_send, label_ctrl_enter;
    aria::Property<std::string> lang_zh, lang_en;
    aria::Property<std::string> plugins_enable, plugins_calculator, plugins_calculator_desc;
    aria::Property<std::string> plugins_time, plugins_time_desc;
    aria::Property<std::string> preset_standard, preset_creative, preset_minimal;

    // Fired after save() — the shell re-reads theme/language and restyles.
    aria::detail::TypedSignal<> settings_saved;

private:
    agent::SettingsStore& store_;
};
