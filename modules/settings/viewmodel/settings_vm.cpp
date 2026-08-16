// AriaAgent — SettingsVm implementation.
#include "settings/viewmodel/settings_vm.hpp"

#include "i18n/I18n.h"

#include <cstdlib>

SettingsVm::SettingsVm(agent::SettingsStore& store) : store_(store) {
    // Defaults (env overrides, then persisted JSON overrides in load()).
    if (const char* p = std::getenv("ARIA_LLM_BASE_URL"); p && *p) base_url = p;
    else base_url = "https://api.deepseek.com";
    if (const char* p = std::getenv("ARIA_LLM_API_KEY"); p && *p) api_key = p;
    if (const char* p = std::getenv("ARIA_LLM_MODEL"); p && *p) model = p;
    else model = "deepseek-chat";
    if (const char* p = std::getenv("ARIA_LLM_SYSTEM_PROMPT"); p && *p) {
        system_prompt = p;
    } else {
        system_prompt = agent::i18n::str("default_system_prompt");
    }
    theme          = 2;
    language       = "zh-CN";
    streaming      = true;
    enter_behavior = 0;
    preset         = 0;

    // Localized UI text (BaseVm re-pulls on language change).
    text(title, "settings_title");
    text(nav_general, "nav_general");
    text(nav_model, "nav_model");
    text(nav_plugins, "nav_plugins");
    text(nav_presets, "nav_presets");
    text(save_text, "save");
    text(cancel_text, "cancel");
    text(label_system_prompt, "gen_system_prompt");
    text(label_language, "gen_language");
    text(label_appearance, "gen_appearance");
    text(label_enter_behavior, "gen_enter_behavior");
    text(label_streaming, "gen_streaming");
    text(label_base_url, "model_base_url");
    text(label_api_key, "model_api_key");
    text(label_model_name, "model_name");
    text(hint_model, "model_hint");
    text(hint_plugins, "plugins_hint");
    text(hint_presets, "presets_hint");
    text(label_theme_follow, "gen_theme_follow");
    text(label_theme_light, "gen_theme_light");
    text(label_theme_dark, "gen_theme_dark");
    text(label_enter_send, "gen_enter_send");
    text(label_ctrl_enter, "gen_ctrl_enter");
    text(lang_zh, "lang_zh");
    text(lang_en, "lang_en");
    text(plugins_enable, "plugins_enable");
    text(plugins_calculator, "plugins_calculator");
    text(plugins_calculator_desc, "plugins_calculator_desc");
    text(plugins_time, "plugins_time");
    text(plugins_time_desc, "plugins_time_desc");
    text(preset_standard, "preset_standard");
    text(preset_creative, "preset_creative");
    text(preset_minimal, "preset_minimal");

    load();
}

void SettingsVm::load() {
    agent::SettingsValues v;
    v.base_url       = base_url.get();
    v.api_key        = api_key.get();
    v.model          = model.get();
    v.system_prompt  = system_prompt.get();
    v.theme          = theme.get();
    v.language       = language.get();
    v.streaming      = streaming.get();
    v.enter_behavior = enter_behavior.get();
    v.preset         = preset.get();
    store_.load(v);
    base_url       = v.base_url;
    api_key        = v.api_key;
    model          = v.model;
    system_prompt  = v.system_prompt;
    theme          = v.theme;
    language       = v.language;
    streaming      = v.streaming;
    enter_behavior = v.enter_behavior;
    preset         = v.preset;
    // Apply the persisted language to the i18n service (so AppText and the
    // VMs are in the right language at startup).
    agent::i18n::set_language(language.get());
}

void SettingsVm::save() {
    // Preset → system prompt override (unless the user chose standard).
    if (preset.get() == 1) {
        system_prompt = "You are a creative assistant. Think divergently, "
                        "explore unusual angles, and offer original ideas.";
    } else if (preset.get() == 2) {
        system_prompt = "You are a concise assistant. Answer directly and "
                        "briefly, without excess detail.";
    }

    agent::SettingsValues v;
    v.base_url       = base_url.get();
    v.api_key        = api_key.get();
    v.model          = model.get();
    v.system_prompt  = system_prompt.get();
    v.theme          = theme.get();
    v.language       = language.get();
    v.streaming      = streaming.get();
    v.enter_behavior = enter_behavior.get();
    v.preset         = preset.get();
    store_.save(v);

    // Switch the language (BaseVm + AppText refresh automatically).
    agent::i18n::set_language(language.get());

    settings_saved.emit();
}
